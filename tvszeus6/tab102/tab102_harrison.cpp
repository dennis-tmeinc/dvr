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
#include <sys/sem.h>
#include <sys/mman.h>
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "../ioprocess/diomap.h"

#define MCUCMD_SETRTC              0x07
#define MCUCMD_BOOTREADY           0x08
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
enum {TASK_UNKNOWN, TASK_PREDOWN, TASK_START, TASK_DOWN};

#define MCULEN 0 /* index of mcu length in the packet */
#define MCUCMD 3 /* index of mcu command in the packet */
#define MAXMCUDATA 12 /* MCU-->DM: MCU_CMD_PEAK_DATA is the longest */
#define UPLOAD_ACK_SIZE 10
#define MAX_TAB_INPUT 12
#define TAB_INPUT_START 6 /* 7th input */

//#define NET_DEBUG
#ifdef NET_DEBUG
#define dprintf(...) writeNetDebug(__VA_ARGS__)
#else
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
//#define dprintf(...)
#endif

#define MCU_BUF_SIZE (4 * 1024)
unsigned char mcubuf[MCU_BUF_SIZE];
int mcudatasize;
#define DEFAULT_DIRECTION   (7)
int gsensor_direction = DEFAULT_DIRECTION ;

unsigned int input_map_table [MAX_TAB_INPUT] = {
  (1<<7),     // bit 0
  (1<<6),     // bit 1
  (1<<5),     // bit 2
  (1<<4),     // bit 3
  (1<<3),     // bit 4
  (1<<2),     // bit 5
  (1<<1),     // bit 6
  1,          // bit 7
  (1<<8),    // bit 8
  (1<<9),    // bit 9
  (1<<10),    // bit 10
  (1<<11),    // bit 11
} ;

char tab102b_port_dev[64] = "/dev/ttyUSB1";
int tab102b_port_baudrate = 19200;     // tab102b default baud
char hostname[128] = "BUS001";
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
unsigned short g_refX, g_refY, g_refZ, g_peakX, g_peakY, g_peakZ;
unsigned char g_order, g_live, g_mcudebug, g_task;
int g_mcu_recverror, g_fw_ver;
struct dio_mmap * p_dio_mmap ;
char dvriomap[256] = "/var/dvr/dvriomap" ;

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

int semid2, semid3;

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
union semun {
  int val;
  struct semid_ds *buf;
  unsigned short int *array;
  struct seminfo *__buf;
};
#endif

static void prepare_semaphore()
{
  key_t unique_key;
  union semun options;

  /* for shared inputmap */ 
  unique_key = ftok("/dev/null", 'b');
  semid2 = semget(unique_key, 1, IPC_CREAT | IPC_EXCL | 0666);

  if (semid2 == -1) {
    if (errno == EEXIST) {
      semid2 = semget(unique_key, 1, 0);
      if (semid2 == -1) {
	fprintf(stderr, "semget error(%s)\n", strerror(errno));
	exit(1);
      }
    } else {
      fprintf(stderr, "semget error(%s)\n", strerror(errno));
      exit(1);
    }
  } else {
    options.val = 1;
    semctl(semid2, 0, SETVAL, options);
  }

  /* for shared gforce peak value */ 
  unique_key = ftok("/dev/null", 'c');
  semid3 = semget(unique_key, 1, IPC_CREAT | IPC_EXCL | 0666);

  if (semid3 == -1) {
    if (errno == EEXIST) {
      semid3 = semget(unique_key, 1, 0);
      if (semid3 == -1) {
	fprintf(stderr, "semget error(%s)\n", strerror(errno));
	exit(1);
      }
    } else {
      fprintf(stderr, "semget error(%s)\n", strerror(errno));
      exit(1);
    }
  } else {
    options.val = 1;
    semctl(semid3, 0, SETVAL, options);
  }
}

void clear_tab102b_sensor_input(unsigned int *bitmap)
{
  unsigned int mask = 0x0003ffc0;
  *bitmap &= ~mask;
}

/* get bit # from mask */
static int getBitNumber(unsigned int bitMask)
{
  int i;
  unsigned int mask;

  for (i = 0; i < 32; i++) {
    mask = 1 << i;

    if (bitMask & mask) {
      return i;
    }
  }

  return -1;
}

void set_gforce_data(float fb, float lr, float ud)
{
  struct sembuf sb = {0, -1, 0};

  if (!p_dio_mmap)
    return;

  sb.sem_op = -1;
  semop(semid3, &sb, 1);

  p_dio_mmap->gforce_forward_0 = fb;
  p_dio_mmap->gforce_right_0 = lr;
  p_dio_mmap->gforce_down_0 = ud;
  p_dio_mmap->gforce_log0++;

  sb.sem_op = 1;
  semop(semid3, &sb, 1);
}

void update_inputmap(unsigned int bitmap, int *changed)
{
  struct sembuf sb = {0, -1, 0};

  if (changed) *changed = 0;

  if (!p_dio_mmap) {
    return;
  }

  sb.sem_op = -1;
  semop(semid2, &sb, 1);

  unsigned int imap, imap_save;
  int i;
  imap = imap_save = p_dio_mmap->inputmap ;
  //dprintf("cur:%x\n", imap);
  clear_tab102b_sensor_input(&imap);
  //dprintf("clv:%x\n", imap);
  for( i=0; i<MAX_TAB_INPUT; i++ ) {
    //dprintf("i:%d,table:%x,bitmap:%x\n", i,input_map_table[i],bitmap);
    if( bitmap & input_map_table[i] )
      imap |= (1<<(getBitNumber(input_map_table[i])+TAB_INPUT_START)) ;
  }
  //dprintf("upd:%x\n", imap);

  p_dio_mmap->inputmap = imap ;

  sb.sem_op = 1;
  semop(semid2, &sb, 1);

  if ((imap != imap_save) && changed)
    *changed = 1;
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

  if (fd < 0)
    return -1;

  ret = write(fd, buf, size);
  if (ret < 0) {
    perror("writeCom");
    return -1;
  }

  dprintf("DM-->MCU(%d)\n", ret);
  dump(buf, ret);

  return ret;
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

int sendEnableDI(int fd, unsigned char enable)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x07; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_ENABLEDI; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = enable; // DI enable:0x40, disable:0
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendAck(int fd, unsigned char cmd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x04; // RF module addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = cmd;
  txbuf[bi++] = 0x03; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
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
  int v, use_12 = 1;

  if ((g_fw_ver >= 0x3233) && (g_fw_ver <= 0x3237))
    use_12 = 0;

  bi = 0;
  if (use_12) {
    txbuf[bi++] = 0x2b; // len
  } else {
    txbuf[bi++] = 0x2a; // len
  }
  txbuf[bi++] = 0x04; // Tab module addr
  txbuf[bi++] = 0x00; // my addr
  if (use_12) {
    txbuf[bi++] = MCUCMD_SETTRIGGER2; // cmd
  } else {
    txbuf[bi++] = MCUCMD_SETTRIGGER; // cmd
  }
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

  if (use_12) {
    txbuf[bi++] = direction_table[gsensor_direction][2]; // direction
  }

  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

void exit_with_code(int code)
{
  unlink("/var/dvr/tab102.pid"); // for tab102.sh
  unlink("/var/dvr/tab102check"); // let start.sh continue
  exit(code);
}

void appinit(int use_iomap) {
  int i, fd;
  string v ;
  config dvrconfig("/etc/dvr/dvr.conf");
  char *p;

  FILE *pidf = fopen( "/var/dvr/tab102.pid", "w" );
  if( pidf ) {
    fprintf(pidf, "%d", (int)getpid() );
    fclose(pidf);
  }


  // get tab102b serial port setting
  v = dvrconfig.getvalue( "system", "hostname");
  if( v.length()>0 ) {
    strncpy( hostname, v.getstring(), sizeof(hostname) );
  } 
    
  i = dvrconfig.getvalueint( "glog", "tab102b_enable");
  if (!i)
    exit_with_code(0);

  if (use_iomap) {
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
   
  g_sensor_trigger_forward = 0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_forward);
    if (g_sensor_trigger_forward < 0)
      g_sensor_trigger_forward = -g_sensor_trigger_forward;
  }
  g_sensor_trigger_backward = -0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_backward);
    if (g_sensor_trigger_backward > 0)
      g_sensor_trigger_backward = -g_sensor_trigger_backward;
  }
  g_sensor_trigger_right = 0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_right);
    if (g_sensor_trigger_right < 0)
      g_sensor_trigger_right = -g_sensor_trigger_right;
  }
  g_sensor_trigger_left = -0.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_left);
    if (g_sensor_trigger_left > 0)
      g_sensor_trigger_left = -g_sensor_trigger_left;
  }
  g_sensor_trigger_down = 1.0+2.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_down);
    if (g_sensor_trigger_down < 1.0)
      g_sensor_trigger_down = 1.0+2.5;
  }
  g_sensor_trigger_up = 1.0-2.5 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_trigger");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_trigger_up);
    if (g_sensor_trigger_up > 1.0)
      g_sensor_trigger_up = 1.0-2.5;
  }
   
  g_sensor_base_forward = 0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_forward);
    if (g_sensor_base_forward < 0)
      g_sensor_base_forward = -g_sensor_base_forward;
  }
  g_sensor_base_backward = -0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_backward);
    if (g_sensor_base_backward > 0)
      g_sensor_base_backward = -g_sensor_base_backward;
  }
  g_sensor_base_right = 0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_right);
    if (g_sensor_base_right < 0)
      g_sensor_base_right = -g_sensor_base_right;
  }
  g_sensor_base_left = -0.2 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_left);
    if (g_sensor_base_left > 0)
      g_sensor_base_left = -g_sensor_base_left;
  }
  g_sensor_base_down = 1.0+2.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_down);
    if (g_sensor_base_down < 1.0)
      g_sensor_base_down = 1.0+2.5;
  }
  g_sensor_base_up = 1.0-2.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_base");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_base_up);
    if (g_sensor_base_up > 1.0)
      g_sensor_base_up = 1.0-2.5;
  }
    
  g_sensor_crash_forward = 3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_forward_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_forward);
    if (g_sensor_crash_forward < 0)
      g_sensor_crash_forward = -g_sensor_crash_forward;
  }
  g_sensor_crash_backward = -3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_backward_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_backward);
    if (g_sensor_crash_backward > 0)
      g_sensor_crash_backward = -g_sensor_crash_backward;
  }
  g_sensor_crash_right = 3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_right_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_right);
    if (g_sensor_crash_right < 0)
      g_sensor_crash_right = -g_sensor_crash_right;
  }
  g_sensor_crash_left = -3.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_left_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_left);
    if (g_sensor_crash_left > 0)
      g_sensor_crash_left = -g_sensor_crash_left;
  }
  g_sensor_crash_down = 1.0+5.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_down_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_down);
    if (g_sensor_crash_down < 1.0)
      g_sensor_crash_down = 1.0+2.5;
  }
  g_sensor_crash_up = 1.0-5.0 ;
  v = dvrconfig.getvalue( "io", "gsensor_up_crash");
  if( v.length()>0 ) {
    sscanf(v.getstring(),"%f", &g_sensor_crash_up);
    if (g_sensor_crash_up > 1.0)
      g_sensor_crash_up = 1.0-2.5;
  }

  int dforward, dupward ;
  dforward = 1;
  v = dvrconfig.getvalue( "io", "gsensor_forward");
  if( v.length()>0 ) {
    dforward = atoi(v.getstring());
  }

  dupward = 5;
  v = dvrconfig.getvalue( "io", "gsensor_upward");
  if( v.length()>0 ) {
    dupward = atoi(v.getstring());
  }
  for(i=0; i<24; i++) {
    if( dforward == direction_table[i][0] &&
	dupward == direction_table[i][1] ) {
      gsensor_direction = i ;
      break;
    }
  }
}

/*
 * read_nbytes:read at least rx_size and as much data as possible with timeout
 * return:
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

void createStringWithVersion(char *string, int size, char *status)
{
  snprintf(string, size, "%s(%x)",
	   status, g_fw_ver);
}

void writeTab102Status(char *status)
{
  FILE *fp;
  /* avoid having tab102 & tab102live */
  if (unlink("/var/dvr/tab102"))
    perror("unlink");
  if (unlink("/var/dvr/tab102live"))
    perror("unlink");

  fp = fopen(g_live ? "/var/dvr/tab102live" : "/var/dvr/tab102", "w");
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
  int retry = 3;
  struct timeval tv;
  struct tm bt;
  unsigned char buf[1024];

  while (retry > 0) {
    gettimeofday(&tv, NULL);
    gmtime_r(&tv.tv_sec, &bt);
    tcflush(fd, TCIOFLUSH);
    sendSetRTC(fd, &bt);

    if (read_nbytes(fd, buf, sizeof(buf), 6, 1, 1, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_SETRTC) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 6)) {
	  dprintf("checksum error\n");
	} else {
	  char str[256];
	  createStringWithVersion(str, sizeof(str), "rtc");
	  writeTab102Status(str);
	  break;
	}
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

/*
 * return:
 *  - 0: success, 1: error
 */
int setTrigger(int fd)
{
  int retry = 3;
  struct timeval tv;
  struct tm bt;
  unsigned char buf[1024];

  while (retry > 0) {
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &bt);
    tcflush(fd, TCIOFLUSH);
    sendSetTrigger(fd);

    if (read_nbytes(fd, buf, sizeof(buf), 6, 1, 1, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  ((buf[3] == MCUCMD_SETTRIGGER) || (buf[3] == MCUCMD_SETTRIGGER2)) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 6)) {
	  dprintf("checksum error\n");
	} else {
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
  int retry = 3;
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
  int retry = 3;
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
  struct tm tm;

  time_t t = time(NULL);
  localtime_r(&t, &tm);
  snprintf(filename, sizeof(filename),
	   "/var/dvr/%s_%04d%02d%02d%02d%02d%02d_TAB102log_L.%s",
	   hostname,
	   tm.tm_year + 1900,
	   tm.tm_mon + 1,
	   tm.tm_mday,
	   tm.tm_hour,
	   tm.tm_min,
	   tm.tm_sec,
	   (type == TYPE_CONTINUOUS) ? "log" : "peak");
  FILE *fp;   
  dprintf("opening %s\n", filename);
  fp = fopen(filename, "w");
  if (fp) {
    dprintf("OK\n");
    safe_fwrite(buf, 1, len, fp);
    fclose(fp);
  }
}

int checkContinuousData(int fd)
{
  int retry = 3;
  unsigned char buf[1024];
  int nbytes, surplus = 0, uploadSize;

  dprintf("checkContinuousData\n");
  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendUploadRequest(fd);
    
    nbytes = read_nbytes(fd, buf, sizeof(buf), UPLOAD_ACK_SIZE, 3, 0, 0);
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
		sendUploadConfirm(fd);
		writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
				TYPE_CONTINUOUS);
		char str[256];
		createStringWithVersion(str, sizeof(str), "cont_data");
		writeTab102Status(str);
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

int checkPeakData(int fd)
{
  int retry = 3;
  unsigned char buf[1024];
  int nbytes, surplus = 0, uploadSize;
  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendUploadPeakRequest(fd);
#if 0    
      int t = 0;
    while(1) {
      nbytes = read(fd, buf, sizeof(buf));
      if (!t && nbytes >= UPLOAD_ACK_SIZE) {
	if ((buf[0] == 0x0a) &&
	    (buf[1] == 0x00) &&
	    (buf[2] == 0x04) &&
	    (buf[3] == MCUCMD_UPLOADPEAK) &&
	    (buf[4] == 0x03)) {
	  uploadSize = (buf[5] << 24) | (buf[6] << 16) | 
	    (buf[7] << 8) | buf[8];
	  dprintf("UPLOAD acked:%d\n", uploadSize);
	}
      }
      if (nbytes >0) {
	t += nbytes;
	dump(buf, nbytes);
      }
      dprintf("%d bytes(total:%d)\n", nbytes, t);
    }
#endif

    nbytes = read_nbytes(fd, buf, sizeof(buf), UPLOAD_ACK_SIZE, 20, 0, 0);
    //dprintf("%d bytes\n", nbytes);
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
		sendUploadPeakConfirm(fd);
		writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
				TYPE_PEAK);
		char str[256];
		createStringWithVersion(str, sizeof(str), "peak_data");
		writeTab102Status(str);
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

int is_correct_version_byte(unsigned char b)
{
  if (((b >= '0') && (b <= '9')) || 
      ((b >= 'A') && (b <= 'F')) || 
      ((b >= 'a') && (b <= 'f'))) {
    return 1;
  }

  return 0;
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

int enablePeak(int fd)
{
  int retry = 3;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendEnablePeak(fd);

    if (read_nbytes(fd, buf, sizeof(buf), 6, 1, 1, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_ENABLEPEAK) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 6)) {
	  dprintf("checksum error\n");
	} else {
	  dprintf("peak enabled\n");
	  break;
	}
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int enableDI(int fd, unsigned char enable)
{
  int retry = 3;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendEnableDI(fd, enable);

    if (read_nbytes(fd, buf, sizeof(buf), 6, 1, 1, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_ENABLEDI) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 6)) {
	  dprintf("checksum error\n");
	} else {
	  dprintf("DI enabled\n");
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
  int retry = 3;
  unsigned char buf[1024];

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendBootReady(fd);

    if (read_nbytes(fd, buf, sizeof(buf), 6, 1, 1, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x04) &&
	  (buf[3] == MCUCMD_BOOTREADY) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, 6)) {
	  dprintf("checksum error\n");
	} else {
	  dprintf("ADC started\n");
	  break;
	}
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

int checkTab102()
{
  char line[64];
  FILE *fp;   
  char *str = NULL;

  fp = fopen("/var/dvr/tab102", "r");
  if (fp) {
    str = safe_fgets(line, sizeof(line), fp);
    fclose(fp);
  }

  if (str) {
    return 1;
  }

  return 0;
}

int checkTab102Check()
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

/* can be called many times until it returns 0 */
int getOnePacketFromBuffer(unsigned char *buf, int bufsize,
			   unsigned char *rbuf, int rLen) {
  if (rLen) {
    /* can't handle too big data */
    if (rLen > MCU_BUF_SIZE)
      rLen = MCU_BUF_SIZE;

    /* add received data to the mcu buffer */
    if (mcudatasize + rLen > MCU_BUF_SIZE) {
      /* we are receiving garbages or not handling fast enough */
      fprintf(stderr, "overflow:giving up %d b\n", mcudatasize);
      mcudatasize = 0;
    }

    /* address missing bytes problem on mDVR510x */
    /* check if the new packet is complete,
     * then discard the old partial packet */
    if (mcudatasize) {
      fprintf(stderr, "********(size:%d,%d,%02x)\n",rLen,mcudatasize,mcubuf[0]);
      if (rLen == rbuf[0]) {
	if ((rLen > 1) && (rbuf[1] == 0x00)) {
	  if ((rLen > 2) && (rbuf[2] == 0x01)) {
	    /* discard the old data */
	    fprintf(stderr, "discarding %d bytes\n",mcudatasize);
	    if (mcudatasize > 4) {
	      fprintf(stderr, "%02x %02x %02x %02x ...\n",
		      mcubuf[0],mcubuf[1],mcubuf[2],mcubuf[3]);
	    }
	    mcudatasize = 0;
	  }
	}
      }
    }
    
    //fprintf(stderr, "copying %d b to mcubuf(size:%d)\n",rLen,mcudatasize);
    memcpy(mcubuf + mcudatasize, rbuf, rLen);
    mcudatasize += rLen;
  }

  if (mcudatasize == 0)
    return 0;

  /* check the length, if it's too long, it's a wrong byte */
  if (mcubuf[MCULEN] > MAXMCUDATA) {
    fprintf(stderr, "length %d too big for cmd %02x\n",
	    mcubuf[MCULEN],mcubuf[MCUCMD]);
    dump(mcubuf, mcudatasize);
    /* discard the data */
    mcudatasize = 0;
    return 0;
  }

  /* check address, too */
  if (((mcudatasize > 1) && (mcubuf[1] != 0x00)) || 
      ((mcudatasize > 2) && (mcubuf[2] != 0x04))) {
    /* discard the data */
    mcudatasize = 0;
    return 0;
  }

  //fprintf(stderr, "len:%d,bufsize:%d\n",mcubuf[0], mcudatasize);
  if (mcudatasize >= mcubuf[0]) {
    if (bufsize < mcubuf[0]) {
      /* buf too small */
      mcudatasize = 0;
      return 0;
    }
    if (mcubuf[0] == 0) {
      /* something very wrong */
      mcudatasize = 0;
      return 0;
    }

    if (verifyChecksum(mcubuf, mcubuf[0])) {
      fprintf(stderr, "Checksum Error\n");
      /* discard the data */
      mcudatasize = 0;
      return 0;
    }

    /* copy one frame to the buffer */
    memcpy(buf, mcubuf, mcubuf[0]);
    //fprintf(stderr, "copied one frame(%d)\n",mcubuf[0]);
    /* save the size to return in a temporary variable */
    bufsize = mcubuf[0];
    if (mcudatasize > mcubuf[0]) {
      /* copy remaining bytes to the head of mcubuffer */
      //fprintf(stderr, "saving remainging bytes(%d)\n",mcudatasize - bufsize);
      memmove(mcubuf, mcubuf + bufsize, mcudatasize - bufsize);
    }
    mcudatasize -= bufsize;

    return bufsize;
  }

  return 0;
}

void convertPeakData(unsigned short peakX,
		     unsigned short peakY,
		     unsigned short peakZ,
		     float *f_peakFB,
		     float *f_peakLR,
		     float *f_peakUD)
{
  unsigned short refFB = 0x200, refLR = 0x200, refUD = 0x200;
  unsigned short peakFB = peakX, peakLR = peakY, peakUD = peakZ;

  if (g_fw_ver <= 0x3237) {
    dprintf("conversion required.\n");
    if (((g_order & 0xf0) == 0x10) || ((g_order & 0xf0) == 0x20)) {
      if (((g_order & 0x0f) == 0x01) || ((g_order & 0x0f) == 0x02)) {
	refFB = g_refX;
	refLR = g_refY;
	refUD = g_refZ;
	peakFB = peakX;
	peakLR = peakY;
	peakUD = peakZ;
      } else {
	refFB = g_refY;
	refLR = g_refX;
	refUD = g_refZ;
	peakFB = peakY;
	peakLR = peakX;
	peakUD = peakZ;
      }
    } else if (((g_order & 0xf0) == 0x30) || ((g_order & 0xf0) == 0x40)) {
      if (((g_order & 0x0f) == 0x01) || ((g_order & 0x0f) == 0x02)) {
	refFB = g_refY;
	refLR = g_refZ;
	refUD = g_refX;
	peakFB = peakY;
	peakLR = peakZ;
	peakUD = peakX;
      } else {
	refFB = g_refZ;
	refLR = g_refY;
	refUD = g_refX;
	peakFB = peakZ;
	peakLR = peakY;
	peakUD = peakX;
      }
    } else if (((g_order & 0xf0) == 0x50) || ((g_order & 0xf0) == 0x60)) {
      if (((g_order & 0x0f) == 0x01) || ((g_order & 0x0f) == 0x02)) {
	refFB = g_refX;
	refLR = g_refZ;
	refUD = g_refY;
	peakFB = peakX;
	peakLR = peakZ;
	peakUD = peakY;
      } else {
	refFB = g_refZ;
	refLR = g_refX;
	refUD = g_refY;
	peakFB = peakZ;
	peakLR = peakX;
	peakUD = peakY;
      }
    }
  }

  *f_peakFB = ((int)peakFB - (int)refFB) / 37.0;
  *f_peakLR = ((int)peakLR - (int)refLR) / 37.0;
  *f_peakUD = ((int)peakUD - (int)refUD) / 37.0;
}

#define MAX_MCU_PACKET_SIZE 255
static int handle_mcu(int fd, unsigned char *rxbuf, int len)
{
  int packetLen;
  unsigned char buf[MAX_MCU_PACKET_SIZE];
  time_t t;
  int tlen = len;
  /*
  if (g_mcudebug) {
    fprintf(stderr, "handle_mcu:\n");
    dump(rxbuf, len);
  }
  */
  g_mcu_recverror = 0;

  t = time(NULL);
  while (1) {
    packetLen = getOnePacketFromBuffer(buf, MAX_MCU_PACKET_SIZE, rxbuf, len);
    //fprintf(stderr, "packetlen1:%d\n", packetLen);
    if (len)
      len = 0; // used the buffer. Don't use it again.

    if (packetLen < 6) {
      //fprintf(stderr, "packetlen:%d\n", packetLen);
      break;
    }

    if (g_mcudebug) {
      dprintf("packet:\n");
      dump(buf, packetLen);
    }

    if (verifyChecksum(buf, packetLen)) {
      fprintf(stderr, "*Checksum Error*\n");
      dump(rxbuf, tlen);
      dump(buf, packetLen);
      continue;
    }
    
    if ((buf[0] == 0x0c) &&
	(buf[1] == 0x00) &&
	(buf[2] == 0x04) &&
	(buf[3] == MCUCMD_PEAK_DATA) &&
	(buf[4] == 0x02)) { /* GForce peak data */
      if (packetLen < buf[0])
	continue;
      sendAck(fd, MCUCMD_PEAK_DATA);

      float fb, lr, ud;
      convertPeakData((buf[5] << 8) | buf[6],
		      (buf[7] << 8) | buf[8],
		      (buf[9] << 8) | buf[10],
		      &fb, &lr, &ud);
      dprintf("Accelerometer(%d,%d), FB=%.2f, LR=%.2f, UD=%.2f.\n",
	      p_dio_mmap->gforce_log0, p_dio_mmap->gforce_log1,
	      fb, lr, ud);
      // save to log
      set_gforce_data(fb, lr, ud);
    } else if ((buf[0] == 0x08) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x04) &&
	       (buf[3] == MCUCMD_DI_DATA) &&
	       (buf[4] == 0x02)) {
      if (packetLen < 8)
	continue;

      unsigned int bitmap = (buf[5] << 8) | buf[6];
      update_inputmap(bitmap, NULL);
      sendAck(fd, MCUCMD_DI_DATA);
    }
  } // while
  
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

int is_version_2_5()
{
  FILE *fp;
  char line[256];

  fp = fopen("/davinci/dvr/firmwareid", "r");
  if (fp) {
    if (fgets(line, sizeof(line), fp)) {
      if (!strncmp(line, "MDVR602.2.5.", 12)) {
	return 1;
      }
    }
    fclose(fp);
  }

  return 0;
}

/* is_version_2_5() should be called first */
int is_background(int argc, char **argv, int ver_2_5)
{
  int i;

  if (argc == 1) {
    g_task = TASK_DOWN;
    return 0;
  } else {
    for (i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "-f")) {
	  return 0;
      } else if (!strcmp(argv[i], "-down")) {
	g_task = TASK_PREDOWN;
	return 0;
      } else if (!strcmp(argv[i], "-start")) {
	g_task = TASK_START;
      }
    }
  }

  /* case of "-start" */
  if (g_task == TASK_START && ver_2_5) {
    return 1;
  }

  return 0;
}

int init_all(int use_iomap)
{
  int comFd;

  appinit(use_iomap);

  comFd = openCom(tab102b_port_dev, tab102b_port_baudrate);
  if (comFd == -1) {
    exit_with_code(1);
  }

  /* for unknown reason, this helps to ensure it always works.
   * Test method: rmmod,insmod,open
   */
  if (strstr(tab102b_port_dev, "USB")) {
    close(comFd);
    sleep(3);
    comFd = openCom(tab102b_port_dev, tab102b_port_baudrate);
    if (comFd == -1) {
      exit_with_code(1);
    }
  }

  return comFd;
}

/* command line:
 * tab102 -down   : check pre-download and exit
 * tab102 -start  : start ADC, and for 2.5 stay daemon, or exit otherwise.
 * tab102         : check /var/dvr/tab102 and check download if found.
 */
int main(int argc, char **argv)
{
  int comFd = -1, ver_2_5;
  unsigned char buf[1024];

  /* TVS22g support */
  ver_2_5 = is_version_2_5();
  if (is_background(argc, argv, ver_2_5)) {
    if (daemon(0, 0) < 0) {
      perror("daemon");
      exit(1);
    }
  }

#ifdef NET_DEBUG
  connectDebug();
  writeNetDebug("tab102 started");
#endif


  if (g_task == TASK_PREDOWN) {
    comFd = init_all(0);

    if (setTab102RTC(comFd)) {
      exit_with_code(2);
    }

    /* these files are copied by "dvrsvr -d" */
    if (checkContinuousData(comFd)) {
      exit_with_code(3);
    }
      
    if (!ver_2_5) {
      if (checkPeakData(comFd)) {
	exit_with_code(4);
      }
    }
  } else if (g_task == TASK_START) {
    if (ver_2_5) {
      g_live = 1;
      prepare_semaphore();
      comFd = init_all(1);

      /* wait until Tab102b is detected */
      while (1) {
	if (!setTab102RTC(comFd)) {
	  break;
	}
	sleep(5);
      }

      getFirmwareVersion(comFd);
      
      /* only MCU v2.3 and higher supports this command */
      if (g_fw_ver >= 0x3233)
	setTrigger(comFd);

      get0G(comFd);
      startADC(comFd);
      enablePeak(comFd);
      enableDI(comFd, 0x40);

      /* do rx loop and check for "/var/dvr/tab102check" file */
      while (1) {
	int bytes;
	bytes = read_nbytes(comFd, buf, sizeof(buf), 1, 1, 1, 0);
	if (bytes >= 1) {
	  handle_mcu(comFd, buf, bytes);
	}
	
	/* msg from tab102.sh? */
	if (checkTab102Check()) {
	  checkContinuousData(comFd);
	  break;
	}
      }
    } else {
      comFd = init_all(0);
      if (setTab102RTC(comFd)) {
	exit_with_code(2);
      }
     
      getFirmwareVersion(comFd);

      /* only MCU v2.3 and higher supports this command */
      if (g_fw_ver >= 0x3233)
	setTrigger(comFd);

      if (startADC(comFd)) {
	exit_with_code(5);
      }
    }
  } else {
    comFd = init_all(0);
    if (checkTab102()) {
      /* these files are copied by tab102.sh */
      if (checkContinuousData(comFd)) {
	exit_with_code(3);
      }
      
      if (checkPeakData(comFd)) {
	exit_with_code(4);
      }
    }
  }

  close(comFd);

  exit_with_code(0);
  return 0;
}

