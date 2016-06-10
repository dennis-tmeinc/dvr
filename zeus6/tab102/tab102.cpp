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

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "../ioprocess/diomap.h"
#include <sys/mman.h>

#define MCUCMD_SETRTC              0x07
#define MCUCMD_BOOTREADY           0x08
#define MCUCMD_GETTRIGGER          0x0d
#define MCUCMD_FW_VER              0x0e
#define MCUCMD_ENABLEPEAK          0x0f
#define MCUCMD_SETTRIGGER          0x11
#define MCUCMD_SETTRIGGER2         0x12
#define MCUCMD_GET0G               0x18
#define MCUCMD_UPLOAD              0x19
#define MCUCMD_UPLOADACK           0x1A
#define MCUCMD_ENABLEDI            0x1B
#define MCUCMD_DI_DATA             0x1C
#define MCUCMD_PEAK_DATA           0x1E
#define MCUCMD_UPLOADPEAK          0x1F
#define MCUCMD_UPLOADPEAKACK       0x20

enum {TYPE_CONTINUOUS, TYPE_PEAK};

#define UPLOAD_ACK_SIZE 10

//#define NET_DEBUG
#ifdef NET_DEBUG
#define dprintf(...) writeNetDebug(__VA_ARGS__)
#else
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
//#define dprintf(...)
#endif


char tab102b_port_dev[64] = "/dev/ttyS1";
int tab102b_port_baudrate = 19200;     // tab102b default baud
char hostname[128] = "BUS001";
char tab102b_firmware_version[80];
static int comFd=-1;
struct dio_mmap * p_dio_mmap ;
char dvriomap[256] = "/var/dvr/dvriomap" ;

unsigned int mstarttime=0;

// 0:Front,1:Back, 2:Right, 3:Left, 4:Bottom, 5:Top 
char direction_table[24][3] = 
{
  {0, 2, 0x62}, // Forward:front, Upward:right    Leftward:top
  {0, 3, 0x52}, // Forward:Front, Upward:left,    Leftward:bottom
  {0, 4, 0x22}, // Forward:Front, Upward:bottom,  Leftward:right 
  {0, 5, 0x12}, // Forward:Front, Upward:top,    Leftward:left 
  {1, 2, 0x61}, // Forward:back,  Upward:right,    Leftward:bottom 
  {1, 3, 0x51}, // Forward:back,  Upward:left,    Leftward:top
  {1, 4, 0x21}, // Forward:back,  Upward:bottom,    Leftward:left
  {1, 5, 0x11}, // Forward:back, Upward:top,    Leftward:right 
  {2, 0, 0x42}, // Forward:right, Upward:front,    Leftward:bottom
  {2, 1, 0x32}, // Forward:right, Upward:back,    Leftward:top
  {2, 4, 0x28}, // Forward:right, Upward:bottom,    Leftward:back
  {2, 5, 0x18}, // Forward:right, Upward:top,    Leftward:front
  {3, 0, 0x41}, // Forward:left, Upward:front,    Leftward:top
  {3, 1, 0x31}, // Forward:left, Upward:back,    Leftward:bottom
  {3, 4, 0x24}, // Forward:left, Upward:bottom,    Leftward:front
  {3, 5, 0x14}, // Forward:left, Upward:top,    Leftward:back
  {4, 0, 0x48}, // Forward:bottom, Upward:front,    Leftward:left
  {4, 1, 0x38}, // Forward:bottom, Upward:back,    Leftward:right
  {4, 2, 0x68}, // Forward:bottom, Upward:right,    Leftward:front
  {4, 3, 0x58}, // Forward:bottom, Upward:left,    Leftward:back
  {5, 0, 0x44}, // Forward:top, Upward:front,    Leftward:right
  {5, 1, 0x34}, // Forward:top, Upward:back,    Leftward:left
  {5, 2, 0x64}, // Forward:top, Upward:right,    Leftward:back
  {5, 3, 0x54}  // Forward:top, Upward:left,    Leftward:front
};
////////////////////////////////////////////////////
//add for tab102b setting
#define DEFAULT_DIRECTION   (7)
static int gsensor_direction = DEFAULT_DIRECTION ;
float g_sensor_trigger_forward ;
float g_sensor_trigger_backward ;
float g_sensor_trigger_right ;
float g_sensor_trigger_left ;
float g_sensor_trigger_down ;
float g_sensor_trigger_up ;
float g_sensor_base_forward ;
float g_sensor_base_backward ;
float g_sensor_base_right ;
float g_sensor_base_left ;
float g_sensor_base_down ;
float g_sensor_base_up ;
float g_sensor_crash_forward ;
float g_sensor_crash_backward ;
float g_sensor_crash_right ;
float g_sensor_crash_left ;
float g_sensor_crash_down ;
float g_sensor_crash_up ;

////////////////////////////
int last_gsensor_direction = DEFAULT_DIRECTION ;
float last_g_sensor_trigger_forward ;
float last_g_sensor_trigger_backward ;
float last_g_sensor_trigger_right ;
float last_g_sensor_trigger_left ;
float last_g_sensor_trigger_down ;
float last_g_sensor_trigger_up ;
float last_g_sensor_base_forward ;
float last_g_sensor_base_backward ;
float last_g_sensor_base_right ;
float last_g_sensor_base_left ;
float last_g_sensor_base_down ;
float last_g_sensor_base_up ;
float last_g_sensor_crash_forward ;
float last_g_sensor_crash_backward ;
float last_g_sensor_crash_right ;
float last_g_sensor_crash_left ;
float last_g_sensor_crash_down ;
float last_g_sensor_crash_up ;


unsigned short g_refX, g_refY, g_refZ, g_peakX, g_peakY, g_peakZ;
unsigned char g_order; //, g_live, g_mcudebug, g_task;
int g_fw_ver;

void print_setting(){
 printf("\ng_sensor_trigger_forward:%f\n",g_sensor_trigger_forward);
 printf("g_sensor_trigger_backward:%f\n",g_sensor_trigger_backward);
 printf("g_sensor_trigger_right:%f\n",g_sensor_trigger_right);
 printf("g_sensor_trigger_left:%f\n",g_sensor_trigger_left);
 printf("g_sensor_trigger_down:%f\n",g_sensor_trigger_down);
 printf("g_sensor_trigger_up:%f\n",g_sensor_trigger_up);
 printf("g_sensor_base_forward:%f\n",g_sensor_base_forward);
 printf("g_sensor_base_backward:%f\n",g_sensor_base_backward);
 printf("g_sensor_base_right:%f\n",g_sensor_base_right);
 printf("g_sensor_base_left:%f\n",g_sensor_base_left);
 printf("g_sensor_base_down:%f\n",g_sensor_base_down);
 printf("g_sensor_base_up:%f\n",g_sensor_base_up);
 printf("g_sensor_crash_forward:%f\n",g_sensor_crash_forward);
 printf("g_sensor_crash_backward:%f\n", g_sensor_crash_backward);
 printf("g_sensor_crash_right:%f\n", g_sensor_crash_right);
 printf("g_sensor_crash_left:%f\n", g_sensor_crash_left);
 printf("g_sensor_crash_down:%f\n", g_sensor_crash_down);
 printf("g_sensor_crash_up:%f\n", g_sensor_crash_up);

}
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


/* x: 0 - 99,999 */
int binaryToBCD(int x)
{
  int m, I, b, cc;
  int bcd;

  cc = (x % 100) / 10;
  b = (x % 1000) / 100;
  I = (x % 10000) / 1000;
  m = x / 10000;

  bcd = (m * 9256 + I * 516 + b * 26 + cc) * 6 + x;

  return bcd;
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
  dprintf("%s opened\n", dev);

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

static unsigned char getChecksum(unsigned char *buf, int size)
{
  unsigned char cs = 0;
  int i;

  for (i = 0; i < size; i++) {
    cs += buf[i];
  }

  cs = 0xff - cs;
  cs++;

  return  cs;
}

static int verifyChecksum(unsigned char *buf, int size)
{
  unsigned char cs = 0;
  int i;

  for (i = 0; i < size; i++) {
    cs += buf[i];
  }

  if (cs)
    return 1;

  return  0;
}

static int writeCom(int fd, unsigned char *buf, int size)
{
  int ret;
  int left;
  int total=0;
  if (fd < 0)
    return -1;
  left=size;
  while(left>0){
      ret = write(fd, buf+total, left);
      if (ret < 0) {
	perror("writeCom");
	return -1;
      }
      left-=ret;
      total+=ret;
  }
  dprintf("DM-->MCU(%d)\n", ret);
  dump(buf, ret);

  return ret;
}

// clear receiving buffer
void com_clear(int microsecondsdelay,int fd)
{
    int i;
    char rbuf[1000] ;
    struct timeval tv;
    tv.tv_sec = microsecondsdelay/1000000;
    tv.tv_usec = microsecondsdelay%1000000;
    for(i=0;i<1000;i++) {
        if(blockUntilReadable(fd, &tv) > 0 ) {
            read(fd, rbuf, sizeof(rbuf));
        }
        else {
            break;
        }
    }
}

void zero_com(int fd)
{
    unsigned char sbuf[256] ;
    memset(sbuf,0,256);
    writeCom(fd,sbuf,256);
}

int set_mcuReboot()
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );              // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return 0;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
        p_dio_mmap->mcu_cmd = 5;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        return 1 ;
    }
    return 0 ;
}

int sendReset(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // dest addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = 00; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendBootReset(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x05; // len
  txbuf[bi++] = 0x01; // dest addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = 0x04; // cmd
  txbuf[bi++] = 0x02; // req

  return writeCom(fd, txbuf, bi);
}

int sendUpdateFirmware(int fd)
{
  int bi;
  unsigned char txbuf[32];
  bi = 0;
  txbuf[bi++] = 0x05; // len
  txbuf[bi++] = 0x01; // dest addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = 01; // cmd
  txbuf[bi++] = 0x02; // req

  return writeCom(fd, txbuf, bi);
}


int tab102_recvmsg( unsigned char * recv, int size,int fd)
{
    int n;
    int bytes=0;
    int total=0;
    int left=0;
    int times=0;
    int i=0;
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if( blockUntilReadable(fd, &tv) > 0 ) {
        read(fd,recv, 1);
        n=(int) *recv ;
       // printf("n=%02x ",n);
        if( n>=5 && n<=size ) {
            total=1;
            left=n-1;
            while(times<10){
                if(blockUntilReadable(fd, &tv) > 0){
			bytes=read(fd,recv+total,left);
			/*
                        for(i=0;i<bytes;++i){
                           printf("%02x ",recv[total+i]);
                        }
                        */
			total+=bytes;
			left=left-bytes;
			if(left==0)
			break;
                }
                times++;
            }
          //  printf("n=%d %d\n",n,recv[0]);
            if( n==recv[0]){
                if((recv[1]==0) &&(recv[2]==4)){
                   if( verifyChecksum(recv, n) == 0)
                     return n;
                  // else
                  //   printf("check sum wrong\n");
                } else if((recv[0]==5)&&(recv[1]==0)&&(recv[2]==1)){
                    return n;
                }
            }

        }
    }
    com_clear(100,fd);
    return 0 ;
}

/* return:
 *    n: # of bytes received
 *   -1: bad size parameters
 */  

int read_nbytes(int fd, unsigned char *buf, int bufsize,
		int rx_size, int timeout_in_secs,
		int showdata, int showprogress)
{
  struct timeval tv;
  int total = 0, bytes;

  if (bufsize < rx_size)
    return -1;

  while (1) {
    bytes = 0;
    tv.tv_sec = timeout_in_secs;
    tv.tv_usec = 0;
    if (blockUntilReadable(fd, &tv) > 0) {
      bytes = read(fd, buf + total, bufsize - total);
      if (bytes > 0) {
	if (showdata)
	  dump(buf + total, bytes);
	total += bytes;
	if (showprogress) dprintf(".");
      }

      if (total >= rx_size)
	break;
    }

    if (bytes <= 0) { // timeout without receiving any data
      break;
    }
  }

  return total;
}


int tab102_recv_enteracommand(int fd)
{
    int res = 0 ;
    int bytes;
    int i=0;
    unsigned char enteracommand[200] ;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    while(blockUntilReadable(fd, &tv) > 0 ) {
        memset( enteracommand, 0, sizeof(enteracommand));
	bytes=  read(fd,enteracommand,sizeof(enteracommand));
        for(i=0;i<bytes;++i){
            printf("%02x ",enteracommand[i]);
        }
        printf("\n");
        if( strcasestr( (char*)enteracommand, "Enter a command" )) {
            res=1 ;
        }
    }
    return res ;
}


int burn_firmware( char * firmwarefile) 
{
    int res = 0 ;
    FILE * fwfile ;
    char c ;
    int rd ;
    int i=0;
    int retry;
    unsigned char responds[200] ;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    fwfile=fopen(firmwarefile, "rb");
    
    if( fwfile==NULL ) {
        return 0 ;                  // failed, can't open firmware file
    }
    // 
    printf("Start update tab102b firmware: %s\n", firmwarefile);

    res=0;
    comFd = openCom(tab102b_port_dev, 115200);
    if (comFd == -1) {
	printf("can't reopen com port failed\n");
	return 0;
    }
    retry=5;
        //sleep(2);
    while(retry>0){
	if(tab102_recv_enteracommand(comFd)){
		res=1;
		break;
	}
	retry--;
    }
    if(res==0){
        sendBootReset(comFd);
	retry=3;
	while(retry>0){
		if(tab102_recv_enteracommand(comFd)){
			res=1;
			break;
		}
		retry--;
	}
    }
    if(res==0){
	printf("can't get enter a command\n");
	return 0;     
    }
 
    printf("Done.\n");

    // clean out serial buffer
    memset( responds, 0, sizeof(responds)) ;
    writeCom(comFd, responds, sizeof(responds));
    com_clear(1000000,comFd);
    
    
    printf("Erasing.\n");
    rd=0 ;
    sleep(2);
    if( sendUpdateFirmware(comFd) ) {
         retry=5;
         while(retry>0){

         	rd=tab102_recvmsg( responds, sizeof(responds),comFd);
                if(rd>0)
                    break;
                retry--;
         }
    }
    for(i=0;i<rd;++i){
       printf("%02x ",responds[i]);
    }
    printf("\n rd=%d\n",rd);
    if( rd>=5 && 
       responds[rd-5]==0x05 && 
       responds[rd-4]==0x0 && 
       responds[rd-3]==0x01 && 
       responds[rd-2]==0x01 && 
       responds[rd-1]==0x03 ) {
       // ok ;
       printf("Done.\n");
       res=0 ;
    }
    else {
       printf("Failed.\n");
       return 0;                    // can't start fw update
    }
    
    printf("Programming.\n");
    // send firmware 
    while(1){
       rd=fread(responds,1,16,fwfile);
       if(rd>0){
           writeCom(comFd, responds,rd);
       } else {
          break;
       }
       usleep(100);
    } //while(1);

    fclose( fwfile );
    close(comFd);
    sleep(5);
    return 1 ;
}


int RequestReset(int fd)
{
  int retry = 5;
  int i;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendReset(fd);
    for(i=0;i<3;++i){
	if (read_nbytes(fd, buf, sizeof(buf), 6, 3, 1, 0) >= 6) {
	  if ((buf[0] == 0x06) &&
	      (buf[1] == 0x00) &&
	      (buf[2] == 0x04) &&
	      (buf[3] == 00) &&
	      (buf[4] == 0x03)) {
	    if (verifyChecksum(buf, 6)) {
	      dprintf("checksum error\n");
	    } else {	      
	      return 0;
	    }
	  }
	}
    }
    sleep(1);
    retry--;
  }

  return -1;
}

int update_firmware( char * firmwarefile,int fd) 
{
    int res = 0 ;
    FILE * fwfile ;
    char c ;
    int rd ;
    int i=0;
    int retry;
    unsigned char responds[200] ;
    struct timeval tv;

    fwfile=fopen(firmwarefile, "rb");
    
    FILE* fw=fopen("/home/www/updatelog","w");
    if( fwfile==NULL ) {
        return 0 ;                  // failed, can't open firmware file
    }
    // 
    fprintf(fw,"Start update tab102b firmware: %s\n", firmwarefile);
    
    // reset mcu
    printf("Reset Tab102.\n");
    res=0;
    
    if(RequestReset(fd)==0){
        close(comFd);
        comFd = openCom(tab102b_port_dev, 115200);
        if (comFd == -1) {
            fprintf(fw,"can't reopen com port failed\n");
            return 0;
        }
        fd=comFd;
        retry=5;
        //sleep(2);
        while(retry>0){
		if(tab102_recv_enteracommand(fd)){
                        res=1;
			break;
		}
                retry--;
        }
        if(res==0){
                sendBootReset(fd);
                retry=10;
		while(retry>0){
			if(tab102_recv_enteracommand(fd)){
				res=1;
				break;
			}
			retry--;
		}
        }
	if(res==0){
		fprintf(fw,"can't get enter a command\n");
		return 0;     
	}
    } 
    if(res==0){
        fprintf(fw,"Reset Failed\n");
        return 0;     
    }

    fprintf(fw,"Reset Done.\n");
    
    // clean out serial buffer
    memset( responds, 0, sizeof(responds)) ;
 //   writeCom(fd, responds, sizeof(responds));
//    com_clear(1000000,fd);
    
    
    fprintf(fw,"Erasing.\n");
    rd=0 ;
    sleep(2);
    if( sendUpdateFirmware(fd) ) {
         retry=15;
         while(retry>0){
	        tv.tv_sec = 1;
                tv.tv_usec = 0;
	       if(blockUntilReadable(fd, &tv) > 0){
		  rd=read(fd,responds,sizeof(responds));
		  if(rd>0)
		      break;
	       }
                retry--;
         }
    }
    for(i=0;i<rd;++i){
       fprintf(fw,"%02x ",responds[i]);
    }
    fprintf(fw,"\n");
    if( rd>=5 && 
       responds[rd-5]==0x05 && 
       responds[rd-4]==0x0 && 
       responds[rd-3]==0x01 && 
       responds[rd-2]==0x01 && 
       responds[rd-1]==0x03 ) {
       // ok ;
       fprintf(fw,"Erase Done.\n");
       res=0 ;
    }
    else {
       fprintf(fw,"Erase Failed.\n");
       fclose(fw);
       return 0;                    // can't start fw update
    }
    
    fprintf(fw,"Programming.\n");
    // send firmware 
    while(1){
       rd=fread(responds,1,16,fwfile);
       if(rd>0){
           //writeCom(fd, responds,rd);
           write(fd, responds,rd);
       } else {
          break;
       }
       usleep(100);
    } //while(1);

    fclose( fwfile );
    sleep(5);
    fprintf(fw,"program done!");
    fclose(fw);
    return 1 ;
}


int sendUploadRequest(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // dest addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_UPLOAD; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendUploadPeakRequest(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // dest addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_UPLOADPEAK; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendSetRTC(int fd, struct tm *t)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x0d; // len
  txbuf[bi++] = 0x04; // dest addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_SETRTC; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_sec);  // sec
  txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_min);  // min
  txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_hour); // hour
  txbuf[bi++] = t->tm_wday + 1; // day of week
  txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_mday); // day
  txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_mon + 1); // month
  txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_year - 100); // year
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendBootReady(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_BOOTREADY; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendUploadAck(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x07; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_UPLOADACK; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = 0x01;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendUploadPeakAck(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x07; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_UPLOADPEAKACK; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = 0x01;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}


int sendSetTrigger(int fd)
{
  int bi;
  unsigned char txbuf[64];
  int v;
  int value;
  bi = 0;
  txbuf[bi++] = 0x2b; // len
  txbuf[bi++] = 0x04; // Tab module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = 0x12; //MCUCMD_SETTRIGGER; // cmd
  txbuf[bi++] = 0x02; // req

  // base trigger
  v = (int)((g_sensor_base_forward - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_base_backward - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_base_right - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_base_left - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_base_down - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_base_up - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;

  // peak trigger
  v = (int)((g_sensor_trigger_forward - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_trigger_backward - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_trigger_right - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_trigger_left - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_trigger_down - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_trigger_up - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;

  // crash trigger
  v = (int)((g_sensor_crash_forward - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_crash_backward - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_crash_right - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_crash_left - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_crash_down - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  v = (int)((g_sensor_crash_up - 0.0) * 37.0) + 0x200;
  txbuf[bi++] = (v >> 8) & 0xff;
  txbuf[bi++] =  v & 0xff;
  txbuf[bi++]= direction_table[gsensor_direction][2]; // direction
  
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;


  return writeCom(fd, txbuf, bi);
}

/*
 * return:
 *  - 0: success, 1: error
 */
int setTrigger(int fd)
{
  int retry = 3;
  int recok=0;
  int i=0;
  struct timeval tv;
  struct tm bt;
  unsigned char buf[1024];
//  print_setting();
  while (retry > 0) {
    gettimeofday(&tv, NULL);
    com_clear(100000,fd);      
   // localtime_r(&tv.tv_sec, &bt);
    tcflush(fd, TCIOFLUSH);
    sendSetTrigger(fd);
    recok=0;
    for(i=0;i<3;++i){
	if (read_nbytes(fd, buf, sizeof(buf), 6, 3, 1, 0) >= 6) {
	  if ((buf[0] == 0x06) &&
	      (buf[1] == 0x00) &&
	      (buf[2] == 0x04) &&
	      (buf[3] == 0x12) &&
	      (buf[4] == 0x03)) {
	    if (verifyChecksum(buf, 6)) {
	      dprintf("checksum error\n");
	    } else {
	        recok=1;
		break;
	    }
	  }
	}
    }
    if(recok)
      break;
    sleep(1);
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int sendGetTrigger(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_GETTRIGGER; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}
int getTrigger(int fd)
{
  int retry = 3;
  int bytes;
  int value=0;
  int done=0;
  struct timeval tv;
  struct tm bt;
  unsigned char buf[1024];
  while (retry > 0&&!done) {
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &bt);
    tcflush(fd, TCIOFLUSH);
    sendGetTrigger(fd);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if(blockUntilReadable(fd, &tv) > 0){
       bytes=tab102_recvmsg( buf, sizeof(buf),fd);
       printf("bytes=%d\n",bytes);
       if(bytes>0x20){
          if((buf[1]==0x04)&&
             (buf[2]==0x00)&&
             (buf[3]==MCUCMD_GETTRIGGER)&&
             (buf[4]==0x03)){
             //get base forward
             value=(int)((buf[5]<<8)|buf[6]);
             printf("base forward:%x %x %d\n",buf[5],buf[6],value);
             //get base backward
             value=(int)((buf[7]<<8)|buf[8]);
             printf("base backward:%x %x %d\n",buf[7],buf[8],value);
             //get base right
             value=(int)((buf[9]<<8)|buf[10]);
             printf("base right:%x %x %d\n",buf[9],buf[10],value);
             //get base left
             value=(int)((buf[11]<<8)|buf[12]);
             printf("base left:%x %x %d\n",buf[11],buf[12],value);
              //get base down
             value=(int)((buf[13]<<8)|buf[14]);
             printf("base down:%x %x %d\n",buf[13],buf[14],value);
             //get base up
             value=(int)((buf[15]<<8)|buf[16]);
             printf("base up:%x %x %d\n",buf[15],buf[16],value);
              ///////////////////////////////////////////////////////////
             //get peak value;
             //get peak forward
             value=(int)((buf[17]<<8)|buf[18]);
             printf("peak forward:%x %x %d\n",buf[17],buf[18],value);
             //get peak backward
             value=(int)((buf[19]<<8)|buf[20]);
             printf("peak backward:%x %x %d\n",buf[19],buf[20],value);
             //get peak right
             value=(int)((buf[21]<<8)|buf[22]);
             printf("peak right:%x %x %d\n",buf[21],buf[22],value);
             //get peak left
             value=(int)((buf[23]<<8)|buf[24]);
             printf("peak left:%x %x %d\n",buf[23],buf[24],value);
              //get peak down
             value=(int)((buf[25]<<8)|buf[26]);
             printf("peak down:%x %x %d\n",buf[25],buf[26],value);
             //get peakup
             value=(int)((buf[27]<<8)|buf[28]);
             printf("peak up:%x %x %d\n",buf[27],buf[28],value);

             /////////////////////////////////////////////////////////////
             //get crash value;
             //get crash forward
             value=(int)((buf[29]<<8)|buf[30]);
             printf("crash forward:%x %x %d\n",buf[29],buf[30],value);
             //get crash backward
             value=(int)((buf[31]<<8)|buf[32]);
             printf("crash backward:%x %x %d\n",buf[31],buf[32],value);
             //get crash right
             value=(int)((buf[33]<<8)|buf[34]);
             printf("crash right:%x %x %d\n",buf[33],buf[34],value);
             //get crash left
             value=(int)((buf[35]<<8)|buf[36]);
             printf("crash left:%x %x %d\n",buf[35],buf[36],value);
              //get crash down
             value=(int)((buf[37]<<8)|buf[38]);
             printf("crash down:%x %x %d\n",buf[37],buf[38],value);
             //get crash up
             value=(int)((buf[39]<<8)|buf[40]);
             printf("crash up:%x %x %d\n",buf[39],buf[40],value);
             done=1;
          }
       }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int isConfigureChanged()
{
  if(last_gsensor_direction !=gsensor_direction)
    return 1;
  if(last_g_sensor_trigger_forward !=g_sensor_trigger_forward)
    return 1;
  if(last_g_sensor_trigger_backward !=g_sensor_trigger_backward)
    return 1;
  if(last_g_sensor_trigger_right !=g_sensor_trigger_right)
    return 1;
  if(last_g_sensor_trigger_left !=g_sensor_trigger_left)
    return 1;
  if(last_g_sensor_trigger_down !=g_sensor_trigger_down)
    return 1;
  if(last_g_sensor_trigger_up !=g_sensor_trigger_up)
    return 1;
  if(last_g_sensor_base_forward !=g_sensor_base_forward)
    return 1;
  if(last_g_sensor_base_backward !=g_sensor_base_backward)
    return 1;
  if(last_g_sensor_base_right !=g_sensor_base_right)
    return 1;
  if(last_g_sensor_base_left !=g_sensor_base_left)
    return 1;
  if(last_g_sensor_base_down !=g_sensor_base_down)
    return 1;
  if(last_g_sensor_base_up !=g_sensor_base_up)
    return 1;
  if(last_g_sensor_crash_forward !=g_sensor_crash_forward)
    return 1;
  if(last_g_sensor_crash_backward !=g_sensor_crash_backward)
    return 1;
  if(last_g_sensor_crash_right !=g_sensor_crash_right)
    return 1;
  if(last_g_sensor_crash_left !=g_sensor_crash_left)
    return 1;
  if(last_g_sensor_crash_down !=g_sensor_crash_down)
    return 1;
  if(last_g_sensor_crash_up !=g_sensor_crash_up)
    return 1;
  return 0;
  
}

void ChangeStoredValue()
{
  last_gsensor_direction =gsensor_direction ;
  last_g_sensor_trigger_forward =g_sensor_trigger_forward ;
  last_g_sensor_trigger_backward =g_sensor_trigger_backward;
  last_g_sensor_trigger_right =g_sensor_trigger_right;
  last_g_sensor_trigger_left =g_sensor_trigger_left;
  last_g_sensor_trigger_down =g_sensor_trigger_down;
  last_g_sensor_trigger_up =g_sensor_trigger_up;
  last_g_sensor_base_forward =g_sensor_base_forward;
  last_g_sensor_base_backward =g_sensor_base_backward;
  last_g_sensor_base_right =g_sensor_base_right;
  last_g_sensor_base_left =g_sensor_base_left;
  last_g_sensor_base_down =g_sensor_base_down;
  last_g_sensor_base_up =g_sensor_base_up;
  last_g_sensor_crash_forward =g_sensor_crash_forward;
  last_g_sensor_crash_backward =g_sensor_crash_backward;
  last_g_sensor_crash_right =g_sensor_crash_right;
  last_g_sensor_crash_left =g_sensor_crash_left;
  last_g_sensor_crash_down =g_sensor_crash_down;
  last_g_sensor_crash_up =g_sensor_crash_up;      
}

void reloadconfig()
{
  int i,fd;
  string v ;
  config dvrconfig(CFG_FILE);
  
  // get tab102b serial port setting
  v = dvrconfig.getvalue( "system", "hostname");
  if( v.length()>0 ) {
    strncpy( hostname, v.getstring(), sizeof(hostname) );
  } 
    
  i = dvrconfig.getvalueint( "glog", "tab102b_enable");
  if (!i)
    exit(0);
    
  // get tab102b serial port setting
  v = dvrconfig.getvalue( "glog", "tab102b_port");
  if( v.length()>0 ) {
    strcpy( tab102b_port_dev, v.getstring() );
  } 
    
  i = dvrconfig.getvalueint("glog", "tab102b_baudrate");
  if( i>=1200 && i<=115200 ) {
    tab102b_port_baudrate=i ;
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
  //////////////////////////////////////////////
  g_sensor_trigger_forward = 0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_forward);
  }
  g_sensor_trigger_backward = -0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_backward);
  }
  g_sensor_trigger_right = 0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_right);
  }
  g_sensor_trigger_left = -0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_left);
  }
  g_sensor_trigger_down = 1.0+2.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_down);
  }
  g_sensor_trigger_up = 1.0-2.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_up);
  }
    
  g_sensor_base_forward = 0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_forward);
  }
  g_sensor_base_backward = -0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_backward);
  }
  g_sensor_base_right = 0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_right);
  }
  g_sensor_base_left = -0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_left);
  }
  g_sensor_base_down = 1.0+2.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_down);
  }
  g_sensor_base_up = 1.0-2.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_up);
  }
    
  g_sensor_crash_forward = 3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_forward);
  }
  g_sensor_crash_backward = -3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_backward);
  }
  g_sensor_crash_right = 3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_right);
  }
  g_sensor_crash_left = -3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_left);
  }
  g_sensor_crash_down = 1.0+5.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_down);
  }
  g_sensor_crash_up = 1.0-5.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_up);
  }
  
  int dforward, dupward ;
  dforward = dvrconfig.getvalueint( "io", "gsensor_forward");	
  dupward  = dvrconfig.getvalueint( "io", "gsensor_upward");	
  gsensor_direction = DEFAULT_DIRECTION ;
  for(i=0; i<24; i++) {
      if( dforward == direction_table[i][0] && dupward == direction_table[i][1] ) {
	  gsensor_direction = i ;
	  break;
      }
  }   
}

void appinit() {
  int i,fd;
  string v ;
  config dvrconfig( CFG_FILE );
  
  // get tab102b serial port setting
  v = dvrconfig.getvalue( "system", "hostname");
  if( v.length()>0 ) {
    strncpy( hostname, v.getstring(), sizeof(hostname) );
  } 
    
  i = dvrconfig.getvalueint( "glog", "tab102b_enable");
  if (!i)
    exit(0);
    
  // get tab102b serial port setting
  v = dvrconfig.getvalue( "glog", "tab102b_port");
  if( v.length()>0 ) {
    strcpy( tab102b_port_dev, v.getstring() );
  } 
    
  i = dvrconfig.getvalueint("glog", "tab102b_baudrate");
  if( i>=1200 && i<=115200 ) {
    tab102b_port_baudrate=i ;
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
  tzset();
  //////////////////////////////////////////////
  g_sensor_trigger_forward = 0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_forward);
  }
  g_sensor_trigger_backward = -0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_backward);
  }
  g_sensor_trigger_right = 0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_right);
  }
  g_sensor_trigger_left = -0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_left);
  }
  g_sensor_trigger_down = 1.0+2.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_down);
  }
  g_sensor_trigger_up = 1.0-2.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_up);
  }
    
  g_sensor_base_forward = 0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_forward);
  }
  g_sensor_base_backward = -0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_backward);
  }
  g_sensor_base_right = 0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_right);
  }
  g_sensor_base_left = -0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_left);
  }
  g_sensor_base_down = 1.0+2.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_down);
  }
  g_sensor_base_up = 1.0-2.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_up);
  }
    
  g_sensor_crash_forward = 3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_forward);
  }
  g_sensor_crash_backward = -3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_backward);
  }
  g_sensor_crash_right = 3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_right);
  }
  g_sensor_crash_left = -3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_left);
  }
  g_sensor_crash_down = 1.0+5.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_down);
  }
  g_sensor_crash_up = 1.0-5.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_up);
  }
  
  int dforward, dupward ;
  dforward = dvrconfig.getvalueint( "io", "gsensor_forward");	
  dupward  = dvrconfig.getvalueint( "io", "gsensor_upward");	
  gsensor_direction = DEFAULT_DIRECTION ;
  for(i=0; i<24; i++) {
      if( dforward == direction_table[i][0] && dupward == direction_table[i][1] ) {
	  gsensor_direction = i ;
	  break;
      }
  }  
  last_gsensor_direction =gsensor_direction ;
  last_g_sensor_trigger_forward =g_sensor_trigger_forward ;
  last_g_sensor_trigger_backward =g_sensor_trigger_backward;
  last_g_sensor_trigger_right =g_sensor_trigger_right;
  last_g_sensor_trigger_left =g_sensor_trigger_left;
  last_g_sensor_trigger_down =g_sensor_trigger_down;
  last_g_sensor_trigger_up =g_sensor_trigger_up;
  last_g_sensor_base_forward =g_sensor_base_forward;
  last_g_sensor_base_backward =g_sensor_base_backward;
  last_g_sensor_base_right =g_sensor_base_right;
  last_g_sensor_base_left =g_sensor_base_left;
  last_g_sensor_base_down =g_sensor_base_down;
  last_g_sensor_base_up =g_sensor_base_up;
  last_g_sensor_crash_forward =g_sensor_crash_forward;
  last_g_sensor_crash_backward =g_sensor_crash_backward;
  last_g_sensor_crash_right =g_sensor_crash_right;
  last_g_sensor_crash_left =g_sensor_crash_left;
  last_g_sensor_crash_down =g_sensor_crash_down;
  last_g_sensor_crash_up =g_sensor_crash_up;  
  
  //open the io map
  v = dvrconfig.getvalue( "system", "iomapfile");

  if( v.length() > 0 ) {
    strncpy( dvriomap, v.getstring(), sizeof(dvriomap));
  }
  
  for( i=0; i<10; i++ ) {				// retry 10 times
    fd = open(dvriomap, O_RDWR );
    if( fd>0 ) {
      break ;
    }
    sleep(1);
  }
  if( fd<0 ) {
    return;
  }
  
  p=(char *)mmap( NULL, sizeof(struct dio_mmap),
		  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
  close( fd );		// fd no more needed
  if( p==(char *)-1 || p==NULL ) {
    return;
  }
  p_dio_mmap = (struct dio_mmap *)p ;   
  p_dio_mmap->usage++;
}


void writeTab102Status(char *status)
{
  FILE *fp;
  fp = fopen("/var/dvr/tab102", "w");
  if (fp) {
    fprintf(fp, "%s\n", status);
    fclose(fp);
  }
}

/*
 * return:
 *  - 0: success, 1: error
 */
int setTab102RTC(int fd)
{
  int retry = 10;
  int recok=0;
  int i=0;
  int mreset=0;
  static struct timeval tm;
  int starttime0;
  struct timeval tv;
  struct tm bt;
  unsigned char buf[1024];

  while (retry > 0) {
    gettimeofday(&tv, NULL);
    com_clear(100000,fd);   
    tcflush(fd, TCIOFLUSH);    
    localtime_r(&tv.tv_sec, &bt);
    sendSetRTC(fd, &bt);
    recok=0;
    for(i=0;i<3;++i){
	if (read_nbytes(fd, buf, sizeof(buf), 6, 3, 1, 0) >= 6) {
	  // printf("rtc:%x %x %x %x\n",buf[0],buf[1],buf[2],buf[3]);
	  if ((buf[0] == 0x06) &&
	      (buf[1] == 0x00) &&
	      (buf[2] == 0x04) &&
	      (buf[3] == MCUCMD_SETRTC) &&
	      (buf[4] == 0x03)) {
	    if (verifyChecksum(buf, 6)) {
	      dprintf("checksum error\n");
	    } else {
	      writeTab102Status("rtc");
	      recok=1;
	      break;
	    }
	  }
	}	
    }
    if(recok){
        if(retry==10)
          break;
	if(mreset)
	  break;
        RequestReset(fd);
	gettimeofday(&tm, NULL);
	starttime0=tm.tv_sec;    
	while(1){
           sleep(1);
	   gettimeofday(&tm, NULL);
	   if((tm.tv_sec-starttime0)>6)
	     break;
	}
	mreset=1;
    }
    retry--;
    sleep(1);
  }

  return (retry > 0) ? 0 : 1;
}



int sendVersion(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = 0x0e; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

// get MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int tab102_version(char * version,int fd)
{
    unsigned char responds[1024] ;
    int  sz ;
    int  n=0;
    int retry=10;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    while(retry>0){
        retry--;
        com_clear(100000,fd);
        if(sendVersion(fd)) {
                sz=tab102_recvmsg( responds, sizeof(responds),fd);
		if( sz > 6 ) {
			if( responds[3]=='\x0e' &&
			responds[4]=='\x03' ) 
			{
				memcpy( version, &responds[5], sz-6 );
				version[sz-6]=0 ;
				g_fw_ver = (toupper(responds[20]) << 8) | toupper(responds[22]);				
				return sz-6 ;
			}
		}
        }
    }
    return 0 ;
}

int Tab102_sendGforceAck(int fd)
{
   int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = 0x1e; // cmd
  txbuf[bi++] = 0x03; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);   
}

void Tab102_convertPeakData(unsigned short peakX,
		     unsigned short peakY,
		     unsigned short peakZ,
		     float *f_peakFB,
		     float *f_peakLR,
		     float *f_peakUD)
{
  unsigned short refFB = 0x200, refLR = 0x200, refUD = 0x200;
  unsigned short peakFB = peakX, peakLR = peakY, peakUD = peakZ;

  *f_peakFB = ((int)peakFB - (int)refFB) / 37.0;
  *f_peakLR = ((int)peakLR - (int)refLR) / 37.0;
  *f_peakUD = ((int)peakUD - (int)refUD) / 37.0;
}

void Tab102_checkPeakdata(unsigned char* ibuf)
{
     float gback, gright, gbuttom ;
     float gbusforward, gbusright, gbusdown ; 
     struct timeval timenow;
     Tab102_convertPeakData((ibuf[5] << 8) | ibuf[6],
		    (ibuf[7] << 8) | ibuf[8],
		    (ibuf[9] << 8) | ibuf[10],
		    &gbusforward, &gbusright, &gbusdown);	
#if 0
	  printf("before: [5]:%x [6]:%x [7]:%x [8]:%x [9]:%x [10]:%x\n",ibuf[5],ibuf[6],ibuf[7],ibuf[8],ibuf[9],ibuf[10]);
	      printf("Accelerometer, converted right=%.2f , forward=%.2f , down  =%.2f .\n",     
	      gbusright,
	      gbusforward,
	      gbusdown );
#endif        

      // save to log
      if( p_dio_mmap->gforce_log0==0 ) {
	  p_dio_mmap->gforce_right_0 = gbusright ;
	  p_dio_mmap->gforce_forward_0 = gbusforward ;
	  p_dio_mmap->gforce_down_0 = gbusdown ;
	  p_dio_mmap->gforce_log0 = 1 ;
      }
      else {
	  p_dio_mmap->gforce_right_1 = gbusright ;
	  p_dio_mmap->gforce_forward_1 = gbusforward ;
	  p_dio_mmap->gforce_down_1 = gbusdown ;
	  p_dio_mmap->gforce_log1 = 1 ;
      }                
      p_dio_mmap->gforce_right_d=gbusright;
      p_dio_mmap->gforce_forward_d=gbusforward;
      p_dio_mmap->gforce_down_d=gbusdown;
      p_dio_mmap->gforce_changed=1;
      gettimeofday(&timenow, NULL);
      mstarttime=timenow.tv_sec;
#if 0
     printf("p_dio_mmap->gforce_right_d=%0.2f gforce_forward_d=%0.2f  gforce_down_d=%0.2f\n", \
           p_dio_mmap->gforce_right_d,  \
	   p_dio_mmap->gforce_forward_d,  \
	   p_dio_mmap->gforce_down_d);
#endif
}


int Tab102_input(int usdelay,int fd)
{
    int res = 0 ;
    int n ;
    int repeat ;
    unsigned char ibuf[50] ;
    int udelay = usdelay ;
    struct timeval  timenow;
    for(repeat=0; repeat<10; repeat++ ) {            
            n = tab102_recvmsg( ibuf, sizeof(ibuf),fd ) ;     // mcu_recv() will process mcu input messages
            if( n>0 ) {
#if 0	      
	        printf("n=%d=====\n",n);
		int i=0;
		for(i=0;i<n;++i)
		   printf("%x ",ibuf[i]);
#endif		
                if((ibuf[0]=='\x0c') &&
		   (ibuf[1]=='\x00') &&
	           (ibuf[2]=='\x04') &&
		   (ibuf[4]=='\x02') &&
		   (ibuf[3]== '\x1e' )) {    		  
                   Tab102_sendGforceAck(fd);			  
		//   printf("process packet\n");
                   Tab102_checkPeakdata(ibuf);
	   
                }                
            } else {
	      break; 
	    }
    }
    gettimeofday(&timenow, NULL);
 //   printf("timenow:%d mstarttime:%d\n",timenow.tv_sec,mstarttime);
    if(timenow.tv_sec-mstarttime>5){
        p_dio_mmap->gforce_changed=0;
	//printf("gforce changed is 0\n");
	p_dio_mmap->gforce_changed=0;
	p_dio_mmap->gforce_forward_d=0.0;
	p_dio_mmap->gforce_down_d=0.0;
	p_dio_mmap->gforce_right_d=0.0;	
    }
    return res;
}

int is_correct_version_byte(unsigned char b)
{
  if (((b >= '0') && (b <= '9')) || 
      ((b >= 'A') && (b <= 'F')) || 
      ((b >= 'a') && (b <= 'f'))) {
    return 1;
  }

  return 0;
}

int sendVersionRequest(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_FW_VER; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int getFirmwareVersion(int fd)
{
  int retry = 3;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendVersionRequest(fd);

    if (read_nbytes(fd, buf, sizeof(buf), 24, 1, 1, 0) >= 6) {
      if ((buf[0] == 0x18) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_FW_VER) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 24)) {
	  dprintf("checksum error\n");
	} else {
	  if ((buf[19] == 'V') &&
	      is_correct_version_byte(buf[20]) && 
	      (buf[21] == '.') &&  
	      is_correct_version_byte(buf[22])) {
	    g_fw_ver = (toupper(buf[20]) << 8) | toupper(buf[22]);
	    dprintf("FW_VER:%x\n", g_fw_ver);
	  }
	  break;
	}
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}


void sendUploadConfirm(int fd)
{
  int retry = 10;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendUploadAck(fd);

    if (read_nbytes(fd, buf, sizeof(buf), 6, 3, 1, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_UPLOADACK) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 6)) {
	  dprintf("checksum error\n");
	} else {
	  dprintf("Upload confirm Acked\n");
	  break;
	}
      }
    }
    retry--;
  }
}

void sendUploadPeakConfirm(int fd)
{
  int retry = 5;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendUploadPeakAck(fd);

    if (read_nbytes(fd, buf, sizeof(buf), 6, 20, 1, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_UPLOADPEAKACK) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 6)) {
	  dprintf("checksum error\n");
	} else {
	  dprintf("Upload Peak confirm Acked\n");
	  break;
	}
      }
    }
    retry--;
  }
}

static size_t safe_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret = 0;
  
  do {
    clearerr(stream);
    ret += fwrite((char *)ptr + (ret * size), size, nmemb - ret, stream);
  } while (ret < nmemb && ferror(stream) && errno == EINTR);
  
  return ret;
}

static char *safe_fgets(char *s, int size, FILE *stream)
{
  char *ret;
  
  do {
    clearerr(stream);
    ret = fgets(s, size, stream);
  } while (ret == NULL && ferror(stream) && errno == EINTR);
  
  return ret;
}

void writeTab102Data(unsigned char *buf, int len, int type)
{
  char filename[256];
  char curdisk[256];
  char path[256];
  int copy=0;
  int r = 0;
  struct tm tm;
  FILE *fp = fopen("/var/dvr/dvrcurdisk", "r");
  if (fp != NULL) {
    fscanf(fp,"%s",&curdisk);
 //   r = fread(curdisk, 1, sizeof(curdisk) - 1, fp);
   // curdisk[r]='\0';
    fclose(fp);
  } else{
    dprintf("no /var/dvr/dvrcurdisk");
    return;
  }
 // printf("curdisk:%s\n",curdisk);
  snprintf(path, sizeof(path), "%s/smartlog", curdisk);
  if(mkdir(path, 0777) == -1 ) {
    if(errno == EEXIST) {
      copy = 1; // directory exists
    }
  } else {
    copy = 1; // directory created
  }
  time_t t = time(NULL);
  localtime_r(&t, &tm);
#if 1
  snprintf(filename, sizeof(filename),
	   "%s/smartlog/%s_%04d%02d%02d%02d%02d%02d_TAB102log_L.%s",
	   curdisk,hostname,
	   tm.tm_year + 1900,
	   tm.tm_mon + 1,
	   tm.tm_mday,
	   tm.tm_hour,
	   tm.tm_min,
	   tm.tm_sec,
	   (type == TYPE_CONTINUOUS) ? "log" : "peak");
#endif
 // FILE *fp;   
  dprintf("opening %s len=%d\n", filename,len);
  fp = fopen(filename, "w");
  if (fp) {
    dprintf("OK\n");
    safe_fwrite(buf, 1, len, fp);
    fclose(fp);
  } else{
    dprintf("open failed\n");
  }
}

int checkContinuousData(int fd)
{
  int retry = 5;
  unsigned char buf[1024];
  int nbytes, surplus = 0, uploadSize;
  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    printf("send upload request\n");
    sendUploadRequest(fd);
    
    nbytes = read_nbytes(fd, buf, sizeof(buf), UPLOAD_ACK_SIZE, 3, 0, 0);
    printf("check continus data:%d bytes\n",nbytes);
    int i=0;
    for(i=0;i<nbytes;++i)
      printf("%x ",buf[i]);
    printf("\n");
    if (nbytes >= UPLOAD_ACK_SIZE) {
      if ((buf[0] == 0x0a) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_UPLOAD) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, UPLOAD_ACK_SIZE)) {
	  dprintf("checksum error\n");
	} else {
	  uploadSize = (buf[5] << 24) | (buf[6] << 16) | 
	    (buf[7] << 8) | buf[8];
	  dprintf("UPLOAD acked:%d\n", uploadSize);
	  if (uploadSize) {
	    //1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
	    // + 8(0g + checksum)
	    int bufsize = uploadSize + 1024;
	    unsigned char *tbuf = (unsigned char *)malloc(bufsize);
	    if (!tbuf)
	      return 1;
  
	    // just in case we got more data 
	    if (nbytes > UPLOAD_ACK_SIZE) {
	      surplus = nbytes - UPLOAD_ACK_SIZE;
	      memcpy(tbuf, buf + UPLOAD_ACK_SIZE, surplus);
	      //dprintf("surplus moved:%d\n", surplus);
	    }

	    nbytes = read_nbytes(fd, tbuf + surplus, bufsize - surplus,
				 uploadSize + UPLOAD_ACK_SIZE + 8 - surplus,
				 3, 0, 1);
	    int downloaded = nbytes + surplus;
	    dprintf("UPLOAD data:%d\n", downloaded);
	    if (downloaded >= uploadSize + UPLOAD_ACK_SIZE + 8) {
	      if (!verifyChecksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
		writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
				TYPE_CONTINUOUS);
		sendUploadConfirm(fd);				
		writeTab102Status("data");
	      }
	    }
	    free(tbuf);
	  } else {
	     sendUploadConfirm(fd);		
	  }
	  break;
	}      
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int checkPeakData(int fd)
{
  int retry = 5;
  unsigned char buf[1024];
  int nbytes, surplus = 0, uploadSize;
  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    printf("send upload peak request\n");
    sendUploadPeakRequest(fd);
    nbytes = read_nbytes(fd, buf, sizeof(buf), UPLOAD_ACK_SIZE, 20, 0, 0);
    printf("checkPeakData:%d bytes\n", nbytes);
    int i=0;
    for(i=0;i<nbytes;++i)
      printf("%x ",buf[i]);
    printf("\n");
    if (nbytes >= UPLOAD_ACK_SIZE) {
      if ((buf[0] == 0x0a) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_UPLOADPEAK) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, UPLOAD_ACK_SIZE)) {
	  dprintf("checksum error\n");
	} else {
	  uploadSize = (buf[5] << 24) | (buf[6] << 16) | 
	    (buf[7] << 8) | buf[8];
	  dprintf("UPLOAD acked:%d\n", uploadSize);
	  if (uploadSize) {
	    //1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
	    // + 8(0g + direction + checksum)
	    int bufsize = uploadSize + 1024;
	    unsigned char *tbuf = (unsigned char *)malloc(bufsize);
	    if (!tbuf)
	      return 1;
  
	    // just in case we got more data 
	    if (nbytes > UPLOAD_ACK_SIZE) {
	      surplus = nbytes - UPLOAD_ACK_SIZE;
	      memcpy(tbuf, buf + UPLOAD_ACK_SIZE, surplus);
	      //dprintf("surplus moved:%d\n", surplus);
	    }
	    int downloaded;
	    downloaded = surplus;

	    if (surplus < uploadSize + UPLOAD_ACK_SIZE + 8) {
	      nbytes = read_nbytes(fd, tbuf + surplus, bufsize - surplus,
				   uploadSize + UPLOAD_ACK_SIZE + 8 - surplus,
				   20, 0, 1);
	      //dprintf("%d bytes\n", nbytes);
	      downloaded = nbytes + surplus;
	    }
	    dprintf("UPLOAD data:%d\n", downloaded);
	    if (downloaded >= uploadSize + UPLOAD_ACK_SIZE + 8) {
	      if (!verifyChecksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
		writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
				TYPE_PEAK);
		sendUploadPeakConfirm(fd);				
		writeTab102Status("data");
	      }
	    }
	    free(tbuf);
	  }
	  break;
	}      
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int startADC(int fd)
{
  int retry = 10;
  int i=0;
  int recok=0;
  unsigned char buf[1024];

  while (retry > 0) {
    com_clear(100000,fd);      
    tcflush(fd, TCIOFLUSH);
    printf("send boot ready\n");
    sendBootReady(fd);
    recok=0;
    for(i=0;i<3;++i){
	if (read_nbytes(fd, buf, sizeof(buf), 6, 3, 1, 0) >= 6) {
	  printf("startADC:%x %x %x %x\n",buf[0],buf[1],buf[2],buf[3]);
	  if ((buf[0] == 0x06) &&
	      (buf[1] == 0x00) &&
	      (buf[2] == 0x04) &&
	      (buf[3] == MCUCMD_BOOTREADY) &&
	      (buf[4] == 0x03)) {
	    if (verifyChecksum(buf, 6)) {
	      dprintf("checksum error\n");
	    } else {
	      dprintf("ADC started\n");
	      recok=1;
	      break;
	    }
	  }
	}	
    }
    if(recok)
      break;
    sleep(1);
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int send0GRequest(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_GET0G; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int get0G(int fd)
{
  int retry = 3;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    send0GRequest(fd);

    if (read_nbytes(fd, buf, sizeof(buf), 13, 1, 1, 0) >= 6) {
      if ((buf[0] == 0x0d) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_GET0G) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 13)) {
	  dprintf("checksum error\n");
	} else {
	  g_refX = (buf[5] << 8) | buf[6];
	  g_refY = (buf[7] << 8) | buf[8];
	  g_refZ = (buf[9] << 8) | buf[10];
	  g_order = buf[11];
	  dprintf("0G x:%d, y:%d, z:%d, 0x%x\n", g_refX,g_refY,g_refZ, g_order);
	  break;
	}
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int sendEnablePeak(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_ENABLEPEAK; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int enablePeak(int fd)
{
  int retry = 10;
  int i=0;
  int recok=0;
  unsigned char buf[1024];

  while (retry > 0) {
    com_clear(100000,fd);  
    tcflush(fd, TCIOFLUSH);
    sendEnablePeak(fd);
    recok=0;
    for(i=0;i<3;++i){
	if (read_nbytes(fd, buf, sizeof(buf), 6, 3, 1, 0) >= 6) {
	  if ((buf[0] == 0x06) &&
	      (buf[1] == 0x00) &&
	      (buf[2] == 0x04) &&
	      (buf[3] == MCUCMD_ENABLEPEAK) &&
	      (buf[4] == 0x03)) {
	    if (verifyChecksum(buf, 6)) {
	      dprintf("checksum error\n");
	    } else {
	      dprintf("peak enabled\n");
	      recok=1;
	      break;
	    }
	  }
	}
    }
    if(recok)
      break;
    sleep(1);
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int checkTab102()
{
  char line[64];
  FILE *fp;   
  char *str = NULL;

  fp = fopen("/var/dvr/tab102check", "r");
  if (fp) {
    str = safe_fgets(line, sizeof(line), fp);
    fclose(fp);
  }
  if (str) {
    return 1;
  }
  return 0;
}

int checkTriggerset()
{
  FILE *fp;   
  fp = fopen("/var/dvr/triggerset", "r");
  if (fp) {
    fclose(fp);
    unlink("/var/dvr/triggerset");
    return 1;
  }
  return 0;  
  
}
int setTabLiveFlag()
{
  FILE* fw;
  pid_t tabpid=getpid();
  fw=fopen("/var/dvr/tab102.pid","w");
  if(fw){
    fprintf(fw,"%d",(int)tabpid);
    fclose(fw);
  }
  p_dio_mmap->tab102_isLive=1;
  p_dio_mmap->tab102pid=tabpid; 
}

int main(int argc, char **argv)
{

  unsigned char buf[1024];
  FILE* fw=NULL;
  static struct timeval tm;
  int starttime0,count;
#ifdef NET_DEBUG
  connectDebug();
  writeNetDebug("tab102 started");
#endif
  if(argc>=3){
    printf("argv[1]:%s  argv[2]:%s\n",argv[1],argv[2]);
    if( strcasecmp( argv[1], "-tw" )==0 ){
        burn_firmware( argv[2]);
        return 0;
    }
  }
  appinit();
  fw=fopen("/var/dvr/tab102log","w");   
  comFd = openCom(tab102b_port_dev, tab102b_port_baudrate);
  if (comFd == -1) {
     fprintf(fw,"open Com port failed\n");
     fclose(fw);   
     exit(1);
  }

  //check update tab102b firmware
#if 1 
  if( argc>=3 ) {
	if( strcasecmp( argv[1], "-fw" )==0 ) {
		if( argv[2] && update_firmware( argv[2],comFd) ) {
                        set_mcuReboot();
			return 0 ;
		}
		else {
                     //   set_mcuReboot();
			return 1;
		}
	}
  }
#endif
  if ((argc >= 2) && !strcmp(argv[1], "-rtc")) {
 
       // zero_com(comFd);
	if (setTab102RTC(comFd)) {
		fprintf(fw,"set rtc failed\n");
		fclose(fw);
		exit(2);
	}
#if 0	
	if (checkContinuousData(comFd)) {
		printf("check continuous data failed\n");
	// exit(3);
	}
	
	if (checkPeakData(comFd)) {
		printf("check peak data failed\n");
	//  exit(4);
	}
#endif	

	if( tab102_version( tab102b_firmware_version,comFd ) ) {
	// printf("MCU: %s\n", mcu_firmware_version );
		FILE * tab102versionfile=fopen("/var/dvr/tab102version", "w");
		if( tab102versionfile ) {
			fprintf( tab102versionfile, "%s", tab102b_firmware_version );
			fclose( tab102versionfile );
		}
	} else{
		fprintf(fw,"can't get tab102b version\n");
		fclose(fw);
		exit(3);
	}
        if(setTrigger(comFd)){
	   fprintf(fw,"setTrigger failed\n"); 
	}
	
       // get0G(comFd);	
	gettimeofday(&tm, NULL);
	starttime0=tm.tv_sec;    
	while(1){
           sleep(1);
	   gettimeofday(&tm, NULL);
	   if((tm.tv_sec-starttime0)>10)
	     break;
	}
	
 	if(enablePeak(comFd)){
	   fprintf(fw,"enable peak failed\n"); 
	   fclose(fw);
	   exit(4);
	}   
		
	if (startADC(comFd)) {
		fprintf(fw,"start adc failed\n");
		fclose(fw);
		exit(5);
	}	
	setTabLiveFlag();
        while(1){
	  Tab102_input(5000,comFd);	  
	  if (checkTab102()) {
	     fprintf(fw,"checkContinuousData\n"); 
	     checkContinuousData(comFd);
	     unlink("/var/dvr/tab102check");
	     break;
	  }
	  
	  if(checkTriggerset()){
	     reloadconfig();
	     if(isConfigureChanged()){
	        setTrigger(comFd);
		ChangeStoredValue();
	     }
	  }
       }
	 
  } else if((argc >= 2) && !strcmp(argv[1], "-st")){
      setTrigger(comFd);
  } else if((argc >= 2) && !strcmp(argv[1], "-gt")){
      getTrigger(comFd);
  } else if((argc >= 2) && !strcmp(argv[1], "-reset")) {
     sendReset(comFd); 
  } else {
    if (checkTab102()) {
	if (checkContinuousData(comFd)) {
		printf("check continuous data failed\n");
		fclose(fw);
		exit(3);
	}
	if (checkPeakData(comFd)) {
		printf("check peak data failed\n");
		fclose(fw);
		exit(4);
	}
    }
  }
  if(fw)
    fclose(fw); 
  sendReset(comFd); 
  close(comFd);

  return 0;
}

