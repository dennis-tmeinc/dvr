/////////////////////////////////////////////////////////////////////////////////////////////////////////
// MCU POWER DOWN CODE
//01: software watchdog reset the power
//02: CPU request system reset
//03: key off before system boot up ready
//04: key off power
//05: bootup time out
//06: power input is too low/high when power on
//07: power input is too low/high after system already on
//08: power input is not stable
//09: HD heater on
//0a: temperature is too high when power on
//0b: temperature is too high after power on
//0c: hardware reset
//0d: CPU request power off
//0e: Normal power off
//////////////////////////////////////////////////////////////////////////////////////////////////
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
#include <sys/mount.h>
#include <sys/file.h>

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

#include "serial.h"
#include "mcu.h"

// options
int standbyhdoff = 0 ;
int usewatchdog = 0 ;
int tab102b_enable;
int buzzer_enable;
int wifi_enable_ex=0;
int wifi_poweron=0;
int motion_control;
int gforcechange=0;
int gevent_on=0;
float battery_low ;		// low battery voltage

int battery_drop_report = 0 ;


unsigned int gforce_start=0;
unsigned int gRecordMode=0;
unsigned int gHBDRecording=0;
unsigned int gHBDChecked=0;
unsigned int gHardDiskon=0;

#ifndef MCU_DEBUG
#define MCU_DEBUG
#endif

// Do system reset on <standby --> ignition on> 
//#define RESET_ON_IGNON 1
#ifdef APP_TVS
#define RESET_ON_IGNON 0

#else
#define RESET_ON_IGNON 1
#endif

// change this to 0xffff to temporarily disable device power off
#define DEVICEOFF 0

// HARD DRIVE LED and STATUS
#define HDLED	  (0x10)
#define FLASHLED  (0x20)
//#define  MCU_DEBUG
#define  DAEMON

// input pin maping
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)
#define RECVBUFSIZE     (100)

int mTempState=0;
int mTempPreState=0;
int mStateNum=0;
void mcu_event_remove();

unsigned int input_map_table [MCU_INPUTNUM] = 
	{ 0, 1, 2, 3, 4, 5, 10, 11, 12 };

// virtual MIC io pins
#define PWII_MIC1_MIC			(1<<(MCU_INPUTNUM))
#define PWII_MIC1_EMG			(2<<(MCU_INPUTNUM))
#define PWII_MIC2_MIC			(4<<(MCU_INPUTNUM))
#define PWII_MIC2_EMG			(8<<(MCU_INPUTNUM))

// translate output bits
#define MAXOUTPUT    (8)
unsigned int output_map_table [MAXOUTPUT] = 
	{ 0, 1, 2, 3, 4, 5, 6, 7 } ;

struct gforce_trigger_point {
  float fb_pos, fb_neg; /* forward: pos */
  float lr_pos, lr_neg; /* right  : pos */
  float ud_pos, ud_neg; /* down   : pos */
};

struct gforce_trigger_point peak_tp = {2.0, -2.0, 2.0, -2.0, 5.0, -3.0};

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

char dvrcurdisk[128] = "/var/dvr/dvrcurdisk" ;
char dvriomap[256] = "/var/dvr/dvriomap" ;
char * pidfile = "/var/dvr/ioprocess.pid" ;
int mcupowerdelaytime = 0 ;
char mcu_firmware_version[80] ;
char hostname[128] = "BUS001";

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = CFG_FILE ;
char logfile[128] = "dvrlog.txt" ;
string temp_logfile;
int watchdogenabled=0 ;
int watchdogtimeout=30 ;
int watchdogtimeset=30;

int gpsvalid = 0 ;
int gforce_log_enable=0 ;
int output_inverted=0 ;
unsigned int iosensor_inverted=0; 
char shutdowncodestr[256]="";

unsigned int panelled=0 ;
unsigned int devicepower=0xffff;

pid_t   pid_smartftp = 0 ;
pid_t   pid_tab102 = 0 ;

#define WRITE_INPUT 0

int app_mode = APPMODE_QUIT ;
static int app_mode_r = APPMODE_QUIT;
static unsigned int modeendtime = 0;
static unsigned int runtime;
    
void set_app_mode(int mode, int modetimeout)
{
  modeendtime = runtime+modetimeout*1000; 	//  in seconds 
  app_mode=mode ;
  p_dio_mmap->current_mode =  mode;
}

int  mode_timeout()
{
	return runtime > modeendtime ;
}

void sig_handler(int signum)
{
    if( signum==SIGUSR2 ) {
      app_mode_r = app_mode;
      set_app_mode(APPMODE_REINITAPP, 0) ;
   }
    else {
      set_app_mode(APPMODE_QUIT, 0);
    }
}

// return bcd value
int getbcd(unsigned char c)
{
    return (int)(c/16)*10 + c%16 ;
}

#define dvr_log1 dvr_log

const char *logfilelnk=VAR_DIR "/dvrlogfile" ;
static string tmplogfile ;

// write log to log file.
// return
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(const char *fmt, ...)
{
    char lbuf[512];
    FILE *flog;
    time_t ti;
    int l;
    va_list ap ;

    flog=NULL ;
    // check logfile, it must be a symlink to hard disk
    if( readlink(logfilelnk, lbuf, 512)>10 ) {
        flog = fopen(logfilelnk, "a");
    }

    ti = time(NULL);
    ctime_r(&ti, lbuf);
    l = strlen(lbuf);
    if (lbuf[l - 1] == '\n')
        l-- ;
    lbuf[l++]=':' ;
    lbuf[l++]='I' ;
    lbuf[l++]='O' ;
    lbuf[l++]=':' ;
    va_start( ap, fmt );
    vsprintf( &(lbuf[l]), fmt, ap );
    va_end( ap );
    
    printf("%s\n", lbuf);
    
    
    static int dbgout=0 ;
	if( dbgout <= 0 )
		dbgout = open("/var/dvr/dbgout", O_WRONLY|O_NONBLOCK) ;
	if( dbgout>0 ) {
		write( dbgout, lbuf, strlen(lbuf) );
		if( write( dbgout, "\n", 1 )<=0 ) {
			close( dbgout );
			dbgout = 0 ;
		}
	}


    if (flog) {
		
		flock( fileno(flog), LOCK_EX );
		
        fprintf(flog, "%s\n", lbuf);
        
        fflush( flog );
        flock( fileno(flog), LOCK_UN );
        
        if( fclose( flog )==0 ) {
            return 1;
        }
    }

    // log to temperary log file
    flog = fopen(tmplogfile, "a");

    if (flog) {
		flock( fileno(flog), LOCK_EX );
				
        fprintf(flog, "%s *\n", lbuf);

        fflush( flog );
        flock( fileno(flog), LOCK_UN );
        fclose( flog ) ;
    }

    return 0 ;
}


int getshutdowndelaytime()
{
    config dvrconfig(dvrconfigfile);
    int v ;
    v = dvrconfig.getvalueint( "system", "shutdowndelay");
    if( v<10 ) {
        v=10 ;
    }
    /*
    if( v>7200 ) {
        v=7200 ;
    }*/
    return v ;
}

int getstandbytime()
{
    config dvrconfig(dvrconfigfile);
    int v ;
    v = dvrconfig.getvalueint( "system", "standbytime");
    if( v<10 ) {
        v=10;
    }
    if( v>43200 ) {
        v=43200 ;
    }
    return v ;
}

static time_t starttime=0 ;
void initruntime()
{
	struct timespec tp ;
	clock_gettime(CLOCK_MONOTONIC, &tp);
    starttime=tp.tv_sec ;
}

// return runtime in milliseconds
unsigned int getruntime()
{
	struct timespec tp ;
	clock_gettime(CLOCK_MONOTONIC, &tp);
    return (unsigned int)(tp.tv_sec-starttime)*1000 + tp.tv_nsec/1000000 ;
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
    int hrtc = open("/dev/rtc0", O_WRONLY );
    if( hrtc<0 ) {
		hrtc = open("/dev/rtc", O_WRONLY );
	}
    if( hrtc>0 ) {
        gmtime_r(&utctime, &ut);
		if(ut.tm_year>100)
           ioctl( hrtc, RTC_SET_TIME, &ut ) ;
        close( hrtc );
    }
}

unsigned int mcu_doutputmap ;

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

#if 0
static char direction_table[24][2] = 
{
    {0, 2}, {0, 3}, {0, 4}, {0,5},      // Front -> Forward
    {1, 2}, {1, 3}, {1, 4}, {1,5},      // Back  -> Forward
    {2, 0}, {2, 1}, {2, 4}, {2,5},      // Right -> Forward
    {3, 0}, {3, 1}, {3, 4}, {3,5},      // Left  -> Forward
    {4, 0}, {4, 1}, {4, 2}, {4,3},      // Buttom -> Forward
    {5, 0}, {5, 1}, {5, 2}, {5,3},      // Top   -> Forward
};
#else
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

#endif

#define DEFAULT_DIRECTION   (7)
int gsensor_direction = DEFAULT_DIRECTION ;
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
int  g_sensor_available ;


//unsigned short g_refX, g_refY, g_refZ, g_peakX, g_peakY, g_peakZ;
//unsigned char g_order, g_live, g_mcudebug, g_task;
//int g_mcu_recverror, g_fw_ver;

void convertPeakData(unsigned short peakX,
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

int  check_trigger_event(float gforward,float gright,float gdown)
{
  if(gforward >=peak_tp.fb_pos || gforward<= peak_tp.fb_neg)
    return 1;
  if(gright>=peak_tp.lr_pos || gright<=peak_tp.lr_neg)
    return 1;
  if(gdown>=peak_tp.ud_pos || gdown<=peak_tp.ud_neg)
    return 1;
  
  return 0;
}

// map hardware input to diomap
static void dinputmap( unsigned int imap ) {
	unsigned int imap2=0 ;
	int i;
	for( i=0; i<MCU_INPUTNUM; i++ ) {
		if( imap & (1<<input_map_table[i]) )
			imap2 |= 1<<i ;
	}		

	// preserver mic status
    p_dio_mmap->inputmap =  (p_dio_mmap->inputmap & (0xffff<<MCU_INPUTNUM) ) | imap2;
    
    // hdlock = (imap1 & (HDINSERTED|HDKEYLOCK) )==0 ;		// HD plugged in and locked
    hdlock = (imap & (HDKEYLOCK) )==0 ;	                // HD plugged in and locked
    hdinserted = (imap & HDINSERTED)==0 ;  // HD inserted
}

// default mic status
static unsigned int mic_stat = 0 ;

// trigger time of EMG1, EMG2
static int emg1_time, emg2_time ;

// callback on receive mcu input msg
int mcu_oninput( char * ibuf )
{
    unsigned int imap1, imap2 ;
    int i;
    float gback, gright, gbuttom ;
    float gbusforward, gbusright, gbusdown ;

	switch( ibuf[3] ) {
	case '\x1c' :                   // digital input event
		mcu_response(ibuf);
		
		// get digital input map
		imap1 = (unsigned char)ibuf[5]+256*(unsigned int)(unsigned char)ibuf[6] ;
		dvr_log("GPIO input: %04x", (unsigned int)(unsigned short int)imap1 );
		dinputmap( imap1 );
		break;

	case '\x56':					// wireless mic input
		mcu_response(ibuf);

		imap1 = (~((unsigned int)(unsigned char)ibuf[5])) & 0x0f  ;
		dvr_log( "PWZ6 Mic inputs: %08x .", imap1);
		imap2 = imap1 ^ mic_stat ;
		mic_stat = imap1 ;
		
		if( imap2 & 1 ) {		//  MIC1 (REC1/MUTE1)
			// update virtual digital input
			if( mic_stat & 1 ) {
				p_dio_mmap->inputmap |= PWII_MIC1_MIC ;
				
				// mark it as if mic1 on
				p_dio_mmap->pwii_output |= PWII_MIC1_ON  ;
			}
			else {
				p_dio_mmap->inputmap &= ~ PWII_MIC1_MIC ;
				// mic off command
				// mcu_mic_off( 0 );
				p_dio_mmap->pwii_output &= ~PWII_MIC1_ON  ;
			}
		}
		if( imap2 & 2 ) {		//  EMG1
			p_dio_mmap->inputmap |= PWII_MIC1_EMG ;
			emg1_time = runtime ;
			/*
			if( mic_stat & 2 ) {
				p_dio_mmap->inputmap |= PWII_MIC1_EMG ;
				// p_dio_mmap->pwii_output |= PWII_MIC1_ON  ;
			}
			else {
				p_dio_mmap->inputmap &= ~ PWII_MIC1_EMG  ;
			}
			*/
		}
		
		if( imap2 & 4 ) {		//  MIC2 (REC2/MUTE2)
			// update virtual digital input
			if( mic_stat & 4 ) {
				p_dio_mmap->inputmap |= PWII_MIC2_MIC ;
				
				// mark it as if mic2 on
				p_dio_mmap->pwii_output |= PWII_MIC2_ON  ;
			}
			else {
				p_dio_mmap->inputmap &= ~ PWII_MIC2_MIC ;
				// redundant mic off command
				// mcu_mic_off( 1 );
				p_dio_mmap->pwii_output &= ~PWII_MIC2_ON  ;
			}
		}
		if( imap2 & 8 ) {		//  EMG2
			p_dio_mmap->inputmap |= PWII_MIC2_EMG ;
			emg2_time = runtime ;
			/*
			if( mic_stat & 8 ) {
				p_dio_mmap->inputmap |= PWII_MIC2_EMG ;
			}
			else {
				p_dio_mmap->inputmap &= ~ PWII_MIC2_EMG  ;
			}
			*/
		}
		
		// to turn off LED on camera
		if( (mic_stat & 5) == 0 ) {
			mcu_mic_ledoff();
			mcu_amberled(0);
		}
		else {
			mcu_amberled(1);
		}

		break;
		
	case '\x08' :               // ignition off event
		mcu_response(ibuf,2,0,0);
		
		mcupowerdelaytime = 0 ;
		p_dio_mmap->poweroff = 1 ;	// send power off message to DVR
		break;
		
	case '\x09' :	// ignition on event
		mcu_response(ibuf,1,watchdogenabled);
		
		p_dio_mmap->poweroff = 0 ;	// send power on message to DVR
          
		break ;

	case '\x40' :               // Accelerometer data
		mcu_response(ibuf);
		
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
	case '\x1e':	  
		mcu_response(ibuf);
		if((ibuf[0]=='\x0c') &&
		   (ibuf[2]=='\x04')){
					 //static char rsp_accel[10] = "\x06\x04\x00\x1e\x03\xcc" ;
					 //mcu_send(rsp_accel);
			 convertPeakData((ibuf[5] << 8) | ibuf[6],
					 (ibuf[7] << 8) | ibuf[8],
							 (ibuf[9] << 8) | ibuf[10],
					 &gbusforward, &gbusright, &gbusdown);	
#ifdef MCU_DEBUG
				  //  printf("before: [5]:%x [6]:%x [7]:%x [8]:%x [9]:%x [10]:%x\n",ibuf[5],ibuf[6],ibuf[7],ibuf[8],ibuf[9],ibuf[10]);
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
			
			//if(check_trigger_event(gbusforward,gbusright,gbusdown)){
			//    p_dio_mmap->mcu_cmd=7; 
			//}
			
			p_dio_mmap->gforce_right_d=gbusright;
			p_dio_mmap->gforce_forward_d=gbusforward;
			p_dio_mmap->gforce_down_d=gbusdown;
			gforcechange=1;
			p_dio_mmap->gforce_changed=1;
			gforce_start=getruntime();
		}
		break;
		
	case '\x49' :
		mcu_response(ibuf);
		{
			char *breport[]={
				"fully charged" ,
				"charging",
				"disconnected", "unknown" };
			p_dio_mmap->battery_voltage = 0.016178 * (int)((unsigned char)ibuf[7]+256*(unsigned int)(unsigned char)ibuf[6]) ; 
			if( ibuf[5]<0 || ibuf[5]>3 )ibuf[5]=3 ;
			p_dio_mmap->battery_state = ibuf[5] ;
			dvr_log("Battery report, battery is %s, voltage: %.2f V.", breport[ibuf[5]], p_dio_mmap->battery_voltage );
		}
		break ;
		
	case '\x4a' :
		mcu_response(ibuf);
		{
			if( battery_drop_report == 0 ) {
				dvr_log( "Battery voltage dropped... " );
				battery_drop_report = 1 ;
			}
		}
		break ;
		
	default :
		mcu_response(ibuf);
		dvr_log( "Unknown msg from MCU: 0x%02x", (int)ibuf[3]);
		break;
	}
    return 0 ;
}

// return gsensor available
int mcu_gsensorinit()
{
    float trigger_right, trigger_left ;
    float trigger_back, trigger_front ;
    float trigger_bottom, trigger_top ;
    
    int   t_right, t_left ;
    int   t_back, t_front ;
    int   t_bottom, t_top ;
    
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
        t_right  = (signed char)(trigger_right*0xe) ;    // Right Trigger
        t_left   = (signed char)(trigger_left*0xe) ;     // Left Trigger
    }
    else {
        t_right  = (signed char)(trigger_left*0xe) ;     // Right Trigger
        t_left   = (signed char)(trigger_right*0xe) ;    // Left Trigger
    }
    
    if( trigger_back >= trigger_front ) {
        t_back   = (signed char)(trigger_back*0xe) ;    // Back Trigger
        t_front  = (signed char)(trigger_front*0xe) ;    // Front Trigger
    }
    else {
        t_back   = (signed char)(trigger_front*0xe) ;    // Back Trigger
        t_front  = (signed char)(trigger_back*0xe) ;    // Front Trigger
    }
    
    if( trigger_bottom >= trigger_top ) {
        t_bottom  = (signed char)(trigger_bottom*0xe) ;    // Bottom Trigger
        t_top     = (signed char)(trigger_top*0xe) ;    // Top Trigger
    }
    else {
        t_bottom  = (signed char)(trigger_top*0xe) ;    // Bottom Trigger
        t_top     = (signed char)(trigger_bottom*0xe) ;    // Top Trigger
    }
    
    g_sensor_available = 0 ;
    char * responds = mcu_cmd( 0x34, 6,
		t_right, t_left,
		t_back, t_front,
		t_bottom, t_top ) ;
	if( responds ){
		g_sensor_available = responds[5] ;
		return g_sensor_available ;
	}
    return 0 ;
}

int doutput_init()
{
	unsigned int outputmap  = 0 ;
    unsigned int omap = 0 ;
    unsigned int outputmapx ;
    int i;

    outputmapx = (outputmap^output_inverted) ;

    // translate output bits ;
    for(i=0; i<MAXOUTPUT; i++) {
        if( outputmapx & (1<<output_map_table[i]) ) {
            omap|=(1<<i) ;
        }
    }
   
    mcu_dio_output( omap ) ;
    mcu_doutputmap = outputmap ;
	return 0;
}

// try to trigger digital output 
int doutput_trigger()
{
	mcu_doutputmap = ~mcu_doutputmap ;
}

int doutput()
{
    unsigned int outputmap  ;
    unsigned int outputmapx ;
    int omap ;
    int i ;
    
    if( app_mode>APPMODE_SHUTDOWNDELAY ){
       p_dio_mmap->outputmap&=0x24;   //0x04;
    }
    outputmap = p_dio_mmap->outputmap ;
    
    if (!buzzer_enable) {
        outputmap &= (~0x100) ;     // buzzer off
    }

    if( outputmap == mcu_doutputmap ) return 1 ;

    //printf("\np_dio_mmap->outputmap:%x\n",p_dio_mmap->outputmap);
    outputmapx = (outputmap^output_inverted) ;

    // bit 2 is beep. (from July 6, 2009)
    // translate output bits ;
    omap = 0 ;
    for(i=0; i<MAXOUTPUT; i++) {
        if( outputmapx & (1<<output_map_table[i]) ) {
            omap|=(1<<i) ;
        }
    }
    
    mcu_dio_output( omap ) ;
    mcu_doutputmap = outputmap ;
    
    return 0;
}


// get mcu digital input
int mcu_dinput()
{
    dinputmap( (unsigned int )mcu_dio_input() ) ;
    return 1 ;
}

#ifdef PWII_APP   
// pwz_x related

void input_check()
{
	// extra check for PWII mic emg status, (moved to dvrsvr.dio)
}

// check pwii output	
int mcu_pwii_output()
{
	static unsigned int s_pwii_output = 0 ;
	int x_pwii_output = p_dio_mmap->pwii_output ^ s_pwii_output ;
	s_pwii_output ^= x_pwii_output ;
	
	// ZOOM IN SUPPORT
	if( x_pwii_output & PWII_LP_ZOOMIN ) {
		if( s_pwii_output & PWII_LP_ZOOMIN ) {
#ifdef APP_PWZ8			
			mcu_camera_nightmode(1);	// night mode
#else 
			mcu_camera_zoomin(1);		// zoom in
#endif			
		}
		else {
#ifdef APP_PWZ8			
			mcu_camera_nightmode(0);	// night mode off
#else 
			mcu_camera_zoomin(0);		// zoom out
#endif			
		}
	}
	
	// check for mic output

	// mic1
	if( x_pwii_output & PWII_MIC1_ON ) {
		if( s_pwii_output & PWII_MIC1_ON  ) {
			mcu_mic_on( 0 );			
		}
		else {
			mcu_mic_off( 0 );
		}
	}
	// mic2
	if( x_pwii_output & PWII_MIC2_ON ) {
		if( s_pwii_output & PWII_MIC2_ON  ) {
			mcu_mic_on( 1 );			
		}
		else {
			mcu_mic_off( 1 );
		}
	}
	
	// covert mode ?
	if( x_pwii_output & PWII_COVERT_MODE ){
		if( s_pwii_output & PWII_COVERT_MODE ) {
			mcu_covert(1) ;
			mcu_amberled(0);
		}
		else {
			mcu_covert( 0 ) ;
			// to force ipcam LED back to 'NORMAL', by sending a doutput message
			doutput_trigger() ;
			// restore amber led
			mcu_amberled( (mic_stat & 5) != 0 );
		}
	}
	
}

#endif

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
    mcu_write( responds, sizeof(responds));
    mcu_clear(1000000);
    
    
    printf("Erasing.\n");
    rd=0 ;
    if( mcu_write( cmd_updatefirmware, 5 ) ) {
        if( mcu_dataready (10000000 ) ) {
            rd=mcu_read ( responds, sizeof(responds) ) ;
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
      if( mcu_write( &c, 1)!=1 ) {
            break;
      }
        if( c=='\n' && mcu_dataready (0) ) {
            rd = mcu_read (responds, sizeof(responds) );
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
    if( res==0 && mcu_dataready(5000000) ) {
            rd = mcu_read (responds, sizeof(responds) );
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

void mcu_setrtc()
{
	if( mcu_cmd( 7, 7,
		bcd( p_dio_mmap->rtc_second ),
		bcd( p_dio_mmap->rtc_minute ),
		bcd( p_dio_mmap->rtc_hour ),
		0,						// day of week ?
		bcd( p_dio_mmap->rtc_day ),
		bcd( p_dio_mmap->rtc_month),
		bcd( p_dio_mmap->rtc_year) ) ) 
	{
		p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
	}
	else {
		dvr_log("mcu_setrtc:07");  	
		p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.    
	}
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
    int ret;
    struct tm ptm;
    for( retry=0; retry<3; retry++ ) {
        ret=mcu_r_rtc(&ptm,&rtc);
        if( ret ) {
            tv.tv_sec=rtc ;
            tv.tv_usec=0;
            settimeofday(&tv, NULL);
            // also set onboard rtc
            rtc_set((time_t)tv.tv_sec);
            break;
        }
    }
}

#if 0
void time_syncgps ()
{
    struct timeval tv ;
    int diff ;
    time_t gpstime ;
    struct tm ut;
    if( p_dio_mmap->gps_valid ) {
        gpstime = (time_t) p_dio_mmap->gps_gpstime ;
	gmtime_r(&gpstime, &ut);
	if(ut.tm_year>100&&ut.tm_year<130){
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
}
#else
void time_syncgps ()
{
    struct timeval tv ;
    int diff ;
    time_t gpstime ;
    struct tm ut;
    if( p_dio_mmap->gps_valid ) {
        gpstime = (time_t) p_dio_mmap->gps_gpstime ;
	gmtime_r(&gpstime, &ut);
	if(ut.tm_year>100&&ut.tm_year<130){
	    gettimeofday(&tv, NULL);
	    diff = (int)gpstime - (int)tv.tv_sec ;
	    if( diff>2 || diff<-2 ) {
		tv.tv_sec=gpstime ;
		tv.tv_usec= 0;
		settimeofday( &tv, NULL );
		dvr_log ( "Time sync use GPS time." );
		// set mcu time
		mcu_w_rtc(gpstime) ;
		// also set onboard rtc
		rtc_set(gpstime);		
	    }
	}
    }
}
#endif
void time_syncmcu()
{
    int diff ;
    struct timeval tv ;
    time_t rtc ;
    struct tm ptm;
    int ret;
    ret=mcu_r_rtc(&ptm,&rtc);
    if( ret>0 ) {
        gettimeofday(&tv, NULL);
        //diff = (int)rtc - (int)tv.tv_sec ;
        diff = rtc - tv.tv_sec ;
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
            doutput();
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
        static char snwpath[]="/mnt/nand/dvr/setnetwork" ;
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

void tab102_reset()
{
   int pid=fork();
   if(pid==0){
      dvr_log("make tab102 reset");       
      execl( "/mnt/nand/dvr/tab102", "/mnt/nand/dvr/tab102","-reset",
	   NULL 
	   );
      dvr_log("make tab102 reset failed");
   } else if(pid<0){
      dvr_log("make tab102 reset failed");
   }  
}

void tab102_ready()
{
   int pid=fork();
   if(pid==0){
      dvr_log("make tab102 boot ready");       
      execl( "/mnt/nand/dvr/tab102", "/mnt/nand/dvr/tab102","-rtc",
	   NULL 
	   );
      dvr_log("make tab102 boot ready failed");
   } else if(pid<0){
      dvr_log("make tab102 boot ready failed");
   }
}

void tab102_settrigger()
{
  FILE *fp;   
  fp = fopen("/var/dvr/triggerset", "w");
  if (fp) {
    fprintf(fp,"trigger");
    fclose(fp);
  }
  return;        
  
}

void tab102_setup()
{
   int pid=fork();
   if(pid==0){
      execl( "/mnt/nand/dvr/tab102", "/mnt/nand/dvr/tab102","-st",
	   NULL 
	   );
      dvr_log("set up tab102b failed");
   } else if(pid<0){
      dvr_log("set up tab102b failed");
   }
}

void tab102_start()
{
  dvr_log( "Start Tab102 downloading.");
  pid_tab102 = fork();
  if( pid_tab102==0 ) {     // child process
    execl( "/mnt/nand/dvr/tab101check", "/mnt/nand/dvr/tab101check",
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

static char smartftp_dev[64] ;

void smartftp_start()
{
    int ret;
  
    dvr_log( "Start smart server uploading.");
    pid_smartftp = fork();
    if( pid_smartftp==0 ) {     // child process
        char hostname[128] ;
	// char mountdir[250] ;
        
        // get BUS name
        gethostname(hostname, 128) ;
        
      // reset network, this would restart wifi adaptor
 //       system("/mnt/nand/dvr/setnetwork"); 
        dvr_log("dev:%s hostname:%s",smartftp_dev,hostname);
        ret=execlp( "/mnt/nand/dvr/smartftp", "/mnt/nand/dvr/smartftp",
              smartftp_dev,
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
                //if(exitcode==0){
		  //p_dio_mmap->mcu_cmd=8; 
		 // mcu_event_remove();
		//}                
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

void reloadconfig()
{
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    int i;

    v = dvrconfig.getvalue( "system", "logfile");
    if (v.length() > 0){
        strncpy( logfile, v.getstring(), sizeof(logfile));
    }
    v = dvrconfig.getvalue( "system", "temp_logfile");
    if (v.length() > 0){
        temp_logfile=v;
      //  strncpy( temp_logfile, v.getstring(), sizeof(temp_logfile));
    }
        
    v = dvrconfig.getvalue( "system", "currentdisk");
    if (v.length() > 0){
        strncpy( dvrcurdisk, v.getstring(), sizeof(dvrcurdisk));
    }

    // initialize shared memory
    p_dio_mmap->inputnum = dvrconfig.getvalueint( "io", "inputnum");	
    if( p_dio_mmap->inputnum<=0 || p_dio_mmap->inputnum>32 )
        p_dio_mmap->inputnum = MCU_INPUTNUM ;	
    p_dio_mmap->outputnum = dvrconfig.getvalueint( "io", "outputnum");	
    if( p_dio_mmap->outputnum<=0 || p_dio_mmap->outputnum>32 )
        p_dio_mmap->outputnum = MCU_OUTPUTNUM ;	
    
    output_inverted = 0 ;
    
    for( i=0; i<32 ; i++ ) {
        char outinvkey[50] ;
        sprintf(outinvkey, "output%d_inverted", i+1);
        if( dvrconfig.getvalueint( "io", outinvkey ) ) {
            output_inverted |= (1<<i) ;
        }
    }
    
    // smartftp variable
    smartftp_disable = dvrconfig.getvalueint("system", "smartftp_disable");
    
    tab102b_enable = dvrconfig.getvalueint( "glog", "tab102b_enable");
    buzzer_enable = dvrconfig.getvalueint( "io", "buzzer_enable");
    wifi_enable_ex= dvrconfig.getvalueint("system", "ex_wifi_enable");
    wifi_poweron=dvrconfig.getvalueint("network", "wifi_poweron");
    // gforce sensor setup

   // gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");
    gforce_log_enable=tab102b_enable;
}

// check battery voltage
void battery_check()
{
	int battery_state ;
	int battery_voltage ;
    battery_state = mcu_battery_check( &battery_voltage );
    if( battery_state<0 || battery_state>3 ) 
		battery_state=3 ;
    p_dio_mmap->battery_state = battery_state ;
    p_dio_mmap->battery_voltage = 0.016178 * battery_voltage ;

	if( app_mode > APPMODE_RUN && p_dio_mmap->battery_voltage < battery_low ) {
		set_app_mode(APPMODE_SHUTDOWN, 1) ;
		dvr_log( "Battery low, voltage: %.1fV, shutdown system..." , (double)(p_dio_mmap->battery_voltage) );
	}
    
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
    int cap_ch ;    
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

    tzset();
    cap_ch = dvrconfig.getvalueint("system", "totalcamera");
    gRecordMode=4;
    for(i=0;i<cap_ch;++i){
        char section[20];
	int mRecMode=4;
	int enabled=0;
	sprintf(section, "camera%d", i+1);	
	enabled = dvrconfig.getvalueint( section, "enable");
	if(enabled){
	  mRecMode=dvrconfig.getvalueint( section, "recordmode");
	  if(mRecMode==0){
	    gRecordMode=0;
	    break;
	  }
	}
    }
    v = dvrconfig.getvalue( "system", "logfile");
    if (v.length() > 0){
        strncpy( logfile, v.getstring(), sizeof(logfile));
    }
        
    v = dvrconfig.getvalue("system", "temp_logfile");
    if (v.length() > 0) {
        tmplogfile = v ;
    }
    else {
        tmplogfile = VAR_DIR "/logfile" ;
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
    
    iosensor_inverted=0;
    for(i=1;i<p_dio_mmap->inputnum;++i){
        char section[20] ;
	int value;
        sprintf( section, "sensor%d", i+1 );
	value=dvrconfig.getvalueint(section, "inverted");
	//printf("sensor%d:%d\n",i,value);
	if(value){
	    iosensor_inverted|=(0x01<<i); 
	}
    }
    for(i=p_dio_mmap->inputnum;i<16;++i){
        iosensor_inverted|=(0x01<<i);       
    }
    
    if(motion_control){
        iosensor_inverted|=0x01;
    } 
    
    // check if sync rtc wanted
    
    g_syncrtc = dvrconfig.getvalueint("io", "syncrtc");
    
    // initilize mcu (serial) port
    mcu_init( dvrconfig ) ;


    // smartftp variable
    smartftp_disable = dvrconfig.getvalueint("system", "smartftp_disable");
   
    // initialize mcu (power processor)
    int mD1;
    if( mcu_bootupready (&mD1) ) {
    //    dvr_log1("MCU ready.");
        mcu_clear(100000);
        mcu_readcode();
        
        // get MCU version
        char mcu_firmware_version[80] ;
        if( mcu_version( mcu_firmware_version ) ) {
            dvr_log("MCU version: %s", mcu_firmware_version );
            FILE * mcuversionfile=fopen("/var/dvr/mcuversion", "w");
            if( mcuversionfile ) {
                fprintf( mcuversionfile, "%s", mcu_firmware_version );
                fclose( mcuversionfile );
            }
        }
        
    }
    else {
        dvr_log1("MCU failed,eagle reboot.");
       // system("/bin/reboot"); 
       return 0;
    }
    
#ifdef APP_PWZ5
    i = dvrconfig.getvalueint( "system", "pan_zoom_camera");	
    mcu_set_pancam(i);
    mcu_poe_power( 1 );
	mcu_campower();
#endif
    
// #ifdef TVS_APP
    // setup GP2 polarity for TVS
//    mcu_initsensor2(dvrconfig.getvalueint( "sensor2", "inverted" ));
// #endif
    
    if( g_syncrtc ) {
        printf("start syc rtc\n");
        mcu_rtctosys();
    }
    
#if 0
    if(mD1!=motion_control){
        if(motion_control)
    	   mcu_motioncontrol_enable();
        else
    	   mcu_motioncontrol_disable();
    }
#else    
    if(mcu_iosensorrequest()){
       mcu_iosensor_send(); 
    }
#endif
 

    tab102b_enable = dvrconfig.getvalueint( "glog", "tab102b_enable");
    buzzer_enable = dvrconfig.getvalueint( "io", "buzzer_enable");
    wifi_enable_ex= dvrconfig.getvalueint("system", "ex_wifi_enable");
    wifi_poweron=dvrconfig.getvalueint("network", "wifi_poweron");
    gHBDRecording=dvrconfig.getvalueint("system", "hbdrecording");
    // gforce sensor setup
    
       
    // external smart upload device name
    v = dvrconfig.getvalue( "network", "smartftp_dev");
    if (v.length() > 0){
		strncpy( smartftp_dev, v.getstring(), sizeof(smartftp_dev));
    }
    else {
		strcpy(smartftp_dev,"eth0");
		if(!wifi_enable_ex){
		  if(readWifiDeviceName(smartftp_dev, sizeof(smartftp_dev))){
			  //using fixed wifi
			  strcpy(smartftp_dev,"wlan0");
		  }
		}
	} 

    //gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");
    gforce_log_enable=tab102b_enable;
#if 0
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
#else
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
#endif
    // gsensor trigger setup
#if 0    
    if( mcu_gsensorinit() ) {       // g sensor available
        dvr_log("G sensor detected!");
        pidf = fopen( "/var/dvr/gsensor", "w" );
        if( pidf ) {
            fprintf(pidf, "1");
            fclose(pidf);
        }
    }
#endif    
    usewatchdog = dvrconfig.getvalueint("io", "usewatchdog");
    watchdogtimeout=dvrconfig.getvalueint("io", "watchdogtimeout");
    if( watchdogtimeout<10 )
        watchdogtimeout=10 ;
    if( watchdogtimeout>200 )
        watchdogtimeout=200 ;

    watchdogtimeset=watchdogtimeout;
    
	battery_low = 7.4 ;
    v = dvrconfig.getvalue( "system", "battery_low");
    if( !v.isempty() ) {
		sscanf( (char *)v, "%f", &battery_low );
	}
	if( battery_low < 7.2 ) battery_low = 7.2 ;
    
    pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }

    // initialize timer
    initruntime();
    p_dio_mmap->fileclosed=0;
    p_dio_mmap->gforce_changed=0;
    
    return 1;
}

void re_appinit()
{
    FILE * pidf ;
    int fd ;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    int i;
    int cap_ch ;    
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

    cap_ch = dvrconfig.getvalueint("system", "totalcamera");
    gRecordMode=4;
    for(i=0;i<cap_ch;++i){
        char section[20];
	int mRecMode=4;
	int enabled=0;
	sprintf(section, "camera%d", i+1);	
	enabled = dvrconfig.getvalueint( section, "enable");
	if(enabled){
	  mRecMode=dvrconfig.getvalueint( section, "recordmode");
	  if(mRecMode==0){
	    gRecordMode=0;
	    break;
	  }
	}
    }
    v = dvrconfig.getvalue( "system", "logfile");
    if (v.length() > 0){
        strncpy( logfile, v.getstring(), sizeof(logfile));
    }
    v = dvrconfig.getvalue( "system", "temp_logfile");
    if (v.length() > 0){
      temp_logfile=v;
      //  strncpy( temp_logfile, v.getstring(), sizeof(temp_logfile));
    }
        
    v = dvrconfig.getvalue( "system", "currentdisk");
    if (v.length() > 0){
        strncpy( dvrcurdisk, v.getstring(), sizeof(dvrcurdisk));
    }

    // initialize shared memory
    p_dio_mmap->inputnum = dvrconfig.getvalueint( "io", "inputnum");	
    if( p_dio_mmap->inputnum<=0 || p_dio_mmap->inputnum>32 )
        p_dio_mmap->inputnum = MCU_INPUTNUM ;	
    p_dio_mmap->outputnum = dvrconfig.getvalueint( "io", "outputnum");	
    if( p_dio_mmap->outputnum<=0 || p_dio_mmap->outputnum>32 )
        p_dio_mmap->outputnum = MCU_INPUTNUM ;	

    motion_control= dvrconfig.getvalueint("io","sensor_powercontrol");
    
    output_inverted = 0 ;
    
    for( i=0; i<32 ; i++ ) {
        char outinvkey[50] ;
        sprintf(outinvkey, "output%d_inverted", i+1);
        if( dvrconfig.getvalueint( "io", outinvkey ) ) {
            output_inverted |= (1<<i) ;
        }
    }
    
    iosensor_inverted=0;
    for(i=1;i<p_dio_mmap->inputnum;++i){
        char section[20] ;
	int value;
        sprintf( section, "sensor%d", i+1 );
	value=dvrconfig.getvalueint(section, "inverted");
	//printf("sensor%d:%d\n",i,value);
	if(value){
	    iosensor_inverted|=(0x01<<i); 
	}
    }
    for(i=p_dio_mmap->inputnum;i<16;++i){
        iosensor_inverted|=(0x01<<i);       
    }
    
    if(motion_control){
        iosensor_inverted|=0x01;
    } 


    // smartftp variable
    smartftp_disable = dvrconfig.getvalueint("system", "smartftp_disable");

  
    if(mcu_iosensorrequest()){
       mcu_iosensor_send(); 
    }


    tab102b_enable = dvrconfig.getvalueint( "glog", "tab102b_enable");
    buzzer_enable = dvrconfig.getvalueint( "io", "buzzer_enable");
    wifi_enable_ex= dvrconfig.getvalueint("system", "ex_wifi_enable");
    wifi_poweron=dvrconfig.getvalueint("network", "wifi_poweron");
    gHBDRecording=dvrconfig.getvalueint("system", "hbdrecording");
    
           
    // external smart upload device name
    v = dvrconfig.getvalue( "network", "smartftp_dev");
    if (v.length() > 0){
		strncpy( smartftp_dev, v.getstring(), sizeof(smartftp_dev));
    }
    else {
		strcpy(smartftp_dev,"wlan0");
	} 
    
    // gforce sensor setup

    //gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");
    gforce_log_enable=tab102b_enable;
#if 0
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
#else
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
#endif
    
    usewatchdog = dvrconfig.getvalueint("io", "usewatchdog");
    watchdogtimeout=dvrconfig.getvalueint("io", "watchdogtimeout");
    if( watchdogtimeout<10 )
        watchdogtimeout=10 ;
    if( watchdogtimeout>200 )
        watchdogtimeout=200 ;
    watchdogtimeset=watchdogtimeout;
    
	battery_low = 7.4 ;
    v = dvrconfig.getvalue( "system", "battery_low");
    if( !v.isempty() ) {
		sscanf( (char *)v, "%f", &battery_low );
	}
	if( battery_low < 7.2 ) battery_low = 7.2 ;
    
    // initialize timer
    initruntime();
    p_dio_mmap->fileclosed=0;
    p_dio_mmap->gforce_changed=0;  
    return;
}

// app finish, clean up
void appfinish()
{
    int lastuser ;
    
    // close serial port
    mcu_finish();
    
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

void FormatFlashToFAT32()
{  
    char path[128];
    char devname[60];
    char syscommand[80];
    int ret=0;
    pid_t format_id;
    char* ptemp;
    FILE *fp;
    
    fp=fopen("/var/dvr/flashcopydone","r");
    if(!fp)
      return;
    fclose(fp);
    fp= fopen("/var/dvr/flashdiskdir", "r"); 
    if(!fp){
       return; 
    }
    if (fscanf(fp, "%s", path) != 1){
       fclose(fp);    
       return;           
    } 
    fclose(fp);
    ret=umount2(path,MNT_FORCE);
    if(ret<0)
      return;    
    dvr_log("umounted:%s",path);
    remove(path);
    
    ptemp=&path[17];
    sprintf(devname,"/dev/%s",ptemp);  
    
    format_id=fork();
    if(format_id==0){     
       execlp("/mnt/nand/mkdosfs", "mkdosfs", "-F","32", devname, NULL );  
       exit(0);
    } else if(format_id>0){    
        int status=-1;
        waitpid( format_id, &status, 0 ) ;   
        if( WIFEXITED(status) && WEXITSTATUS(status)==0 ) {
            ret=1 ;
        }	
    }    
    if(ret>=0){
#if 0      
	dvr_log("write diskid to flash");
	mkdir("/mnt/disk",0777);
	ret=mount(devname,"/mnt/disk","vfat",MS_MGC_VAL,"");
	fp=fopen("/mnt/disk/diskid","w");
	if(fp){
	  fprintf(fp,"FLASH");
	  fclose(fp);
	  dvr_log("%s is formated to FAT32",devname);	  
	} else {
	   dvr_log("Format Flash Failed to write diskid"); 
	}
#else
        dvr_log("%s is formated to FAT32",devname);	
	remove("/var/dvr/backupdisk");
#endif
    } else {
       dvr_log("Format Flash failed"); 
    }
}


int isHBDRecording()
{
    FILE *fp;    
    fp = fopen("/var/dvr/backupdisk", "r"); 
    if(!fp){
       return 0; 
    } 
    fclose(fp);
    return 1;
}

void mcu_event_ocur()
{
    dvr_log("mcu_event_ocur sent to MCU");
    if( mcu_cmd( 0x43, 1, 1 ) ) {
      gevent_on=0;
	}
	else {
     dvr_log("mcu_event_remove code:43");     
	}
}

void mcu_event_remove()
{
    dvr_log("mcu_event_remove sent to MCU");
    if( mcu_cmd( 0x43, 1, 0 ) ) {
      gevent_on=0;
	}
	else {
     dvr_log("mcu_event_remove code:43");     
	}
}

int recordingDiskReady()
{
    FILE *fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (fp != NULL) {
      fclose(fp);
      return 1;
    } 
    return 0;
}

#define EXTERNAL_TAB101

int main(int argc, char * argv[])
{
    int i;
    int hdpower=0 ;                             // assume HD power is off
    int hdkeybounce = 0 ;                       // assume no hd
    unsigned int gpsavailable = 0 ;
    unsigned int smartftp_endtime = 0;
    unsigned int mstarttime;
    unsigned int gps_check=0;
    unsigned int gps_startcheck=0;
    unsigned int gps_started=0;
    unsigned int mcustart=0;
    
    //int enable_Tab102=0;
    int hdtemp1=0,hdtemp2=0;
    if ((argc >= 2) &&
	(!strcmp(argv[1], "-f") ||      /* force foreground */
	 !strcmp(argv[1], "-fw") ||     /* MCU upgrade */
	 !strcmp(argv[1], "-reboot"))) { /* system reset */
    } else {
#ifdef DAEMON
      if (daemon(0, 0) < 0) {
	perror("daemon");
	exit(1);
      }
#endif
    }
  //  printf("this is in daemon mode\n");
    if( appinit()==0 ) {
      //  system("/bin/reboot"); 
        return 1;
    }

//    signal(SIGUSR1, sig_handler);
//    signal(SIGUSR2, sig_handler);
    
	mcu_watchdogenable() ;    
    mcu_watchdogkick() ;// kick watchdog

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
                usewatchdog=1;
                mcu_watchdogenable() ;
                sleep(delay+20) ;
                mcu_reboot ();
                return 1;
            }
            else if( strcasecmp( argv[i], "-fwreset" )==0 ) {
	               mcu_reset();
                return 0;
            }            
        }
    }
  
    runtime = mcustart = mstarttime = getruntime() ;
    set_app_mode(APPMODE_RUN,0) ;
    
    // setup signal handler	
    signal(SIGQUIT, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR2, sig_handler);
    signal(SIGPIPE, SIG_IGN );
    
    printf( "DVR IO process started!\n");
   
    // get default digital input
    mcu_dinput();

    // initialize output
    doutput_init() ;
    
    // initialize panel led
    panelled = 0xff ;
    p_dio_mmap->panel_led = 0;
    
    // initialize device power
    devicepower = 0 ;
    p_dio_mmap->devicepower = 0xffff;	// assume all device is power on
 //   mcu_wifipoweroff();
    if(wifi_enable_ex){  
        if(wifi_poweron) {
          mcu_poepoweron(); 
	} else {
	  mcu_poepoweroff();
	} 
	mcu_wifipoweroff();
    } else {
	if(wifi_poweron){
	  mcu_wifipoweron();
	}
	else {
	  mcu_wifipoweroff();
	}
	mcu_poepoweroff();
    }

    p_dio_mmap->tab102_ready=0;
    p_dio_mmap->tab102_isLive=0;
    p_dio_mmap->synctimestart=0;
    p_dio_mmap->gps_connection=0;
    
    while( app_mode ) {

        // do input pin polling
        mcu_input(5000);      
		
#ifdef PWII_APP   
		mcu_pwii_output();
#endif

        // do digital output
        doutput();
        
        // check input status (PWZ8)
        input_check();
  
        runtime=getruntime() ;
		if( app_mode==APPMODE_RUN ){	
			if(gps_started==0){
				if(runtime-mstarttime>30000){
					system("/mnt/nand/dvr/glog");
					gps_started=1;
					gps_startcheck=runtime;
					dvr_log("gps started");
				}	      
			} 
			else {
				if(gps_check==0){	    
					if(runtime-gps_startcheck>60000){   //1minutes
						if(!p_dio_mmap->gps_valid){
							system("/mnt/nand/dvr/glog");
							dvr_log("gps first restarted");
							p_dio_mmap->gps_connection=0;
						}
						gps_check=1;
					}
				} 
				else if(gps_check==1){
					if(runtime-gps_startcheck>180000){ //2minutes	     
						if((!p_dio_mmap->gps_connection)&&(!p_dio_mmap->gps_valid)){
							system("/mnt/nand/dvr/glog");
							dvr_log("gps second restarted");
						}
						gps_check=2;
					}	  
				}	      	      	      
			}
		}
	
#ifdef EXTERNAL_TAB101	
		if(p_dio_mmap->tab102_ready==0){ 
			if((p_dio_mmap->dvrstatus & DVR_RUN)!=0){ 
				if(runtime-mstarttime>20000){ 
					tab102_ready(); 
					p_dio_mmap->tab102_ready=1; 
				} 
			}
		}	
#else	
		if(gforcechange>0){
			if(runtime-gforce_start>5000){
				gforcechange=0;
				p_dio_mmap->gforce_changed=0;
				p_dio_mmap->gforce_forward_d=0.0;
				p_dio_mmap->gforce_down_d=0.0;
				p_dio_mmap->gforce_right_d=0.0;
			}
		}
	
        if(p_dio_mmap->tab102_ready==0){
			// printf("tab102_0\n");
			if( p_dio_mmap->dvrpid>0 && 
              (p_dio_mmap->dvrstatus & DVR_RUN) )
			{
				// printf("tab102_1\n");
				if(recordingDiskReady()){
					//  printf("tab102b_2\n");
					if (gforce_log_enable){
						//mcu_watchdogdisable();
						// kick watchdog
						mcu_watchdogkick () ;			
						try{
						  if(Tab102b_setup()<0){
							p_dio_mmap->tab102_isLive=0; 
						  } else {
							p_dio_mmap->tab102_isLive=1; 
						  }
						}
						catch(...){
						   p_dio_mmap->tab102_isLive=0;
						   dvr_log("Tab101 set up exception is captured");
						}
						//watchdogtimeout=watchdogtimeset;
								   // mcu_watchdogenable();
					}    
					p_dio_mmap->tab102_ready=1;
				}
			}
        }	
#endif        
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
				// dvr_log( "HD power off.");
				mcu_hdpoweroff();
				p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
            }
            else if( p_dio_mmap->mcu_cmd == 2 ) {
				//	dvr_log( "HD power on.");
             	mcu_hdpoweron();
				p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
            } 
            else if(p_dio_mmap->mcu_cmd == 5){
                dvr_log( "MCU reboot.");
                p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
                mcu_reboot ();
				exit(1);
            } else if(p_dio_mmap->mcu_cmd==7){
				// dvr_log("Lock event happens") ;
				p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
				if(!gevent_on){		  
					mcu_event_ocur();
				}
			}
			else if(p_dio_mmap->mcu_cmd==8){
				// dvr_log("Lock event removed") ;
				p_dio_mmap->mcu_cmd  = 0 ;	// cmd finish
				//  if(gevent_on)
				mcu_event_remove();
			}
			else if(p_dio_mmap->mcu_cmd==10){
				//  dvr_log("flash flash led");
				p_dio_mmap->mcu_cmd=0;
                p_dio_mmap->outputmap |= FLASHLED ;   
            } else if(p_dio_mmap->mcu_cmd==11){
                p_dio_mmap->outputmap &= ~FLASHLED;
                p_dio_mmap->mcu_cmd=0;
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
						set_app_mode(APPMODE_REBOOT, 0) ;
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

            if( i > -127 && i < 127 ) {
                static int saveiotemp = 0 ;
                if( i>50 && 
                   (i-saveiotemp) > 1 ) 
                {
                    dvr_log ( "system temperature: %d", i);
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

                if( hdtemp1>50 && 
                   (hdtemp1-savehdtemp) > 1 ) 
                {
                    dvr_log ( "hard disk1 temperature: %d", hdtemp1);
                    savehdtemp=hdtemp1 ;
                    sync();
                }

                p_dio_mmap->hdtemperature1 = hdtemp1 ;
            }
            else {
                p_dio_mmap->hdtemperature1=-128 ;
            }

            if(p_dio_mmap->hdtemperature1<0){
                mTempState=1;
  
            } else if(p_dio_mmap->hdtemperature1>10){
                mTempState=3; 
            } else {
                mTempState=2;
            }
            if(mTempState==mTempPreState)
                mStateNum++;
            else
                mStateNum=0;
            // The Buzzer beep 4time at 0.5s interval every 30s when HDD or system temperature is over 66C
            if( p_dio_mmap->iotemperature > 66 || p_dio_mmap->hdtemperature1 > 66 ||p_dio_mmap->hdtemperature2 > 66) {
                static int hitempbeep=0 ;
                if( ++hitempbeep>3 ) {
                    buzzer( 4, 1000, 500 ) ;
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
        
        // battery check timer
        static unsigned int battery_timer ;
        if( (runtime - battery_timer)> 15000 ) {    // 15 seconds battery check interval
			battery_timer = runtime ;
			battery_check();
		}
                
        static unsigned int appmode_timer ;
        if( (runtime - appmode_timer)> 3000 ) {    // 3 seconds mode timer
            appmode_timer=runtime ;

            // printf("mode %d\n", app_mode);
            // do power management mode switching
            if( app_mode==APPMODE_RUN ) {        // running mode
                static int app_run_bell = 0 ;
                if( app_run_bell==0 &&
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrstatus & DVR_RECORD) ) 
                {
                    app_run_bell=1 ;
                    buzzer(1, 1000, 500);
                }
                
				if(!gHBDChecked){
					if(recordingDiskReady()){		  
						gHBDRecording=isHBDRecording();
						if(gHBDRecording){
							mcu_wifipoweroff();		      
						}
						gHBDChecked=1;
					}
                }                
                
                if( p_dio_mmap->poweroff )       // ignition off detected
                {		  
					gHBDRecording=isHBDRecording();
                    //turn on wifi power
                    if(!gHBDRecording){		  		       
						if(wifi_enable_ex){	
							mcu_poepoweron();
						}
						else {
							mcu_wifipoweron();
						}	
					} else {
						if(wifi_enable_ex){	
							mcu_poepoweron();
						}		      
					}		  
		  
                    set_app_mode(APPMODE_SHUTDOWNDELAY, getshutdowndelaytime() ) ;  // hutdowndelay start ;
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
                //        if( p_dio_mmap->glogpid>0 ) {
                //            kill( p_dio_mmap->glogpid, SIGUSR1 );
                 //       }
                        set_app_mode(APPMODE_NORECORD, 60) ;    // start standby mode
                        dvr_log("Shutdown delay timeout, to stop recording (mode %d).", app_mode);
                    }
                    
                }
                else {
                    p_dio_mmap->devicepower = 0xffff ;
                    set_app_mode(APPMODE_RUN, 0) ;   // back to normal
					
					battery_drop_report = 0 ;		// to enable battery status report, (this is like cheating but to fix problem)
					
                    dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);
                    //turn off wifi power
                    if(!wifi_poweron) {
                        mcu_wifipoweroff();
			mcu_poepoweroff();
		    }
                }
            }
            else if( app_mode==APPMODE_NORECORD ) {  
                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();
                   // close recording and run smart ftp
		    if( p_dio_mmap->dvrpid>0 && 
		      (p_dio_mmap->dvrstatus & DVR_RUN) &&
		      ((p_dio_mmap->dvrstatus & DVR_RECORD)==0) ) 
		    {			    
			    //p_dio_mmap->dvrcmd = 2; // disable liveview/playback
			    if(p_dio_mmap->fileclosed){
			      dvr_log("Recording stopped.");
			      modeendtime = runtime ;
			    }        
		    }
		    sync();
 
		    if( runtime>=modeendtime) {
#if 1		        
			if( p_dio_mmap->glogpid > 0 ) {
			    kill(p_dio_mmap->glogpid, SIGTERM);
			}			
			if(gHBDRecording){
			    p_dio_mmap->devicepower=(p_dio_mmap->devicepower&0xffee);  
			}
#endif			

#ifdef EXTERNAL_TAB101
			
			tab102_start();
			p_dio_mmap->nodiskcheck = 1;
			mcu_watchdogkick();
#else
			// start Tab102 downloading			
			if (gforce_log_enable) {			  
			    
			     // p_dio_mmap->tab102start=1;			
                              if(p_dio_mmap->tab102_isLive){	
				  mcu_watchdogdisable();
				  mcu_poweroffdelay_N (600);				
			          Tab102b_ContinousDownload();
				  watchdogtimeout=watchdogtimeset;
                                  mcu_watchdogenable(watchdogtimeout);				 
			      }			      
			      //p_dio_mmap->tab102start=0;			
			}
			p_dio_mmap->nodiskcheck = 1;
			
			if(gHBDRecording){
			    mcu_poweroffdelay_N (100);			
			    tab102_start();			
			    mcu_watchdogkick();	
			    modeendtime = runtime + 600000 ;			    
			} else {
			   modeendtime = runtime + 10000 ;	
			}	
#endif			
			set_app_mode(APPMODE_PRESTANDBY, 600) ;
			dvr_log("Enter Pre-standby mode. (mode %d).", app_mode);
		    }
		}
                else {
#if 0		  
                    // start dvr recording
                    p_dio_mmap->devicepower = 0xffff ;
                    set_app_mode(APPMODE_RUN) ;   // back to normal
                    dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);
		    if(!wifi_poweron) {
                      mcu_wifipoweroff(); 
		      mcu_poepoweroff();
		    }
                    if( p_dio_mmap->dvrpid>0 && 
                       (p_dio_mmap->dvrstatus & DVR_RUN) &&
                       (p_dio_mmap->dvrcmd == 0 )) 
                    {
                        p_dio_mmap->dvrcmd = 4;     // start recording
                    }
#else
                    dvr_log("Power on switch after shutdown delay. System reboot");
                    sync();
                    mcu_setwatchdogtime(10);
                    mcu_reboot ();
		    exit(1);
#endif                    
		}
            }
            else if( app_mode==APPMODE_PRESTANDBY ) {  // get Tab102 data

                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();
		    
		    // check if tab102 downloading ended
#ifdef EXTERNAL_TAB101
                   if(gHBDRecording){
			if(p_dio_mmap->dvrstatus & DVR_NETWORK)
			{
			   p_dio_mmap->devicepower=0xfffe ;
			  // printf("turn on camera\n");
			} else {
			   //printf("turn off camera\n");
			    p_dio_mmap->devicepower=0xffee ;
			}		      
		    }
		    
		    // check if tab102 downloading ended
		    if (tab102_wait()) {
		      //check_tab102_data();
			modeendtime = runtime ;        
		    //  p_dio_mmap->nodiskcheck = 0;
		    }
#else
		    if(gHBDRecording){
			if (tab102_wait()) {
			    //check_tab102_data();
			      modeendtime = runtime ;        
			}
			if(p_dio_mmap->dvrstatus & DVR_NETWORK)
			{
			   p_dio_mmap->devicepower=0xfffe ;
			  // printf("turn on camera\n");
			} else {
			   //printf("turn off camera\n");
			    p_dio_mmap->devicepower=0xffee ;
			}
		    }	      		    
#endif		     
		    if( runtime>=modeendtime) {
#if 1		      
			  if(gHBDRecording&&p_dio_mmap->dvrpid>0){
			      if(p_dio_mmap->ishybrid_copy==0){
				  mcu_poweroffdelay_N(120);		
				  FormatFlashToFAT32();	
				  if(!wifi_enable_ex){
				     mcu_wifipoweron();
				  }
				  set_app_mode(APPMODE_STANDBY, getstandbytime()) ;
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
				     // p_dio_mmap->ftpstart=1;
				  }
				  dvr_log("Enter standby mode. (mode %d).", app_mode);
				  sync();
				  buzzer( 3, 1000, 500);
			      }
			  } else {
			      if(gHBDRecording){
				if(!wifi_enable_ex){
				   mcu_wifipoweron();
				}
			      }
			      set_app_mode(APPMODE_STANDBY, getstandbytime() ) ;
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
				 // p_dio_mmap->ftpstart=1;
			      }
			      dvr_log("Enter standby mode. (mode %d).", app_mode);
			      sync();
			      buzzer( 3, 1000, 500);						
			  }	
#else
			  set_app_mode(APPMODE_STANDBY, getstandbytime()) ;
#endif			  
		    }// if( runtime>=modeendtime) 
		}
                else {   
#if 0		  
		    if(tab102_wait()){
			p_dio_mmap->tab102_ready=0;	
			
			mstarttime=runtime;
			p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
			set_app_mode(APPMODE_RUN, 0) ;          // back to normal
			dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);	    
			if( pid_tab102 > 0 ) {
			    tab102_kill();
			}
			
			tab102_reset();
			
			if( p_dio_mmap->dvrpid>0 && 
			  (p_dio_mmap->dvrstatus & DVR_RUN) &&
			  p_dio_mmap->dvrcmd == 0 )
			{
			    p_dio_mmap->dvrcmd = 4 ;             // start recording.
			    p_dio_mmap->nodiskcheck = 0;
			}
			if(!wifi_poweron) {
			   mcu_wifipoweroff();
			   mcu_poepoweroff();
			}		
		    } else {
		        tab102_kill();
		    }
#else
                    p_dio_mmap->nodiskcheck = 0;
		    sync();
		   // tab102_kill();		    
                    dvr_log("Power on switch after pre_standby. System reboot");
		    sync();
		    // just in case...
		    mcu_setwatchdogtime(10);                   // 10 seconds watchdog time out
                    mcu_reboot ();
		    exit(1);
#endif		    
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
		    if(wifi_enable_ex==0){
		      
		      if (smartftp_ended) {
			  p_dio_mmap->devicepower=DEVICEOFF ;     
			// turn off all devices power
		      } else {
			 //turn camera power off
			 // mcu_devicepower(4,0);
			 p_dio_mmap->devicepower=(p_dio_mmap->devicepower&0xffef);
		      }
		    } else {
		       p_dio_mmap->devicepower=DEVICEOFF ;  
		    }
                }
                if( p_dio_mmap->poweroff ) {         // ignition off
				  if (smartftp_ended) {
							// turn off HD power ?
#if 0		    
							if( standbyhdoff ) {
								  p_dio_mmap->outputmap &= ~HDLED ;   // turn off HDLED
								if( hdpower ) {
									hdpower=0;
									mcu_hdpoweroff ();
								}
							}
#endif                    
							if( runtime>modeendtime  ) {
								// start shutdown
								p_dio_mmap->devicepower=DEVICEOFF ;    // turn off all devices power
								modeendtime = runtime+10000;//90000 ;
								set_app_mode(APPMODE_SHUTDOWN,0) ;    // turn off mode
								dvr_log("Standby timeout, system shutdown. (mode %d).", app_mode );
								buzzer( 5, 1000, 500 );

							 //   umount_recdisk();
					if(p_dio_mmap->dvrpid>0){
						kill(p_dio_mmap->dvrpid, SIGTERM );
					}
					
								if( p_dio_mmap->glogpid > 0 ) {
									kill(p_dio_mmap->glogpid, SIGTERM);
								}
								
								if( pid_smartftp > 0 ) {
									smartftp_kill();
								}
								sync();
								system("/mnt/nand/dvr/umountdisks") ;
							}
				  }
                }
                else {                                          // ignition on
#if RESET_ON_IGNON	
			dvr_log("Power on switch after standby. System reboot");
			sync();		
	
			if( pid_smartftp > 0 ) {
			    smartftp_kill();
			}
			sleep(10);
			sync();
			system("/mnt/nand/dvr/umountdisks") ;
			// just in case...
			mcu_setwatchdogtime(10);               // 10 seconds watchdog time out
			sleep(5);
			mcu_reboot ();
			exit(1);
#else
             
                   // we should start glog, since it'v been killed.
            //      if( p_dio_mmap->glogpid > 0 ) {
           //             kill(p_dio_mmap->glogpid, SIGUSR2);
            //      }
			if(tab102_wait()&&smartftp_wait()){
				p_dio_mmap->tab102_ready=0;    
				mstarttime=runtime;
				p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
				set_app_mode(APPMODE_RUN,0) ;          // back to normal
				dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);

				tab102_reset();

				if( p_dio_mmap->dvrpid>0 && 
					(p_dio_mmap->dvrstatus & DVR_RUN) &&
					p_dio_mmap->dvrcmd == 0 )
				{
					p_dio_mmap->dvrcmd = 4 ;             // start recording.
					p_dio_mmap->nodiskcheck = 0;
				}  
				if(!wifi_poweron)
					mcu_wifipoweroff();
		    } 
		    else {
				if( pid_tab102 > 0 ) {
					tab102_kill();			
				}	
				if( pid_smartftp > 0 ) {
					smartftp_kill();
				}			
		    }
#endif
                }
            }
            else if( app_mode==APPMODE_SHUTDOWN ) {// turn off mode, no keep power on 
                sync();
                if( runtime>modeendtime ) {     // system suppose to turn off during these time
                    // hard ware turn off failed. May be ignition turn on again? Reboot the system
                //    dvr_log("Hardware shutdown failed. Try reboot by software!" );
                //    set_app_mode(APPMODE_REBOOT) ;
                mcu_setwatchdogtime(10);            // 10 seconds watchdog time out
				 mcu_reboot();
				 exit(1);
				 set_app_mode(APPMODE_QUIT,0) ;                 // quit IOPROCESS
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
		    exit(1);
                   // system("/bin/reboot");                  // do software reboot
                    set_app_mode(APPMODE_QUIT,0) ;                 // quit IOPROCESS
                }
            }
            else if( app_mode==APPMODE_REINITAPP ) {
	      
		//printf("apply again\n");
		if(p_dio_mmap->poweroff){	    
                 //   printf("apply called\n");
		    reloadconfig();
		    if(wifi_enable_ex){
		       mcu_poepoweron();
		       mcu_wifipoweroff();
		    } else {
		       if(!gHBDRecording)
		         mcu_wifipoweron();
		       mcu_poepoweroff();
		    }
		    p_dio_mmap->poweroff=1;
		    p_dio_mmap->tab102_ready=1;
		    set_app_mode(app_mode_r, 0) ;
		} else {
                    dvr_log("IO re-initialize.");		  
		   // appfinish();
		    //appinit();
		    re_appinit();
		   // tab102_setup();
		       	   
		    if(p_dio_mmap->tab102_isLive){
		      // tab102_settrigger();
		      Tab102b_setTrigger();	
		    }
		    mcu_wifipoweroff();
		    mcu_poepoweroff();
		    if(wifi_poweron) {
		      sleep(1);
		      if(wifi_enable_ex){
			mcu_poepoweron();
		      } else {
			if(!gHBDRecording)
			   mcu_wifipoweron();
		      }
		    }
		    set_app_mode(APPMODE_RUN,0) ;
		}			      
            }
            else {
                // no good, enter undefined mode 
                dvr_log("Error! Enter undefined mode %d.", app_mode );
                set_app_mode(APPMODE_RUN,0) ;              // we retry from mode 1
            }

            // updating watch dog state
            if( usewatchdog && watchdogenabled==0 ) {
				mcu_watchdogenable();
				watchdogenabled = 1 ;
            }

            // kick watchdog
            if( watchdogenabled )
                mcu_watchdogkick () ;

            // DVRSVR watchdog running?	    
            if(p_dio_mmap->dvrwatchdog >= 0)
            {      	
	          if(app_mode < APPMODE_STANDBY){
				++(p_dio_mmap->dvrwatchdog) ;
				if( p_dio_mmap->dvrwatchdog == 50 ) {  // DVRSVR dead for 2.5 min?		  
					if( kill( p_dio_mmap->dvrpid, SIGUSR2 )!=0 ) {
						// dvrsvr may be dead already, pid not exist.
						p_dio_mmap->dvrwatchdog = 110 ;
					}
					else {
						dvr_log( "Dvr watchdog failed, try restart DVR!!!");
					}                  
				}
				else if( p_dio_mmap->dvrwatchdog > 110 ) {	
					buzzer( 10, 1000, 500 );
					dvr_log( "Dvr watchdog failed, system reboot.");
					set_app_mode(APPMODE_REBOOT,5) ;
					p_dio_mmap->dvrwatchdog=1 ;
				}
			  }
            }
            
	    static int nodatawatchdog = 0;
	    if( (p_dio_mmap->dvrstatus & DVR_RUN) && 
	        (p_dio_mmap->dvrstatus & DVR_NETWORK)==0 &&
			(p_dio_mmap->dvrstatus & DVR_NODATA ) &&    // some camera not data in
			app_mode==1 )
	    {
			if( ++nodatawatchdog > 40 ) { //2 minutes   // 1 minute
				buzzer( 10, 1000, 500 );
				// let system reset by watchdog
				dvr_log( "One or more camera not working, system reset.");
				set_app_mode(APPMODE_REBOOT,5) ;
			}
	    }
	    else 
	    {
			nodatawatchdog = 0 ;
	    }
	    
	    static int norecordwatchdog=0;
	    if( (p_dio_mmap->dvrstatus & DVR_RUN) &&
			(p_dio_mmap->dvrstatus & DVR_DISKREADY) &&
			(p_dio_mmap->dvrstatus & DVR_RECORD)==0 &&
			(p_dio_mmap->dvrstatus & DVR_NETWORK)==0 &&
			app_mode==APPMODE_RUN  && gRecordMode==0)
		{
	      
			if(++norecordwatchdog>40){            //2 minutes
			  if( kill( p_dio_mmap->dvrpid, SIGUSR2 )!=0 ) {
				  dvr_log("No Record, system reboot");
				  sync();
				  mcu_setwatchdogtime(10);
				  mcu_reboot ();
				  exit(0);
			  } else {
				  dvr_log("no recording, restart DVR"); 
			  }
			}
	    }
	    else {
	      norecordwatchdog=0;
	    }	    			            
        }

        static unsigned int hd_timer ;
        if( (runtime - hd_timer)> 500 ) {    // 0.5 seconds to check hard drive
			hd_timer=runtime ;
			//   printf("hdkeybounce:%d\n",hdkeybounce);
			// check HD plug-in state
			if( hdlock && hdkeybounce<4 ) {       //10     // HD lock on
#if 0
                if( hdpower==0 ) {
                    // turn on HD power
                    mcu_hdpoweron() ;
                }
#endif
				hdkeybounce = 0 ;
               
		//dvr_log("hdinserted:%d",hdinserted);
				if( hdinserted &&                      // hard drive inserted
					!pid_tab102 && // tab102 is not uploading data
                    p_dio_mmap->dvrpid > 0 &&
                    p_dio_mmap->dvrwatchdog >= 0 &&
                    (p_dio_mmap->dvrstatus & DVR_RUN) &&
                    (p_dio_mmap->dvrstatus & DVR_DISKREADY )==0 )        
                    // dvrsvr is running but no disk detected
				{
					if(app_mode < APPMODE_NORECORD){
						p_dio_mmap->outputmap ^= HDLED ;
			
						if( ++hdpower>2400)  
						{
							dvr_log("Hard drive failed, mcu reboot!");
							buzzer( 10, 1000, 500 );

							// turn off HD led
							p_dio_mmap->outputmap &= ~HDLED ;;

							mcu_setwatchdogtime(10);		       // 10 seconds watchdog time out
							mcu_reboot ();
							exit(1);
						} 	
#if 0			
						else {
							if(mTempState==3)
								system("/mnt/nand/dvr/tdevmount /mnt/nand/dvr/tdevhotplug");
						}	
#endif			
					} 		    
					else {
						hdpower=1;
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
                hdkeybounce++ ;		
                p_dio_mmap->outputmap ^= HDLED ;      // flash led			
                if( hdkeybounce<4 ) {        // wait for key lock stable  //10
                    hd_dvrpid_save=0 ;
		 //  dvr_log("key lock off");	    
                }
                else if( hdkeybounce==4 ){                 //10
                   // hdkeybounce++ ;
                    // suspend DVRSVR process
                    if( p_dio_mmap->dvrpid>0 ) {
                        hd_dvrpid_save=p_dio_mmap->dvrpid ;
                        kill(hd_dvrpid_save, SIGTERM);  // request dvrsvr to turn down
                        // turn on HD LED
                     //   p_dio_mmap->outputmap |= HDLED ;
			//dvr_log("turn down dvr");
			//p_dio_mmap->outputmap ^= HDLED ;      // flash led	
                    }
                    // p_dio_mmap->outputmap ^= HDLED ;      // flash led	
                } 
                else if( hdkeybounce<40 ) {                  //11
                    if( p_dio_mmap->dvrpid <= 0 )  {
                      //  hdkeybounce++ ;
                        sync();
                        // umount disks
                        system("/mnt/nand/dvr/umountdisks") ;
			dvr_log("umount disks");
			hdkeybounce=40;
                    }
                }
                else if( hdkeybounce==41 ) {               //12
                    // do turn off hd power
		     sync();	
		     mcu_hdpoweroff();       // FOR NEW MCU, THIS WOULD TURN OFF SYSTEM POWER?!
                }
                else if( hdkeybounce>=42) {
		     mcu_reboot ();
		     exit(0);
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
                    //panled|=4 ;
		     p_dio_mmap->outputmap|=0x04;
                    if( videolostbell==0 ) {
                        buzzer( 5, 1000, 500 );
                        videolostbell=1;
                    }
                }
                else {
                    videolostbell=0 ;
		    p_dio_mmap->outputmap&=~0x04;
                }
            }
            
            if( p_dio_mmap->dvrpid>0 && (p_dio_mmap->dvrstatus & DVR_NODATA)!=0 && app_mode==APPMODE_RUN ) {
               // panled|=2 ;     // error led
	        p_dio_mmap->outputmap|=0x08;
            } else {
	        p_dio_mmap->outputmap&=~0x08;
	    }
#if 0       
            // update panel LEDs
            if( panelled != panled ) {
                for( i=0; i<PANELLEDNUM; i++ ) {
                    if( (panelled ^ panled) & (1<<i) ) {
                        mcu_led(i, panled & (1<<i) );
                    }
                }
                panelled = panled ;
            }
#endif            
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
		    if(!p_dio_mmap->synctimestart){
			time_syncmcu();
			adjtime_timer=runtime ;
		    }
                }
            }
        }
        mTempPreState=mTempState;
    }
    
    if( watchdogenabled ) {
        mcu_watchdogdisable();
    }
    
    appfinish();
    
    printf( "DVR IO process ended!\n");
    return 0;
}

