/* --- Changes ---
 *
 * 08/11/2009 by Harrison
 *   1. System reset on <standby --> ignition on> 
 *   2. "smartftp" can finish it job even after standby time expires
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
#include <time.h>
#include <sched.h>
 #include <pthread.h>

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

// options
int standbyhdoff = 0 ;
int usewatchdog = 0 ;
int tab102b_enable;
int buzzer_enable;
int motion_control;

// Do system reset on <standby --> ignition on> 
#define RESET_ON_IGNON
// change this to 0xffff to temporarily disable device power off
#define DEVICEOFF 0

// HARD DRIVE LED and STATUS
#define HDLED	(0x10)

// input pin maping
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)
#define RECVBUFSIZE (50)

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

// translate output bits
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

int hdlock=0 ;	// HD lock status
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

#define SERIAL_DELAY	(1000000) //(100000)
#define DEFSERIALBAUD	(115200)

char dvrcurdisk[128] = "/var/dvr/dvrcurdisk" ;
char dvriomap[256] = "/var/dvr/dvriomap" ;
char serial_dev[256] = "/dev/ttyS1" ;
char * pidfile = "/var/dvr/ioprocess.pid" ;
int serial_handle = 0 ;
int serial_baud = 115200 ;
int mcupowerdelaytime = 0 ;
char mcu_firmware_version[80] ;

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;
char logfile[128] = "dvrlog.txt" ;
char temp_logfile[128] ;
int watchdogenabled=0 ;
int watchdogtimeout=30 ;
int gpsvalid = 0 ;
int gforce_log_enable=0 ;
int output_inverted=0 ;


int app_mode = APPMODE_QUIT ;

unsigned int panelled=0 ;
unsigned int devicepower=0xffff;

pid_t   pid_smartftp = 0 ;
pid_t   pid_tab102 = 0 ;


#define WRITE_INPUT 0

void set_app_mode(int mode)
{
  app_mode=mode ;
  p_dio_mmap->current_mode =  mode;
}

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
    char hostname[128] ;
    FILE *flog;
    time_t ti;
    struct tm t ;

    int l;
    va_list ap ;

    flog = fopen(dvrcurdisk, "r");
    if( flog ) {
        if( fscanf(flog, "%s", lbuf )==1 ) {
            gethostname(hostname, 128);
            sprintf(logfilename, "%s/_%s_/%s", lbuf, hostname, logfile);
        }
        fclose(flog);
    } else{
      return 0;
    }

    ti = time(NULL);
    localtime_r(&ti, &t);
    // no record before 2005
    if( t.tm_year < 105 || t.tm_year>150 ) {
        return 0 ;
    }  
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

// atomically swap value
int atomic_swap( int *m, int v)
{
    register int result ;
/*
    result=*m ;
    *m = v ;
*/
    asm volatile (
        "swp  %0, %2, [%1]\n\t"
        : "=r" (result)
        : "r" (m), "r" (v)
    ) ;
    return result ;
}

/*
void dio_lock()
{
    int i;
    for( i=0; i<1000; i++ ) {
        if( p_dio_mmap->lock>0 ) {
            usleep(1);
        }
        else {
            break ;
        }
    }
    p_dio_mmap->lock=1;
}
*/

void dio_lock()
{
    while( atomic_swap( &(p_dio_mmap->lock), 1 ) ) {
        sched_yield();      // or sleep(0)
    }
}

void dio_unlock()
{
    p_dio_mmap->lock=0;
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
   // printf("check ready\n");
    if (select(serial_handle + 1, &fds, NULL, NULL, &timeout) > 0) {
     //   printf("ready\n");
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
int config_serial(int fd){
    struct termios newtio,oldtio;
    if(tcgetattr(fd,&oldtio)!=0){
       printf("tcgetattr failed:%d\n",errno);
       return -1;
    }
    bzero(&newtio,sizeof(newtio));
    newtio.c_cflag|=CLOCAL|CREAD;
    newtio.c_cflag&=~CSIZE;
    //set stop bits
    newtio.c_cflag|=CS8;
    //set no parity
    newtio.c_cflag&=~PARENB;
 
    //set baud rate 115200
    if(cfsetispeed(&newtio,B115200)<0){
       printf("cfsetspeed failed:%d\n",errno);
       return -1;
    }

    if(cfsetospeed(&newtio,B115200)<0){
       printf("cfsetospeed failed:%d\n",errno);
       return -1;
    }
    //set stop bits 1
    newtio.c_cflag&=~CSTOPB;
    //set waiting time
    newtio.c_cc[VTIME]=50;
    newtio.c_cc[VMIN]=1;
    tcflush(fd,TCIFLUSH);
    //activate the new setting
    if(tcsetattr(fd,TCSANOW,&newtio)<0){
      printf("tcsetattr failed:%d\n",errno);
      return -1;
    }
    return 0;
}
// initialize serial port
void serial_init() 
{
    int port=0 ;
    printf("serial init\n");
    if( serial_handle > 0 ) {
        close( serial_handle );
        serial_handle = 0 ;
    }
    if( strcmp( serial_dev, "/dev/ttyS1")==0 ) {
        port=1 ;
    }
    serial_handle = open( serial_dev, O_RDWR | O_NOCTTY | O_NDELAY );
    if( serial_handle > 0 ) {
        fcntl(serial_handle, F_SETFL, 0);
        if(config_serial(serial_handle)<0){
           printf("configure serial port1 failed\n");
           return;
        }
          printf("serial initialed\n");
    }
    else {
        // even no serail port, we still let process run
        printf("Serial port failed!\n");
    }
}

#define MCU_INPUTNUM (6)
#define MCU_OUTPUTNUM (4)

unsigned int mcu_doutputmap ;

// check data check sum
// return 0: checksum correct
int mcu_checksum( char * data ) 
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
void mcu_calchecksum( char * data )
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
    mcu_calchecksum( cmd );

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

int mcu_recv( char * recv, int size, int usdelay=SERIAL_DELAY );
int mcu_checkinputbuf(char * ibuf);

static int mcu_recverror = 0 ;

// receive one data package from mcu
// return 0: failed
//       >0: bytes of data received
int mcu_recvmsg( char * recv, int size )
{
    int n;
    int bytes=0;
    int total=0;
    int left=0;
    int times=0;
#ifdef MCU_DEBUG
    // for debugging 
    struct timeval tv ;
    gettimeofday(&tv, NULL);
    struct tm * ptm ;
    ptm = localtime(&tv.tv_sec);
    double sec ;
    sec = ptm->tm_sec + tv.tv_usec/1000000.0 ;
    
    printf("%02d:%02d:%02.3f Recv: ", ptm->tm_hour, ptm->tm_min, sec);
#endif    

    if( serial_read(recv, 1) ) {
        n=(int) *recv ;
     //   printf("size=%d n=%d\n",size,n);
        if( n>=5 && n<=size ) {
#if 1
            total=1;
            left=n-1;
            while(times<10){
                if(serial_dataready(100000)){
			bytes=serial_read(recv+total,left);
			total+=bytes;
			left=left-bytes;
			if(left==0)
			break;
                }
                times++;
            }
#else
            n=serial_read(recv+1,n-1)+1;
#endif
            
            if( n==recv[0] && 
                recv[1]==0 && 
                recv[2]==1 &&
                mcu_checksum( recv ) == 0 ) 
            {
#ifdef MCU_DEBUG
                printf("\n");
#endif
                return n ;
            }
        }
    }

#ifdef MCU_DEBUG
    // for debugging 
    printf(" - ERROR!\n");
#endif

    serial_clear();
    return 0 ;
}
// receiving a respondes message from mcu
int mcu_recv( char * recv, int size, int usdelay )
{
    int n,i;
    if( size<5 ) {	// minimum packge size?
        return 0 ;
    }
    
    while(serial_dataready(usdelay) ) {
        n = mcu_recvmsg (recv, size) ;

#if WRITE_INPUT
        for(i=0;i<n;++i){
            printf("%x ",recv[i]);
        }
        printf("\n");
#endif
        if( n>0 ) {
            mcu_recverror=0 ;
            if( recv[4]=='\x02' ) {             // is it a input message ?
                mcu_checkinputbuf(recv) ;
                continue ;
            }
            else {
                if( recv[3]=='\x1f' ) {    // it is a error report, ignor this for now
#ifdef MCU_DEBUG
                    printf(" A error report detected!\n" );
#endif
                    continue ;
                }
                return n ;                      // it could be a respondes message
            }
        }
        else {
            break;
        }
    }

    if( ++mcu_recverror>5 ) {
        sleep(1);
      //  serial_init();
        mcu_recverror=0 ;
    }

    return 0 ;
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
        
static char direction_table[24][2] = 
{
    {0, 2}, {0, 3}, {0, 4}, {0,5},      // Front -> Forward
    {1, 2}, {1, 3}, {1, 4}, {1,5},      // Back  -> Forward
    {2, 0}, {2, 1}, {2, 4}, {2,5},      // Right -> Forward
    {3, 0}, {3, 1}, {3, 4}, {3,5},      // Left  -> Forward
    {4, 0}, {4, 1}, {4, 2}, {4,3},      // Buttom -> Forward
    {5, 0}, {5, 1}, {5, 2}, {5,3},      // Top   -> Forward
};

#define DEFAULT_DIRECTION   (7)
static int gsensor_direction = DEFAULT_DIRECTION ;
int  g_sensor_available ;
float g_sensor_trigger_forward ;
float g_sensor_trigger_backward ;
float g_sensor_trigger_right ;
float g_sensor_trigger_left ;
float g_sensor_trigger_down ;
float g_sensor_trigger_up ;

int mcu_checkinputbuf(char * ibuf)
{
    unsigned int imap1, imap2 ;
    int i;
    float gback, gright, gbuttom ;
    float gbusforward, gbusright, gbusdown ;
    
    if( ibuf[4]=='\x02' ) {                 // mcu initiated message ?
        switch( ibuf[3] ) {
        case '\x1c' :                   // dinput event
            static char rsp_dinput[7] = "\x06\x01\x00\x1c\x03\xcc" ;
            mcu_send(rsp_dinput);
            // get digital input map
            imap1 = (unsigned char)ibuf[5]+256*(unsigned int)(unsigned char)ibuf[6] ;
            imap2 = 0 ;
            for( i=0; i<MAXINPUT; i++ ) {
                if( imap1 & input_map_table[i] )
                    imap2 |= (1<<i) ;
            }
            // hdlock = (imap1 & (HDINSERTED|HDKEYLOCK) )==0 ;	// HD plugged in and locked
            hdlock = (imap1 & (HDKEYLOCK) )==0 ;	// HD plugged in and locked
            hdinserted = (imap1 & HDINSERTED)==0 ;  // HD inserted
            p_dio_mmap->inputmap = imap2;
            break;

        case '\x08' :               // ignition off event
            static char rsp_poweroff[10] = "\x08\x01\x00\x08\x03\x00\x00\xcc" ;
            mcu_send(rsp_poweroff);
            mcupowerdelaytime = 0 ;
            p_dio_mmap->poweroff = 1 ;						// send power off message to DVR
#ifdef MCU_DEBUG
            printf("Ignition off, mcu delay %d s\n", mcupowerdelaytime );
#endif            
            break;
            
        case '\x09' :	// ignition on event
            static char rsp_poweron[10] = "\x07\x01\x00\x09\x03\x00\xcc" ;
            rsp_poweron[5]=(char)watchdogenabled;
            mcu_send( rsp_poweron );
            p_dio_mmap->poweroff = 0 ;						// send power on message to DVR
#ifdef MCU_DEBUG
            printf("ignition on\n");
#endif            
            break ;

        case '\x40' :               // Accelerometer data
            static char rsp_accel[10] = "\x06\x01\x00\x40\x03\xcc" ;
            mcu_send(rsp_accel);
            
            gright  = ((float)(signed char)ibuf[5])/0xe ;
            gback   = ((float)(signed char)ibuf[6])/0xe ;
            gbuttom = ((float)(signed char)ibuf[7])/0xe ;

#ifdef MCU_DEBUG
            printf("Accelerometer, --------- right=%.2f , back   =%.2f , buttom=%.2f .\n",     
                   gright,
                   gback,
                   gbuttom );
#endif            
            
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

#ifdef MCU_DEBUG
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
                if( gforce_log_enable ) {
                    p_dio_mmap->gforce_log0 = 1 ;
                }
            }
            else {
                p_dio_mmap->gforce_right_1 = gbusright ;
                p_dio_mmap->gforce_forward_1 = gbusforward ;
                p_dio_mmap->gforce_down_1 = gbusdown ;
                if( gforce_log_enable ) {
                    p_dio_mmap->gforce_log1 = 1 ;
                }
            }                

            break;
            
        default :
#ifdef MCU_DEBUG
            printf("Other message from MCU.\n");
#endif
            break;
        }
        return 1 ;
    }
    return 0 ;
}

// check mcu input
// parameter
//      usdelay - micro-seconds waiting
// return 
//		1: got something, digital input or power off input?
//		0: nothing
int mcu_input(int usdelay)
{
    int res = 0 ;
    int n ;
    int repeat ;
    char ibuf[RECVBUFSIZE] ;
    int udelay = usdelay ;
    
    for(repeat=0; repeat<10; repeat++ ) {
        if( serial_dataready(udelay) ) {
            n = mcu_recvmsg( ibuf, sizeof(ibuf) ) ;     // mcu_recv() will process mcu input messages
            if( n>0 ) {
                udelay=10 ;
                if( ibuf[3] != '\x1f' ) {       // ignor error reports for now
                    res=mcu_checkinputbuf(ibuf);
                }
#ifdef MCU_DEBUG
                else {
                    printf( "Error Report Detected!\n");
                }
#endif                
            }
        }
        else {
            break;
        }
    }
    return res;

}

int mcu_bootupready(int* D1)
{
    static char cmd_bootupready[8]="\x07\x01\x00\x11\x02\x00\xcc" ;
    char responds[RECVBUFSIZE] ;
    int retry=10;
    int i=0;
    int size;
    while( --retry>0 ) {
        serial_clear() ;
        if(mcu_send(cmd_bootupready)) {
            if( mcu_recv( responds, RECVBUFSIZE ) ) {
                size=responds[0];
                for(i=0;i<size;++i)
                   printf("%x ",responds[i]);
                printf("\n");
                if( responds[3]=='\x11' &&
                   responds[4]=='\x03' ) {
                      if(responds[6]==0x80)
                         *D1=1;
                       else
                         *D1=0;
                       return 1 ;
                   }
            }
        }
        sleep(1);
    }
    return 0 ;
}

int code ;

int mcu_readcode()
{
    int i=0;
    static char cmd_readcode[8]="\x06\x01\x00\x41\x02\xcc" ;
    unsigned char responds[RECVBUFSIZE] ;
    if(mcu_send(cmd_readcode)) {
        if( mcu_recv( (char *)responds, RECVBUFSIZE ) ) {
            if( responds[3]=='\x41' &&
               responds[4]=='\x03' && 
               responds[0]>8 ) 
            {
                printf("mcu read code\n");
                for(i=0;i<responds[0];++i){
                    printf("%x ",responds[i]);
                }
                printf("\n");
                if( responds[5]!=(unsigned char)0xff ) {
                    code = responds[5] ;      
                    struct tm ttm ;
                    time_t tt ;
                    memset( &ttm, 0, sizeof(ttm) );
                    ttm.tm_year = getbcd(responds[12]) + 100 ;
                    ttm.tm_mon  = getbcd(responds[11]) - 1;
                    ttm.tm_mday = getbcd(responds[10]) ;
                    ttm.tm_hour = getbcd(responds[8]) ;
                    ttm.tm_min  = getbcd(responds[7]) ;
                    ttm.tm_sec  = getbcd(responds[6]) ;
                    ttm.tm_isdst= -1 ;
                    tt=timegm(&ttm);
                    if( (int)tt>0 ) {
                        localtime_r(&tt, &ttm);
                        dvr_log( "MCU shutdown code: %02x at %04d-%02d-%02d %02d:%02d:%02d",
                                code,
                                ttm.tm_year+1900,
                                ttm.tm_mon+1,
                                ttm.tm_mday,
                                ttm.tm_hour,
                                ttm.tm_min,
                                ttm.tm_sec );
                    }
                }
                return 1 ;
            }
        }
    }
    dvr_log( "Read MCU shutdown code failed.");
    return 0 ;
}

int mcu_reboot()
{
    static char cmd_reboot[8]="\x06\x01\x00\x12\x02\xcc" ;
    mcu_send(cmd_reboot) ;
    // no responds
    return 0 ;
}

// return gsensor available
int mcu_gsensorinit()
{
    static char cmd_gsensorinit[16]="\x0c\x01\x00\x34\x02\x01\x02\x03\x04\x05\x06\xcc" ;
    char responds[RECVBUFSIZE] ;
    int retry=10;
    float trigger_back, trigger_front ;
    float trigger_right, trigger_left ;
    float trigger_bottom, trigger_top ;
    
    if( !gforce_log_enable ) {
        return 0 ;
    }
           
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
        cmd_gsensorinit[5]  = (signed char)(trigger_right*0xe) ;    // Right Trigger
        cmd_gsensorinit[6]  = (signed char)(trigger_left*0xe) ;     // Left Trigger
    }
    else {
        cmd_gsensorinit[5]  = (signed char)(trigger_left*0xe) ;     // Right Trigger
        cmd_gsensorinit[6]  = (signed char)(trigger_right*0xe) ;    // Left Trigger
    }
    
    if( trigger_back >= trigger_front ) {
        cmd_gsensorinit[7]  = (signed char)(trigger_back*0xe) ;    // Back Trigger
        cmd_gsensorinit[8]  = (signed char)(trigger_front*0xe) ;    // Front Trigger
    }
    else {
        cmd_gsensorinit[7]  = (signed char)(trigger_front*0xe) ;    // Back Trigger
        cmd_gsensorinit[8]  = (signed char)(trigger_back*0xe) ;    // Front Trigger
    }
    
    if( trigger_bottom >= trigger_top ) {
        cmd_gsensorinit[9]  = (signed char)(trigger_bottom*0xe) ;    // Bottom Trigger
        cmd_gsensorinit[10] = (signed char)(trigger_top*0xe) ;    // Top Trigger
    }
    else {
        cmd_gsensorinit[9]  = (signed char)(trigger_top*0xe) ;    // Bottom Trigger
        cmd_gsensorinit[10] = (signed char)(trigger_bottom*0xe) ;    // Top Trigger
    }
    
    g_sensor_available = 0 ;
    while( --retry>0 ) {
        serial_clear() ;
        if(mcu_send(cmd_gsensorinit)) {
            if( mcu_recv( responds, RECVBUFSIZE ) ) {
                if( responds[3]=='\x34' &&
                   responds[4]=='\x03'&&
                   responds[0]>6 ) 
                {
                    g_sensor_available = responds[5] ;
                    return g_sensor_available ;
                }
            }
        }
        sleep(1);
    }
    return 0 ;
}

// get MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_version(char * version)
{
    static char cmd_firmwareversion[8]="\x06\x01\x00\x2d\x02\xcc" ;
    char responds[RECVBUFSIZE] ;
    int  sz ;
    while(1){
        if(mcu_send(cmd_firmwareversion)) {
                printf("version request sent\n");
               // sz= serial_read(responds, sizeof(responds));
		sz = mcu_recv( responds, sizeof(responds));
		if( sz > 0 ) {
			printf("version\n");
			if( responds[3]=='\x2d' &&
			responds[4]=='\x03' ) 
			{
			memcpy( version, &responds[5], sz-6 );
			version[sz-6]=0 ;
			return sz-6 ;
			}
		}
        }
    }
    return 0 ;
}

int mcu_doutput()
{
    static char cmd_doutput[8] ="\x07\x01\x00\x31\x02\x00\xcc" ;
    char responds[RECVBUFSIZE] ;
    unsigned int outputmap = p_dio_mmap->outputmap ;
    unsigned int outputmapx ;
    int i ;

    if (!buzzer_enable) {
        outputmap &= (~0x100) ;     // buzzer off
    }

    if( outputmap == mcu_doutputmap ) return 1 ;

    outputmapx = (outputmap^output_inverted) ;

    // bit 2 is beep. (from July 6, 2009)
    // translate output bits ;
    cmd_doutput[5] = 0 ;
    for(i=0; i<MAXOUTPUT; i++) {
        if( outputmapx & output_map_table[i] ) {
            cmd_doutput[5]|=(1<<i) ;
        }
    }
    
    // send output command
    if( mcu_send(cmd_doutput)) {
        if( mcu_recv( responds, RECVBUFSIZE ) ) {
            if( responds[3]=='\x31' &&
               responds[4]=='\x3' ) 
            {
                mcu_doutputmap=outputmap ;
            }
        }
    }
    return 1;
}

// get mcu digital input
int mcu_dinput()
{
    static char cmd_dinput[8]="\x06\x01\x00\x1d\x02\xcc" ;
    char responds[RECVBUFSIZE] ;
    unsigned int imap1, imap2 ;
    int i;

    mcu_input(10000);
    if(mcu_send(cmd_dinput)) {
        if( mcu_recv( responds, RECVBUFSIZE ) ) {
            if( responds[3]=='\x1d' &&
               responds[4]=='\x03' ) {
                   // get digital input map
                   imap1 = (unsigned char)responds[5]+256*(unsigned int)(unsigned char)responds[6] ; ;
                   imap2 = 0 ;
                   for( i=0; i<MAXINPUT; i++ ) {
                       if( imap1 & input_map_table[i] )
                           imap2 |= (1<<i) ;
                   }
                   p_dio_mmap->inputmap = imap2;
                   // hdlock = (imap1 & (HDINSERTED|HDKEYLOCK) )==0 ;	// HD plugged in and locked
                   hdlock = (imap1 & (HDKEYLOCK) )==0 ;	                // HD plugged in and locked
                   hdinserted = (imap1 & HDINSERTED)==0 ;  // HD inserted
                   return 1 ;
               }
        }
    }
    return 0 ;
}

static int delay_inc = 10 ;

// set more mcu power off delay (keep power alive), (called every few seconds)
void mcu_poweroffdelay()
{
    static char cmd_poweroffdelaytime[10]="\x08\x01\x00\x08\x02\x00\x00\xcc" ;
    char responds[RECVBUFSIZE] ;
    if( mcupowerdelaytime < 50 ) {
        cmd_poweroffdelaytime[6]=delay_inc;
    }
    else {
        cmd_poweroffdelaytime[6]=0 ;
    }
    if( mcu_send(cmd_poweroffdelaytime) ) {
        if( mcu_recv( responds, RECVBUFSIZE ) ) {
            if( responds[3]=='\x08' &&
                responds[4]=='\x03' ) {
                   mcupowerdelaytime = ((unsigned)(responds[5]))*256+((unsigned)responds[6]) ;
#ifdef MCU_DEBUG        
                   printf("mcu delay %d s\n", mcupowerdelaytime );
#endif                    
               }
        }
    }
}

void mcu_watchdogenable()
{
    int retry ;
    char responds[RECVBUFSIZE] ;
    static char cmd_watchdogtimeout[10]="\x08\x01\x00\x19\x02\x00\x00\xcc" ;
    static char cmd_watchdogenable[10]="\x06\x01\x00\x1a\x02\xcc" ;

    cmd_watchdogtimeout[5] = watchdogtimeout/256 ;
    cmd_watchdogtimeout[6] = watchdogtimeout%256 ;
    
    for( retry=0; retry<5; retry++) {
        if( mcu_send(cmd_watchdogtimeout) ) {
            // ignor the responds, if any.
            if( mcu_recv( responds,RECVBUFSIZE )>0 ) {
                break;
            }
        }
    }
   
    for( retry=0; retry<5; retry++) {
        if( mcu_send(cmd_watchdogenable) ) {
            // ignor the responds, if any.
            if( mcu_recv( responds, RECVBUFSIZE )>0 ) {
                break;
            }
        }
    }
    
    watchdogenabled = 1 ;
}

void mcu_watchdogdisable()
{
    int retry ;
    char responds[RECVBUFSIZE] ;
    static char cmd_watchdogdisable[10]="\x06\x01\x00\x1b\x02\xcc" ;
    for( retry=0; retry<5; retry++) {
        if( mcu_send(cmd_watchdogdisable) ) {
            // ignor the responds, if any.
            if( mcu_recv( responds, RECVBUFSIZE )>0 ) {
                break;
            }
        }
    }
    watchdogenabled = 0 ;
}

// return 0: error, >0 success
int mcu_watchdogkick()
{
    int retry ;
    char responds[RECVBUFSIZE] ;
    static char cmd_kickwatchdog[10]="\x06\x01\x00\x18\x02\xcc" ;
    for( retry=0; retry<10; retry++ ) {
        if( mcu_send(cmd_kickwatchdog) ) {
            if( mcu_recv( responds, RECVBUFSIZE ) > 0 ) {
                if( responds[3]==0x18 && responds[4]==0x03 ) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int mcu_cdctest()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_cdctest[8]="\x06\x05\x00\x04\x02\xcc";
    if( mcu_send(cmd_cdctest) ) {
       if( mcu_recv( responds, RECVBUFSIZE ) > 0 ){
           if( responds[3]==0x04 && responds[4]==0x03 ) {
               return 1;
           }
       } 
    }
    return 0;
}

int mcu_flashtest()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_flashtest[8]="\x06\x01\x00\x36\x02\xcc";
    if( mcu_send(cmd_flashtest) ) {
        if( mcu_recv( responds, RECVBUFSIZE ) > 0 ) {
            if( responds[3]==0x36 && 
               responds[4]==3 ) 
            {
                return (int)(signed char)responds[5] ;
            }
        }
    }
    return 0;
}
int mcu_gforcetest(int* xx,int* yy,int* zz)
{
    char responds[RECVBUFSIZE] ;
    static char cmd_gforcetest[8]="\x06\x01\x00\x37\x02\xcc";
    if( mcu_send(cmd_gforcetest) ) {
        if( mcu_recv( responds, RECVBUFSIZE ) > 0 ) {
           // dvr_log("gforce:%x %x %x %x %x %x %x %x %x", \
           //         responds[0],responds[1],responds[2],\
           //         responds[3],responds[4],responds[5],responds[6],responds[7]);
            if( responds[0]==0x09&&responds[3]==0x37 && 
               responds[4]==3) 
            {
               // dvr_log("gforce received");
                if(responds[5]&0x80){
                  *xx=-(0xff-responds[5]+1);
                }else{
                   *xx=responds[5];
                }
                if(responds[6]&0x80){
                  *yy=-(0xff-responds[6]+1);
                }else{
                   *yy=responds[6];
                }
                if(responds[7]&0x80){
                  *zz=-(0xff-responds[7]+1);
                }else{
                   *zz=responds[7];
                }
                // dvr_log("xx=%d yy=%d zz=%d",*xx,*yy,*zz);
                return 0;
            }
        }
    }
    return -1;
}

// get io board temperature
int mcu_iotemperature()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_iotemperature[8]="\x06\x01\x00\x0b\x02\xcc" ;
    if( mcu_send(cmd_iotemperature) ) {
        if( mcu_recv( responds, RECVBUFSIZE ) > 0 ) {
            if( responds[3]==0x0b && 
               responds[4]==3 ) 
            {
                return (int)(signed char)responds[5] ;
            }
        }
    }
    return -128;
}

// get hd temperature
int mcu_hdtemperature(int *hd1,int *hd2)
{
    char responds[RECVBUFSIZE] ;
    static char cmd_hdtemperature[8]="\x06\x01\x00\x0c\x02\xcc" ;
    if( mcu_send(cmd_hdtemperature) ) {
        if( mcu_recv( responds,RECVBUFSIZE ) > 0 ) {
            if( responds[3]==0x0c && 
               responds[4]==3 ) 
            {
                *hd1=(int)(signed char)responds[5] ;
                *hd2=(int)(signed char)responds[7] ;
                return 0 ;
            }
        }
    }
    return -1;
}

void mcu_wifipoweron()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_wifipoweron[10]="\x07\x01\x00\x38\x02\x01\xcc" ;
    if( mcu_send(cmd_wifipoweron) ) {
        // ignor the responds, if any.
        mcu_recv( responds, RECVBUFSIZE );
    }
}

void mcu_wifipoweroff()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_wifipoweroff[10]="\x07\x01\x00\x38\x02\x00\xcc" ;
    if( mcu_send(cmd_wifipoweroff) ) {
        // ignor the responds, if any.
        mcu_recv( responds, RECVBUFSIZE );
    }
}

void mcu_motioncontrol_enable()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_motioncontrol[10]="\x07\x01\x00\x13\x02\x01\xcc" ;
    if( mcu_send(cmd_motioncontrol) ) {
        // ignor the responds, if any.
        mcu_recv( responds, RECVBUFSIZE );
    }
}

void mcu_motioncontrol_disable()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_motioncontrol[10]="\x07\x01\x00\x13\x02\x00\xcc" ;
    if( mcu_send(cmd_motioncontrol) ) {
        // ignor the responds, if any.
        mcu_recv( responds, RECVBUFSIZE );
    }
}

void mcu_hdpoweron()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_hdpoweron[10]="\x06\x01\x00\x28\x02\xcc" ;
    if( mcu_send(cmd_hdpoweron) ) {
        // ignor the responds, if any.
        mcu_recv( responds, RECVBUFSIZE );
    }
}

void mcu_hdpoweroff()
{
    char responds[RECVBUFSIZE] ;
    static char cmd_hdpoweroff[10]="\x06\x01\x00\x29\x02\xcc" ;
    if( mcu_send(cmd_hdpoweroff) ) {
        // ignor the responds, if any.
        mcu_recv( responds, RECVBUFSIZE );
    }
}

// return time_t: success
//             -1: failed
time_t mcu_r_rtc( struct tm * ptm = NULL )
{
    int retry ;
    struct tm ttm ;
    time_t tt ;
    int i=0;
    static char cmd_readrtc[8] = "\x06\x01\x00\x06\x02\xcc" ;
    unsigned char responds[RECVBUFSIZE] ;
    for( retry=0; retry<5; retry++ ) {
        if( mcu_send( cmd_readrtc ) ) {
            if( mcu_recv( (char *)responds, RECVBUFSIZE ) ) {
                if( responds[3]==6 &&
                   responds[4]==3 ) 
                {
                    for(i=0;i<responds[0];++i){
                        printf("%x ",responds[i]);
                    }
                    printf("\n");
                    memset( &ttm, 0, sizeof(ttm) );
                    ttm.tm_year = getbcd(responds[11]) + 100 ;
                    ttm.tm_mon  = getbcd(responds[10]) - 1;
                    ttm.tm_mday = getbcd(responds[9]) ;
                    ttm.tm_hour = getbcd(responds[7]) ;
                    ttm.tm_min  = getbcd(responds[6]) ;
                    ttm.tm_sec  = getbcd(responds[5]) ;
                    ttm.tm_isdst= -1 ;
                    
                    if( ttm.tm_year>105 && ttm.tm_year<135 &&
                       ttm.tm_mon>=0 && ttm.tm_mon<=11 &&
                       ttm.tm_mday>=1 && ttm.tm_mday<=31 &&
                       ttm.tm_hour>=0 && ttm.tm_hour<=24 &&
                       ttm.tm_min>=0 && ttm.tm_min<=60 &&
                       ttm.tm_sec>=0 && ttm.tm_sec<=60 ) 
                    {
                        tt = timegm( &ttm );
                        if( (int)tt > 0 ) {
                            if(ptm) {
                                *ptm = ttm ;
                            }
                            return tt ;
                        }
                    }
                }
            }
        }
    }
    dvr_log("Read clock from MCU failed!");
    return (time_t)-1;
}

void mcu_readrtc()
{
    time_t t ;
    struct tm ttm ;
    t = mcu_r_rtc(&ttm) ;
    if( (int)t > 0 ) {
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
    p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.
}

// 3 LEDs On Panel
//   parameters:
//      led:  0= USB flash LED, 1= Error LED, 2 = Video Lost LED
//      flash: 0=off, 1=flash
void mcu_led(int led, int flash)
{
    static char cmd_ledctrl[10] = "\x08\x01\x00\x2f\x02\x00\x00\xcc" ;
    char responds[RECVBUFSIZE] ;
    cmd_ledctrl[5]=(char)led ;
    cmd_ledctrl[6]=(char)(flash!=0) ;
    mcu_send( cmd_ledctrl );
    mcu_recv(responds, RECVBUFSIZE);        // ignor responds
}

// Device Power
//   parameters:
//      device:  0= GPS, 1= Slave Eagle boards, 2=Network switch
//      poweron: 0=poweroff, 1=poweron
void mcu_devicepower(int device, int poweron )
{
    static char cmd_devicepower[10] = "\x08\x01\x00\x2e\x02\x00\x00\xcc" ;
    char responds[RECVBUFSIZE] ;
    cmd_devicepower[5]=(char)device ;
    cmd_devicepower[6]=(char)(poweron!=0) ;
    mcu_send( cmd_devicepower );
    mcu_recv(responds, RECVBUFSIZE);        // ignor responds
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

#if 0
void writeDebug(char *str)
{
  FILE *fp;

  fp = fopen("/var/dvr/iodebug", "a");
  if (fp != NULL) {
    fprintf(fp, "%s\n", str);
    fclose(fp);
  }
}
#endif

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
    printf("Done.\n");
    
    // clean out serial buffer
    memset( responds, 0, sizeof(responds)) ;
    serial_write( responds, sizeof(responds));
    serial_clear(1000000);
    
    
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
        printf("Done.\n");
    }
    else {
        printf("Failed.\n");
    }
    return res ;
}

static char bcd(int v)
{
    return (char)(((v/10)%10)*0x10 + v%10) ;
}

// return 1: success
//        0: failed
int mcu_w_rtc(time_t tt)
{
    static char cmd_setrtc[16]="\x0d\x01\x00\x07\x02" ;
    char responds[20] ;
    struct tm t ;
    gmtime_r( &tt, &t);
    cmd_setrtc[5] = bcd( t.tm_sec ) ;
    cmd_setrtc[6] = bcd( t.tm_min ) ;
    cmd_setrtc[7] = bcd( t.tm_hour );
    cmd_setrtc[8] = bcd( t.tm_wday ) ;
    cmd_setrtc[9] = bcd(  t.tm_mday );
    cmd_setrtc[10] = bcd( t.tm_mon + 1 );
    cmd_setrtc[11] = bcd( t.tm_year ) ;
    
    if( mcu_send(cmd_setrtc) ) {
        if( mcu_recv(responds, 20 ) ) {
            if( responds[3]==7 &&
               responds[4]==3 ) 
            {
                return 1;
            }
        }
    }
    return 0 ;
}

void mcu_setrtc()
{
    static char cmd_setrtc[16]="\x0d\x01\x00\x07\x02" ;
    char responds[20] ;
    cmd_setrtc[5] = bcd( p_dio_mmap->rtc_second ) ;
    cmd_setrtc[6] = bcd( p_dio_mmap->rtc_minute );
    cmd_setrtc[7] = bcd( p_dio_mmap->rtc_hour );
    cmd_setrtc[8] = 0 ;						// day of week ?
    cmd_setrtc[9] = bcd( p_dio_mmap->rtc_day );
    cmd_setrtc[10] = bcd( p_dio_mmap->rtc_month) ;
    cmd_setrtc[11] = bcd( p_dio_mmap->rtc_year) ;
    
    if( mcu_send(cmd_setrtc) ) {
        if( mcu_recv(responds, 20 ) ) {
            if( responds[3]==7 &&
               responds[4]==3 ) {
                   p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
                   return ;
               }
        }
    }
    p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.
}

// sync system time to rtc
void mcu_syncrtc()
{
    time_t t ;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    t = (time_t) current_time.tv_sec ;
    if( mcu_w_rtc(t) ) {
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
    }
    else {
        p_dio_mmap->rtc_cmd   = -1 ;	// cmd error
    }
}

void mcu_rtctosys()
{
    struct timeval tv ;
    int retry ;
    time_t rtc ;
    for( retry=0; retry<10; retry++ ) {
        rtc=mcu_r_rtc();
        if( (int)rtc>0 ) {
            tv.tv_sec=rtc ;
            tv.tv_usec=0;
            settimeofday(&tv, NULL);
            // also set onboard rtc
            rtc_set((time_t)tv.tv_sec);
            break;
        }
    }
}

void time_syncgps ()
{
    struct timeval tv ;
    int diff ;
    time_t gpstime ;
    if( p_dio_mmap->gps_valid ) {
        gpstime = (time_t) p_dio_mmap->gps_gpstime ;
        gettimeofday(&tv, NULL);
        diff = (int)gpstime - (int)tv.tv_sec ;
        if( diff>5 || diff<-5 ) {
            tv.tv_sec=gpstime ;
            tv.tv_usec= 0;
            settimeofday( &tv, NULL );
            dvr_log ( "Time sync use GPS time." );
        }
        else if( diff>1 || diff<-1 ) {
            tv.tv_sec =(time_t)diff ;
            tv.tv_usec = 0 ;
            adjtime( &tv, NULL );
        }
        // set mcu time
        mcu_w_rtc(gpstime) ;
        // also set onboard rtc
        rtc_set(gpstime);
    }
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
        if( diff>5 || diff<-5 ) {
            tv.tv_sec=rtc ;
            tv.tv_usec=0;
            settimeofday( &tv, NULL );
            dvr_log ( "Time sync use MCU time." );
        }
        else if( diff>1 || diff<-1 ){
            tv.tv_sec =(time_t)diff ;
            tv.tv_usec = 0 ;
            adjtime( &tv, NULL );
        }
        // set onboard rtc
        rtc_set(rtc);
    }
}

void dvrsvr_down()
{
    pid_t dvrpid = p_dio_mmap->dvrpid ;
    if( dvrpid > 0 ) {
        kill( dvrpid, SIGUSR1 ) ;				// suspend DVRSVR
        while( p_dio_mmap->dvrpid ) {
            p_dio_mmap->outputmap ^= HDLED ;
            mcu_input(200000);
            mcu_doutput();
        }
        sync(); 
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
        static char snwpath[]="/dav/dvr/setnetwork" ;
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
void tab102_ready()
{
   dvr_log("make tab102 boot ready");
   int pid=fork();
   if(pid==0){
      execl( "/dav/dvr/tab102", "/dav/dvr/tab102","-rtc",
	   NULL 
	   );
      dvr_log("make tab102 boot ready failed");
   } else if(pid<0){
     dvr_log("make tab102 boot ready failed");
   }
}

void tab102_start()
{
  dvr_log( "Start Tab102 downloading.");
  pid_tab102 = fork();
  if( pid_tab102==0 ) {     // child process
    execl( "/dav/dvr/tab102.sh", "/dav/dvr/tab102.sh",
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
  int ret;
  if (!readWifiDeviceName(dev, sizeof(dev))) {
    dvr_log( "Start smart server uploading.");
    pid_smartftp = fork();
    if( pid_smartftp==0 ) {     // child process
        char hostname[128] ;
//        char mountdir[250] ;
        
        // get BUS name
        gethostname(hostname, 128) ;
        
// reset network, this would restart wifi adaptor
        system("/dav/dvr/setnetwork"); 
        dvr_log("dev:%s hostname:%s",dev,hostname);
        ret=execlp( "/dav/dvr/smartftp", "/dav/dvr/smartftp",
              dev,
              hostname,
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

        dvr_log( "Start smart server uploading failed!");

        exit(101);      // error happened.
    }
    else if( pid_smartftp<0 ) {
        pid_smartftp=0;
        dvr_log( "Start smart server failed!");
    }
//    smartftp_log( "Smartftp started." );
  }
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
    close( fd );	// fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    
    if( p_dio_mmap->usage <=0 ) {
        memset( p_dio_mmap, 0, sizeof( struct dio_mmap ) );
    }
    
    p_dio_mmap->usage++;                           // add one usage

    if( p_dio_mmap->iopid > 0 ) { 	// another ioprocess running?
        pid_t pid = p_dio_mmap->iopid ;
        // kill it
        if( kill( pid,  SIGTERM )==0 ) {
            // wait for it to quit
            waitpid( pid, NULL, 0 );
            sync();
            sleep(2);
        }
        else {
            sleep(5);
        }
    }

    // initialize shared memory
    p_dio_mmap->inputnum = dvrconfig.getvalueint( "io", "inputnum");	
    if( p_dio_mmap->inputnum<=0 || p_dio_mmap->inputnum>32 )
        p_dio_mmap->inputnum = MCU_INPUTNUM ;	
    p_dio_mmap->outputnum = dvrconfig.getvalueint( "io", "outputnum");	
    if( p_dio_mmap->outputnum<=0 || p_dio_mmap->outputnum>32 )
        p_dio_mmap->outputnum = MCU_INPUTNUM ;	

    motion_control= dvrconfig.getvalueint("io","sensor_powercontrol");

    p_dio_mmap->panel_led = 0;
    p_dio_mmap->devicepower = 0xffff;	// assume all device is power on
    
    p_dio_mmap->iopid = getpid () ;	// io process pid
    
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
   
    // initialize mcu (power processor)
     int mD1;
    if( mcu_bootupready (&mD1) ) {
        printf("MCU UP!\n");
        dvr_log("MCU ready.");
        serial_clear(100000);
        mcu_readcode();
    }
    else {
        printf("MCU failed!\n");
        dvr_log("MCU failed.");
    }

    if(mD1!=motion_control){
        if(motion_control)
    	   mcu_motioncontrol_enable();
        else
    	   mcu_motioncontrol_disable();
    }

    if( g_syncrtc ) {
        printf("start syc rtc\n");
        mcu_rtctosys();
    }
    
    tab102b_enable = dvrconfig.getvalueint( "glog", "tab102b_enable");
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
    
    // gsensor trigger setup
    if( mcu_gsensorinit() ) {       // g sensor available
        dvr_log("G sensor detected!");
        pidf = fopen( "/var/dvr/gsensor", "w" );
        if( pidf ) {
            fprintf(pidf, "1");
            fclose(pidf);
        }
    }
    
    usewatchdog = dvrconfig.getvalueint("io", "usewatchdog");
    watchdogtimeout=dvrconfig.getvalueint("io", "watchdogtimeout");
    if( watchdogtimeout<10 )
        watchdogtimeout=10 ;
    if( watchdogtimeout>200 )
        watchdogtimeout=200 ;

    pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }

    // initialize timer
    initruntime();
    p_dio_mmap->gforce_test=0;
    p_dio_mmap->mcu_flashtest=0;
    p_dio_mmap->mcu_flashok=0;
    p_dio_mmap->mcu_gforceok=0;
    p_dio_mmap->gforce_xx=0;
    p_dio_mmap->gforce_yy=0;
    p_dio_mmap->gforce_zz=0;
    p_dio_mmap->tab102_ready=0;
    p_dio_mmap->fileclosed=0;
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

void writeTab102Status(char *status)
{
  FILE *fp;
  fp = fopen("/var/dvr/tab102", "w");
  if (fp) {
    fprintf(fp, "%s\n", status);
    fclose(fp);
  }
}

#if 0
int copy_tab102_data()
{
  char path[256];
  FILE *fp;
  int ret = 1, copy = 0;

  fp = fopen(dvrcurdisk, "r");
  if( fp ) {
    if( fscanf(fp, "%s", path)==1 ) {
      strncat(path, "/smartlog", sizeof(path));
      if(mkdir(path, 0777) == -1 ) {
	if(errno == EEXIST) {
	  copy = 1; // directory exists
	}
      } else {
	copy = 1; // directory created
      }
    }
    fclose(fp);
  }

  if (copy) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
	     "mv -f /var/dvr/*.log /var/dvr/*.peak %s",
	     path);
    system(cmd);
    ret = 0;
  }

  return ret;
}

void check_tab102_data()
{
  char line[64];
  FILE *fp;   
  char *str = NULL;

  fp = fopen("/var/dvr/tab102", "r");
  if (fp) {
    str = safe_fgets(line, sizeof(line), fp);
    fclose(fp);
  }

  if (str && !strncmp(str, "data", 4)) {
    if(!copy_tab102_data()) {
      writeTab102Status("copy");
      //dvr_log("copied tab102 (mode %d).", app_mode);
    }
  }
}
#endif

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
    int xx,yy,zz;
    int enable_Tab102=0;
    int hdtemp1,hdtemp2;
    if ((argc >= 2) &&
	(!strcmp(argv[1], "-f") ||      /* force foreground */
	 !strcmp(argv[1], "-fw") ||     /* MCU upgrade */
	 !strcmp(argv[1], "-reboot"))) { /* system reset */
    } else {
#if 0
      if (daemon(0, 0) < 0) {
	perror("daemon");
	exit(1);
      }
#endif
    }
    if( appinit()==0 ) {
        return 1;
    }
    printf("init succeed\n");
    // get MCU version
    if( mcu_version( mcu_firmware_version ) ) {
       // printf("MCU: %s\n", mcu_firmware_version );
        FILE * mcuversionfile=fopen("/var/dvr/mcuversion", "w");
        if( mcuversionfile ) {
            fprintf( mcuversionfile, "%s", mcu_firmware_version );
            fclose( mcuversionfile );
        }
    } else{
       printf("can't get mcu version\n");
    }

//    signal(SIGUSR1, sig_handler);
//    signal(SIGUSR2, sig_handler);
    
    // disable watchdog at begining
//  mcu_watchdogdisable();

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
                sleep(delay+20) ;
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
    
    printf( "DVR IO process started!\n");
    
   
    // get default digital input
    mcu_dinput();

    // initialize output
    mcu_doutputmap=~(p_dio_mmap->outputmap) ;
    
    // initialize panel led
    panelled = 0xff ;
    p_dio_mmap->panel_led = 0;
    
    // initialize device power
    devicepower = 0 ;
    p_dio_mmap->devicepower = 0xffff;	// assume all device is power on
    
    while( app_mode ) {
        
        // do digital output
        mcu_doutput();

        // do input pin polling
        //
        mcu_input(5000);
        
        if(p_dio_mmap->gforce_test){
          // dvr_log("gforce_test");
           if(mcu_gforcetest(&xx,&yy,&zz)<0){
               p_dio_mmap->gforce_xx=xx;
               p_dio_mmap->gforce_yy=yy;
               p_dio_mmap->gforce_zz=zz;
               p_dio_mmap->mcu_gforceok=0;
           //    dvr_log("gforce is not ok");
           }else{
               p_dio_mmap->gforce_xx=xx;
               p_dio_mmap->gforce_yy=yy;
               p_dio_mmap->gforce_zz=zz;
               if(xx==0&&yy==0&&zz==0){
		 p_dio_mmap->mcu_gforceok=0;
            //     dvr_log("gforce data wrong");
               }else{
               p_dio_mmap->mcu_gforceok=1;
           //    dvr_log("gforce is ok");
               }
           }
           p_dio_mmap->gforce_test=0;
        }
        if(p_dio_mmap->mcu_flashtest){
	    p_dio_mmap->mcu_flashok= mcu_flashtest();
            p_dio_mmap->mcu_flashtest=0;
        }
        if(p_dio_mmap->tab102_ready==0){
           if((p_dio_mmap->dvrstatus & DVR_RECORD)!=0){
              tab102_ready();
              p_dio_mmap->tab102_ready=1;
           }
        }
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
#if 1
        if( p_dio_mmap->mcu_cmd != 0 ) {
            if( p_dio_mmap->mcu_cmd == 1 ) {
	      dvr_log( "HD power off.");
              mcu_hdpoweroff();
	     // if (!mcu_hdpower(0))
	      p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
            }
            else if( p_dio_mmap->mcu_cmd == 2 ) {
	      	dvr_log( "HD power on.");
             	mcu_hdpoweron();
	     // if (!mcu_hdpower(1))
		p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
            }
            else {
                p_dio_mmap->mcu_cmd=-1;	 // command error, (unknown cmd)
            }
        }
#endif

        static unsigned int cpuusage_timer ;
#if 0
        if( (runtime - cpuusage_timer)> 5000 ) {    // 5 seconds to monitor cpu usage
            cpuusage_timer=runtime ;
            static int usage_counter=0 ;
            if(p_dio_mmap->isftprun==0&&p_dio_mmap->ishybrid_copy==0){
		if( cpuusage()>0.995) {
		
			if( ++usage_counter>12 ) { // CPU keey busy for 1 minites
				buzzer( 10, 250, 250 );
				// let system reset by watchdog
				dvr_log( "CPU usage at 100%% for 60 seconds, system reset.");
				set_app_mode(APPMODE_REBOOT) ;
			}
		}
		else {
			usage_counter=0 ;
		}
            }
        }
#endif

        // Buzzer functions
        buzzer_run( runtime ) ;
        
        static unsigned int temperature_timer ;
        static unsigned int templog_timer;
        if( (runtime - temperature_timer)> 10000 ) {    
           // 10 seconds to read temperature
            temperature_timer=runtime ;

            i=mcu_iotemperature();
#ifdef MCU_DEBUG        
            printf("Temperature: io(%d)",i );
#endif
            if( i > -127 && i < 127 ) {
                static int saveiotemp = 0 ;
                if( i>=75 && 
                   (i-saveiotemp) > 2 ) 
                {
                    dvr_log ( "High system temperature: %d", i);
                    saveiotemp=i ;
                    sync();
                }
                
                p_dio_mmap->iotemperature = i ;
            }
            else {
                p_dio_mmap->iotemperature=-128 ;
            }
            
            if(mcu_hdtemperature(&hdtemp1,&hdtemp2)){
               hdtemp1=-128;
               hdtemp2=-128;
            }
            //check the first hard disk1
            if(hdtemp1 > -127 && hdtemp1 < 127 ) {
                static int savehdtemp = 0 ;

                if( hdtemp1>=75 && 
                   (hdtemp1-savehdtemp) > 2 ) 
                {
                    dvr_log ( "High hard disk1 temperature: %d", hdtemp1);
                    savehdtemp=hdtemp1 ;
                    sync();
                }

                p_dio_mmap->hdtemperature1 = hdtemp1 ;
            }
            else {
                p_dio_mmap->hdtemperature1=-128 ;
            }

            // The Buzzer beep 4time at 0.5s interval every 30s when HDD or system temperature is over 66C
            if( p_dio_mmap->iotemperature > 66 || p_dio_mmap->hdtemperature1 > 66) {
                static int hitempbeep=0 ;
                if( ++hitempbeep>3 ) {
                    buzzer( 4, 250, 250 ) ;
                    hitempbeep=0 ;
                }
            }
            
            static int hightemp_norecord = 0 ;
            if( p_dio_mmap->iotemperature > 83 || p_dio_mmap->hdtemperature1 > 83) {
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
            else if(p_dio_mmap->iotemperature < 75 && p_dio_mmap->hdtemperature1 < 75) {
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

            // printf("mode %d\n", app_mode);
#if 0
	    // check if we need to copy tab102 files to HD
	    if ((app_mode != APPMODE_PRESTANDBY) &&
		(app_mode != APPMODE_STANDBY)) {
	        check_tab102_data();
	    }
#endif
            // do power management mode switching
            if( app_mode==APPMODE_RUN ) {        // running mode
                static int app_run_bell = 0 ;
                if( app_run_bell==0 &&
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrstatus & DVR_RECORD) ) 
                {
                    app_run_bell=1 ;
                    buzzer(1, 1000, 100);
                }
                
                if( p_dio_mmap->poweroff )       // ignition off detected
                {
                    modeendtime = runtime + getshutdowndelaytime()*1000;
                    set_app_mode(APPMODE_SHUTDOWNDELAY) ;  // hutdowndelay start ;
                    dvr_log("Power off switch, enter shutdown delay (mode %d).", app_mode);
                }
            }
            else if( app_mode==APPMODE_SHUTDOWNDELAY ) {   // shutdown delay
                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay() ;
                    if( runtime>modeendtime  ) {
                        // stop dvr recording
                        if( p_dio_mmap->dvrpid>0 && 
                           (p_dio_mmap->dvrstatus & DVR_RUN) &&
                           (p_dio_mmap->dvrcmd == 0 )) 
                        {
                                p_dio_mmap->dvrcmd = 3; // stop recording
                        }
                        sync();
                        // stop glog recording
                        if( p_dio_mmap->glogpid>0 ) {
                            kill( p_dio_mmap->glogpid, SIGUSR1 );
                        }
                        modeendtime = runtime + 120000 ;
                        set_app_mode(APPMODE_NORECORD) ;    // start standby mode
                        dvr_log("Shutdown delay timeout, to stop recording (mode %d).", app_mode);
                    }
                }
                else {
                    // start dvr recording

                    if( p_dio_mmap->dvrpid>0 && 
                       (p_dio_mmap->dvrstatus & DVR_RUN) &&
                       (p_dio_mmap->dvrcmd == 0 )) 
                    {
                       if(p_dio_mmap->dvrstatus & DVR_RECORD){
                          dvr_log("Ignition is on again");
                       } else {
                        	p_dio_mmap->dvrcmd = 4;     // start recording
                          dvr_log("Ignition is on and restart recording");
                       }
                    } else {
                       dvr_log("Ignition is on:%d",p_dio_mmap->dvrcmd);
                    }
                    p_dio_mmap->devicepower = 0xffff ;
                    set_app_mode(APPMODE_RUN) ;   // back to normal
                    dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);
                }
            }
            else if( app_mode==APPMODE_NORECORD ) {  
                // close recording and run smart ftp

                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();
                }

                if( p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   ((p_dio_mmap->dvrstatus & DVR_RECORD)==0) ) 
                {
                        dvr_log("Recording stopped.");
                        //p_dio_mmap->dvrcmd = 2; // disable liveview/playback
                        if(p_dio_mmap->fileclosed)
                           modeendtime = runtime ;
                }
                sync();
 
                if( runtime>=modeendtime) {
		  // start Tab102 downloading
		  if (tab102b_enable) {
		    tab102_start();
		  }
		  p_dio_mmap->nodiskcheck = 1;
		  // set next app_mode
		  modeendtime = runtime + 600000 ;
		  set_app_mode(APPMODE_PRESTANDBY) ;
		  dvr_log("Enter Pre-standby mode. (mode %d).", app_mode);
                }
            }
            else if( app_mode==APPMODE_PRESTANDBY ) {  // get Tab102 data

                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();
                }

		// check if tab102 downloading ended
		if (tab102_wait()) {
		  //check_tab102_data();
		  modeendtime = runtime ;        
		//  p_dio_mmap->nodiskcheck = 0;
		}
                
                if( runtime>=modeendtime&&(p_dio_mmap->ishybrid_copy==0)) {
		  set_app_mode(APPMODE_STANDBY) ;
		  modeendtime = runtime+getstandbytime()*1000+30000 ;
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
		    smartftp_retry=3 ;
		    smartftp_start();
                  //  p_dio_mmap->isftprun=1;
		  }
		  dvr_log("Enter standby mode. (mode %d).", app_mode);
		  buzzer( 3, 250, 250);
                }
            }
            else if( app_mode==APPMODE_STANDBY ) {  // standby

                p_dio_mmap->outputmap ^= HDLED ;  // flash LED slowly for standy mode

                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();
                }
                
                // continue wait for smartftp
		int smartftp_ended = 1;
                if( pid_smartftp > 0 ) {
                    smartftp_ended = smartftp_wait();
                }
		if( runtime>smartftp_endtime  ) {
                    smartftp_ended = 1;
		}
              //  smartftp_ended=0;
                if( p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrstatus & DVR_NETWORK) )
                {
                    p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
                }
                else {
		  if (smartftp_ended) {
		    p_dio_mmap->devicepower=DEVICEOFF ;     
                     // turn off all devices power
		    /*
		    if ((p_dio_mmap->dvrpid == 0) &&
			(p_dio_mmap->dvrcmd == 0)) {
		      p_dio_mmap->dvrcmd = 5; // resume dvrsvr (no recording)
		    }
		    */
		  }
                }

                if( p_dio_mmap->poweroff ) {         // ignition off
		  if (smartftp_ended) {
                    // turn off HD power ?
                    if( standbyhdoff ) {
                          p_dio_mmap->outputmap &= ~HDLED ;   // turn off HDLED
                        if( hdpower ) {
                            hdpower=0;
                            mcu_hdpoweroff ();
                        }
                    }
                    
                    if( runtime>modeendtime  ) {
                        // start shutdown
                        p_dio_mmap->devicepower=DEVICEOFF ;    // turn off all devices power
                        modeendtime = runtime+150000;//90000 ;
                        set_app_mode(APPMODE_SHUTDOWN) ;    // turn off mode
                        dvr_log("Standby timeout, system shutdown. (mode %d).", app_mode );
                        buzzer( 5, 250, 250 );

                     //   umount_recdisk();
			if(p_dio_mmap->dvrpid>0){
				kill(p_dio_mmap->dvrpid, SIGUSR1 );
			}
                       // if( p_dio_mmap->dvrpid > 0 ) {
                        //    kill(p_dio_mmap->dvrpid, SIGTERM);
                       // }
                        if( p_dio_mmap->glogpid > 0 ) {
                            kill(p_dio_mmap->glogpid, SIGTERM);
                        }
                        if( pid_smartftp > 0 ) {
                            smartftp_kill();
                        }
                        sync();
                        system("/dav/dvr/umountdisks") ;
                    }
		  }
                }
                else {                                          // ignition on
#ifdef RESET_ON_IGNON
                    dvr_log("Power on switch after standby. System reboot");
		    sync();
		    // just in case...
                    watchdogtimeout=10 ;                    // 10 seconds watchdog time out
                    mcu_watchdogenable() ;
                    mcu_reboot ();
		    exit(1);
#else
                    if( p_dio_mmap->dvrpid>0 && 
                       (p_dio_mmap->dvrstatus & DVR_RUN) &&
                       p_dio_mmap->dvrcmd == 0 )
                    {
                        p_dio_mmap->dvrcmd = 4 ;             // start recording.
                    }
                    
                    // we should start glog, since it'v been killed.
                    if( p_dio_mmap->glogpid > 0 ) {
                        kill(p_dio_mmap->glogpid, SIGUSR2);
                    }
                    
                    p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
                    set_app_mode(APPMODE_RUN) ;          // back to normal
                    dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);
                    if( pid_tab102 > 0 ) {
                        tab102_kill();
			p_dio_mmap->nodiskcheck = 0;
                    }
                    if( pid_smartftp > 0 ) {
                        smartftp_kill();
                    }
#endif
                }
            }
            else if( app_mode==APPMODE_SHUTDOWN ) {// turn off mode, no keep power on 
                sync();
                if( runtime>modeendtime ) {     
                     // system suppose to turn off during these time
                    // hard ware turn off failed. May be ignition turn on again? Reboot the system
                    dvr_log("Hardware shutdown failed. Try reboot by software!" );
                    set_app_mode(APPMODE_REBOOT) ;
                }
            }
            else if( app_mode==APPMODE_REBOOT ) {
                static int reboot_begin=0 ;
                sync();
                if( reboot_begin==0 ) {
                    if( p_dio_mmap->dvrpid > 0 ) {
                        kill(p_dio_mmap->dvrpid, SIGTERM);
                    }
                    if( p_dio_mmap->glogpid > 0 ) {
                        kill(p_dio_mmap->glogpid, SIGTERM);
                    }
                    // let MCU watchdog kick in to reboot the system
                    watchdogtimeout=10 ;            // 10 seconds watchdog time out
                    mcu_watchdogenable() ;
                    usewatchdog=0 ;         // don't call watchdogenable again!
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
                   (p_dio_mmap->dvrstatus & DVR_NODATA ) &&    // some camera not data in
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
#if 1
                if( hdpower==0 ) {
                    // turn on HD power
                    mcu_hdpoweron() ;
                }
#endif
                hdkeybounce = 0 ;
               
                if( hdinserted &&                      // hard drive inserted
		    !pid_tab102 && // tab102 is not uploading data
                    p_dio_mmap->dvrpid > 0 &&
                    p_dio_mmap->dvrwatchdog >= 0 &&
                    (p_dio_mmap->dvrstatus & DVR_RUN) &&
                    (p_dio_mmap->dvrstatus & DVR_DISKREADY )==0 )        
                    // dvrsvr is running but no disk detected
                {
                    p_dio_mmap->outputmap ^= HDLED ;
                    
                    if( ++hdpower>120 )                // 50 seconds
                    {
                        dvr_log("Hard drive failed, turn hard driver off and on!");
                        buzzer( 10, 250, 250 );

                        // turn off HD led
                        p_dio_mmap->outputmap &= ~HDLED ;
                        mcu_reboot ();
                      //  mcu_hdpoweroff();
                       // mcu_hdpoweroff();  
                       // FOR NEW MCU, THIS WOULD TURN OFF SYSTEM POWER?!
                        hdpower=0;
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
                p_dio_mmap->outputmap ^= HDLED ;      // flash led
                if( hdkeybounce<10 ) {        // wait for key lock stable
                    hdkeybounce++ ;
                    hd_dvrpid_save=0 ;
                }
                else if( hdkeybounce==10 ){
                    hdkeybounce++ ;
                    // suspend DVRSVR process
                    if( p_dio_mmap->dvrpid>0 ) {
                        hd_dvrpid_save=p_dio_mmap->dvrpid ;
                        kill(hd_dvrpid_save, SIGUSR1);  // request dvrsvr to turn down
                        // turn on HD LED
                        p_dio_mmap->outputmap |= HDLED ;
                    }
                }
                else if( hdkeybounce==11 ) {
                    if( p_dio_mmap->dvrpid <= 0 )  {
                        hdkeybounce++ ;
                        sync();
                        // umount disks
                        system("/dav/dvr/umountdisks") ;
                    }
                }
                else if( hdkeybounce==12 ) {
                    // do turn off hd power
                    sync();
                    mcu_hdpoweroff();       // FOR NEW MCU, THIS WOULD TURN OFF SYSTEM POWER?!
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
                        mcu_devicepower(i, p_dio_mmap->devicepower & (1<<i) );
                    }
                }
                devicepower = p_dio_mmap->devicepower ;
                
                // reset network interface, some how network interface dead after device power changes
                setnetwork();
            }
            
        }
        
        // adjust system time with RTC
        static unsigned int adjtime_timer ;
        if( gpsvalid != p_dio_mmap->gps_valid ) {
            gpsvalid = p_dio_mmap->gps_valid ;
            if( gpsvalid ) {
                time_syncgps () ;
                adjtime_timer=runtime ;
                gpsavailable = 1 ;
            }
        }
        if( (runtime - adjtime_timer)> 600000 ||
            (runtime < adjtime_timer) )
        {    // call adjust time every 10 minute
            if( g_syncrtc ) {
                if( gpsavailable ) {
                    if( gpsvalid ) {
                        time_syncgps();
                        adjtime_timer=runtime ;
                    }
                }
                else {
                    time_syncmcu();
                    adjtime_timer=runtime ;
                }
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

