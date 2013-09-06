/* --- Changes ---
 *
 * 08/11/2009 by Harrison
 *   1. System reset on <standby --> ignition on> 
 *   2. "smartftp" can finish its job even after standby time expires
 *   3. device power off delayed until "smartftp" exits
 * 08/13/2009 by Harrison
 *   1. Fix: 100% --> 100%% in the log message 
 * 08/14/2009 by Harrison
 *   1. don't start smartftp if wifi module is not detected. 
 *   2. changed timeout value after mcu_reset to 30sec. 
 *
 * 08/24/2009 by Harrison
 *   1. Save current_mode for httpd
 *      to support setup change during standby mode. 
 *
 * 09/11/2009 by Harrison
 *   1. Add tab102 (post processing) support
 *
 * 09/25/2009 by Harrison
 *   1. Make this daemon
 *
 * 09/29/2009 by Harrison
 *   1. Added buzzer_enable setup
 *
 * 10/27/2009 by Harrison
 *   1. Start smartftp only when recording disk is available
 *   2. Added Hybrid disk support (hdpower)
 *   3. Removed tab102 data copy (done in dvrsvr now).
 *
 * 11/18/2009 by Harrison
 *   1. Give more time to hard disk detection (50s --> 100s),
 *      after this, system resets.
 * 11/19/2009 by Harrison
 *   1. Give more time for Hybrid copy (30min --> 3 hours).
 *   2. Kill tab102 on entering standby mode.
 *
 * 11/20/2009 by Harrison
 *   1. Added semaphore for gps data.
 * 
 * 12/17/2009 by Harrison
 *   1. rewritten MCU communication part.
 *   2. changed HD fail timeout value for Hybrid/Multiple support.
 * 
 * 01/12/2009 by Harrison
 *   1. reset serial port when MCU doesn't respond.
 *   2. dvrsvr prerun check added.
 *
 * 03/01/2009 by Harrison
 *   1. added gforce support (602).
 *   2. fixed temperature display bug.
 *
 * 05/21/2010 by Harrison
 *   1. added support for external wifi.
 *
 * 08/06/2010 by Harrison
 *   1. supports MCU firmware version 6.0 for DI change ack.

 * 01/13/2011 by Harrison
 *   1. added support for TVS22g (14 sensors).
 *   2. fixed tab102b detection bug. 
 *   3. fixed DI bit bug (hdinserted, etc)
 *   4. show delta value for GForce
 */

#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <errno.h>
#include <sys/sem.h>
// #include <pthread.h>
#include <ctype.h>
#include "../cfg.h"

#include "../dvrsvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

//#define MCU_DEBUG
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
//#define dprintf(...)

#define MCULEN 0 /* index of mcu length in the packet */
#define MCUCMD 3 /* index of mcu command in the packet */
#define MAXMCUDATA 22 /* MCU-->DM: MCU_CMD_FW_VER is the longest */

#define MCUCMD_RESET               0x00
#define MCUCMD_UPGRADE             0x01
#define MCUCMD_POWEROFF_DELAY      0x05
#define MCUCMD_RTC                 0x06
#define MCUCMD_SETRTC              0x07
#define MCUCMD_IGNOFF              0x08
#define MCUCMD_IGNOFFCANCEL        0x09
#define MCUCMD_IOTEMP              0x0B
#define MCUCMD_USBHDTEMP           0x0C
#define MCUCMD_BOOTREADY           0x11
#define MCUCMD_POWEROFF            0x12
#define MCUCMD_POWERCONTROL        0x13
#define MCUCMD_WATCHDOGENABLE      0x1A
#define MCUCMD_WATCHDOGDISABLE     0x1B
#define MCUCMD_DICHANGE            0x1C
#define MCUCMD_DIQUERY             0x1D
#define MCUCMD_TEST                0x1E
#define MCUCMD_WATCHDOG            0x18
#define MCUCMD_WATCHDOGTIME        0x19
#define MCUCMD_DOON                0x25
#define MCUCMD_DOOFF               0x26
#define MCUCMD_DOFLASH             0x27
#define MCUCMD_USBHDON             0x28
#define MCUCMD_USBHDOFF            0x29
#define MCUCMD_FW_VER              0x2D
#define MCUCMD_DEVICE_POWER        0x2E
#define MCUCMD_LED                 0x2F
#define MCUCMD_DOMAP               0x31
#define MCUCMD_RFID_SET            0x32
#define MCUCMD_RFID_QUERY          0x33
#define MCUCMD_GFORCE_INIT         0x34
#define MCUCMD_GFORCE_UPLOAD       0x35
#define MCUCMD_GFORCE_UPLOADACK    0x36
#define MCUCMD_GFORCE_PEAK         0x40
#define MCUCMD_SHUTDOWN_CODE       0x41

#define WATCHDOG_ENABLE 0
#define MAX_MCU_FW_STR 16 /* not incluing null terminator */
#define UPLOAD_ACK_SIZE 10
enum {TYPE_CONTINUOUS, TYPE_PEAK};

// options
int standbyhdoff = 0 ;
int usewatchdog = 0 ;
int buzzer_enable;
char g_hostname[128] = "BUS001";
//unsigned short sensor_input_active_bitmap;
char g_mcudebug, g_mcudebug2;
int  g_devicepower_allways_on;

// Do system reset on <standby --> ignition on> 
// change this to 0xffff to temporarily disable device power off
#define DEVICEOFF 0

// HARD DRIVE LED and STATUS
#define HDLED	(0x10)

// input pin maping
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)
#define RECVBUFSIZE (50)

#define MAXINPUT_WITH_TAB102    (14)
#define MAXINPUT    (12)
unsigned int input_map_table [MAXINPUT] = {
    1,          // bit 0
    (1<<1),     // bit 1
    (1<<2),     // bit 2
    (1<<3),     // bit 3
    (1<<4),     // bit 4
    (1<<5),     // bit 5
    (1<<10),    // bit 6
    (1<<11),    // bit 7
    (1<<12),    // bit 8
    (1<<13),    // bit 9
    (1<<14),    // bit 10
    (1<<15),    // bit 11
} ;

// translate output14 b0 bits
#define MAXOUTPUT    (8)
unsigned int output_map_table [MAXOUTPUT] = {
    1,              // bit 0
    (1<<1),         // bit 1
    (1<<8),         // bit 2   (buzzer)
    (1<<3),         // bit 3
    (1<<4),         // bit 4
    (1<<2),         // bit 5
    (1<<6),         // bit 6
    (1<<7)          // bit 7
} ;

int hdlock=0 ;								// HD lock status
int hdinserted=0 ;

#define PANELLEDNUM (3)
#define DEVICEPOWERNUM (5)

struct baud_table_t {
    speed_t baudv ;
    int baudrate ;
} baud_table[7] = {
    { B2400, 	2400},
    { B4800,	4800},
    { B9600,	9600},
    { B19200,	19200},
    { B38400,	38400},
    { B57600,	57600},
    { B115200,	115200} 
} ;

struct dio_mmap * p_dio_mmap=NULL ;

#define SERIAL_DELAY_SEC	(0)
#define SERIAL_DELAY	(200000)
#define DEFSERIALBAUD	(115200)

char dvrcurdisk[128] = "/var/dvr/dvrcurdisk" ;
char dvriomap[256] = "/var/dvr/dvriomap" ;
char serial_dev[256] = "/dev/ttyS1" ;
char * pidfile = "/var/dvr/ioprocess.pid" ;
int serial_handle = 0 ;
int serial_baud = 115200 ;
int mcupowerdelaytime = 0 ;
char mcu_firmware_version[80] ;
char g_mcu_fw_damaged, g_poweroff, reserved[2];

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;
char logfile[128] = "dvrlog.txt" ;
char temp_logfile[128] ;
int watchdogenabled=0 ;
int watchdogtimeout=30 ;
int gpsvalid = 0 ;
int gforce_log_enable=0 ;
int output_inverted=0 ;
int mcu_recverror = 0 ;
int g_wifi_proto;
char nocpucheck, g_bootupsent, g_startrecordingsent, g_stoprecordingsent;
unsigned int g_adjtime_timer ;

int app_mode = APPMODE_QUIT ;

unsigned int panelled=0 ;
unsigned int devicepower=0xffff;

pid_t   pid_smartftp = 0 ;
pid_t   pid_tab102 = 0 ;

struct gps {
  int		gps_valid ;
  double	gps_speed ;	// knots
  double	gps_direction ; // degree
  double  gps_latitude ;	// degree, + for north, - for south
  double  gps_longitud ;      // degree, + for east,  - for west
  double  gps_gpstime ;	// seconds from 1970-1-1 12:00:00 (utc)
};
int semid, semid2, semid3;

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

// g value converting vectors , (Forward, Right, Down) = [vecttabe] * (Back, Right, Buttom)
static signed char g_convert[24][3][3] = 
{
//  Front -> Forward    
    {   // Front -> Forward, Right -> Upward
        {-1, 0, 0},
        { 0, 0,-1},
        { 0,-1, 0}
    },
    {   // Front -> Forward, Left  -> Upward
        {-1, 0, 0},
        { 0, 0, 1},
        { 0, 1, 0}
    },
    {   // Front -> Forward, Bottom  -> Upward
        {-1, 0, 0},
        { 0, 1, 0},
        { 0, 0,-1}
    },
    {   // Front -> Forward, Top -> Upward
        {-1, 0, 0},
        { 0,-1, 0},
        { 0, 0, 1}
    },
//---- Back->Forward
    {   // Back -> Forward, Right -> Upward
        { 1, 0, 0},
        { 0, 0, 1},
        { 0,-1, 0}
    },
    {   // Back -> Forward, Left -> Upward
        { 1, 0, 0},
        { 0, 0,-1},
        { 0, 1, 0}
    },
    {   // Back -> Forward, Button -> Upward
        { 1, 0, 0},
        { 0,-1, 0},
        { 0, 0,-1}
    },
    {   // Back -> Forward, Top -> Upward
        { 1, 0, 0},
        { 0, 1, 0},
        { 0, 0, 1}
    },
//---- Right -> Forward
    {   // Right -> Forward, Front -> Upward
        { 0, 1, 0},
        { 0, 0, 1},
        { 1, 0, 0}
    },
    {   // Right -> Forward, Back -> Upward
        { 0, 1, 0},
        { 0, 0,-1},
        {-1, 0, 0}
    },
    {   // Right -> Forward, Bottom -> Upward
        { 0, 1, 0},
        { 1, 0, 0},
        { 0, 0,-1}
    },
    {   // Right -> Forward, Top -> Upward
        { 0, 1, 0},
        {-1, 0, 0},
        { 0, 0, 1}
    },
//---- Left -> Forward
    {   // Left -> Forward, Front -> Upward
        { 0,-1, 0},
        { 0, 0,-1},
        { 1, 0, 0}
    },
    {   // Left -> Forward, Back -> Upward
        { 0,-1, 0},
        { 0, 0, 1},
        {-1, 0, 0}
    },
    {   // Left -> Forward, Bottom -> Upward
        { 0,-1, 0},
        {-1, 0, 0},
        { 0, 0,-1}
    },
    {   // Left -> Forward, Top -> Upward
        { 0,-1, 0},
        { 1, 0, 0},
        { 0, 0, 1}
    },
//---- Bottom -> Forward
    {   // Bottom -> Forward, Front -> Upward
        { 0, 0, 1},
        { 0,-1, 0},
        { 1, 0, 0}
    },
    {   // Bottom -> Forward, Back -> Upward
        { 0, 0, 1},
        { 0, 1, 0},
        {-1, 0, 0}
    },
    {   // Bottom -> Forward, Right -> Upward
        { 0, 0, 1},
        {-1, 0, 0},
        { 0,-1, 0}
    },
    {   // Bottom -> Forward, Left -> Upward
        { 0, 0, 1},
        { 1, 0, 0},
        { 0, 1, 0}
    },
//---- Top -> Forward
    {   // Top -> Forward, Front -> Upward
        { 0, 0,-1},
        { 0, 1, 0},
        { 1, 0, 0}
    },
    {   // Top -> Forward, Back -> Upward
        { 0, 0,-1},
        { 0,-1, 0},
        {-1, 0, 0}
    },
    {   // Top -> Forward, Right -> Upward
        { 0, 0,-1},
        { 1, 0, 0},
        { 0,-1, 0}
    },
    {   // Top -> Forward, Left -> Upward
        { 0, 0,-1},
        {-1, 0, 0},
        { 0, 1, 0}
    }

} ;
       
// 0:Front,1:Back, 2:Right, 3:Left, 4:Bottom, 5:Top 
static char direction_table[24][3] = 
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
int g_mcuinputnum;

int dvr_log(char *fmt, ...);
int getbcd(unsigned char c);
void serial_init() ;
int queryDI(int fd);

static void prepare_semaphore()
{
  key_t unique_key;
  union semun options;

  /* for shared gps data (and peak OSD) */ 
  unique_key = ftok("/dev/null", 'a');
  semid = semget(unique_key, 1, IPC_CREAT | IPC_EXCL | 0666);

  if (semid == -1) {
    if (errno == EEXIST) {
      semid = semget(unique_key, 1, 0);
      if (semid == -1) {
	fprintf(stderr, "semget error(%s)\n", strerror(errno));
	exit(1);
      }
    } else {
      fprintf(stderr, "semget error(%s)\n", strerror(errno));
      exit(1);
    }
  } else {
    options.val = 1;
    semctl(semid, 0, SETVAL, options);
  }

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

void get_gps_data(struct gps *g)
{
  struct sembuf sb = {0, -1, 0};

  if (!p_dio_mmap) {
    g->gps_valid = 0;
    return;
  }

  sb.sem_op = -1;
  semop(semid, &sb, 1);

  g->gps_valid = p_dio_mmap->gps_valid;
  g->gps_speed = p_dio_mmap->gps_speed;
  g->gps_direction = p_dio_mmap->gps_direction;
  g->gps_latitude = p_dio_mmap->gps_latitude;
  g->gps_longitud = p_dio_mmap->gps_longitud;
  g->gps_gpstime = p_dio_mmap->gps_gpstime;

  sb.sem_op = 1;
  semop(semid, &sb, 1);
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

void clear_mcu_sensor_input(unsigned int *bitmap)
{
  int i;
  unsigned int mask;

  for (i = 0; i < g_mcuinputnum; i++) {
    mask = 1 << i;
    *bitmap &= ~mask;
  }
}

void update_inputmap(unsigned int mcu_bitmap, int *changed)
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
  clear_mcu_sensor_input(&imap);
  for( i=0; i<g_mcuinputnum; i++ ) {
    if( mcu_bitmap & input_map_table[i] )
      imap |= (1<<i) ;
  }

  p_dio_mmap->inputmap = imap ;

  sb.sem_op = 1;
  semop(semid2, &sb, 1);

  if ((imap != imap_save) && changed)
    *changed = 1;
}

void set_app_mode(int mode)
{
  app_mode=mode ;
  p_dio_mmap->current_mode =  mode;
}

#define WIDTH   16
#define DBUFSIZE 1024
int dump(unsigned char *s, int len)
{
    char buf[DBUFSIZE],lbuf[DBUFSIZE],rbuf[DBUFSIZE];
    unsigned char *p;
    int line,i;
#if 0    // debug
    struct timeval tv;
    gettimeofday(&tv, NULL);
    dprintf("time:%d.%06d\n", tv.tv_sec, tv.tv_usec);
#endif

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

  if (g_mcudebug) {
    if (buf[MCUCMD] != MCUCMD_WATCHDOG) {
      dprintf("DM-->MCU(%d)\n", ret);
      dump(buf, ret);
      dprintf("<--\n", ret);
    } else {
      if (g_mcudebug2) {
	dprintf("DM-->MCU(%d)\n", ret);
	dump(buf, ret);
	dprintf("<--\n", ret);
      }
    }
  }

  return ret;
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

/*
 * read_nbytes:read at least rx_size and as much data as possible with timeout
 * return:
 *    n: # of bytes received
 *   -1: bad size parameters
 */  

int read_nbytes(int fd, unsigned char *buf, int bufsize,
		int rx_size, struct timeval *timeout_tv,
		int showdata, int showprogress)
{
  struct timeval tv;
  int total = 0, bytes;

  if (bufsize < rx_size)
    return -1;

  while (1) {
    bytes = 0;
    tv = *timeout_tv;
    if (blockUntilReadable(fd, &tv) > 0) {
      bytes = read(fd, buf + total, bufsize - total);
      if (bytes > 0) {
	if (showdata) {
	  dprintf("received:\n");
	  dump(buf + total, bytes);
	}
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

static int sendBootupReady(int fd)
{
  int bi;
  unsigned char buf[8];

  bi = 0;
    
  buf[bi++] = 0x07;
  buf[bi++] = 0x01; // dest addr
  buf[bi++] = 0x00; // src addr
  buf[bi++] = MCUCMD_BOOTREADY; // cmd
  buf[bi++] = 0x02; // query command
  buf[bi++] = WATCHDOG_ENABLE; // 0: wdt disable, 1:wdt enable
  buf[bi] = getChecksum(buf, bi); // checksum
  bi++;
    
  return writeCom(fd, buf, bi);
}

static int sendShutdownCode(int fd)
{
  int bi;
  unsigned char buf[8];

  bi = 0;
    
  buf[bi++] = 0x06;
  buf[bi++] = 0x01; // dest addr
  buf[bi++] = 0x00; // src addr
  buf[bi++] = MCUCMD_SHUTDOWN_CODE; // cmd
  buf[bi++] = 0x02; // query command
  buf[bi] = getChecksum(buf, bi); // checksum
  bi++;
    
  return writeCom(fd, buf, bi);
}

static int sendRTCReq(int fd)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_RTC; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

static int sendMCUFWVersionReq(int fd)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_FW_VER; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

static int sendDOBitmap(int fd, unsigned char bitmap)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x07; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_DOMAP; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = bitmap; // data
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int setRTC(int fd, struct tm *t)
{
  int bi;
  unsigned char buf[16];

  if ((t == NULL) || (t->tm_year < 100))
    return 1;

  bi = 0;

  buf[bi++] = 0x0d; // # of bytes
  buf[bi++] = 0x01; // dest addr
  buf[bi++] = 0x00; // src addr
  buf[bi++] = MCUCMD_SETRTC; // cmd
  buf[bi++] = 0x02; // DVR -> MCU
  buf[bi++] = (unsigned char)binaryToBCD(t->tm_sec);  // sec
  buf[bi++] = (unsigned char)binaryToBCD(t->tm_min);  // min
  buf[bi++] = (unsigned char)binaryToBCD(t->tm_hour); // hour
  buf[bi++] = t->tm_wday + 1; // day of week
  buf[bi++] = (unsigned char)binaryToBCD(t->tm_mday); // day
  buf[bi++] = (unsigned char)binaryToBCD(t->tm_mon + 1); // month
  buf[bi++] = (unsigned char)binaryToBCD(t->tm_year - 100); // year
  buf[bi] = getChecksum(buf, bi); // checksum
  bi++;

  return writeCom(fd, buf, bi);
}

int sendTurnHdPower(int fd, int on)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = on ? MCUCMD_USBHDON : MCUCMD_USBHDOFF; // cmd: HD power
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendReadIOTempReq(int fd)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_IOTEMP; // cmd: read IO temp
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendReadHdTempReq(int fd)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_USBHDTEMP; // cmd: read HD temp
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendIgnOffExtraDelay(int fd, int delay)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x08; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_IGNOFF;
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = (delay >> 8) & 0xff;
  txbuf[bi++] = delay & 0xff;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendIgnOffAck(int fd, int delay)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x08; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_IGNOFF;
  txbuf[bi++] = 0x03; // ack
  txbuf[bi++] = (delay >> 8) & 0xff;
  txbuf[bi++] = delay & 0xff;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendIgnOffCancelAck(int fd, int watchdog)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x07; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_IGNOFFCANCEL;
  txbuf[bi++] = 0x03; // ack
  txbuf[bi++] = (unsigned char)watchdog;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendWatchdogTimeout(int fd, int timeout)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x08; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_WATCHDOGTIME;
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = (timeout >> 8) & 0xff ;
  txbuf[bi++] = timeout & 0xff;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendWatchdogEnable(int fd, int enable)
{
  int bi;
  unsigned char txbuf[6];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = enable ? MCUCMD_WATCHDOGENABLE : MCUCMD_WATCHDOGDISABLE;
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendWatchdog(int fd)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_WATCHDOG;
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendLedControl(int fd, unsigned char led, int flash)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x08; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_LED;
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = led;
  txbuf[bi++] = flash ? 1 : 0;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendDeviceControl(int fd, unsigned char device, int on)
{
  int bi;
  unsigned char txbuf[8];

  bi = 0;
  txbuf[bi++] = 0x08; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_DEVICE_POWER;
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = device;
  txbuf[bi++] = on ? 1 : 0;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

static int sendGForceInit(int fd)
{
  int bi;
  unsigned char txbuf[32];

  float trigger_back, trigger_front ;
  float trigger_right, trigger_left ;
  float trigger_bottom, trigger_top ;
  unsigned char XP, XN, YP, YN, ZP, ZN;
  float base_back, base_front ;
  float base_right, base_left ;
  float base_bottom, base_top ;
  unsigned char BXP, BXN, BYP, BYN, BZP, BZN;
  float crash_back, crash_front ;
  float crash_right, crash_left ;
  float crash_bottom, crash_top ;
  unsigned char CXP, CXN, CYP, CYN, CZP, CZN;
    
  trigger_back = 
    ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_trigger_forward + 
    ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_trigger_right + 
    ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_trigger_down ;
  trigger_right = 
    ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_trigger_forward + 
    ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_trigger_right + 
    ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_trigger_down ;
  trigger_bottom = 
    ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_trigger_forward + 
    ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_trigger_right + 
    ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_trigger_down ;
  
  trigger_front = 
    ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_trigger_backward +
    ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_trigger_left + 
    ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_trigger_up ;
  trigger_left = 
    ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_trigger_backward +
    ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_trigger_left + 
    ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_trigger_up ;
  trigger_top = 
    ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_trigger_backward +
    ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_trigger_left + 
    ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_trigger_up ;
  
  if( trigger_right >= trigger_left ) {
    XP  = (signed char)(trigger_right*0xe) ;    // Right Trigger
    XN  = (signed char)(trigger_left*0xe) ;     // Left Trigger
  }
  else {
    XP  = (signed char)(trigger_left*0xe) ;     // Right Trigger
    XN  = (signed char)(trigger_right*0xe) ;    // Left Trigger
  }
  
  if( trigger_back >= trigger_front ) {
    YP  = (signed char)(trigger_back*0xe) ;    // Back Trigger
    YN  = (signed char)(trigger_front*0xe) ;    // Front Trigger
  }
  else {
    YP  = (signed char)(trigger_front*0xe) ;    // Back Trigger
    YN  = (signed char)(trigger_back*0xe) ;    // Front Trigger
  }
  
  if( trigger_bottom >= trigger_top ) {
    ZP  = (signed char)(trigger_bottom*0xe) ;    // Bottom Trigger
    ZN = (signed char)(trigger_top*0xe) ;    // Top Trigger
  }
  else {
    ZP  = (signed char)(trigger_top*0xe) ;    // Bottom Trigger
    ZN = (signed char)(trigger_bottom*0xe) ;    // Top Trigger
  }

  base_back = 
    ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_base_forward + 
    ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_base_right + 
    ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_base_down ;
  base_right = 
    ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_base_forward + 
    ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_base_right + 
    ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_base_down ;
  base_bottom = 
    ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_base_forward + 
    ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_base_right + 
    ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_base_down ;
  
  base_front = 
    ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_base_backward +
    ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_base_left + 
    ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_base_up ;
  base_left = 
    ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_base_backward +
    ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_base_left + 
    ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_base_up ;
  base_top = 
    ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_base_backward +
    ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_base_left + 
    ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_base_up ;
  
  if( base_right >= base_left ) {
    BXP  = (signed char)(base_right*0xe) ;    // Right Trigger
    BXN  = (signed char)(base_left*0xe) ;     // Left Trigger
  }
  else {
    BXP  = (signed char)(base_left*0xe) ;     // Right Trigger
    BXN  = (signed char)(base_right*0xe) ;    // Left Trigger
  }
  
  if( base_back >= base_front ) {
    BYP  = (signed char)(base_back*0xe) ;    // Back Trigger
    BYN  = (signed char)(base_front*0xe) ;    // Front Trigger
  }
  else {
    BYP  = (signed char)(base_front*0xe) ;    // Back Trigger
    BYN  = (signed char)(base_back*0xe) ;    // Front Trigger
  }
  
  if( base_bottom >= base_top ) {
    BZP  = (signed char)(base_bottom*0xe) ;    // Bottom Trigger
    BZN = (signed char)(base_top*0xe) ;    // Top Trigger
  }
  else {
    BZP  = (signed char)(base_top*0xe) ;    // Bottom Trigger
    BZN = (signed char)(base_bottom*0xe) ;    // Top Trigger
  }

  crash_back = 
    ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_crash_forward + 
    ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_crash_right + 
    ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_crash_down ;
  crash_right = 
    ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_crash_forward + 
    ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_crash_right + 
    ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_crash_down ;
  crash_bottom = 
    ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_crash_forward + 
    ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_crash_right + 
    ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_crash_down ;
  
  crash_front = 
    ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_crash_backward +
    ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_crash_left + 
    ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_crash_up ;
  crash_left = 
    ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_crash_backward +
    ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_crash_left + 
    ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_crash_up ;
  crash_top = 
    ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_crash_backward +
    ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_crash_left + 
    ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_crash_up ;
  
  if( crash_right >= crash_left ) {
    CXP  = (signed char)(crash_right*0xe) ;    // Right Trigger
    CXN  = (signed char)(crash_left*0xe) ;     // Left Trigger
  }
  else {
    CXP  = (signed char)(crash_left*0xe) ;     // Right Trigger
    CXN  = (signed char)(crash_right*0xe) ;    // Left Trigger
  }
  
  if( crash_back >= crash_front ) {
    CYP  = (signed char)(crash_back*0xe) ;    // Back Trigger
    CYN  = (signed char)(crash_front*0xe) ;    // Front Trigger
  }
  else {
    CYP  = (signed char)(crash_front*0xe) ;    // Back Trigger
    CYN  = (signed char)(crash_back*0xe) ;    // Front Trigger
  }
  
  if( crash_bottom >= crash_top ) {
    CZP  = (signed char)(crash_bottom*0xe) ;    // Bottom Trigger
    CZN = (signed char)(crash_top*0xe) ;    // Top Trigger
  }
  else {
    CZP  = (signed char)(crash_top*0xe) ;    // Bottom Trigger
    CZN = (signed char)(crash_bottom*0xe) ;    // Top Trigger
  }

  bi = 0;
  txbuf[bi++] = 0x1a; // len
  txbuf[bi++] = 0x01; // mcu addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_GFORCE_INIT; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = 0x01; // enable GForce
  txbuf[bi++] = direction_table[gsensor_direction][2]; // direction
  txbuf[bi++] = BXP; // X+
  txbuf[bi++] = BXN; // X-
  txbuf[bi++] = BYP; // Y+
  txbuf[bi++] = BYN; // Y-
  txbuf[bi++] = BZP; // Z+
  txbuf[bi++] = BZN; // Z-
  txbuf[bi++] = XP; // X+
  txbuf[bi++] = XN; // X-
  txbuf[bi++] = YP; // Y+
  txbuf[bi++] = YN; // Y-
  txbuf[bi++] = ZP; // Z+
  txbuf[bi++] = ZN; // Z-
  txbuf[bi++] = CXP; // X+
  txbuf[bi++] = CXN; // X-
  txbuf[bi++] = CYP; // Y+
  txbuf[bi++] = CYN; // Y-
  txbuf[bi++] = CZP; // Z+
  txbuf[bi++] = CZN; // Z-
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendUploadRequest(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x07; // len
  txbuf[bi++] = 0x01; // dest addr
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_GFORCE_UPLOAD; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = direction_table[gsensor_direction][2]; // direction
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendUploadAck(int fd, int success)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x07; // len
  txbuf[bi++] = 0x01; // dest
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_GFORCE_UPLOADACK; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = (unsigned char)success;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendPowerOffDelay(int fd, int delay)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x08; // len
  txbuf[bi++] = 0x01; // dest
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_POWEROFF_DELAY; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi++] = (delay >> 8) & 0xff ;
  txbuf[bi++] = delay & 0xff;
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendDIQuery(int fd)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x06; // len
  txbuf[bi++] = 0x01; // dest
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_DIQUERY; // cmd
  txbuf[bi++] = 0x02; // req
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

int sendDIChangeAck(int fd, unsigned char *bitmaps)
{
  int bi;
  unsigned char txbuf[32];

  bi = 0;
  txbuf[bi++] = 0x09; // len
  txbuf[bi++] = 0x01; // dest
  txbuf[bi++] = 0x00; // my addr
  txbuf[bi++] = MCUCMD_DICHANGE; // cmd
  txbuf[bi++] = 0x03; // req
  txbuf[bi++] = bitmaps[0]; // bitmaps received
  txbuf[bi++] = bitmaps[1]; // bitmaps received
  txbuf[bi++] = bitmaps[2]; // bitmaps received
  txbuf[bi] = getChecksum(txbuf, bi); // checksum
  bi++;

  return writeCom(fd, txbuf, bi);
}

void sendUploadConfirm(int fd, int success)
{
  int retry = 3;
  unsigned char buf[1024];
  struct timeval tv;

  while (retry > 0) {
    tcflush(fd, TCIOFLUSH);
    sendUploadAck(fd, success);

    tv.tv_sec = 20;
    tv.tv_usec = 0;
    if (read_nbytes(fd, buf, sizeof(buf), 6, &tv, g_mcudebug, 0) >= 6) {
      if ((buf[0] == 0x06) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x01) &&
	  (buf[3] == MCUCMD_GFORCE_UPLOADACK) &&
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

static size_t safe_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret = 0;
  
  do {
    clearerr(stream);
    ret += fwrite((char *)ptr + (ret * size), size, nmemb - ret, stream);
  } while (ret < nmemb && ferror(stream) && errno == EINTR);
  
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
	   g_hostname,
	   tm.tm_year + 1900,
	   tm.tm_mon + 1,
	   tm.tm_mday,
	   tm.tm_hour,
	   tm.tm_min,
	   tm.tm_sec,
	   (type == TYPE_CONTINUOUS) ? "log" : "peak");
  FILE *fp;   
  dprintf("opening %s...", filename);
  fp = fopen(filename, "w");
  if (fp) {
    dprintf("OK");
    safe_fwrite(buf, 1, len, fp);
    fclose(fp);
  }
  dprintf("\n");
}

void writeGforceStatus(char *status)
{
  FILE *fp;
  fp = fopen("/var/dvr/gforce", "w");
  if (fp) {
    fprintf(fp, "%s\n", status);
    fclose(fp);
  }
}

int checkContinuousData(int fd)
{
  int retry = 3;
  unsigned char buf[1024];
  char log[32];
  int nbytes, surplus = 0, uploadSize;
  struct timeval tv;
  int success;
  int error = 0;

  while (retry > 0) {
    success = 0;

    sprintf(log, "read:%d", retry);
    writeGforceStatus(log);

    tcflush(fd, TCIOFLUSH);
    sendUploadRequest(fd);
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    nbytes = read_nbytes(fd, buf, sizeof(buf), UPLOAD_ACK_SIZE,
			 &tv, g_mcudebug2, 0);
    if (nbytes >= UPLOAD_ACK_SIZE) {
      if ((buf[0] == 0x0a) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x01) &&
	  (buf[3] == MCUCMD_GFORCE_UPLOAD) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, UPLOAD_ACK_SIZE)) {
	  error = 1;
	  dprintf("checksum error\n");
	} else {
	  uploadSize = (buf[5] << 24) | (buf[6] << 16) | 
	    (buf[7] << 8) | buf[8];
	  dprintf("UPLOAD acked:%d\n", uploadSize);
	  if (uploadSize) {
	    writeGforceStatus("load");
	    //1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
	    // + 8(0g + checksum)
	    int bufsize = uploadSize + 1024;
	    unsigned char *tbuf = (unsigned char *)malloc(bufsize);
	    if (!tbuf) {
	      error = 100;
	      break;
	    }
  
	    // just in case we got more data 
	    if (nbytes > UPLOAD_ACK_SIZE) {
	      surplus = nbytes - UPLOAD_ACK_SIZE;
	      memcpy(tbuf, buf + UPLOAD_ACK_SIZE, surplus);
	      //dprintf("surplus moved:%d\n", surplus);
	    }

	    tv.tv_sec = 3;
	    tv.tv_usec = 0;
	    nbytes = read_nbytes(fd, tbuf + surplus, bufsize - surplus,
				 uploadSize + UPLOAD_ACK_SIZE + 8 - surplus,
				 &tv, g_mcudebug2, g_mcudebug2 ? 0 : 1);
	    int downloaded = nbytes + surplus;
	    dprintf("UPLOAD data:%d(%d,%d)\n", downloaded,uploadSize,surplus);
	    if (downloaded >= uploadSize + UPLOAD_ACK_SIZE + 8) {
	      if (!verifyChecksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
		success = 1;
		writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
				TYPE_CONTINUOUS);
	      } else {
		dprintf("*********checksum error************\n");
		error = 2;
	      }
	    }
	    sendUploadConfirm(fd, success);
	    free(tbuf);
	  } else {
	    success = 1;
	  }
	  if (success)
	    break;
	}      
      }
    }
    retry--;
  }

  sprintf(log, "done:%d,%d", retry, error);
  writeGforceStatus(log);

  return (retry > 0) ? 0 : 1;
}

#define MCU_BUF_SIZE (4 * 1024)
unsigned char mcubuf[MCU_BUF_SIZE];
int mcudatasize;
char checkedOnce, serialError, res1, res2;

int isDIChangePacket(unsigned char *buf, int bufsize)
{
  int i;

  if (bufsize < 4)
    return 0;

  for (i = 0; i < bufsize - 4; i++) {
    if ((buf[i] == 0x09) &&
	(buf[i + 1] == 0x00) &&
	(buf[i + 2] == 0x01) &&
	(buf[i + 3] == 0x1c)) {
      return i;
    }
  }

  return 0;
}

/* can be called many times until it returns 0 */
int getOnePacketFromBuffer(unsigned char *buf, int bufsize,
			   unsigned char *rbuf, int rLen) {
  int ret;

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
	      if ((mcubuf[0] == 0x09) &&
		  (mcubuf[1] == 0x00) &&
		  (mcubuf[2] == 0x01) &&
		  (mcubuf[3] == 0x1c)) {
		serialError = 1;
	      }
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
    if (mcudatasize < 4) {
      return 0;
    }

    /* try to salvage at least DIChange packet */
    ret = isDIChangePacket(mcubuf, mcudatasize);
    if (ret) {
      memmove(mcubuf, mcubuf + ret, mcudatasize - ret);
    } else {
      fprintf(stderr, "length %d too big for cmd %02x,fw_damaged:%d\n",
	      mcubuf[MCULEN],mcubuf[MCUCMD], g_mcu_fw_damaged);
      dump(mcubuf, mcudatasize);
      /* Due to hardware issue, 
       * we get garbage data, so we cannot compare with
       * 'Enter a command >'
       * Check only the first charater 'E',
       * and if it is 'E' twice consecutively,
       * consider it Bootloader prompt
       */
      if (mcubuf[0] == 'E') {
	if(checkedOnce) {
	  g_mcu_fw_damaged = 1;
	} else {
	  checkedOnce = 1;
	}
      } else {
	checkedOnce = 0;
      }
    
      /* discard the data */
      mcudatasize = 0;
      return 0;
    }
  }

  /* more check on 510x: check address, too */
  if (((mcudatasize > 1) && (mcubuf[1] != 0x00)) || 
      ((mcudatasize > 2) && (mcubuf[2] != 0x01))) {
    /* try to salvage at least DIChange packet */
    ret = isDIChangePacket(mcubuf, mcudatasize);
    if (ret) {
      memmove(mcubuf, mcubuf + ret, mcudatasize - ret);
    } else {
      /* discard the data */
      mcudatasize = 0;
      return 0;
    }
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
      /* try to salvage at least DIChange packet */
      ret = isDIChangePacket(mcubuf, mcudatasize);
      if (ret) {
	memmove(mcubuf, mcubuf + ret, mcudatasize - ret);
      } else {
	/* discard the data */
	mcudatasize = 0;
	return 0;
      }
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

#define MAX_MCU_PACKET_SIZE 255
/* return: command dependant value
 * cmd_done:
 *  1: requested command handled
 *  0:                   not handled
 */
static int handle_mcu(int fd, unsigned char *rxbuf, int len,
		      int cmd, int *cmd_done)
{
  int packetLen;
  unsigned char buf[MAX_MCU_PACKET_SIZE];
  time_t t;
  int ret = 0;
  int tlen = len;
  /*
  if (g_mcudebug) {
    fprintf(stderr, "handle_mcu:\n");
    dump(rxbuf, len);
  }
  */
  mcu_recverror = 0;
  if (cmd_done) *cmd_done = 0;

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
      if (buf[MCUCMD] != MCUCMD_WATCHDOG) {
	fprintf(stderr, "packet:\n");
	dump(buf, packetLen);
      }
    }

    if (verifyChecksum(buf, packetLen)) {
      fprintf(stderr, "*Checksum Error*\n");
      dump(rxbuf, tlen);
      dump(buf, packetLen);
      continue;
    }
    
    if ((buf[0] == 0x09) &&
	(buf[1] == 0x00) &&
	(buf[2] == 0x01) &&
	(buf[3] == MCUCMD_IGNOFF) &&
	(buf[4] == 0x02)) { /* Ignition off */
      if (packetLen < 9)
	continue;
      dprintf("**************Ignition off************\n");
      sendIgnOffAck(fd, 0);
      mcupowerdelaytime = 0 ;
      /* don't change p_dio_mmap->poweroff here, 
       * dvrsvr can be messed if IGN turns off during initialization */  
      //p_dio_mmap->poweroff = 1 ;
      g_poweroff = 1;
    } else if ((buf[0] == 0x08) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_IGNOFF) &&
	       (buf[4] == 0x03)) { /* power off delay ack*/
      if (packetLen < 8)
	continue;
      if (cmd == MCUCMD_IGNOFF) { /* DVR sent poweroff delay cmd */
	if (cmd_done) *cmd_done = 1;
	ret = ((buf[5] << 8) | buf[6]);
      }
    } else if ((buf[0] == 0x06) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_POWEROFF_DELAY) &&
	       (buf[4] == 0x03)) {
      if (cmd == MCUCMD_POWEROFF_DELAY) {
	if (cmd_done) *cmd_done = 1;
      }
    } else if ((buf[0] == 0x06) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_WATCHDOGTIME) &&
	       (buf[4] == 0x03)) { /* watchdog set time ack */
      if (cmd == MCUCMD_WATCHDOGTIME)
	if (cmd_done) *cmd_done = 1;
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_WATCHDOGENABLE) &&
	       (buf[4] == 0x03)) { /* watchdog enable ack*/
      if (cmd == MCUCMD_WATCHDOGENABLE)
	if (cmd_done) *cmd_done = 1;
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_WATCHDOGDISABLE) &&
	       (buf[4] == 0x03)) { /* watchdog disable ack*/
      if (cmd == MCUCMD_WATCHDOGDISABLE)
	if (cmd_done) *cmd_done = 1;
    } else if ((buf[0] == 0x06) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_WATCHDOG) &&
	       (buf[4] == 0x03)) { /* watchdog ack*/
      if (cmd == MCUCMD_WATCHDOG)
	if (cmd_done) *cmd_done = 1;
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_GFORCE_INIT) &&
	       (buf[4] == 0x03)) {
      if (packetLen < buf[0])
	continue;
      //gforce_log_enable = buf[5];
      dprintf("****gforce_log_enable:%d***************\n", gforce_log_enable);
      if (cmd == MCUCMD_GFORCE_INIT) {
	if (cmd_done) *cmd_done = 1;
      }
    } else if ((buf[0] == 0x06) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_SETRTC) &&
	       (buf[4] == 0x03)) {
      if (cmd == MCUCMD_SETRTC)
	if (cmd_done) *cmd_done = 1;
    } else if ((buf[0] == 0x16) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_FW_VER) &&
	       (buf[4] == 0x03)) {     
      if (packetLen < buf[0])
	continue;

      memcpy(mcu_firmware_version, buf + 5, MAX_MCU_FW_STR);
      mcu_firmware_version[MAX_MCU_FW_STR] = '\0';
      if (cmd == MCUCMD_FW_VER) {
	if (cmd_done) *cmd_done = 1;
      }
    } else if ((buf[0] == 0x09) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x02) &&
	       (buf[3] == MCUCMD_RFID_QUERY) &&
	       (buf[4] == 0x03)) { /* RFID */
    } else if ((buf[0] == 0x06) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x02) &&
	       (buf[3] == MCUCMD_RFID_SET) &&
	       (buf[4] == 0x03)) { /* RFID */
    } else if ((buf[0] == 0x06) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_USBHDON) &&
	       (buf[4] == 0x03)) { /* usb hd on ack */
      if (cmd == MCUCMD_USBHDON)
	if (cmd_done) *cmd_done = 1;
    } else if ((buf[0] == 0x06) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_USBHDOFF) &&
	       (buf[4] == 0x03)) { /* usb hd off ack */
      if (cmd == MCUCMD_USBHDOFF)
	if (cmd_done) *cmd_done = 1;
   } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_IGNOFFCANCEL) &&
	       (buf[4] == 0x02)) { /* Ignition off */
      dprintf("Ignition off cancel\n");
      sendIgnOffCancelAck(fd, watchdogenabled);
      //p_dio_mmap->poweroff = 0 ; // send power on message to DVR
      g_poweroff = 0;
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_DOOFF) &&
	       (buf[4] == 0x03)) { /* DO OFF acked */
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_DOON) &&
	       (buf[4] == 0x03)) { /* DO ON acked */
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_DOFLASH) &&
	       (buf[4] == 0x03)) { /* DO OFF acked */
    } else if ((buf[0] == 0x09) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_GFORCE_PEAK) &&
	       (buf[4] == 0x02)) { /* GForce peak data */
      if (packetLen < buf[0])
	continue;
      dprintf("GForce data:%02x %02x %02x\n", buf[5],buf[6],buf[7]);
      float gback, gright, gbuttom ;
      float gbusforward, gbusright, gbusdown ;

      gright  = ((float)(signed char)buf[5])/0xe ;
      gback   = ((float)(signed char)buf[6])/0xe ;
      gbuttom = ((float)(signed char)buf[7])/0xe ;

      dprintf("Accelerometer(%d,%d), right=%.2f, back=%.2f, buttom=%.2f.\n",
	      p_dio_mmap->gforce_log0, p_dio_mmap->gforce_log1,
	      gright,
	      gback,
	      gbuttom );

      // converting
      gbusforward = 
	((float)(signed char)(g_convert[gsensor_direction][0][0])) * gback + 
	((float)(signed char)(g_convert[gsensor_direction][0][1])) * gright + 
	((float)(signed char)(g_convert[gsensor_direction][0][2])) * gbuttom ;
      gbusright = 
	((float)(signed char)(g_convert[gsensor_direction][1][0])) * gback + 
	((float)(signed char)(g_convert[gsensor_direction][1][1])) * gright + 
	((float)(signed char)(g_convert[gsensor_direction][1][2])) * gbuttom ;
      gbusdown = 
	((float)(signed char)(g_convert[gsensor_direction][2][0])) * gback + 
	((float)(signed char)(g_convert[gsensor_direction][2][1])) * gright + 
	((float)(signed char)(g_convert[gsensor_direction][2][2])) * gbuttom ;
      
      dprintf("Accelerometer, converted right=%.2f , forward=%.2f , down  =%.2f .\n",     
	     gbusright,
	     gbusforward,
	     gbusdown );
      // save to log
      set_gforce_data(gbusforward, gbusright, gbusdown);
    } else if ((buf[0] == 0x09) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_DICHANGE) &&
	       (buf[4] == 0x02)) { /* DI status changed */
      /* 3f: EV on
       * 3e: EV off 
       */
      dprintf("DI status changed\n");
      if (packetLen < buf[0])
	continue;
      
      unsigned int imap1 = buf[5]+256*(unsigned int)buf[6];
      hdlock = ((buf[5] & HDKEYLOCK) == 0);	  // HD locked
      hdinserted = ((buf[5] & HDINSERTED) == 0);  // HD inserted
      update_inputmap(imap1, NULL);
      sendDIChangeAck(serial_handle, buf + 5);
      //dvr_log("DI status changed:%02x",buf[5]);
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_DIQUERY) &&
	       (buf[4] == 0x03)) { /* DI status query reply */
      if (packetLen < buf[0])
	continue;
      
      int changed;
      unsigned int imap1 = buf[5]+256*(unsigned int)buf[6];
      hdlock = ((buf[5] & HDKEYLOCK) == 0);	  // HD locked
      hdinserted = ((buf[5] & HDINSERTED) == 0);  // HD inserted
      update_inputmap(imap1, &changed);
      if (changed) {
	dprintf("DI status changed:%02x,%02x\n", buf[5], buf[6]);
      }
      if (cmd == MCUCMD_DIQUERY) {
	if (cmd_done) *cmd_done = 1;
      }
    } else if ((buf[0] == 0x08) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_IOTEMP) &&
	       (buf[4] == 0x03)) { /* io temp data */
      if (packetLen < buf[0])
	continue;
      if (cmd == MCUCMD_IOTEMP) {
	if (cmd_done) *cmd_done = 1;
	ret = (int)(signed char)buf[5];
      }
    } else if ((buf[0] == 0x08) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_USBHDTEMP) &&
	       (buf[4] == 0x03)) { /* HD temp data */
      if (packetLen < buf[0])
	continue;
      if (cmd == MCUCMD_USBHDTEMP) {
	if (cmd_done) *cmd_done = 1;
	ret = (int)(signed char)buf[5];
      }
    } else if ((buf[0] == 0x07) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_DOMAP) &&
	       (buf[4] == 0x03)) { /* do bitmap ack */
      if (cmd == MCUCMD_DOMAP)
	if (cmd_done) *cmd_done = 1;
    } else if ((buf[0] == 0x08) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_LED) &&
	       (buf[4] == 0x03)) { /* led control ack */
      if (cmd == MCUCMD_LED)
	if (cmd_done) *cmd_done = 1;
     } else if ((buf[0] == 0x08) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_DEVICE_POWER) &&
	       (buf[4] == 0x03)) { /* device control ack */
      if (cmd == MCUCMD_DEVICE_POWER)
	if (cmd_done) *cmd_done = 1;
   } else if ((buf[0] == 0x0d) &&
	       (buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_RTC) &&
	       (buf[4] == 0x03)) { /* real time clock data */
      if (packetLen < buf[0])
	continue;

      struct tm bt;
      bt.tm_year = (buf[11] >> 4) * 10 + (buf[11] & 0x0f) + 100;
      bt.tm_mon = ((buf[10] >> 4) & 0x01) * 10 + (buf[10] & 0x0f) - 1;
      bt.tm_mday = ((buf[9] >> 4) & 0x03) * 10 + (buf[9] & 0x0f);
      bt.tm_hour = ((buf[7] >> 4) & 0x03) * 10 + (buf[7] & 0x0f);
      bt.tm_min = (buf[6] >> 4) * 10 + (buf[6] & 0x0f);
      bt.tm_sec = (buf[5] >> 4) * 10 + (buf[5] & 0x0f);
      bt.tm_isdst = 0;
      
      if (bt.tm_year > 105 && bt.tm_year < 195 &&
	  bt.tm_mon >= 0 && bt.tm_mon <= 11 &&
	  bt.tm_mday >= 1 && bt.tm_mday <= 31 &&
	  bt.tm_hour >= 0 && bt.tm_hour <= 24 &&
	  bt.tm_min >= 0 && bt.tm_min <= 60 &&
	  bt.tm_sec >= 0 && bt.tm_sec <= 60 ) {
	time_t tt = timegm( &bt );
	if( (int)tt > 0 ) {
	  if (cmd == MCUCMD_RTC) {
	    if (cmd_done) *cmd_done = 1;
	    ret = tt ;
	  }
	}
      }
    } else if ((buf[1] == 0x00) &&
	       (buf[2] == 0x01) &&
	       (buf[3] == MCUCMD_SHUTDOWN_CODE) &&
	       (buf[4] == 0x03)) {
      if (packetLen < 14)
	continue;

      if (buf[5] == 0xff) {
	dvr_log( "MCU shutdown code: none");
      } else {
	struct tm ttm ;
	time_t tt ;
	memset( &ttm, 0, sizeof(ttm) );
	ttm.tm_year = getbcd(buf[12]) + 100 ;
	ttm.tm_mon  = getbcd(buf[11]) - 1;
	ttm.tm_mday = getbcd(buf[10]) ;
	ttm.tm_hour = getbcd(buf[8]) ;
	ttm.tm_min  = getbcd(buf[7]) ;
	ttm.tm_sec  = getbcd(buf[6]) ;
	ttm.tm_isdst= -1 ;
	tt=timegm(&ttm);
	if( (int)tt>0 ) {
	  localtime_r(&tt, &ttm);
	  dvr_log( "MCU shutdown code: %02x "
		   "at %04d-%02d-%02d %02d:%02d:%02d",
		   buf[5],
		   ttm.tm_year+1900,
		   ttm.tm_mon+1,
		   ttm.tm_mday,
		   ttm.tm_hour,
		   ttm.tm_min,
		   ttm.tm_sec );
	}
      }

      if (cmd == MCUCMD_SHUTDOWN_CODE) {
	if (cmd_done) *cmd_done = 1;
      }
    }
  } // while

  return ret;
}
#if 0
static int handle_mcu(int fd, unsigned char *rxbuf, int len,
		      int cmd, int *cmd_done)
{
  int ret;
  serialError = 0;
  ret = _handle_mcu(fd, rxbuf, len, cmd, cmd_done);
  if (serialError == 0)
    return ret;

  queryDI(fd);

  return ret;
}
#endif
void sig_handler(int signum)
{
    if( signum==SIGUSR2 ) {
      set_app_mode(APPMODE_REINITAPP) ;
   }
    else {
      set_app_mode(APPMODE_QUIT);
    }
}

// return bcd value
int getbcd(unsigned char c)
{
    return (int)(c/16)*10 + c%16 ;
}

// write log to file.
// return 
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(char *fmt, ...)
{
    int res = 0 ;
    static char logfilename[512]="" ;
    char lbuf[512];
    FILE *flog;
    time_t ti;
    int l;
    va_list ap ;

    flog = fopen(dvrcurdisk, "r");
    if( flog ) {
        if( fscanf(flog, "%s", lbuf )==1 ) {
            sprintf(logfilename, "%s/_%s_/%s", lbuf, g_hostname, logfile);
        }
        fclose(flog);
    }
        
    ti = time(NULL);
    ctime_r(&ti, lbuf);
    l = strlen(lbuf);
    if (lbuf[l - 1] == '\n')
        lbuf[l - 1] = '\0';

    flog = fopen(logfilename, "a");
    if (flog) {
        fprintf(flog, "%s:IO:", lbuf);
        va_start( ap, fmt );
        vfprintf(flog, fmt, ap );
        va_end( ap );
        fprintf(flog, "\n");		
        if( fclose(flog)==0 ) {
            res=1 ;
        }
    }

    if( res==0 ) {
        flog = fopen(temp_logfile, "a");
        if (flog) {
            fprintf(flog, "%s:IO:", lbuf);
            va_start( ap, fmt );
            vfprintf(flog, fmt, ap );
            va_end( ap );
            fprintf(flog, " *\n");		
            if( fclose(flog)==0 ) {
                res=1 ;
            }
        }
    }
    
#ifdef MCU_DEBUG
    // also output to stdout (if in debugging)
    printf("%s:IO:", lbuf);
    va_start( ap, fmt );
    vprintf(fmt, ap );
    va_end( ap );
    printf("\n");
#endif		
        
    return res ;
}

int getshutdowndelaytime()
{
    config dvrconfig(dvrconfigfile);
    int v ;
    v = dvrconfig.getvalueint( "system", "shutdowndelay");
    if( v<10 ) {
        v=10 ;
    }
    if( v>7200 ) {
        v=7200 ;
    }
    return v ;
}

int getstandbytime()
{
    config dvrconfig(dvrconfigfile);
    int v ;
    v = dvrconfig.getvalueint( "system", "standbytime");
    if( v<10 ) {
        v=10 ;
    }
    if( v>43200 ) {
        v=43200 ;
    }
    return v ;
}

static unsigned int starttime=0 ;
void initruntime()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    starttime=tv.tv_sec ;
}

// return runtime in mili-seconds
unsigned int getruntime()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    return (unsigned int)(tv.tv_sec-starttime)*1000 + tv.tv_usec/1000 ;
}

// set onboard rtc 
void rtc_set(time_t utctime)
{
    struct tm ut ;
    int hrtc = open("/dev/rtc", O_WRONLY );
    if( hrtc>0 ) {
        gmtime_r(&utctime, &ut);
        ioctl( hrtc, RTC_SET_TIME, &ut ) ;
        close( hrtc );
    }
}

// Check if data ready to read on serial port
//     return 0: no data
//            1: yes
int serial_dataready(int microsecondsdelay)
{
    struct timeval timeout ;
    fd_set fds;
    if( serial_handle<=0 ) {
        return 0;
    }
    timeout.tv_sec = microsecondsdelay/1000000 ;
    timeout.tv_usec = microsecondsdelay%1000000 ;
    FD_ZERO(&fds);
    FD_SET(serial_handle, &fds);
    if (select(serial_handle + 1, &fds, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(serial_handle, &fds);
    } else {
        return 0;
    }
}

int serial_read(void * buf, size_t bufsize)
{
    if( serial_handle>0 ) {
#ifdef MCU_DEBUG
        int i, n ;
        char * cbuf ;
        cbuf = (char *)buf ;
        n=read( serial_handle, buf, bufsize);
        for( i=0; i<n ; i++ ) {
            printf("%02x ", (int)cbuf[i] );
        }
        return n ;
#else        
        return read( serial_handle, buf, bufsize);
#endif
    }
    return 0;
}

int serial_write(void * buf, size_t bufsize)
{
    if( serial_handle>0 ) {
#ifdef MCU_DEBUG
        size_t i ;
        char * cbuf ;
        cbuf = (char *)buf ;
        for( i=0; i<bufsize ; i++ ) {
            printf("%02x ", (int)cbuf[i] );
        }
#endif
        return write( serial_handle, buf, bufsize);
    }
    return 0;
}

void serial_clear(int usdelay=100 );

// clear receiving buffer
void serial_clear(int usdelay)
{
    int i;
    char rbuf[1000] ;
    for(i=0;i<1000;i++) {
        if( serial_dataready(usdelay) ) {
#ifdef MCU_DEBUG
            printf("clear: ");
#endif                
            serial_read( rbuf, sizeof(rbuf));
#ifdef MCU_DEBUG
            printf("\n");
#endif                
        }
        else {
            break;
        }
    }
}

// initialize serial port
void serial_init() 
{
    int port=0 ;
   
    if( serial_handle > 0 ) {
        close( serial_handle );
        serial_handle = 0 ;
    }

    if( strcmp( serial_dev, "/dev/ttyS1")==0 ) {
      port=1 ;
    }
    serial_handle = open( serial_dev, O_RDWR | O_NOCTTY/* | O_NDELAY*/ );
    if( serial_handle > 0 ) {
      dprintf("%s opened\n", serial_dev);
      //fcntl(serial_handle, F_SETFL, 0);
        if( port==1 ) {		// port 1
            // Use Hikvision API to setup serial port (RS485)
            InitRS485(serial_handle, serial_baud, DATAB8, STOPB1, NOPARITY, NOCTRL);
        }
        else {
	  struct termios tio;

	  tcgetattr(serial_handle, &tio);

	  /* set speed */
	  cfsetispeed(&tio, B115200);
	  cfsetospeed(&tio, B115200);
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
	  
	  tcflush(serial_handle, TCIFLUSH);
	  tcsetattr(serial_handle, TCSANOW, &tio);
        }
    }
    else {
        // even no serail port, we still let process run
      dprintf("Serial port failed!:%s\n", serial_dev);
    }
}

#define MCU_INPUTNUM (6)
#define MCU_OUTPUTNUM (4)

unsigned int mcu_doutputmap ;

// check data check sum
// return 0: checksum correct
int mcu_checksun( char * data ) 
{
    char ck;
    int i ;
    if( data[0]<5 || data[0]>40 ) {
        return -1 ;
    }
    ck=0;
    for( i=0; i<(int)data[0]; i++ ) {
        ck+=data[i] ;
    }
    return (int)ck;
}

// calculate check sum of data
void mcu_calchecksun( char * data )
{
    char ck = 0 ;
    unsigned int si = (unsigned int) *data ;
    while( --si>0 ) {
        ck-=*data++ ;
    }
    *data = ck ;
}

int mcu_input(int usdelay);

// send command to mcu
// return 0 : failed
//        >0 : success
int mcu_send( char * cmd ) 
{
    if( *cmd<5 || *cmd>40 ) { 	// wrong data
        return 0 ;
    }
    mcu_calchecksun( cmd );

#ifdef  MCU_DEBUG
// for debugging    
    struct timeval tv ;
    gettimeofday(&tv, NULL);
    int r ;
    struct tm * ptm ;
    ptm = localtime(&tv.tv_sec);
    double sec ;
    sec = ptm->tm_sec + tv.tv_usec/1000000.0 ;
    
    printf("%02d:%02d:%02.3f Send: ", ptm->tm_hour, ptm->tm_min, sec);
    r=serial_write(cmd, (int)(*cmd));
    printf("\n");
    return r ;
#else    
    return serial_write(cmd, (int)(*cmd));
#endif
}

// receive "Enter a command" , success: return 1, failed: return 0
int mcu_recv_enteracommand()
{
    int res = 0 ;
    char enteracommand[200] ;
    while( serial_dataready(100000) ) {
        memset( enteracommand, 0, sizeof(enteracommand));
        serial_read(enteracommand, sizeof(enteracommand));
        if( strcasestr( enteracommand, "Enter a command" )) {
            res=1 ;
        }
    }
    return res ;
}


// check mcu input
// parameter
//      usdelay - micro-seconds waiting
// return 
//		1: got something, digital input or power off input?
//		0: nothing
int mcu_input(int usdelay)
{
  unsigned char buf[1024];
  int bytes;
  struct timeval tv;

  tv.tv_sec = usdelay / 1000000;
  tv.tv_usec = usdelay % 1000000;

  /* can be 7(ign off), 8(ign on), 9(dichange, gforce) bytes */
  bytes = read_nbytes(serial_handle, buf, sizeof(buf), 7, &tv, g_mcudebug, 0);
  if (bytes > 0) {
    handle_mcu(serial_handle, buf, bytes, 0, NULL);
  }

  return 1;
}

int mcu_bootupready()
{
  int retry = 3;
  unsigned char buf[1024];
  struct timeval tv;

  if (g_bootupsent)
    return 0;

  while (retry > 0) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    tcflush(serial_handle, TCIOFLUSH);
    sendBootupReady(serial_handle);

    if (read_nbytes(serial_handle, buf, sizeof(buf), 6,
		    &tv, g_mcudebug, 0) >= 6) {
      if ((buf[0] == 0x12) &&
	  (buf[1] == 0x00) &&
	  (buf[2] == 0x01) &&
	  (buf[3] == MCUCMD_BOOTREADY) &&
	  (buf[4] == 0x03)) {
	if (verifyChecksum(buf, buf[0])) {
	  dprintf("checksum error\n");
	} else {
	  dprintf("Bootup ready\n");
	  hdlock = ((buf[5] & HDKEYLOCK) == 0);	  // HD locked
	  hdinserted = ((buf[5] & HDINSERTED) == 0);  // HD inserted
	  unsigned int imap1 = buf[5];
	  update_inputmap(imap1, NULL);
	  g_bootupsent = 1;
	  dprintf("Bootup ready(%x,%d,%d)\n", imap1, hdlock, hdinserted);
	  break;
	}
      }
    }
    retry--;
  }

  return (retry > 0) ? 0 : 1;
}

void check_mcu_error()
{
  mcu_recverror++;
  if (mcu_recverror > 5) {
    sleep(1);
    serial_init();
    mcu_recverror = 0;
  }
}

int code ;

int mcu_readcode()
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes;
  struct timeval tv;
  int done;

  while (retry > 0) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    sendShutdownCode(serial_handle);

    /* mcu version older than v.5.9 sends only 7 bytes */
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 14,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_SHUTDOWN_CODE, &done);
      if (done)
	return 0;
    }
    retry--;
    check_mcu_error();
  }

  dvr_log( "Read MCU shutdown code failed.");
  return 1 ;
}

int mcu_reboot()
{
    static char cmd_reboot[8]="\x06\x01\x00\x12\x02\xcc" ;
    
    mcu_send(cmd_reboot) ;
    // no responds
    return 0 ;
}
#if 0
int queryDI(int fd)
{
  int retry = 3;
  unsigned char buf[1024];
  int done, bytes, rxlen = 7;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendDIQuery(serial_handle);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen,
			&tv, g_mcudebug, 0);
    dump(buf, bytes);
    if (bytes > 0) {
      /* Do not call handle_mcu() here */
      _handle_mcu(serial_handle, buf, bytes, MCUCMD_DIQUERY, &done);
      if (done) {
	return 0;
      }
    }
    retry--;
    check_mcu_error();
  }

  return 1;
}
#endif
int setPowerOffDelay(int fd, int delay)
{
  int retry = 3;
  unsigned char buf[1024];
  int done, bytes, rxlen = 6;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendPowerOffDelay(serial_handle, delay);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_POWEROFF_DELAY, &done);
      if (done)
	return 0;
    }
    retry--;
    check_mcu_error();
  }

  return 1;
}

int mcu_gsensorinit()
{
  int retry = 3;
  unsigned char buf[1024];
  int done, bytes, rxlen = 7;
  struct timeval tv;

  if( !gforce_log_enable ) {
    return 0 ;
  }

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendGForceInit(serial_handle);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_GFORCE_INIT, &done);
      if (done)
	return 1;
    }
    retry--;
    check_mcu_error();
  }

  gforce_log_enable = 0;

  return 0;
}

int mcu_version()
{
  int retry = 3;
  unsigned char buf[1024];
  int done, bytes, rxlen = 22;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = 1;
    tv.tv_usec = SERIAL_DELAY;

    sendMCUFWVersionReq(serial_handle);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen, &tv,
			g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_FW_VER, &done);
      if (done)
	return 0;
    }
    retry--;
    check_mcu_error();
  }

  return 1;
}

int mcu_doutput()
{
  unsigned int outputmap = p_dio_mmap->outputmap ;
  unsigned int outputmapx ;
  int i ;
  unsigned char bitmap = 0;
  int retry = 3;
  
  if (!buzzer_enable) {
    outputmap &= (~0x100) ;     // buzzer off
  }
  
  if( outputmap == mcu_doutputmap ) return 1 ;
  
  outputmapx = (outputmap^output_inverted) ;
  
  // bit 2 is beep. (from July 6, 2009)
  // translate output bits ;
  bitmap = 0 ;
  for(i=0; i<MAXOUTPUT; i++) {
    if( outputmapx & output_map_table[i] ) {
      bitmap |= (1<<i) ;
    }
  }
  
  unsigned char buf[1024];
  int done, bytes, rxlen = 7;
  struct timeval tv;
  
  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;
    
    sendDOBitmap(serial_handle, bitmap);
    
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_DOMAP, &done);
      if (done) {
	mcu_doutputmap=outputmap ;
	return 0;
      }
    }
    
    retry--;
    check_mcu_error();
  }

  return 1;
}

// set more mcu power off delay (keep power alive), (called every few seconds)
int mcu_poweroffdelay(int delay)
{
  unsigned char buf[1024];
  int bytes;
  struct timeval tv;
  int done;
  int retry = 3;
  int poweroffdelaytime = 0;

  if( mcupowerdelaytime < 50 || mcupowerdelaytime < delay ) {
        poweroffdelaytime = delay;
  }

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;
    sendIgnOffExtraDelay(serial_handle, poweroffdelaytime);
    
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 8,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      int ret = handle_mcu(serial_handle, buf, bytes, MCUCMD_IGNOFF, &done);
      if (done) {
	mcupowerdelaytime = ret;
	if (g_mcudebug2)
	  dprintf("mcu delay %d s\n", mcupowerdelaytime );
	return 0;
      }
    }

    retry--;
    check_mcu_error();
  }

  return 1;
}

int mcu_watchdogtimeout(int timeout)
{
  int retry = 3;
  unsigned char buf[1024];
  int done, bytes, rxlen = 6;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendWatchdogTimeout(serial_handle, timeout);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_WATCHDOGTIME, &done);
      if (done)
	return 0;
    }
    retry--;
    check_mcu_error();
  }

  return 1;
}

int mcu_setwatchdogenable(int enable)
{
  int retry = 3;
  unsigned char buf[1024];
  int done, bytes, rxlen = 6;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendWatchdogEnable(serial_handle, enable);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes,
		 enable ? MCUCMD_WATCHDOGENABLE : MCUCMD_WATCHDOGDISABLE,
		 &done);
      if (done)
	return 0;
    }
    retry--;
    check_mcu_error();
  }

  return 1;
}

void mcu_watchdogenable()
{
  if (!mcu_watchdogtimeout(watchdogtimeout))
    dprintf("watch dog timeout set:%d\n", watchdogtimeout);
  if (!mcu_setwatchdogenable(1))
    dprintf("watch dog enabled\n");
  watchdogenabled = 1 ;
}

void mcu_watchdogdisable()
{
  if (!mcu_setwatchdogenable(0))
    dprintf("watch dog disabled\n");
  watchdogenabled = 0 ;
}

// return 0: error, >0 success
int mcu_watchdogkick()
{
  int retry = 3;
  unsigned char buf[1024];
  int done, bytes, rxlen = 6;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendWatchdog(serial_handle);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), rxlen,
			&tv, g_mcudebug2, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_WATCHDOG, &done);
      if (done)
	return 0;
    }
    retry--;
    check_mcu_error();
  }

  return 1;
}

// get io board temperature
int mcu_iotemperature()
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes, done, ret;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendReadIOTempReq(serial_handle);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 8,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      ret = handle_mcu(serial_handle, buf, bytes, MCUCMD_IOTEMP, &done);
      if (done) 
	return ret;
    }

    retry--;
    check_mcu_error();
  }

  return -128 ;
}

// get hd temperature
int mcu_hdtemperature()
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes, done, ret;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;
    
    sendReadHdTempReq(serial_handle);
    
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 8,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      ret = handle_mcu(serial_handle, buf, bytes, MCUCMD_USBHDTEMP, &done);
      if (done) 
	return ret;
    }

    retry--;
    check_mcu_error();
  }

  return -128 ;
}


int mcu_hdpower(int on)
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes, done;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendTurnHdPower(serial_handle, on);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 6,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes,
		 on ? MCUCMD_USBHDON : MCUCMD_USBHDOFF, &done);
      if (done)
	return 0;
    }

    retry--;
    check_mcu_error();
  }

  return 1 ;
}

// return time_t: success
//             -1: failed
time_t mcu_r_rtc()
{
  int retry = 3;
  unsigned char buf[1024];
  time_t tt ;
  int bytes, done;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;

    sendRTCReq(serial_handle);

    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 13,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      tt = handle_mcu(serial_handle, buf, bytes, MCUCMD_RTC, &done);
      if (done)
	return tt;
    }
    retry--;
    check_mcu_error();
  }

  dvr_log("Read clock from MCU failed!");
  return (time_t)-1;
}

void mcu_readrtc()
{
    time_t t ;
    struct tm ttm ;
    t = mcu_r_rtc() ;
    if( (int)t > 0 ) {
      if (gmtime_r(&t, &ttm)) {
        p_dio_mmap->rtc_year  = ttm.tm_year+1900 ;
        p_dio_mmap->rtc_month = ttm.tm_mon+1 ;
        p_dio_mmap->rtc_day   = ttm.tm_mday ;
        p_dio_mmap->rtc_hour  = ttm.tm_hour ;
        p_dio_mmap->rtc_minute= ttm.tm_min ;
        p_dio_mmap->rtc_second= ttm.tm_sec ;
        p_dio_mmap->rtc_millisecond = 0 ;
        
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
        return ;
      }
    }
    p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.
}

// 3 LEDs On Panel
//   parameters:
//      led:  0= USB flash LED, 1= Error LED, 2 = Video Lost LED
//      flash: 0=off, 1=flash
int mcu_led(int led, int flash)
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes, done;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;
    
    sendLedControl(serial_handle, (unsigned char)led, flash);
    
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 8, 
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_LED, &done);
      if (done) {
	return 0;
      }
    }

    retry--;
    check_mcu_error();
  }

  return 1 ;
}

// Device Power
//   parameters:
//      device:  0= GPS, 1= Slave Eagle boards, 2=Network switch
//      poweron: 0=poweroff, 1=poweron
int mcu_devicepower(int device, int poweron )
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes, done;
  struct timeval tv;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;
    
    sendDeviceControl(serial_handle, (unsigned char)device, poweron);
    
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 8,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_DEVICE_POWER, &done);
      if (done) {
	return 0;
      }
    }
    
    retry--;
    check_mcu_error();
  }

  return 1 ;
}

// ?
int mcu_reset()
{
    static char cmd_reset[8]="\x06\x01\x00\x00\x02\xcc" ;
    if( mcu_send( cmd_reset ) ) {
        if( serial_dataready (30000000) ) {
            if( mcu_recv_enteracommand() ) {
                return 1;
            }
        }
    }
    return 0 ;
}	

void writeDebug(char *str)
{
  FILE *fp;

  fp = fopen("/var/dvr/iodebug", "a");
  if (fp != NULL) {
    fprintf(fp, "%s\n", str);
    fclose(fp);
  }
}

// update mcu firmware
// return 1: success, 0: failed
int mcu_update_firmware( char * firmwarefile) 
{
    int res = 0 ;
    FILE * fwfile ;
    int c ;
    int rd ;
    char responds[200] ;
    static char cmd_updatefirmware[8] = "\x05\x01\x00\x01\x02\xcc" ;
    
    fwfile=fopen(firmwarefile, "rb");
    
    if( fwfile==NULL ) {
        return 0 ;                  // failed, can't open firmware file
    }
    
    // 
    printf("Start update MCU firmware: %s\n", firmwarefile);
    
    // reset mcu
    printf("Reset MCU.\n");
    if( mcu_reset()==0 ) {              // reset failed.
        printf("Failed\n");
        return 0;
    }
    writeDebug("reset");
    printf("Done.\n");
    
    // clean out serial buffer
    memset( responds, 0, sizeof(responds)) ;
    serial_write( responds, sizeof(responds));
    serial_clear(1000000);
    
    
    writeDebug("erasing");
    printf("Erasing.\n");
    rd=0 ;
    if( serial_write( cmd_updatefirmware, 5 ) ) {
        if( serial_dataready (10000000 ) ) {
            rd=serial_read ( responds, sizeof(responds) ) ;
        }
    }
    
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
    
    writeDebug("Programming");
    printf("Programming.\n");
    // send firmware 
    res = 0 ;
    while( (c=fgetc( fwfile ))!=EOF ) {
      if( serial_write( &c, 1)!=1 ) {
            break;
      }
        if( c=='\n' && serial_dataready (0) ) {
            rd = serial_read (responds, sizeof(responds) );
            if( rd>=5 &&
               responds[0]==0x05 && 
               responds[1]==0x0 && 
               responds[2]==0x01 && 
               responds[3]==0x03 && 
               responds[4]==0x02 ) {
                   res=1 ;
                    break;                  // program done.
            }
            else if( rd>=5 &&
               responds[rd-5]==0x05 && 
               responds[rd-4]==0x0 && 
               responds[rd-3]==0x01 && 
               responds[rd-2]==0x03 && 
               responds[rd-1]==0x02 ) {
                   res=1 ;
                    break;                  // program done.
            }
            else if( rd>=6 &&
               responds[0]==0x06 && 
               responds[1]==0x0 && 
               responds[2]==0x01 && 
               responds[3]==0x01 && 
               responds[4]==0x03 ) {
                    break;                  // receive error report
            }
            else if( rd>=6 &&
               responds[rd-6]==0x06 && 
               responds[rd-5]==0x0 && 
               responds[rd-4]==0x01 && 
               responds[rd-3]==0x01 && 
               responds[rd-2]==0x03 ) {
                    break;                  // receive error report
            }
        }
    }

    fclose( fwfile );
    
    // wait a bit (5 seconds), for firmware update done signal
    if( res==0 && serial_dataready(5000000) ) {
            rd = serial_read (responds, sizeof(responds) );
            if( rd>=5 &&
               responds[0]==0x05 && 
               responds[1]==0x0 && 
               responds[2]==0x01 && 
               responds[3]==0x03 && 
               responds[4]==0x02 ) {
               res=1 ;
            }
            else if( rd>=5 &&
               responds[rd-5]==0x05 && 
               responds[rd-4]==0x0 && 
               responds[rd-3]==0x01 && 
               responds[rd-2]==0x03 && 
               responds[rd-1]==0x02 ) {
               res=1 ;
            }
    }
    if( res ) {
      writeDebug("success");
        printf("Done.\n");
    }
    else {
      writeDebug("fail");
        printf("Failed.\n");
    }
    return res ;
}

// return 1: success
//        0: failed
int mcu_w_rtc(time_t tt)
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes, done;
  struct timeval tv;
  struct tm bt ;

  gmtime_r(&tt, &bt);

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;
    
    setRTC(serial_handle, &bt);
    
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 6,
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_SETRTC, &done);
      if (done) {
	return 0;
      }
    }
    
    retry--;
    check_mcu_error();
  }

  return 1;
}

void mcu_setrtc()
{
  int retry = 3;
  unsigned char buf[1024];
  int bytes, done;
  struct timeval tv;
  struct tm bt;

  bt.tm_year = p_dio_mmap->rtc_year - 1900;
  bt.tm_mon = p_dio_mmap->rtc_month - 1;
  bt.tm_mday = p_dio_mmap->rtc_day;
  bt.tm_hour = p_dio_mmap->rtc_hour;
  bt.tm_min = p_dio_mmap->rtc_minute;
  bt.tm_sec = p_dio_mmap->rtc_second;
  bt.tm_isdst = 0;

  while (retry > 0) {
    tv.tv_sec = SERIAL_DELAY_SEC;
    tv.tv_usec = SERIAL_DELAY;
    
    setRTC(serial_handle, &bt);
    
    bytes = read_nbytes(serial_handle, buf, sizeof(buf), 6, 
			&tv, g_mcudebug, 0);
    if (bytes > 0) {
      handle_mcu(serial_handle, buf, bytes, MCUCMD_SETRTC, &done);
      if (done) {
	p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
	return;
      }
    }
    
    retry--;
    check_mcu_error();
  }

  p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.
  return ;
}

// sync system time to rtc
void mcu_syncrtc()
{
    time_t t ;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    t = (time_t) current_time.tv_sec ;
    if( !mcu_w_rtc(t) ) {
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
    }
    else {
        p_dio_mmap->rtc_cmd   = -1 ;	// cmd error
    }
}

void mcu_rtctosys()
{
    struct timeval tv ;
    time_t rtc ;
    rtc=mcu_r_rtc();
    if( (int)rtc>0 ) {
      tv.tv_sec=rtc ;
      tv.tv_usec=0;
      settimeofday(&tv, NULL);
      // also set onboard rtc
      rtc_set((time_t)tv.tv_sec);
      dprintf("system time set to:%ld\n", rtc);
    }
}

int stop_dvr_recording()
{
  dprintf("stop_dvr_recording\n");
  if( p_dio_mmap->dvrpid>0 && 
      (p_dio_mmap->dvrstatus & DVR_RUN) &&
      (p_dio_mmap->dvrcmd == 0 )) {
    p_dio_mmap->dvrcmd = 3;
    dprintf("stop_dvr_recording signaled\n");
    return 0;
  }

  return 1;
}

int start_dvr_recording()
{
  dprintf("start_dvr_recording\n");
  if( p_dio_mmap->dvrpid>0 && 
      (p_dio_mmap->dvrstatus & DVR_RUN) &&
      (p_dio_mmap->dvrcmd == 0 )) {
    p_dio_mmap->dvrcmd = 4;
    dprintf("start_dvr_recording signaled\n");
    return 0;
  }

  return 1;
}

void do_gpssync(time_t gpstime)
{
  struct timeval tv ;
  
  tv.tv_sec=gpstime ;
  tv.tv_usec= 0;
  settimeofday( &tv, NULL );
  // set mcu time
  mcu_w_rtc(gpstime) ;
  // also set onboard rtc
  rtc_set(gpstime);
}

int time_syncgps ()
{
    struct timeval tv ;
    int diff ;
    time_t gpstime ;
    struct gps g;

    dprintf("time_syncgps\n");
    get_gps_data(&g);

    if( g.gps_valid ) {
        gpstime = (time_t) g.gps_gpstime ;
	/* sanity check */
	if (gpstime < 946728000) /* year 2000 */
	  return 0;

        gettimeofday(&tv, NULL);
        diff = (int)gpstime - (int)tv.tv_sec ;
        if( diff>2 || diff<-2 ) {
	  /* update time only when recording is stopped */
	  if (g_stoprecordingsent) {
	    /* acked by dvrsvr? */
	    if (p_dio_mmap->dvrcmd == 0) {
	      g_stoprecordingsent = 0;
	      dvr_log ( "Time sync using GPS time:%d.",diff );
	      do_gpssync(gpstime);
	      start_dvr_recording();
	    } else {
	      return 1; // don't update adjtimer yet
	    }
	  } else {
	    if (g_startrecordingsent) {
	      stop_dvr_recording();
	      // Even if dvrsvr is not ready to receive command, 
	      // we can safely change time.
	      g_stoprecordingsent = 1;
	      return 1; // don't update adjtimer yet
	    } else {
	      /* update time if recording is not started at all */
	      dvr_log ( "Time sync using GPS time: %d.",diff );
	      do_gpssync(gpstime);
	    }
	  }
        }
	/*
        else if( diff>1 || diff<-1 ) {
            tv.tv_sec =(time_t)diff ;
            tv.tv_usec = 0 ;
            adjtime( &tv, NULL );
            //dvr_log ( "Time adusted using GPS time:%d.",diff );
        }
	*/
    }

    return 0;
}

void time_syncmcu()
{
    int diff ;
    struct timeval tv ;
    time_t rtc ;
    rtc=mcu_r_rtc();
    if( (int)rtc>0 ) {
        gettimeofday(&tv, NULL);
        diff = (int)rtc - (int)tv.tv_sec ;
        if( diff>2 || diff<-2 ) {
            tv.tv_sec=rtc ;
            tv.tv_usec=0;
            settimeofday( &tv, NULL );
            dvr_log ( "Time sync use MCU time." );
	    // set onboard rtc
	    rtc_set(rtc);
        }
	/*
        else if( diff>1 || diff<-1 ){
            tv.tv_sec =(time_t)diff ;
            tv.tv_usec = 0 ;
            adjtime( &tv, NULL );
        }
	*/
    }
}


// execute setnetwork script to recover network interface.
void setnetwork()
{
    int status ;
    static pid_t pid_network=0 ;
    if( pid_network>0 ) {
        kill(pid_network, SIGTERM);
        waitpid( pid_network, &status, 0 );
    }
    pid_network = fork ();
    if( pid_network==0 ) {
        static char snwpath[]="/davinci/dvr/setnetwork" ;
        execl(snwpath, snwpath, NULL);
        exit(0);    // should not return to here
    }
}

// Buzzer functions
static int buzzer_timer ;
static int buzzer_count ;
static int buzzer_on ;
static int buzzer_ontime ;
static int buzzer_offtime ;

void buzzer(int times, int ontime, int offtime)
{
    if( times>=buzzer_count ) {
        buzzer_timer = getruntime();
        buzzer_count = times ;
        buzzer_on = 0 ;
        buzzer_ontime = ontime ;
        buzzer_offtime = offtime ;
    }
}

// run buzzer, runtime: runtime in ms
void buzzer_run( int runtime )
{
    if( buzzer_count>0 && (runtime - buzzer_timer)>=0 ) {
        if( buzzer_on ) {
            buzzer_count-- ;
            buzzer_timer = runtime+buzzer_offtime ;
            p_dio_mmap->outputmap &= ~0x100 ;     // buzzer off
        }
        else {
            buzzer_timer = runtime+buzzer_ontime ;
            p_dio_mmap->outputmap |= 0x100 ;     // buzzer on
        }
        buzzer_on=!buzzer_on ;
    }
}    


int smartftp_retry ;
int smartftp_disable ;
int smartftp_reporterror ;

static int readWifiDeviceName(char *dev, int bufsize)
{
  char buf[256];
  FILE * fp;
  fp = fopen("/proc/net/wireless", "r");
  if (fp != NULL) {
    int line = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
      line++;
      if (line < 3)
	continue;

      char *ptr;
      ptr = strchr(buf, ':');
      if (ptr) {
	*ptr = '\0';
	/* remove leading spaces */
	ptr = buf;
	while(*ptr == 0x20) {
	  ptr++;
	}
	if (dev) strncpy(dev, ptr, bufsize);
	fclose(fp);
	return 0; 
      }
    }
    fclose(fp);
  }

  return 1;
}

void tab102_start()
{
  dvr_log( "Start Tab102 downloading.");
  pid_tab102 = fork();
  if( pid_tab102==0 ) {     // child process
    execl( "/davinci/dvr/tab102.sh", "/davinci/dvr/tab102.sh",
	   NULL 
	   );
        
    dvr_log( "Start Tab102 failed!");
    
    exit(101);      // error happened.
  }
  else if( pid_tab102<0 ) {
    pid_tab102=0;
    dvr_log( "Start Tab102 failed!");
  }
}

/*
 * return value:
 *          0 - tab102 is still running
 *          1 - tab102 terminated or not started at all or error happened
 */
int tab102_wait()
{
    int tab102_status=0 ;
    pid_t id ;
    int exitcode=-1 ;
    if( pid_tab102>0 ) {
        id=waitpid( pid_tab102, &tab102_status, WNOHANG  );
        if( id==pid_tab102 ) {
            pid_tab102=0 ;
            if( WIFEXITED( tab102_status ) ) {
                exitcode = WEXITSTATUS( tab102_status ) ;
                dvr_log( "\"tab102\" exit. (code:%d)", exitcode );
            }
            else if( WIFSIGNALED(tab102_status)) {
                dvr_log( "\"tab102\" killed by signal %d.", WTERMSIG(tab102_status));
            }
            else {
                dvr_log( "\"tab102\" aborted." );
            }
        } else if (id == 0) {
	  return 0;
	} else { 
	  /* error happened? */
	  dvr_log( "\"tab102\" waitpid error." );
	}
    }

    return 1;
}

void smartftp_start()
{
  char dev[32];
  FILE *fp = fopen(dvrcurdisk, "r");
  if (!fp) {
    return;
  }
  fclose(fp);

  if (readWifiDeviceName(dev, sizeof(dev))) {
    /* no wifi module found, try external wifi */
    strcpy(dev, "eth0");
  }
  /* wifi disabled? */
  if (g_wifi_proto == 2) {
    strcpy(dev, "eth0:1");
  }

  dvr_log( "Start smart server uploading(%s).", dev);
  pid_smartftp = fork();
  if( pid_smartftp==0 ) {     // child process  
    system("/davinci/dvr/setnetwork");  // reset network, this would restart wifi adaptor
        
    execl( "/davinci/dvr/smartftp", "/davinci/dvr/smartftp",
	   dev,
	   g_hostname,
	   "247SECURITY",
	   "/var/dvr/disks",
	   "510",
	   "247ftp",
	   "247SECURITY",
	   "0",
	   smartftp_reporterror?"Y":"N",
	   getenv("TZ"), 
	   NULL 
	   );
        
    //        smartftp_log( "Smartftp start failed." );

    dvr_log( "Start smart server uploading failed!");

    exit(101);      // error happened.
  }
  else if( pid_smartftp<0 ) {
    pid_smartftp=0;
    dvr_log( "Start smart server failed!");
  }
  //    smartftp_log( "Smartftp started." );
}

/*
 * return value:
 *          0 - smartftp is still running
 *          1 - smartftp terminated or not started at all or error happened
 */
int smartftp_wait()
{
    int smartftp_status=0 ;
    pid_t id ;
    int exitcode=-1 ;
    if( pid_smartftp>0 ) {
        id=waitpid( pid_smartftp, &smartftp_status, WNOHANG  );
        if( id==pid_smartftp ) {
            pid_smartftp=0 ;
            if( WIFEXITED( smartftp_status ) ) {
                exitcode = WEXITSTATUS( smartftp_status ) ;
                dvr_log( "\"smartftp\" exit. (code:%d)", exitcode );
                if( (exitcode==3 || exitcode==6) && 
                   --smartftp_retry>0 ) 
                {
                    dvr_log( "\"smartftp\" failed, retry...");
                    smartftp_start();
		    return 0;
                }
            }
            else if( WIFSIGNALED(smartftp_status)) {
                dvr_log( "\"smartftp\" killed by signal %d.", WTERMSIG(smartftp_status));
            }
            else {
                dvr_log( "\"smartftp\" aborted." );
            }
        } else if (id == 0) {
	  return 0;
	} else { 
	  /* error happened? */
	  dvr_log( "\"smartftp\" waitpid error." );
	}
    }

    return 1;
}

// Kill smartftp in case standby timeout or power turn on
void smartftp_kill()
{
    if( pid_smartftp>0 ) {
        kill( pid_smartftp, SIGTERM );
        dvr_log( "Kill \"smartftp\"." );
        pid_smartftp=0 ;
    }
}    

// Kill tab102 in case power turn on
void tab102_kill()
{
    if( pid_tab102>0 ) {
        kill( pid_tab102, SIGTERM );
        dvr_log( "Kill \"tab102\"." );
        pid_tab102=0 ;
    }
}    

int g_syncrtc=0 ;


float cpuusage()
{
    static float s_cputime=0.0, s_idletime=0.0 ;
    float cputime, idletime ;
    float usage=0.0 ;
    FILE * uptime ;
    uptime = fopen( "/proc/uptime", "r" );
    if( uptime ) {
        fscanf( uptime, "%f %f", &cputime, &idletime );
        fclose( uptime );
        usage=1.0 -(idletime-s_idletime)/(cputime-s_cputime) ;
        s_cputime=cputime ;
        s_idletime=idletime ;
    }
    return usage ;
}

// return 
//        0 : failed
//        1 : success
int appinit()
{
    FILE * pidf ;
    int fd ;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    int i;
    
    g_startrecordingsent = 0;

    // setup time zone
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

    v = dvrconfig.getvalue( "system", "hostname");
    if( v.length()>0 ) {
      strncpy( g_hostname, v.getstring(), sizeof(g_hostname) );
    } 
    v = dvrconfig.getvalue( "system", "logfile");
    if (v.length() > 0){
        strncpy( logfile, v.getstring(), sizeof(logfile));
    }
    v = dvrconfig.getvalue( "system", "temp_logfile");
    if (v.length() > 0){
        strncpy( temp_logfile, v.getstring(), sizeof(temp_logfile));
    }
        
    v = dvrconfig.getvalue( "system", "currentdisk");
    if (v.length() > 0){
        strncpy( dvrcurdisk, v.getstring(), sizeof(dvrcurdisk));
    }

    v = dvrconfig.getvalue( "system", "iomapfile");
    char * iomapfile = v.getstring();
    if( iomapfile && strlen(iomapfile)>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    fd = open(dvriomap, O_RDWR | O_CREAT, S_IRWXU);
    if( fd<=0 ) {
        printf("Can't create io map file!\n");
        return 0 ;
    }
    ftruncate(fd, sizeof(struct dio_mmap));
    p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );								// fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    
    if( p_dio_mmap->usage <=0 ) {
        memset( p_dio_mmap, 0, sizeof( struct dio_mmap ) );
    }
    
    p_dio_mmap->usage++;                           // add one usage

    if( p_dio_mmap->iopid > 0 ) { 				// another ioprocess running?
        pid_t pid = p_dio_mmap->iopid ;
        // kill it
        if( kill( pid,  SIGTERM )==0 ) {
            // wait for it to quit
            waitpid( pid, NULL, 0 );
            //sync();
            sleep(2);
        }
        else {
            sleep(5);
        }
    }

    // if ioprocess still running, kill it
    pid_t iopid ;
    pidf=fopen("/var/dvr/ioprocess.pid", "r");
    if( pidf ) {
      iopid=0 ;
      fscanf(pidf, "%d", &iopid);
      fclose( pidf );
      if( iopid>0 ) {
 	kill( iopid, SIGKILL );
      }
    }
        
    // initialize shared memory
    p_dio_mmap->inputnum = dvrconfig.getvalueint( "io", "inputnum");	
    if( p_dio_mmap->inputnum<=0 || p_dio_mmap->inputnum>MAXINPUT_WITH_TAB102 )
        p_dio_mmap->inputnum = MCU_INPUTNUM ;	
    p_dio_mmap->outputnum = dvrconfig.getvalueint( "io", "outputnum");	
    if( p_dio_mmap->outputnum<=0 || p_dio_mmap->outputnum>32 )
        p_dio_mmap->outputnum = MCU_OUTPUTNUM ;	
    g_mcuinputnum = dvrconfig.getvalueint( "io", "mcuinputnum");	
    if( g_mcuinputnum<=0 || g_mcuinputnum>MCU_INPUTNUM )
        g_mcuinputnum = MCU_INPUTNUM ;	

    p_dio_mmap->inputmap = 0;
    p_dio_mmap->panel_led = 0;
    p_dio_mmap->devicepower = 0xffff; // assume all device is power on
    
    p_dio_mmap->iopid = getpid () ; // io process pid
    
    // set sensor input default value
    unsigned int defaultmap = 0;
    //sensor_input_active_bitmap = 0xffff;
    for( i=0; i<p_dio_mmap->inputnum; i++ ) {
      char sec[12] ;
      unsigned int mask = 0;
      sprintf( sec, "sensor%d", i+1);
      if (dvrconfig.getvalueint(sec, "inverted")) {
	mask = (1 << i);
	defaultmap |= mask;
	//sensor_input_active_bitmap &= ~mask;
      }
    }
    p_dio_mmap->inputmap = defaultmap;
    //dvr_log("inputmap:%x",defaultmap);

    output_inverted = 0 ;
    
    for( i=0; i<32 ; i++ ) {
        char outinvkey[50] ;
        sprintf(outinvkey, "output%d_inverted", i+1);
        if( dvrconfig.getvalueint( "io", outinvkey ) ) {
            output_inverted |= (1<<i) ;
        }
    }
    
    // check if sync rtc wanted    
    g_syncrtc = dvrconfig.getvalueint("io", "syncrtc");
    
    // initilize serial port
    v = dvrconfig.getvalue( "io", "serialport");
    if( v.length()>0 ) {
        strcpy( serial_dev, v.getstring() );
    }
    serial_baud = dvrconfig.getvalueint("io", "serialbaudrate");
    if( serial_baud<2400 || serial_baud>115200 ) {
            serial_baud=DEFSERIALBAUD ;
    }
    serial_init ();

    // smartftp variable
    smartftp_disable = dvrconfig.getvalueint("system", "smartftp_disable");
   
    
    buzzer_enable = dvrconfig.getvalueint( "io", "buzzer_enable");

    // gforce sensor setup

    gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");

    // get gsensor direction setup
    
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
    p_dio_mmap->gforce_log0=0;
    p_dio_mmap->gforce_log1=0;
    
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
    
    usewatchdog = dvrconfig.getvalueint("io", "usewatchdog");
    watchdogtimeout=dvrconfig.getvalueint("io", "watchdogtimeout");
    if( watchdogtimeout<10 )
        watchdogtimeout=10 ;
    if( watchdogtimeout>200 )
        watchdogtimeout=200 ;

    g_wifi_proto = dvrconfig.getvalueint("network", "wifi_proto");

    pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }

    // initialize mcu (power processor)
    if( !mcu_bootupready () ) {
        dvr_log("MCU ready.");
        mcu_readcode();
    }
    else {
        dvr_log("MCU failed.");
    }

    if( g_syncrtc ) {
        mcu_rtctosys();
    }

    return 1;
}

// app finish, clean up
void appfinish()
{
    int lastuser ;
    
    // close serial port
    if( serial_handle>0 ) {
        close( serial_handle );
        serial_handle=0;
    }
    
    p_dio_mmap->iopid = 0 ;
    p_dio_mmap->usage-- ;
    
    if( p_dio_mmap->usage <= 0 ) {
        lastuser=1 ;
    }
    else {
        lastuser=0;
    }
    
    // clean up shared memory
    munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    
    if( lastuser ) {
        unlink( dvriomap );         // remove map file.
    }
    
    // delete pid file
    unlink( pidfile );
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

int main(int argc, char * argv[])
{
    int i;
    int hdpower=0 ;                             // assume HD power is off
    int hdkeybounce = 0 ;                       // assume no hd
    unsigned int modeendtime = 0;
    unsigned int gpsavailable = 0 ;
    unsigned int smartftp_endtime = 0;
    int diskready = 0;
    
    if ((argc >= 2) &&
	(!strcmp(argv[1], "-f") ||      /* force foreground */
	 !strcmp(argv[1], "-d") ||     /* MCU debugging */
	 !strcmp(argv[1], "-e") ||     /* MCU debugging(including watchdog) */
	 !strcmp(argv[1], "-fw") ||     /* MCU upgrade */
	 !strcmp(argv[1], "-reboot"))) { /* system reset */
      if (!strcmp(argv[1], "-f")) {
	g_devicepower_allways_on = 1;
      } else if (!strcmp(argv[1], "-d")) {
	g_devicepower_allways_on = 1;
	g_mcudebug = 1;
      } else if (!strcmp(argv[1], "-e")) {
	g_devicepower_allways_on = 1;
	g_mcudebug = 1;
	g_mcudebug2 = 1;
      }
    } else {
      if (daemon(0, 0) < 0) {
	perror("daemon");
	exit(1);
      }
    }
  
    prepare_semaphore();

    if( appinit()==0 ) {
        return 1;
    }

    sleep(1); // requested by Zhen

    if (gforce_log_enable)
      checkContinuousData(serial_handle);
    unlink("/var/dvr/gforcecheck"); // let start.sh continue
    
    // gsensor trigger setup
    if( mcu_gsensorinit() ) {       // g sensor available
        dvr_log("G sensor detected!");
        FILE *pidf = fopen( "/var/dvr/gsensor", "w" );
        if( pidf ) {
            fprintf(pidf, "1");
            fclose(pidf);
        }
    }
    

    // get MCU version
    if( !mcu_version() ) {
        dprintf("MCU: %s\n", mcu_firmware_version );
        FILE * mcuversionfile=fopen("/var/dvr/mcuversion", "w");
        if( mcuversionfile ) {
            fprintf( mcuversionfile, "%s", mcu_firmware_version );
            fclose( mcuversionfile );
        }
    }        

    // initialize timer
    initruntime();
    

    // check if we need to update firmware
    if( argc>=2 ) {
        for(i=1; i<argc; i++ ) {
            if( strcasecmp( argv[i], "-fw" )==0 ) {
                if( argv[i+1] && mcu_update_firmware( argv[i+1] ) ) {
                    appfinish ();
                    return 0 ;
                }
                else {
                    appfinish ();
                    return 1;
                }
            }
            else if( strcasecmp( argv[i], "-reboot" )==0 ) {
                int delay = 5 ;
                if( argv[i+1] ) {
                    delay = atoi(argv[i+1]) ;
                }
                if( delay<5 ) {
                    delay=5 ;
                }
                else if(delay>100 ) {
                    delay=100;
                }
//                sleep(delay);
//                mcu_reboot();
                watchdogtimeout=delay ;
                mcu_watchdogenable () ;
                sleep(delay) ;
                mcu_reboot ();
                return 1;
            }
        }
    }
  
    // setup signal handler	
    set_app_mode(1) ;
    signal(SIGQUIT, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR2, sig_handler);
    
    dprintf( "DVR IO process started!\n");
      
    // initialize output
    mcu_doutputmap=~(p_dio_mmap->outputmap) ;
    
    // initialize panel led
    panelled = 0xff ;
    p_dio_mmap->panel_led = 0;
    
    // initialize device power
    devicepower = 0 ;
    p_dio_mmap->devicepower = 0xffff; // assume all device is power on
    
    while( app_mode ) {
        // do digital output
        mcu_doutput();

        // do input pin polling
        //
        mcu_input(5000);
        
        unsigned int runtime=getruntime() ;
        
        // rtc command check
        if( p_dio_mmap->rtc_cmd != 0 ) {
            if( p_dio_mmap->rtc_cmd == 1 ) {
                mcu_readrtc(); 
            }
            else if( p_dio_mmap->rtc_cmd == 2 ) {
                mcu_setrtc();
            }
            else if( p_dio_mmap->rtc_cmd == 3 ) {
                mcu_syncrtc();
            }
            else {
                p_dio_mmap->rtc_cmd=-1;		// command error, (unknown cmd)
            }
        }

        // mcu command check
        if( p_dio_mmap->mcu_cmd != 0 ) {
            if( p_dio_mmap->mcu_cmd == 1 ) {
	      dvr_log( "HD power off.");
	      if (!mcu_hdpower(0))
		p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
            }
            else if( p_dio_mmap->mcu_cmd == 2 ) {
	      dvr_log( "HD power on.");
	      if (!mcu_hdpower(1))
		p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
            }
            else {
                p_dio_mmap->mcu_cmd=-1;		// command error, (unknown cmd)
            }
        }

        static unsigned int cpuusage_timer ;
        if( (runtime - cpuusage_timer)> 5000 ) {    // 5 seconds to monitor cpu usage
            cpuusage_timer=runtime ;
            static int usage_counter=0 ;
	    if (nocpucheck) {
	      /* no cpu check during file copy */
	      usage_counter=0 ;
	    } else {
	      if( cpuusage()>0.998 ) {
                if( ++usage_counter>36 ) { // CPU keep busy for 3 minites
		  buzzer( 10, 250, 250 );
		  // let system reset by watchdog
		  dvr_log( "CPU usage at 100%% for 180 seconds, system reset.");
		  set_app_mode(APPMODE_REBOOT) ;
                }
	      }
	      else {
                usage_counter=0 ;
	      }
	    }
        }


        // Buzzer functions
        buzzer_run( runtime ) ;
        
        static unsigned int temperature_timer ;
        if( (runtime - temperature_timer)> 10000 ) {    // 10 seconds to read temperature
            temperature_timer=runtime ;

            i=mcu_iotemperature();
	    if (g_mcudebug2)
	      dprintf("Temperature: io(%d)\n",i );

            if( i > -127 && i < 127 ) {
                static int saveiotemp = 0 ;
                if( i>=75 && 
                   (i-saveiotemp) > 2 ) 
                {
                    dvr_log ( "High system temperature: %d", i);
                    saveiotemp=i ;
                    //sync();
                }
                
                p_dio_mmap->iotemperature = i ;
            }
            else {
                p_dio_mmap->iotemperature=-128 ;
            }
            
            i=mcu_hdtemperature();
	    if (g_mcudebug2)
	      dprintf("Temperature: hd(%d)\n", i );

            if( i > -127 && i < 127 ) {
                static int savehdtemp = 0 ;

                if( i>=75 && 
                   (i-savehdtemp) > 2 ) 
                {
                    dvr_log ( "High hard disk temperature: %d", i);
                    savehdtemp=i ;
                    //sync();
                }

                p_dio_mmap->hdtemperature = i ;
            }
            else {
                p_dio_mmap->hdtemperature=-128 ;
            }
            
            // The Buzzer beep 4time at 0.5s interval every 30s when HDD or system temperature is over 66C
            if( p_dio_mmap->iotemperature > 66 || p_dio_mmap->hdtemperature > 66 ) {
                static int hitempbeep=0 ;
                if( ++hitempbeep>3 ) {
                    buzzer( 4, 250, 250 ) ;
                    hitempbeep=0 ;
                }
            }
            
            static int hightemp_norecord = 0 ;
            if( p_dio_mmap->iotemperature > 83 || p_dio_mmap->hdtemperature > 83 ) {
                if( hightemp_norecord == 0 &&
                   app_mode==APPMODE_RUN &&                      
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrcmd == 0 ) &&
                   ((p_dio_mmap->dvrstatus & DVR_RECORD)!=0) )
                {
                    dvr_log( "Stop DVR recording on high temperature."); 
                    p_dio_mmap->dvrcmd = 3;     // stop recording
                    hightemp_norecord = 1 ;
                }
            }
            else if(  p_dio_mmap->iotemperature < 75 && p_dio_mmap->hdtemperature < 75 ) {
                if( hightemp_norecord == 1 &&
                   app_mode==APPMODE_RUN &&                      
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrcmd == 0 ) &&
                   ((p_dio_mmap->dvrstatus & DVR_RECORD)==0) )
                {
                    p_dio_mmap->dvrcmd = 4;     // start recording
                    hightemp_norecord = 0 ;
                    dvr_log( "Resume DVR recording on lower temperature.");
                }
            }
        }
        
                
        static unsigned int appmode_timer ;
        if( (runtime - appmode_timer)> 3000 ) {    // 3 seconds mode timer
            appmode_timer=runtime ;

            //dprintf("mode %d,%d,%x(%d)\n", app_mode,p_dio_mmap->dvrpid,p_dio_mmap->dvrstatus,p_dio_mmap->poweroff);

            // do power management mode switching
            if( app_mode==APPMODE_RUN ) {                         // running mode
	      if (!g_startrecordingsent &&
		  (p_dio_mmap->dvrpid>0) && 
		  (p_dio_mmap->dvrstatus & DVR_RUN) &&
		  (p_dio_mmap->dvrcmd == 0 )) {
		dprintf("start dvrsvr\n");
		p_dio_mmap->dvrcmd = 4;     // start recording
		g_startrecordingsent = 1;
	      }
	      
	      static int app_run_bell = 0 ;
	      if( app_run_bell==0 &&
		  p_dio_mmap->dvrpid>0 && 
		  (p_dio_mmap->dvrstatus & DVR_RUN) &&
		  (p_dio_mmap->dvrstatus & DVR_RECORD) ) 
                {
		  app_run_bell=1 ;
		  buzzer(1, 1000, 100);
                }
	      
	      if( g_poweroff )              // ignition off detected
                {
		  /* ignored during "dvrsvr -d" */
		  if( p_dio_mmap->dvrpid>0 && 
		      (p_dio_mmap->dvrstatus & DVR_RUN)) {
                    modeendtime = runtime + getshutdowndelaytime()*1000;
                    set_app_mode(APPMODE_SHUTDOWNDELAY) ;                        // hutdowndelay start ;
                    dvr_log("Power off switch, enter shutdown delay (mode %d).", app_mode);
		  } else {
                    mcu_poweroffdelay(10) ;
		  }
                }
            }
            else if( app_mode==APPMODE_SHUTDOWNDELAY ) {   // shutdown delay
                if( g_poweroff ) {
                    mcu_poweroffdelay(10) ;
                    if( runtime>modeendtime  ) {
		      p_dio_mmap->poweroff = 1;
		      /* with dvrsvr running, serial port does not work
		       * properly for downloading continuous data from MCU.
		       * so, stop dvr svr instead of just stop recording.
		       */
		      if( p_dio_mmap->dvrpid>0 && 
			  (p_dio_mmap->dvrstatus & DVR_RUN)) {
			/* save diskready status */
			diskready = (p_dio_mmap->dvrstatus & DVR_DISKREADY);
			dprintf("stop dvrsvr\n");
			kill( p_dio_mmap->dvrpid, SIGUSR1 );
			//p_dio_mmap->dvrcmd = 2; // disable liveview/playback
		      }
		      //stop_dvr_recording();
		      //sync();
		      // stop glog recording
		      if( p_dio_mmap->glogpid>0 ) {
			kill( p_dio_mmap->glogpid, SIGUSR1 );
		      }
		      modeendtime = runtime + 120000 ;
		      set_app_mode(APPMODE_NORECORD) ; // start standby mode
		      dvr_log("Shutdown delay timeout, stop recording (mode %d).", app_mode);
                    }
                }
                else {
		  //start_dvr_recording();
		  //p_dio_mmap->devicepower = 0xffff ;
		  p_dio_mmap->poweroff = 0;
		  set_app_mode(APPMODE_RUN) ;                        // back to normal
		  dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);
                }
            }
            else if( app_mode==APPMODE_NORECORD ) {  // close recording and run smart ftp

                if( g_poweroff ) {
                    mcu_poweroffdelay (10);
                }

		if (p_dio_mmap->dvrstatus == 0) { /* dvr stopped? */
		  // download gforce data
		  if (gforce_log_enable) {
		    mcu_watchdogdisable();
                    mcu_poweroffdelay (600);
		    checkContinuousData(serial_handle);
		    // cannot reduce poweroff delay: bug in MCU
		    //setPowerOffDelay(serial_handle, 50);
		    if( usewatchdog && watchdogenabled==0 ) {
		      mcu_watchdogenable();
		    }
		  }
		  unlink("/var/dvr/gforcecheck"); // let start.sh continue

		  if( p_dio_mmap->dvrpid>0) {
		    dprintf("restart dvrsvr:%d\n", p_dio_mmap->dvrpid);
		    kill( p_dio_mmap->dvrpid, SIGUSR2 );
		  }
		  modeendtime = runtime ;        
		}
		
		if( runtime>=modeendtime  ) {
		  // set next app_mode
		  modeendtime = runtime + 120000 ; /* 2 min */
		  set_app_mode(APPMODE_RESTARTDVR) ;
		  dvr_log("%s", gforce_log_enable ? 
			  "Internal GForce check done." :
			  "Internal GForce disabled.");
		}
#if 0             


		  // set next app_mode
		  modeendtime = runtime + 10800000 ; /* 3 hours */
		  set_app_mode(APPMODE_PRESTANDBY) ;
		  dvr_log("Enter Pre-standby mode. (mode %d).", app_mode);
                }
#endif
            }
            else if( app_mode==APPMODE_RESTARTDVR ) {
	      if( g_poweroff ) {
		mcu_poweroffdelay (10);
	      } else {
		dvr_log("Ignition on after Shutdown delay. System reboot");
		watchdogtimeout=10 ; // 10 seconds watchdog time out
		mcu_watchdogenable() ;
		mcu_reboot ();
		exit(1);
	      }
	      
	      /* check if dvrsvr is restarted */
	      if ((p_dio_mmap->dvrpid > 0) &&
		  (p_dio_mmap->dvrstatus & DVR_RUN)) {
		if (diskready) {
		  if (p_dio_mmap->dvrstatus & DVR_DISKREADY) {
		    modeendtime = runtime;        
		  }
		} else {
		  modeendtime = runtime;        
		}
	      }

	      if( runtime>=modeendtime  ) {
		// start Tab102 downloading and do Hybrid disk support
		p_dio_mmap->nodiskcheck = 1;
		tab102_start();

		// set next app_mode
		modeendtime = runtime + 10800000 ; /* 3 hours */
		set_app_mode(APPMODE_PRESTANDBY) ;
		dvr_log("Enter Pre-standby mode. (mode %d).", app_mode);
	      }
            }
            else if( app_mode==APPMODE_PRESTANDBY ) {  // get Tab102 data

                if( g_poweroff ) {
                    mcu_poweroffdelay (10);
                } else {
		  dvr_log("Power on switch after Pre-standby. System reboot");
		  p_dio_mmap->poweroff = 0;
		  p_dio_mmap->nodiskcheck = 0;
		  sleep(5);
		  sync();
		  tab102_kill();
		  watchdogtimeout=10 ; // 10 seconds watchdog time out
		  mcu_watchdogenable() ;
		  mcu_reboot ();
		  exit(1);
		}

		// check if tab102 downloading ended
		if (tab102_wait()) {
		  //done in start.sh now:check_tab102_data(); 
		  modeendtime = runtime ;        
		  p_dio_mmap->nodiskcheck = 0;
		  nocpucheck = 0;
		}
                
                if( runtime>=modeendtime ) {
		  tab102_kill();
		  p_dio_mmap->nodiskcheck = 0;
		  nocpucheck = 0;
		  set_app_mode(APPMODE_STANDBY) ;
		  modeendtime = runtime+getstandbytime()*1000 ;
		  if( smartftp_disable==0 ) {
		    
		    smartftp_endtime = runtime+4*3600*1000 ;
		    // check video lost report to smartftp.
		    if( (p_dio_mmap->dvrstatus & (DVR_VIDEOLOST|DVR_ERROR) )==0 )
		      {
			smartftp_reporterror = 0 ;
		      }
		    else {
		      smartftp_reporterror=1 ;
		    }
		    nocpucheck = 1;
		    smartftp_retry=3 ;
		    smartftp_start();
		  }
		  dvr_log("Enter standby mode. (mode %d).", app_mode);
		  buzzer( 3, 250, 250);
                }
            }
            else if( app_mode==APPMODE_STANDBY ) {                    // standby

                p_dio_mmap->outputmap ^= HDLED ;        // flash LED slowly for standy mode

                if( g_poweroff ) {
                    mcu_poweroffdelay (10);
                }
                
                // continue wait for smartftp
		int smartftp_ended = 1;
                if( pid_smartftp > 0 ) {
                    smartftp_ended = smartftp_wait();
                }
		if( runtime>smartftp_endtime  ) {
                    smartftp_ended = 1;
		}

		if (smartftp_ended) {                
		  nocpucheck = 0;
		}

                if( p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrstatus & DVR_NETWORK) )
                {
                    p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
                }
                else {
		  if (smartftp_ended) {
		    if (g_devicepower_allways_on == 0) {
		      // turn off all devices power
		      p_dio_mmap->devicepower=DEVICEOFF ;
		    }
		    /*
		    if ((p_dio_mmap->dvrpid == 0) &&
			(p_dio_mmap->dvrcmd == 0)) {
		      p_dio_mmap->dvrcmd = 5; // resume dvrsvr (no recording)
		    }
		    */
		  }
                }

                if( g_poweroff ) {         // ignition off
		  if (smartftp_ended) {
                    // turn off HD power ?
                    if( standbyhdoff ) {
                        p_dio_mmap->outputmap &= ~HDLED ;   // turn off HDLED
                        if( hdpower ) {
                            hdpower=0;
                            mcu_hdpower(0);
                        }
                    }
                    
                    if( runtime>modeendtime  ) {
                        // start shutdown
		        if (g_devicepower_allways_on == 0) {
			  // turn off all devices power  
			  p_dio_mmap->devicepower=DEVICEOFF ;
		        }
			// just in case poweroff delay was extended
			setPowerOffDelay(serial_handle, 10);
                        modeendtime = runtime+10000 ;
                        set_app_mode(APPMODE_SHUTDOWN) ;    // turn off mode
                        dvr_log("Standby timeout, system shutdown. (mode %d).", app_mode );
                        buzzer( 5, 250, 250 );
                        if( p_dio_mmap->dvrpid > 0 ) {
                            kill(p_dio_mmap->dvrpid, SIGTERM);
                        }
                        if( p_dio_mmap->glogpid > 0 ) {
                            kill(p_dio_mmap->glogpid, SIGTERM);
                        }
                        if( pid_smartftp > 0 ) {
                            smartftp_kill();
                        }
                    }
		  }
                }
                else {                                          // ignition on
                    dvr_log("Power on switch after standby. System reboot");
		    smartftp_kill();
		    sync();
		    // just in case...
                    watchdogtimeout=10 ;                    // 10 seconds watchdog time out
                    mcu_watchdogenable() ;
                    mcu_reboot ();
		    exit(1);
                }
            }
            else if( app_mode==APPMODE_SHUTDOWN ) {                    // turn off mode, no keep power on 
	      //sync();
                if( runtime>modeendtime ) {     // system suppose to turn off during these time
		  mcu_reboot ();
		  sleep(30);
                    // hard ware turn off failed. May be ignition turn on again? Reboot the system
                    dvr_log("Hardware shutdown failed. Try reboot by software!" );
                    set_app_mode(APPMODE_REBOOT) ;
                }
            }
            else if( app_mode==APPMODE_REBOOT ) {
                static int reboot_begin=0 ;
                //sync();
                if( reboot_begin==0 ) {
                    if( p_dio_mmap->dvrpid > 0 ) {
                        kill(p_dio_mmap->dvrpid, SIGTERM);
                    }
                    if( p_dio_mmap->glogpid > 0 ) {
                        kill(p_dio_mmap->glogpid, SIGTERM);
                    }
                    // let MCU watchdog kick in to reboot the system
                    watchdogtimeout=10 ;                    // 10 seconds watchdog time out
                    mcu_watchdogenable() ;
                    usewatchdog=0 ;                         // don't call watchdogenable again!
                    watchdogenabled=0 ;                     // don't kick watchdog ;
                    reboot_begin=1 ;                        // rebooting in process
                    modeendtime=runtime+20000 ;             // 20 seconds time out, watchdog should kick in already!
                }
                else if( runtime>modeendtime ) { 
                    // program should not go throught here,
                    mcu_reboot ();
                    system("/bin/reboot");                  // do software reboot
                    set_app_mode(APPMODE_QUIT) ;                 // quit IOPROCESS
                }
            }
            else if( app_mode==APPMODE_REINITAPP ) {
                dvr_log("IO re-initialize.");
                appfinish();
                appinit();

		sleep(1); // requested by Zhen

		if (gforce_log_enable) {
		  mcu_watchdogdisable();
		  checkContinuousData(serial_handle);
		}
		unlink("/var/dvr/gforcecheck"); // let start.sh continue
		// gsensor trigger setup
		if( mcu_gsensorinit() ) {       // g sensor available
		  dvr_log("G sensor detected!");
		  FILE *pidf = fopen( "/var/dvr/gsensor", "w" );
		  if( pidf ) {
		    fprintf(pidf, "1");
		    fclose(pidf);
		  }
		}

		initruntime();
                set_app_mode(APPMODE_RUN) ;
            }
            else {
                // no good, enter undefined mode 
                dvr_log("Error! Enter undefined mode %d.", app_mode );
                set_app_mode(APPMODE_RUN) ;              // we retry from mode 1
            }

            // updating watch dog state
            if( usewatchdog && watchdogenabled==0 ) {
                mcu_watchdogenable();
            }

            // kick watchdog
            if( watchdogenabled )
                mcu_watchdogkick () ;

            // DVRSVR watchdog running?
            if( p_dio_mmap->dvrpid>0 && 
		(p_dio_mmap->dvrstatus & DVR_RUN) &&
		p_dio_mmap->dvrwatchdog >= 0 )
	      {      	
                ++(p_dio_mmap->dvrwatchdog) ;
                if( p_dio_mmap->dvrwatchdog == 50 ) {	// DVRSVR dead for 2.5 min?
                    if( kill( p_dio_mmap->dvrpid, SIGUSR2 )!=0 ) {
                        // dvrsvr may be dead already, pid not exist.
                        p_dio_mmap->dvrwatchdog = 110 ;
                    }
                    else {
                        dvr_log( "Dvr watchdog failed, try restart DVR!!!");
                    }
                }
                else if( p_dio_mmap->dvrwatchdog > 110 ) {	
                    buzzer( 10, 250, 250 );
                    dvr_log( "Dvr watchdog failed, system reboot.");
                    set_app_mode(APPMODE_REBOOT) ;
                    p_dio_mmap->dvrwatchdog=1 ;
                }
                
                static int nodatawatchdog = 0;
                if( (p_dio_mmap->dvrstatus & DVR_RUN) && 
                   (p_dio_mmap->dvrstatus & DVR_NODATA ) &&     // some camera not data in
                   app_mode==1 )
                {
                    if( ++nodatawatchdog > 20 ) {   // 1 minute
                        buzzer( 10, 250, 250 );
                        // let system reset by watchdog
                        dvr_log( "One or more camera not working, system reset.");
                        set_app_mode(APPMODE_REBOOT) ;
                    }
                }
                else 
                {
                    nodatawatchdog = 0 ;
                }
            }
        }

        static unsigned int hd_timer ;
        if( (runtime - hd_timer)> 500 ) {    // 0.5 seconds to check hard drive
            hd_timer=runtime ;
            
            // check HD plug-in state
            if( hdlock && hdkeybounce<10 ) {          // HD lock on
                if( hdpower==0 ) {
                    // turn on HD power
		  if (!mcu_hdpower(1))
		    dprintf("hdpower on\n") ;
                }
                hdkeybounce = 0 ;
               
                if(
		   // dvrsvr is running but no disk detected
		   (hdinserted &&                      // hard drive inserted
		    !pid_tab102 && // tab102 is not uploading data
                    p_dio_mmap->dvrpid > 0 &&
                    p_dio_mmap->dvrwatchdog >= 0 &&
                    (p_dio_mmap->dvrstatus & DVR_RUN) &&
		    (p_dio_mmap->dvrstatus & DVR_DISKREADY )==0 ) ||
		   /* mount failed even before dvrsvr starts */ 
		   (hdinserted && !pid_tab102 && p_dio_mmap->dvrpid<=0 ))
		  {
                    p_dio_mmap->outputmap ^= HDLED ;
                    
                    if( ++hdpower>600 )                // 300 seconds
                    {
                        dvr_log("Hard drive failed, system reset!");
                        buzzer( 10, 250, 250 );

                        // turn off HD led
                        hdpower=0;
                        p_dio_mmap->outputmap &= ~HDLED ;

			p_dio_mmap->nodiskcheck = 0;
			sleep(5);
			sync();
			tab102_kill();
			watchdogtimeout=10 ; // 10 seconds watchdog time out
			mcu_watchdogenable() ;
			mcu_reboot ();
			exit(1);
                    }
                }
                else {
                    // turn on HD led
                    if( app_mode < APPMODE_NORECORD ) {
                        if( hdinserted ) {
                            p_dio_mmap->outputmap |= HDLED ;
                        }
                        else {
                            p_dio_mmap->outputmap &= ~HDLED ;
                        }
                    }
                    hdpower=1 ;
                }
            }
            else {                  // HD lock off
                static pid_t hd_dvrpid_save ;
                p_dio_mmap->outputmap ^= HDLED ;                    // flash led
		//dvr_log("Debug:hdkeybounce:%d.",hdkeybounce);
                if( hdkeybounce<10 ) {                          // wait for key lock stable
                    hdkeybounce++ ;
                    hd_dvrpid_save=0 ;
                }
                else if( hdkeybounce==10 ){
                    hdkeybounce++ ;
                    // suspend DVRSVR process
                    if( p_dio_mmap->dvrpid>0 &&
			(p_dio_mmap->dvrstatus & DVR_RUN)) {
                        hd_dvrpid_save=p_dio_mmap->dvrpid ;
                        //dvr_log("Usb key lock off.");
                        kill(hd_dvrpid_save, SIGUSR1);         // request dvrsvr to turn down
                        // turn on HD LED
                        p_dio_mmap->outputmap |= HDLED ;
                    }
                }
                else if( hdkeybounce==11 ) {
                    if( p_dio_mmap->dvrpid <= 0 )  {
                        hdkeybounce++ ;
                        //sync();
                        // umount disks
                        system("/davinci/dvr/umountdisks") ;
                    }
                }
                else if( hdkeybounce==12 ) {
                    // do turn off hd power
                    //sync();
                    mcu_hdpower(0); 
                    hdkeybounce++;
                }
                else if( hdkeybounce<20 ) {
                    hdkeybounce++;
                }
                else if( hdkeybounce==20 ) {
                    hdkeybounce++;
                    if( hd_dvrpid_save > 0 ) {
                        kill(hd_dvrpid_save, SIGUSR2);              // resume dvrsvr
                        hd_dvrpid_save=0 ;
                    }
                    // turn off HD LED
                    p_dio_mmap->outputmap &= ~ HDLED ;

                    // if the board survive (force on cable), we have a new IP address
                    system("ifconfig eth0:247 2.4.7.247") ;
                    
                }
                else {
                    if( hdlock ) {
                        hdkeybounce=0 ;
                    }
                }
            }               // End HD key off
        }

        static unsigned int panel_timer ;
        if( (runtime - panel_timer)> 2000 ) {    // 2 seconds to update pannel
            panel_timer=runtime ;
            unsigned int panled = p_dio_mmap->panel_led ;
            if( p_dio_mmap->dvrpid> 0 && (p_dio_mmap->dvrstatus & DVR_RUN) ) {
                static int videolostbell=0 ;
                if( (p_dio_mmap->dvrstatus & DVR_VIDEOLOST)!=0 ) {
                    panled|=4 ;
                    if( videolostbell==0 ) {
                        buzzer( 5, 500, 500 );
                        videolostbell=1;
                    }
                }
                else {
                    videolostbell=0 ;
                }
            }
            
            if( p_dio_mmap->dvrpid>0 && (p_dio_mmap->dvrstatus & DVR_NODATA)!=0 && app_mode==APPMODE_RUN ) {
                panled|=2 ;     // error led
            }
            
            // update panel LEDs
            if( panelled != panled ) {
                for( i=0; i<PANELLEDNUM; i++ ) {
                    if( (panelled ^ panled) & (1<<i) ) {
                        mcu_led(i, panled & (1<<i) );
                    }
                }
                panelled = panled ;
            }
            
            // update device power
            if( devicepower != p_dio_mmap->devicepower ) {
                for( i=0; i<DEVICEPOWERNUM; i++ ) {
                    if( (devicepower ^ p_dio_mmap->devicepower) & (1<<i) ) {
                        int xx=mcu_devicepower(i, p_dio_mmap->devicepower & (1<<i) );
			dprintf("device:%d\n",xx);
                    }
                }
                devicepower = p_dio_mmap->devicepower ;
                
                // reset network interface, some how network interface dead after device power changes
                //setnetwork();
            }
            
        }
        
        // adjust system time with RTC
	struct gps g;

	get_gps_data(&g);
        if( gpsvalid != g.gps_valid ) {
            gpsvalid = g.gps_valid ;
            if( gpsvalid ) {
	      if (time_syncgps ()) {
		/* sync not complete, should call time_syncgps() again */
		gpsvalid = 0;
	      } else {
                g_adjtime_timer=runtime ;
	      }
	      gpsavailable = 1 ;
            }
        }
        if( g_adjtime_timer &&
	    ((runtime - g_adjtime_timer)> 600000 ||
	     (runtime < g_adjtime_timer)) )
        {    // call adjust time every 10 minute
            if( g_syncrtc ) {
                if( gpsavailable ) {
                    if( gpsvalid ) {
		      if (!time_syncgps()) {
                        g_adjtime_timer=runtime ;
		      }
                    }
                }
		/*
                else {
                    time_syncmcu();
                    g_adjtime_timer=runtime ;
                }
		*/
            }
        }
    }
    
    if( watchdogenabled ) {
        mcu_watchdogdisable();
    }
    
    appfinish();
    
    printf( "DVR IO process ended!\n");
    return 0;
}

