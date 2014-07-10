#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"


#define NET_DEBUG
#ifdef NET_DEBUG
#define dprintf(...) writeNetDebug(__VA_ARGS__)
#else
#define dprintf(...)
//#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#endif


#define GPSBUFLEN 2048

char gps_port_dev[64] = "/dev/ttyS0";
int gps_port_baudrate = 4800;
char hostname[128] = "BUS001";
unsigned char g_gpsBuf[GPSBUFLEN];
int g_gpsBufLen;

#ifdef NET_DEBUG
int dfd = -1;

int connectTcp(char *addr, short port)
{
  int sfd;
  struct sockaddr_in destAddr;

  sfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    return -1;

  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(port);
  destAddr.sin_addr.s_addr = inet_addr(addr);
  memset(&(destAddr.sin_zero), '\0', 8);

  if (connect(sfd, (struct sockaddr *)&destAddr,
	      sizeof(struct sockaddr)) == -1) {
    close(sfd);
    return -1;
  }

  return sfd;
}

void connectDebug()
{
  dfd = connectTcp("192.168.1.220", 11330);
}

void writeNetDebug(char *fmt, ...)
{
  va_list vl;
  char str[256];

  va_start(vl, fmt);
  vsprintf(str, fmt, vl);
  va_end(vl);

  if (dfd != -1) {
    send(dfd, str, strlen(str) + 1, MSG_NOSIGNAL);
  }
}
#endif

#define WIDTH   16
#define DBUFSIZE 1024
int dump(unsigned char *s, int len)
{
    char buf[DBUFSIZE],lbuf[DBUFSIZE],rbuf[DBUFSIZE];
    unsigned char *p;
    int line,i;

    p =(unsigned char *)s;
    for(line = 1; len > 0; len -= WIDTH, line++) {
      memset(lbuf, '\0', DBUFSIZE);
      memset(rbuf, '\0', DBUFSIZE);
      for(i = 0; i < WIDTH && len > i; i++,p++) {
	sprintf(buf,"%02x ",(unsigned char) *p);
	strcat(lbuf,buf);
	sprintf(buf,"%c",(!iscntrl(*p) && *p <= 0x7f) ? *p : '.');
	strcat(rbuf,buf);
      }
      dprintf("%04x: %-*s    %s\n",line - 1, WIDTH * 3, lbuf, rbuf);
    }
    if(!(len%16)) {
	dprintf("\n");
    }
    return line;
}

/* return:
 *   -1: select error
 *    0: timeout
 *    1: data available
 *
 */
static int blockUntilReadable(int socket, struct timeval* timeout) {
  int result = -1;
  do {
    fd_set rd_set;
    FD_ZERO(&rd_set);
    if (socket < 0) break;
    FD_SET((unsigned) socket, &rd_set);
    const unsigned numFds = socket+1;

    /* call select only when no data is available right away */
    unsigned int cbData = 0;
    if (!ioctl(socket, FIONREAD, &cbData) && (cbData > 0)) {
      result = 1;
      break;
    }

    result = select(numFds, &rd_set, NULL, NULL, timeout);
    if (timeout != NULL && result == 0) {
      break; // this is OK - timeout occurred
    } else if (result <= 0) {
#if defined(__WIN32__) || defined(_WIN32)
#else
      if (errno == EINTR || errno == EAGAIN) continue;
#endif
      dprintf("select() error: \n");
      break;
    }
    
    if (!FD_ISSET(socket, &rd_set)) {
      dprintf("select() error - !FD_ISSET\n");
      break;
    }
  } while (0);

  return result;
}


speed_t get_baudrate(int speed)
{
  speed_t baudrate = 0;
  switch (speed) {
  case 110:
    baudrate = B110;
    break;
  case 300:
    baudrate = B300;
    break;
  case 600:
    baudrate = B600;
    break;
  case 1200:
    baudrate = B1200;
    break;
  case 2400:
    baudrate = B2400;
    break;
  case 4800:
    baudrate = B4800;
    break;
  case 9600:
    baudrate = B9600;
    break;
   case 19200:
    baudrate = B19200;
    break;
  case 38400:
    baudrate = B38400;
    break;
  case 57600:
    baudrate = B57600;
    break;
  case 115200:
    baudrate = B115200;
    break;
  default:
    break;
  }

  return baudrate;
}

int openCom(char *dev, int speed)
{
  int fd;
  struct termios tio;
  speed_t baudrate;
  
  baudrate = get_baudrate(speed);
  if (!baudrate) {
    return -1;
  }

  fd = open((char *)dev, O_RDWR | O_NOCTTY/* | O_NDELAY*/);
  if (fd == -1) {
    perror(dev);
    return -1;
  }
  dprintf("%s(%d) opened", dev, fd);

  tcgetattr(fd, &tio);

  /* set speed */
  cfsetispeed(&tio, baudrate);
  cfsetospeed(&tio, baudrate);
  tio.c_cflag |= (CLOCAL | CREAD); /* minimum setting */
  /* 8N1 setting */
  tio.c_cflag &= ~PARENB; /* No parity */
  tio.c_cflag &= ~CSTOPB; /* 1 stop bit */
  tio.c_cflag &= ~CSIZE; /* Mask character size bits and */
  tio.c_cflag |= CS8; /* set the size to 8 bit */
    
  tio.c_cflag &= ~CRTSCTS; /* Use CNEW_RTSCTS, No hardware flowcontrol */
  tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* raw input */
  tio.c_iflag &= ~(IXON | IXOFF | IXANY| ICRNL); /* no software flowcontrol */
  tio.c_oflag &= ~OPOST; /* raw output */
    
  tio.c_cc[VINTR] = 0;
  tio.c_cc[VQUIT] = 0;
  tio.c_cc[VERASE] = 0;
  tio.c_cc[VKILL] = 0;
  tio.c_cc[VEOF] = 4;
  tio.c_cc[VTIME] = 0;
  tio.c_cc[VMIN] = 1; /* minimum number of characters to read */
  tio.c_cc[VSWTC] = 0;
  tio.c_cc[VSTART] = 0;
  tio.c_cc[VSTOP] = 0;
  tio.c_cc[VSUSP] = 0;
  tio.c_cc[VEOL] = 0;
  tio.c_cc[VREPRINT] = 0;
  tio.c_cc[VDISCARD] = 0;
  tio.c_cc[VWERASE] = 0;
  tio.c_cc[VLNEXT] = 0;
  tio.c_cc[VEOL2] = 0;
  
  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &tio);

  return fd;
}

void appinit() {
  int i;
  string s ;
  config dvrconfig("/etc/dvr/dvr.conf");

  // get serial port setting
  s = dvrconfig.getvalue( "system", "hostname");
  if( s.length()>0 ) {
    strncpy( hostname, s.getstring(), sizeof(hostname) );
  } 
    
  i = dvrconfig.getvalueint( "glog", "gpsdisable");
  if (i)
    exit(0);

  s = dvrconfig.getvalue( "glog", "serialport");
  if( s.length()>0 ) {
    strcpy( gps_port_dev, s.getstring() );
  } 
    
  i = dvrconfig.getvalueint("glog", "serialbaudrate");
  if( i>=1200 && i<=115200 ) {
    gps_port_baudrate=i ;
  }
    
  // initialize time zone
  string tz ;
  string tzi ;
  char *p ;

  tz=dvrconfig.getvalue( "system", "timezone" );
  if( tz.length()>0 ) {
      tzi=dvrconfig.getvalue( "timezones", tz.getstring() );
      if( tzi.length()>0 ) {
	p=strchr(tzi.getstring(), ' ' ) ;
	if( p ) {
	  *p=0;
	}
	p=strchr(tzi.getstring(), '\t' ) ;
	if( p ) {
	  *p=0;
	}
	setenv("TZ", tzi.getstring(), 1);
      }
      else {
	setenv("TZ", tz.getstring(), 1);
      }
  }
}

void write_fifo(char *line)
{
  static int gpsfifo=0 ;
  if( gpsfifo<=0 ) {
    gpsfifo=open( "/davinci/gpsfifo", O_WRONLY | O_NONBLOCK );
  }
  if( gpsfifo>0 ) {
    int len = strlen(line);
    if( write( gpsfifo, line, len )!=len ) {
      close(gpsfifo);
      gpsfifo=0;
    }
  }
}

unsigned int handle_gps(int mcufd, unsigned char *buf, int len)
{
  char *token, *str;
  unsigned char *usedUpto = NULL;

  //fprintf(stderr, "handle_gps(len:%d,buflen:%d)\n", len,g_gpsBufLen);
  //dump(buf, len);

  if ((len > 100) ||
      (g_gpsBufLen + len > (int)sizeof(g_gpsBuf))) { /* just in case */
    fprintf(stderr, "gps data too big?\n");
    g_gpsBufLen = 0;
    return 1;
  }

  memcpy(g_gpsBuf + g_gpsBufLen, buf, len);
  g_gpsBufLen += len;
  if (g_gpsBufLen >= (int)sizeof(g_gpsBuf))
    g_gpsBufLen = sizeof(g_gpsBuf) - 1;
  g_gpsBuf[g_gpsBufLen] = '\0';
  //dump(g_gpsBuf, g_gpsBufLen);

  //fprintf(stderr, "strtok:%s\n", (char *)g_gpsBuf);
  str = (char *)g_gpsBuf;
  while (1) {
    token = strstr(str, "\r\n");
    //fprintf(stderr, "token:%p\n", token);
    if (!token)
      break;
    
    token[1] = '\0';
    //fprintf(stderr, "token:%s\n", str);
    
    usedUpto = (unsigned char *)token + 1;
    //fprintf(stderr, "usedUpto:%p, str:%02x\n", usedUpto, *str);
    if (*str == '$') {
      if (!strncmp(str, "$GPRMC", 6)) {
	dprintf(str); 
	//handle_rmc(str, (int)usedUpto - (int)str);
      } else if (!strncmp(str, "$GPGGA", 6)) {
	dprintf(str); 
	//handle_gga(mcufd, str, (int)usedUpto - (int)str);
      }
    }
    
    str = token + 2; /* next statement */
    if (str >= (char *)g_gpsBuf + g_gpsBufLen)
      break;
  }

  if (usedUpto) {
    if (usedUpto < (g_gpsBuf + g_gpsBufLen)) {
      int copyLen = (int)g_gpsBuf + (int)g_gpsBufLen - (int)usedUpto - 1;
      //fprintf(stderr, "moving %d bytes\n", copyLen);
      memmove(g_gpsBuf, usedUpto + 1, copyLen);
      g_gpsBufLen =  copyLen;
      //dump(g_gpsBuf, g_gpsBufLen);
    } else {
      fprintf(stderr,"clearing len:%p,%p,%d\n",usedUpto,g_gpsBuf,g_gpsBufLen);
      g_gpsBufLen = 0; /* something wrong */
    }
  }

  return 0;
}

void closeall(int fd)
{
  int fdlimit = sysconf(_SC_OPEN_MAX);

  while (fd < fdlimit)
    close(fd++);
}

int daemon(int nochdir, int noclose)
{
  switch (fork()) {
  case 0: break;
  case -1: return -1;
  default: _exit(0);
  }

  if (setsid() < 0)
    return -1;

  switch (fork()) {
  case 0: break;
  case -1: return -1;
  default: _exit(0);
  }

  if (!nochdir)
    chdir("/");

  if (!noclose) {
    closeall(0);
    open("/dev/null", O_RDWR);
    dup(0); dup(0);
  }

  return 0;
}

int main(int argc, char **argv)
{
  int comFd;
  int bytes;
  struct timeval tv;
  unsigned char buf[1024];
  
  if (daemon(0, 0) < 0) {
    perror("daemon");
    exit(1);
  }
  
#ifdef NET_DEBUG
  connectDebug();
#endif
  dprintf("glog2 started");

  appinit();

  // check if serial device is stdin ?
  struct stat stdinstat ;
  struct stat devstat ;
  int r1, r2 ;
  r1 = fstat( 0, &stdinstat ) ; // get stdin stat
  r2 = stat( gps_port_dev, &devstat ) ;
  
  dprintf("%d,%d,%d,%d,%d,%d",r1,r2,stdinstat.st_dev,devstat.st_dev,stdinstat.st_ino,devstat.st_ino);
  if( r1==0 && r2==0 && stdinstat.st_dev == devstat.st_dev && stdinstat.st_ino == devstat.st_ino ) { // matched
    dprintf("stdin");
  }

  comFd = openCom(gps_port_dev, gps_port_baudrate);
  if (comFd == -1) {
    exit(1);
  }

  while (1) {
#if 0
    bytes = 0;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (blockUntilReadable(comFd, &tv) > 0) {
      bytes = read(comFd, buf, sizeof(buf));
      if (bytes > 0) {
	handle_gps(comFd, buf, bytes);
      }
    }
#endif
    bytes = read(comFd, buf, sizeof(buf));
    if (bytes > 0) {
      handle_gps(comFd, buf, bytes);
    }
  }
  
  close(comFd);
  
  return 0;
}

