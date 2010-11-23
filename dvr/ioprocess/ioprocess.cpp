#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <sys/stat.h>

#include "../cfg.h"

#include "../dvrsvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "netdbg.h"
#include "mcu.h"
#include "gforce.h"
#include "diomap.h"


// options
int standbyhdoff = 0 ;
int usewatchdog = 0 ;
int hd_timeout = 120 ;       // hard drive ready timeout
unsigned int runtime ;

// HARD DRIVE LED and STATUS
#define HDLED	(0x10)

#define RECVBUFSIZE (50)

// input pin maping
#ifdef MDVR_APP
#define MAXINPUT    (12)
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)
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
    (1<<15)     // bit 11
} ;

#endif  // MDVR_APP

#ifdef TVS_APP
#define MAXINPUT    (12)
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)
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
    (1<<15)     // bit 11
} ;

#endif  // TVS_APP

#ifdef PWII_APP
#define MAXINPUT    (12)
#define HDINSERTED	(1<<6)
#define HDKEYLOCK	(1<<7)
#define PWII_MIC    (1<<11)
unsigned int input_map_table [MAXINPUT] = {
    1,          // bit 0
    (1<<1),     // bit 1
    (1<<2),     // bit 2
    (1<<3),     // bit 3
    (1<<4),     // bit 4
    (1<<5),     // bit 5
    (1<<10),    // bit 6
    (1<<8),     // bit 7
    (1<<12),    // bit 8
    (1<<13),    // bit 9
    (1<<14),    // bit 10
    (1<<15),    // bit 11
} ;

#endif  // PWII_APP


int hdlock=0 ;								// HD lock status
int hdinserted=0 ;

#define PANELLEDNUM (3)
#define DEVICEPOWERNUM (5)

char dvriomap[100] = "/var/dvr/dvriomap" ;
char * pidfile = "/var/dvr/ioprocess.pid" ;

int shutdowndelaytime ;
int wifidetecttime ;
int uploadingtime ;
int archivetime ;
int standbytime ;

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;
char logfile[]="/var/dvr/dvrlogfile";
char temp_logfile[128] ;
int watchdogenabled=0 ;
int watchdogtimeout=30 ;
int gpsvalid = 0 ;

#ifdef PWII_APP
int pwii_tracemark_inverted=0 ;
int pwii_rear_ch ;
int pwii_front_ch ;
#endif

unsigned int panelled=0 ;
unsigned int devicepower=0xffff;

int wifi_on = 0 ;

pid_t   pid_smartftp = 0 ;

void sig_handler(int signum)
{
    if( signum==SIGUSR2 ) {
        p_dio_mmap->iomode=IOMODE_REINITAPP ;
    }
    else {
        p_dio_mmap->iomode=IOMODE_QUIT;
    }
}

// write log to file.
// return 
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(char *fmt, ...)
{
    int res = 0 ;
    char lbuf[512];
    FILE *flog;
    time_t ti;
    int l;
    va_list ap ;
    struct stat st ;

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
    vsprintf(&lbuf[l], fmt, ap );
    va_end( ap );

    // send log to netdbg server
    netdbg_print("%s\n", lbuf);

    // check logfile, it must be a symlink to hard disk
    if( stat(logfile, &st)==0 ) {
        if( S_ISLNK(st.st_mode) ) {
            flog = fopen(logfile, "a");
            if (flog) {
                fprintf(flog, "%s\n", lbuf);
                if( fclose(flog)==0 ) {
                    res=1 ;
                }
            }
        }
    }
    
    if( res==0 ) {
        flog = fopen(temp_logfile, "a");
        if (flog) {
            fprintf(flog, "%s *\n", lbuf);
            if( fclose(flog)==0 ) {
                res=1 ;
            }
        }
    }

    return res ;
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

#ifdef PWII_APP

int  zoomcam_enable = 0 ;
char zoomcam_dev[20] = "/dev/ttyUSB0" ;
int  zoomcam_baud = 115200 ;
int  zoomcam_handle = 0 ;

void zoomcam_open()
{
    if( zoomcam_enable ) {
        zoomcam_handle = serial_open( zoomcam_dev, zoomcam_baud );
    }
    else {
        zoomcam_handle = 0 ;
    }
}

void zoomcam_close()
{
    if( zoomcam_handle>0 ) {
        close( zoomcam_handle );
        zoomcam_handle = 0 ;
    }
}

void zoomcam_prep()
{
    if( zoomcam_handle<=0 ) {
        zoomcam_open();
    }
    if( zoomcam_handle>0 ) {
        char r ;
        while( serial_dataready(zoomcam_handle, 0, NULL) ) {
            read( zoomcam_handle, &r, 1 ) ;
        }
    }
}

void zoomcam_zoomin()
{
    static char cmd_zoomin[8]="\x06\x0f\x00\x04\x02\x00\x00" ;
    zoomcam_prep();
    if( zoomcam_handle>0 ) {
        mcu_calchecksum( cmd_zoomin ) ;
        write( zoomcam_handle, cmd_zoomin, (int)cmd_zoomin[0] ) ;
    }
}

void zoomcam_zoomout()
{
    static char cmd_zoomout[8]="\x06\x0f\x00\x05\x02\x00\x00" ;
    zoomcam_prep();
    if( zoomcam_handle>0 ) {
        mcu_calchecksum( cmd_zoomout ) ;
        write( zoomcam_handle, cmd_zoomout, (int)cmd_zoomout[0] ) ;
    }
}

void zoomcam_led(int on)
{
    static char cmd_zoomled[8]="\x07\x0f\x00\x06\x02\x00\x00" ;
    zoomcam_prep();
    if( zoomcam_handle>0 ) {
        if( on ) {
            cmd_zoomled[5] = 1 ;
        }
        else {
            cmd_zoomled[5] = 0 ;
        }
        mcu_calchecksum( cmd_zoomled ) ;
        write( zoomcam_handle, cmd_zoomled, (int)cmd_zoomled[0] ) ;
    }
}

#endif

#ifdef PWII_APP

// Set C1/C2 LED and set external mic
void mcu_pwii_setc1c2() 
{
   // C1 LED (front)
    dio_lock();
    if( p_dio_mmap->camera_status[pwii_front_ch] & 2 ) {         // front ca
        if( (p_dio_mmap->pwii_output & PWII_LED_C1 ) == 0 ) {
            p_dio_mmap->pwii_output |= PWII_LED_C1 ;
            // turn on zoomcamera led
            zoomcam_led(1) ;
            // turn on mic
            dio_unlock();
            mcu_cmd(MCU_CMD_MICON) ;
            dio_lock();
        }
    }
    else {
        if( (p_dio_mmap->pwii_output & PWII_LED_C1) != 0 ) {
            p_dio_mmap->pwii_output &= (~PWII_LED_C1) ;
            // turn off zoomcamera led
            zoomcam_led(0) ;
            // turn off mic
            dio_unlock();
            mcu_cmd(MCU_CMD_MICOFF);
            dio_lock();
        }
    }
    
   // C2 LED
   if( p_dio_mmap->camera_status[pwii_rear_ch] & PWII_LED_C2 ) {         // rear camera recording?
       p_dio_mmap->pwii_output |= PWII_LED_C2 ;
   }
    else {
       p_dio_mmap->pwii_output &= (~PWII_LED_C2) ;
    }
    dio_unlock();
}

static unsigned int pwii_outs = 0 ;

// update PWII outputs, include leds and powers
void mcu_pwii_output()
{

    unsigned int xouts ;

    mcu_pwii_setc1c2()  ;       // check c1 c2 led.
    
    xouts = pwii_outs ^ p_dio_mmap->pwii_output ;

    if( xouts ) {
        pwii_outs ^= xouts ;
        if( xouts & 0x1000 ) {         // BIT 12: standby mode, 1: standby, 0: running
            if( (pwii_outs&0x1000)!=0 ) {
                // clear LEDs
                // C1 LED 
                mcu_pwii_cmd( PWII_CMD_C1, 1, 0 );
                // C2 LED
                mcu_pwii_cmd( PWII_CMD_C2, 1, 0 );
                // bit 2: MIC LED
                mcu_pwii_cmd( PWII_CMD_LEDMIC, 1, 0 );
                // BIT 11: LCD power
                mcu_pwii_cmd( PWII_CMD_LCD, 1, 0 );
                // standby
                mcu_pwii_cmd( PWII_CMD_STANDBY, 1, 1 );
                // turn off POWER LED
                mcu_pwii_cmd( PWII_CMD_LEDPOWER, 1, 0 );
            }
            else {
                p_dio_mmap->pwii_output |= 0x800 ;  // LCD turned on when get out of standby
                pwii_outs |= 0x800 ;
                mcu_pwii_cmd( PWII_CMD_STANDBY, 1, 0 );
            }
            xouts |= 0x17 ;                   // refresh LEDs
        }

        if( (pwii_outs&0x1000)==0 ) {       // not in standby mode
            // BIT 4: POWER LED
            mcu_pwii_cmd( PWII_CMD_LEDPOWER, 1, ((pwii_outs&0x10)!=0) );
            // C1 LED 
            mcu_pwii_cmd( PWII_CMD_C1, 1, ((pwii_outs&1)!=0) );
            // C2 LED
            mcu_pwii_cmd( PWII_CMD_C2, 1, ((pwii_outs&2)!=0) );
            // bit 2: MIC LED
            mcu_pwii_cmd( PWII_CMD_LEDMIC, 1, ((pwii_outs&4)!=0) );
            // BIT 11: LCD power
            mcu_pwii_cmd( PWII_CMD_LCD, 1, ((pwii_outs&0x800)!=0) );
        }

        if( xouts & 8 ) {           // bit 3: ERROR LED
            if((pwii_outs&8)!=0) {
                if( p_dio_mmap->pwii_error_LED_flash_timer>0 ) {
                    mcu_pwii_cmd( PWII_CMD_LEDERROR, 2, 2, p_dio_mmap->pwii_error_LED_flash_timer );
                }
                else {
                    mcu_pwii_cmd( PWII_CMD_LEDERROR, 2, 1, 0 );
                }
            }
            else {
                mcu_pwii_cmd( PWII_CMD_LEDERROR, 2, 0, 0 );
            }
        }

        if( xouts & 0x100 ) {          // BIT 8: GPS antenna power
            mcu_pwii_cmd( PWII_CMD_POWER_GPSANT, 1, ((pwii_outs&0x100)!=0) );
        }
        
        if( xouts & 0x200 ) {         // BIT 9: GPS POWER
            mcu_pwii_cmd( PWII_CMD_POWER_GPS, 1, ((pwii_outs&0x200)!=0) );
        }

        if( xouts & 0x400 ) {          // BIT 10: RF900 POWER
            mcu_pwii_cmd( PWII_CMD_POWER_RF900, 1, ((pwii_outs&0x400)!=0) );
        }

        if( xouts & 0x2000 ) {          // BIT 13: WIFI power
            mcu_pwii_cmd( PWII_CMD_POWER_WIFI, 1, ((pwii_outs&0x2000)!=0) );
        }
    }
}

unsigned int pwii_keyreltime ;

// auto release keys REC, C2, TM
void pwii_keyautorelease()
{
    if( (p_dio_mmap->pwii_buttons & PWII_BT_AUTORELEASE) && runtime>pwii_keyreltime ) {
        p_dio_mmap->pwii_buttons &= ~PWII_BT_AUTORELEASE ;
    }
}


void mcu_pwii_init()
{
    p_dio_mmap->pwii_output = 0xf10 ;
    pwii_outs = mcu_pwii_ouputstatus() ;
//    pwii_outs = 0xf10 ;
}

// zoom in/out front camera
void mcu_camera_zoom( int zoomin )
{
    if( zoomin ) {
        zoomcam_zoomin();
        mcu_cmd( MCU_CMD_CAMERA_ZOOMIN ) ;
    }
    else {
        zoomcam_zoomout();
        mcu_cmd( MCU_CMD_CAMERA_ZOOMOUT ) ;
    }
}

#endif  // PWII_APP

void mcu_dinput_help(char * ibuf)
{
    unsigned int imap1, imap2 ;
    int i;

    dio_lock();
    // get digital input map
    imap1 = (unsigned char)ibuf[5]+256*(unsigned int)(unsigned char)ibuf[6] ; ;
    imap2 = 0 ;
    for( i=0; i<MAXINPUT; i++ ) {
        if( imap1 & input_map_table[i] )
            imap2 |= (1<<i) ;
    }

    p_dio_mmap->inputmap = imap2;
    // hdlock = (imap1 & (HDINSERTED|HDKEYLOCK) )==0 ;	// HD plugged in and locked
    hdlock = (imap1 & (HDKEYLOCK) )==0 ;	                // HD plugged in and locked
    hdinserted = (imap1 & HDINSERTED)==0 ;  // HD inserted

#ifdef      PWII_APP
    if( imap1 & PWII_MIC ) {
        p_dio_mmap->pwii_output &= ~PWII_LED_MIC ;      // turn off bit2 , MIC LED
    }
    else {
        p_dio_mmap->pwii_output |= PWII_LED_MIC ;      // turn on bit2 , MIC LED
    }
#endif      // PWII_APP

    dio_unlock();
}

int mcu_checkinputbuf(char * ibuf)
{
    if( ibuf[4]=='\x02' && ibuf[2]=='\x01' ) {  // mcu initiated message ?
        switch( ibuf[3] ) {
        case MCU_INPUT_DIO :                    // digital input event
            mcu_response( ibuf );
            mcu_dinput_help(ibuf);
            break;

        case MCU_INPUT_IGNITIONOFF :            // ignition off event
            mcu_response( ibuf, 2, 0, 0 );      // response with two 0 data
            mcupowerdelaytime = 0 ;
            p_dio_mmap->poweroff = 1 ;          // send power off message to DVR
            dvr_log("Ignition off");
            break;
            
        case MCU_INPUT_IGNITIONON :	// ignition on event
            mcu_response( ibuf, 1, watchdogenabled  );      // response with watchdog enable flag
            p_dio_mmap->poweroff = 0 ;						// send power on message to DVR
            dvr_log("Ignition on");
            break ;

        case MCU_INPUT_GSENSOR :                  // g sensor Accelerometer data
            mcu_response( ibuf );
            gforce_log( (int)(signed char)ibuf[5], (int)(signed char)ibuf[6], (int)(signed char)ibuf[7]) ;
            break;
            
        default :
            netdbg_print("Unknown message from MCU.\n");
            break;
        }
    }

#ifdef PWII_APP

    else if( ibuf[4]=='\x02' && ibuf[2]=='\x05' )          // input from MCU5 (MR. Henry?)
    {
       netdbg_print("Messages from Mr. Henry!\n" );
       switch( ibuf[3] ) {
       case PWII_INPUT_REC :                        // Front Camera (REC) button
           dio_lock();
           p_dio_mmap->pwii_buttons &= ~PWII_BT_AUTORELEASE ;     // Release other bits
           p_dio_mmap->pwii_buttons |= PWII_BT_C1 ;               // bit 8: front camera
           pwii_keyreltime = runtime+1000 ;         // auto release in 1 sec
           dio_unlock();
           mcu_response( ibuf, 1, ((p_dio_mmap->pwii_output&PWII_LED_C1)!=0) );  // bit 0: c1 led
//           mcu_response( ibuf, 1, 0 );                                  // bit 0: c1 led
           break;

       case PWII_INPUT_C2 :                         // Back Seat Camera (C2) Starts/Stops Recording
           dio_lock();
           p_dio_mmap->pwii_buttons &= ~PWII_BT_AUTORELEASE ;     // Release other bits
           p_dio_mmap->pwii_buttons |= PWII_BT_C2 ;      // bit 9: back camera
           pwii_keyreltime = runtime+1000 ;         // auto release in 1 sec
           dio_unlock();
           mcu_response( ibuf, 1, ((p_dio_mmap->pwii_output&PWII_LED_C2)!=0) );  // bit 1: c2 led
//           mcu_response( ibuf, 1, 0 );                                  // bit 1: c2 led
           break;

       case PWII_INPUT_TM :                                 // TM Button
           mcu_response( ibuf );
           dio_lock();
           if( ibuf[5] ) {
               p_dio_mmap->pwii_buttons &= ~PWII_BT_AUTORELEASE ;   // Release other bits
               p_dio_mmap->pwii_buttons |= PWII_BT_TM ;     // bit 10: tm button
               pwii_keyreltime = runtime+1000 ;             // auto release in 1 sec
           }
//  ignor TM release, the bit will be auto release after 1 sec           
//           else {
//               p_dio_mmap->pwii_buttons &= (~0x400) ;     // bit 10: tm button
//           }
           dio_unlock();
           break;

       case PWII_INPUT_LP :                                 // LP button
           mcu_response( ibuf );
           if( ibuf[5] ) {
               dio_lock();
               p_dio_mmap->pwii_buttons |= PWII_BT_LP ;     // bit 11: LP button
               dio_unlock();
               mcu_camera_zoom( 1 );
           }
           else {
               dio_lock();
               p_dio_mmap->pwii_buttons &= ~PWII_BT_LP ;    // bit 11: LP button
               dio_unlock();
               mcu_camera_zoom( 0 );
           }
           break;

       case PWII_INPUT_BO :                                 // BIT 12:  blackout, 1: pressed, 0: released, auto release
           dio_lock();
           if( ibuf[5] ) {
                p_dio_mmap->pwii_buttons |= PWII_BT_BO ;    // bit 12: blackout
           }
           else {
                p_dio_mmap->pwii_buttons &= ~PWII_BT_BO ;   // bit 12: blackout
           }
           dio_unlock();
           mcu_response( ibuf );
           break;
           
       case PWII_INPUT_MEDIA :                              // Video play Control
           dio_lock();
           if( (p_dio_mmap->pwii_output & (1<<12))==0 ) {   // pwii not standby
               p_dio_mmap->pwii_buttons &= (~0x3f) ;
               p_dio_mmap->pwii_buttons |= ((unsigned char)ibuf[5])^0x3f ;
           }
           dio_unlock();
           mcu_response( ibuf );
           break;

       case PWII_INPUT_BOOTUP :                         // CDC ask for boot up ready
           mcu_response( ibuf, 1, 1  );                 // possitive response
           dio_lock();
           pwii_outs = ~p_dio_mmap->pwii_output ;       // force re-fresh LED outputs
           dio_unlock();
           break;

       default :
            netdbg_print("Unknown message from PWII MCU.\n");
            break;

       }
    }

#endif    // PWII_APP

    return 0 ;
}


void time_setmcu()
{
    int bsecond, bminute, bhour, bzero, bday, bmonth, byear ;
    dio_lock();
    bsecond = bcd( p_dio_mmap->rtc_second ) ;
    bminute = bcd( p_dio_mmap->rtc_minute ) ;
    bhour   = bcd( p_dio_mmap->rtc_hour ) ;
    bzero   = bcd( 0 );
    bday    = bcd( p_dio_mmap->rtc_day );
    bmonth  = bcd( p_dio_mmap->rtc_month );
    byear   = bcd( p_dio_mmap->rtc_year );
    dio_unlock();
    if( mcu_cmd( MCU_CMD_WRITERTC, 7, 
                bsecond,
                bminute,
                bhour,
                bzero,
                bday,
                bmonth,
                byear ) ) 
    {
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
        return ;
    }
    p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.

}

// sync system time to rtc
void time_syncrtc()
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

void time_rtctosys()
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
    }
}

void time_syncgps ()
{
    struct timeval tv ;
    int diff ;
    time_t gpstime ;
    dio_lock();
    if( p_dio_mmap->gps_valid ) {
        gpstime = (time_t) p_dio_mmap->gps_gpstime ;
        dio_unlock();
        if( (int)gpstime > 1262322000 ) {       // > year 2010
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
    else {
        dio_unlock();
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

void dvrsvr_susp()
{
    int pid ;
    FILE * fpid = fopen("/var/dvr/dvrsvr.pid", "r");
    if( fpid ) {
        if( fscanf( fpid, "%d", &pid )==1 ) {
            kill( pid, SIGUSR1 );
        }
        fclose( fpid );
    }
}

void dvrsvr_resume()
{
    int pid ;
    FILE * fpid = fopen("/var/dvr/dvrsvr.pid", "r");
    if( fpid ) {
        if( fscanf( fpid, "%d", &pid )==1 ) {
            kill( pid, SIGUSR2 );
        }
        fclose( fpid );
    }
}

void glog_susp()
{
    int pid ;
    FILE * fpid = fopen("/var/dvr/glog.pid", "r");
    if( fpid ) {
        if( fscanf( fpid, "%d", &pid )==1 ) {
            kill( pid, SIGUSR1 );
        }
        fclose( fpid );
    }
}

void glog_resume()
{
    int pid ;
    FILE * fpid = fopen("/var/dvr/glog.pid", "r");
    if( fpid ) {
        if( fscanf( fpid, "%d", &pid )==1 ) {
            kill( pid, SIGUSR2 );
        }
        fclose( fpid );
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

static int disk_mounted = 1 ;       // by default assume disks is mounted!
static pid_t pid_disk_mount=0 ;

void mountdisks()
{
    int status ;
    disk_mounted=1 ;
    if( pid_disk_mount>0 ) {
        kill(pid_disk_mount, SIGKILL);
        waitpid( pid_disk_mount, &status, 0 );
    }
    pid_disk_mount = fork ();
    if( pid_disk_mount==0 ) {
        sleep(5) ;
        static char cmdmountdisks[]="/davinci/dvr/mountdisks" ;
        sleep(5) ;
        execl(cmdmountdisks, cmdmountdisks, NULL);
        exit(0);    // should not return to here
    }
}

void umountdisks()
{
    int status ;
    disk_mounted=0 ;
    if( pid_disk_mount>0 ) {
        kill(pid_disk_mount, SIGKILL);
        waitpid( pid_disk_mount, &status, 0 );
    }
    pid_disk_mount = fork ();
    if( pid_disk_mount==0 ) {
        static char cmdumountdisks[]="/davinci/dvr/umountdisks" ;
        execl(cmdumountdisks, cmdumountdisks, NULL);
        exit(0);    // should not return to here
    }
}

// bring up wifi adaptor
static int wifi_run=0 ;
void wifi_up()
{
    static char wifi_up_script[]="/davinci/dvr/wifi_up" ;
    system( wifi_up_script );
    // turn on wifi power. mar 04-2010
    dio_lock();
    p_dio_mmap->pwii_output |= 0x2000 ;
    dio_unlock();
    wifi_run=1 ;
    netdbg_print("Wifi up\n");
}

// bring down wifi adaptor
void wifi_down()
{
    static char wifi_down_script[]="/davinci/dvr/wifi_down" ;
    if( wifi_on ) {
        return ;
    }
    if( wifi_run ) {
        system( wifi_down_script );
        // turn off wifi power
        dio_lock();
        p_dio_mmap->pwii_output &= ~0x2000 ;
        dio_unlock();
        netdbg_print("Wifi down\n");
        wifi_run=0 ;
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
    dio_lock();
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
    dio_unlock();
}    


int smartftp_retry ;
int smartftp_disable ;
int smartftp_reporterror ;
int smartftp_arch ;     // do smartftp arch files
int smartftp_src ;

// src == 0 : do "disk", src==1 : do "arch"
void smartftp_start(int src=0)
{
    smartftp_src = src ;
    dvr_log( "Start smart server uploading. (%s)", (src==0)?"disks":"arch" );
    pid_smartftp = fork();
    if( pid_smartftp==0 ) {     // child process
        char hostname[128] ;
//        char mountdir[250] ;

        // get BUS name
        gethostname(hostname, 128) ;

        // be nice
        nice(10);
        
//      system("/davinci/dvr/setnetwork");  // reset network, this would restart wifi adaptor
        if( src==0 ) {
            execl( "/davinci/dvr/smartftp", "/davinci/dvr/smartftp",
              "rausb0",
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
        }
        else {
            execl( "/davinci/dvr/smartftp", "/davinci/dvr/smartftp",
              "rausb0",
              hostname,
              "247SECURITY",
              "/var/dvr/arch",
              "510",
              "247ftp",
              "247SECURITY",
              "0",
              smartftp_reporterror?"Y":"N",
              getenv("TZ"), 
              NULL 
              );
        }
        dvr_log( "Start smart server uploading failed!");
        exit(101);      // error happened.
    }
    else if( pid_smartftp<0 ) {
        pid_smartftp=0;
    }
//    smartftp_log( "Smartftp started." );
}

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
                    smartftp_start(smartftp_src);
                }
                else if( smartftp_arch && smartftp_src == 0 ){  // do uploading arch disk
                    smartftp_retry=3 ;
                    smartftp_start(1) ;
                }
            }
            else if( WIFSIGNALED(smartftp_status)) {
                dvr_log( "\"smartftp\" killed by signal %d.", WTERMSIG(smartftp_status));
            }
            else {
                dvr_log( "\"smartftp\" aborted." );
            }
        }
    }
    return pid_smartftp ;
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

int g_syncrtc=0 ;

float cpuusage()
{
    static float s_cputime=0.0, s_idletime=0.0 ;
    static float s_usage = 0.0 ;
    float cputime, idletime ;
    FILE * uptime ;
    uptime = fopen( "/proc/uptime", "r" );
    if( uptime ) {
        fscanf( uptime, "%f %f", &cputime, &idletime );
        fclose( uptime );
        if( cputime>s_cputime ) {
            s_usage=1.0 -(idletime-s_idletime)/(cputime-s_cputime) ;
            s_cputime=cputime ;
            s_idletime=idletime ;
        }
    }
    return s_usage ;
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

    // setup netdbg
    if( dvrconfig.getvalueint( "debug", "ioprocess" ) ) {
        i = dvrconfig.getvalueint( "debug", "port" ) ;
        v = dvrconfig.getvalue( "debug", "host" ) ;
        netdbg_init( v.getstring(), i );
    }
    
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

    v = dvrconfig.getvalue( "system", "temp_logfile");
    if (v.length() > 0){
        strncpy( temp_logfile, v.getstring(), sizeof(temp_logfile));
    }
        
    v = dvrconfig.getvalue( "system", "iomapfile");
    char * iomapfile = v.getstring();
    if( iomapfile && strlen(iomapfile)>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    fd = open(dvriomap, O_RDWR | O_CREAT, S_IRWXU);
    if( fd<=0 ) {
        dvr_log("Can't create io map file (%s).", dvriomap);
        return 0 ;
    }
    ftruncate(fd, sizeof(struct dio_mmap));
    close(fd);
    if( dio_mmap( dvriomap )==NULL ) {
        dvr_log( "IO memory map failed!");
        return 0 ;
    }
    
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
        p_dio_mmap->outputnum = MCU_OUTPUTNUM ;	

    p_dio_mmap->panel_led = 0;
    p_dio_mmap->devicepower = 0xffff;				// assume all device is power on
    
    p_dio_mmap->iopid = getpid () ;					// io process pid

    // check if sync rtc wanted
    
    g_syncrtc = dvrconfig.getvalueint("io", "syncrtc");

    // smartftp variable
    smartftp_disable = dvrconfig.getvalueint("system", "smartftp_disable");

    // keep wifi on by default?
    wifi_on = dvrconfig.getvalueint("system", "wifi_on" );
    if( !wifi_on ) {
        wifi_down();
    }

    // initial mcu
    mcu_init (dvrconfig);

#ifdef PWII_APP
    if( mcu_pwii_bootupready() ) {
        dvr_log( "PWII boot up ready!");
        // get MCU version
        char pwii_version[100] ;
        if( mcu_pwii_version( pwii_version ) ) {
            dvr_log("PWII version: %s", pwii_version );
            FILE * pwiiversionfile=fopen("/var/dvr/pwiiversion", "w");
            if( pwiiversionfile ) {
                fprintf( pwiiversionfile, "%s", pwii_version );
                fclose( pwiiversionfile );
            }
        }     
        
    }

    mcu_pwii_init();    // init pwii variables
    
    pwii_front_ch = dvrconfig.getvalueint( "pwii", "front");
    pwii_rear_ch = dvrconfig.getvalueint( "pwii", "rear");

    // zoom camera
    zoomcam_handle = 0 ;
    v = dvrconfig.getvalue( "io", "zoomcam_port");
    if( v.length()>0 ) {
        strncpy( zoomcam_dev, v.getstring(), sizeof(zoomcam_dev) );
    }
    zoomcam_baud = dvrconfig.getvalueint("io", "zoomcam_baudrate");
    if( zoomcam_baud<2400 || zoomcam_baud>115200 ) {
            mcu_baud=DEFSERIALBAUD ;
    }

    zoomcam_enable = dvrconfig.getvalueint("io", "zoomcam_enable");
        
#endif // PWII_APP   

    if( g_syncrtc ) {
        time_rtctosys();
    }

    gforce_init( dvrconfig );
     
    usewatchdog = dvrconfig.getvalueint("io", "usewatchdog");
    watchdogtimeout=dvrconfig.getvalueint("io", "watchdogtimeout");
    if( watchdogtimeout<10 )
        watchdogtimeout=10 ;
    if( watchdogtimeout>200 )
        watchdogtimeout=200 ;

    hd_timeout = dvrconfig.getvalueint("io", "hdtimeout");
    if( hd_timeout<=0 || hd_timeout>3600 ) {
        hd_timeout=180 ;             // default timeout 180 seconds
    }

    shutdowndelaytime = dvrconfig.getvalueint( "system", "shutdowndelay");
    if( shutdowndelaytime<10 ) {
        shutdowndelaytime=10 ;
    }
    else if(shutdowndelaytime>3600*12 ) {
        shutdowndelaytime=3600*12 ;
    }

    standbytime = dvrconfig.getvalueint( "system", "standbytime");
    if(standbytime<0 ) {
        standbytime=0 ;
    }
    else if( standbytime>43200 ) {
        standbytime=43200 ;
    }

    wifidetecttime = dvrconfig.getvalueint( "system", "wifidetecttime");
    if(wifidetecttime<10 | wifidetecttime>600 ) {
        wifidetecttime=60 ;
    }

    uploadingtime = dvrconfig.getvalueint( "system", "uploadingtime");
    if(uploadingtime<=0 || uploadingtime>43200 ) {
        uploadingtime=1800 ;
    }

    archivetime = dvrconfig.getvalueint( "system", "archivetime");
    if(archivetime<=0 || archivetime>7200 ) {
        archivetime=1800 ;
    }

    smartftp_arch = dvrconfig.getvalueint( "system", "archive_upload");

    pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }

    // initialize timer
    initruntime();
    
    return 1;
}

// app finish, clean up
void appfinish()
{

#ifdef PWII_APP    
    zoomcam_close();
#endif
        
    if( watchdogenabled ) {
        mcu_watchdogdisable();
    }

    gforce_finish();
    mcu_finish();
    
    p_dio_mmap->iopid = 0 ;
    p_dio_mmap->usage-- ;
    
    // clean up shared memory
    dio_munmap();

    netdbg_finish();

    // delete pid file
    unlink( pidfile );
}

unsigned int modeendtime ;
void mode_run()
{
    dio_lock();
    p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
    p_dio_mmap->iomsg[0]=0 ;
    p_dio_mmap->iomode=IOMODE_RUN ;    // back to run normal
    dio_unlock();
    glog_resume();
    dvr_log("Power on switch, set to running mode. (mode %d)", p_dio_mmap->iomode);
}

void mode_archive()
{
    dio_lock();
    strcpy( p_dio_mmap->iomsg, "Archiving, Do not remove CF Card.");
    p_dio_mmap->dvrstatus |= DVR_ARCH ;
    p_dio_mmap->iomode=IOMODE_ARCHIVE  ;
    dio_unlock();
    modeendtime = runtime+archivetime*1000 ;   
    dvr_log("Enter archiving mode. (mode %d).", p_dio_mmap->iomode);
#ifdef PWII_APP    
    zoomcam_close();        // close ttyUSB to restore USB performance.
#endif                        
}

void mode_detectwireless()
{
    wifi_up();                                      // turn on wifi
    dio_lock();
    p_dio_mmap->smartserver=0 ;         // smartserver detected
    p_dio_mmap->iomode = IOMODE_DETECTWIRELESS ;
    strcpy( p_dio_mmap->iomsg, "Detecting Smart Server!");
    dio_unlock();
    modeendtime = runtime + wifidetecttime * 1000 ;    
    dvr_log("Start detecting smart server. (mode %d).", p_dio_mmap->iomode);
}

void mode_upload()
{
    // suspend dvrsvr and glog before start uploading.

    glog_susp();
    
    // check video lost report to smartftp.
    if( (p_dio_mmap->dvrstatus & (DVR_VIDEOLOST|DVR_ERROR) )==0 )
    {
        smartftp_reporterror = 0 ;
    }
    else {
        smartftp_reporterror=1 ;
    }
    smartftp_retry=3 ;
    smartftp_start() ;
    dio_lock();
    strcpy( p_dio_mmap->iomsg, "Uploading Video to Smart Server!");
    p_dio_mmap->iomode = IOMODE_UPLOADING ;
    dio_unlock();
    modeendtime = runtime+uploadingtime*1000 ; 
    dvr_log("Enter uploading mode. (mode %d).", p_dio_mmap->iomode);
    buzzer( 3, 250, 250);
}

void mode_standby()
{
    dio_lock();
#ifdef PWII_APP    
    // standby pwii
    p_dio_mmap->pwii_output &= ~0x800 ;         // LCD off
    p_dio_mmap->pwii_output |= 0x1000 ;         // STANDBY mode
#endif
    p_dio_mmap->iomsg[0]=0 ;
    p_dio_mmap->iomode=IOMODE_STANDBY ;
    dio_unlock();
    modeendtime = runtime+standbytime*1000 ;   
    dvr_log("Enter standby mode. (mode %d)", p_dio_mmap->iomode);
}

int main(int argc, char * argv[])
{
    int i;
    int hdpower=0 ;                             // assume HD power is off
    int hdkeybounce = 0 ;                       // assume no hd
    unsigned int gpsavailable = 0 ;
    
    if( appinit()==0 ) {
        return 1;
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
                watchdogtimeout=delay ;
                mcu_watchdogenable () ;
                sleep(delay+20) ;
                mcu_reset();
                return 1;
            }
        }
    }
  
    // setup signal handler	
    p_dio_mmap->iomode = IOMODE_RUN ;
    signal(SIGQUIT, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR2, sig_handler);
    
    dvr_log( "DVR IO process started!");
   
    // get default digital input
    mcu_dinput();

    // initialize output
    mcu_doutputmap=~(p_dio_mmap->outputmap) ;
    
    // initialize panel led
    panelled = 0xff ;
    p_dio_mmap->panel_led = 0;
    
    // initialize device power
    devicepower = 0 ;
    p_dio_mmap->devicepower = 0xffff;				// assume all device is power on
    
    while( p_dio_mmap->iomode ) {
        runtime=getruntime() ;

        // do input pin polling
        static unsigned int mcu_input_timer ;
        if( mcu_input(50000)>0 ) {
            mcu_input_timer = runtime ;
        }
        if( mcu_inputmissed || (runtime - mcu_input_timer)> 10000  ) {
            mcu_input_timer = runtime ;
            mcu_inputmissed=0;
            mcu_dinput();
        }

        // Buzzer functions
        buzzer_run( runtime ) ;
#ifdef PWII_APP        
        pwii_keyautorelease() ;     // auto release pwii key pad (REC, C2, TM)
#endif        

        // do digital output
        mcu_doutput();

#ifdef PWII_APP
        // update PWII outputs
        mcu_pwii_output();
#endif            
        // rtc command check
        if( p_dio_mmap->rtc_cmd != 0 ) {
            if( p_dio_mmap->rtc_cmd == 1 ) {
                mcu_readrtc(); 
            }
            else if( p_dio_mmap->rtc_cmd == 2 ) {
                time_setmcu();
            }
            else if( p_dio_mmap->rtc_cmd == 3 ) {
                time_syncrtc();
            }
            else if( p_dio_mmap->rtc_cmd == 10 ) {      // Ex command, calibrate gforce sensor
                gforce_calibration();
            }
            else {
                p_dio_mmap->rtc_cmd=-1;		// command error, (unknown cmd)
            }
        }

        static unsigned int cpuusage_timer ;
        if( (runtime - cpuusage_timer)> 5000 ) {    // 5 seconds to monitor cpu usage
            cpuusage_timer=runtime ;
            static int usage_counter=0 ;
            if( cpuusage()>0.995 ) {
                if( ++usage_counter>12 ) {          // CPU keey busy for 1 minites
                    buzzer( 10, 250, 250 );
                    // let system reset by watchdog
                    dvr_log( "CPU usage at 100%% for 60 seconds, reset system.");
                    p_dio_mmap->iomode=IOMODE_REBOOT ;
                }
            }
            else {
                usage_counter=0 ;
            }
        }
        
        static unsigned int temperature_timer ;
        if( (runtime - temperature_timer)> 10000 ) {    // 10 seconds to read temperature, and digital input refresh
            temperature_timer=runtime ;
            
            i=mcu_iotemperature();
            netdbg_print("Temperature: io(%d)",i );
            if( i > -127 && i < 127 ) {
                static int saveiotemp = 0 ;
                if( i>=75 && 
                   (i-saveiotemp) > 2 ) 
                {
                    dvr_log ( "High system temperature: %d", i);
                    saveiotemp=i ;
                }
                
                p_dio_mmap->iotemperature = i ;
            }
            else {
                p_dio_mmap->iotemperature=-128 ;
            }
            
            i=mcu_hdtemperature();
            netdbg_print(" hd(%d)\n", i );
            if( i > -127 && i < 127 ) {
                static int savehdtemp = 0 ;

                if( i>=75 && 
                   (i-savehdtemp) > 2 ) 
                {
                    dvr_log ( "High hard disk temperature: %d", i);
                    savehdtemp=i ;
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
                   p_dio_mmap->iomode==IOMODE_RUN &&                      
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
                   p_dio_mmap->iomode==IOMODE_RUN &&                      
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

            // updating watch dog state
            if( usewatchdog && watchdogenabled==0 ) {
                mcu_watchdogenable();
            }

            // kick watchdog
            if( watchdogenabled )
                mcu_watchdogkick () ;

            if( p_dio_mmap->dvrstatus & DVR_FAILED ) {
                mcu_reset();
            }
            
            // do power management mode switching
            if( p_dio_mmap->iomode==IOMODE_RUN ) {                         // running mode
                static int app_run_bell = 0 ;
                if( app_run_bell==0 &&
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) ) 
                {
                    app_run_bell=1 ;                    // dvr started bell
                    buzzer(1, 1000, 100);
                }

                if( p_dio_mmap->poweroff )              // ignition off detected
                {
                    modeendtime = runtime + shutdowndelaytime*1000;
                    p_dio_mmap->iomode=IOMODE_SHUTDOWNDELAY ;                        // hutdowndelay start ;
                    dvr_log("Power off switch, enter shutdown delay (mode %d).", p_dio_mmap->iomode);
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_SHUTDOWNDELAY ) {   // shutdown delay
                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay() ;
                    if( runtime>modeendtime  ) {
                        // start wifi detecting
                        mode_detectwireless();
                    }
                }
                else {
                    mode_run();                                     // back to run normal
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_DETECTWIRELESS ) {  // close recording and run smart ftp
                if( p_dio_mmap->poweroff == 0 ) {
                    wifi_down();
                    mode_run();                                     // back to run normal
                }
                else {
                    mcu_poweroffdelay ();               // keep system alive

                    if( p_dio_mmap->smartserver ) {
                        if( smartftp_disable==0 ) {
                            mode_upload();
                        }
                    }
                    else if( runtime>modeendtime ) {
                        dvr_log( "No smartserver detected!" );
                        // enter archiving mode
                        mode_archive();
                        wifi_down();
                    }
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_UPLOADING ) {                  // file uploading
                if( p_dio_mmap->poweroff==0 ) {
                    if( pid_smartftp > 0 ) {
                        smartftp_kill();
                    }
                    wifi_down();
                    mode_run();                                     // back to run normal
                }
                else {
                    mcu_poweroffdelay ();                       // keep power on
                    // continue wait for smartftp
                    smartftp_wait() ;
                    if( runtime>=modeendtime || pid_smartftp==0 ) {
                        if( pid_smartftp > 0 ) {
                            smartftp_kill();
                        }
                        wifi_down();
                        // enter archiving mode
                        mode_archive();
                    }
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_ARCHIVE ) {   // archiving, For pwii, copy file to CF in CDC
                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();               // keep system alive
                
                    if( p_dio_mmap->dvrpid<=0 ||
                        ( (p_dio_mmap->dvrstatus & DVR_RECORD )==0 &&
                            (p_dio_mmap->dvrstatus & DVR_ARCH )==0 ) ||
                        runtime>modeendtime )
                    {
                        // enter standby mode
                        mode_standby();
                    }
                }
                else {
                    mode_run();                                     // back to run normal
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_STANDBY ) {         // standby
                if( p_dio_mmap->poweroff == 0 ) {      
#ifdef PWII_APP
                    // pwii jump out of standby
                    dio_lock();
                    p_dio_mmap->pwii_output |= 0x800 ;      // LCD on                   
                    p_dio_mmap->pwii_output &= ~0x1000 ;    // standby off
                    dio_unlock();
#endif                    
                    wifi_down();
                    mode_run();
                    if( disk_mounted==0 ) {
                        mountdisks();
                    }
                }
                else {
                    mcu_poweroffdelay ();
                    p_dio_mmap->outputmap ^= HDLED ;        // flash LED slowly to indicate standy mode

                    if( p_dio_mmap->dvrpid>0 && 
                        (p_dio_mmap->dvrstatus & DVR_RUN) &&
                        (p_dio_mmap->dvrstatus & DVR_NETWORK) )
                    {
                        p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
                        if( disk_mounted==0 ) {
                            mountdisks();
                        }
                    }
                    else {
                        p_dio_mmap->devicepower=0 ;        // turn off all devices power
                        if( disk_mounted ) {
                            umountdisks();
                        }
                    }

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
                        p_dio_mmap->devicepower=0 ;    // turn off all devices power
                        p_dio_mmap->iomode=IOMODE_SHUTDOWN ;    // turn off mode
                        modeendtime = runtime+90000 ;
                        dvr_log("Standby timeout, system shutdown. (mode %d).", p_dio_mmap->iomode );
                        buzzer( 5, 250, 250 );
                    }
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_SUSPEND ) {                    // suspend io, for file copying process
                if( p_dio_mmap->poweroff != 0 ) {
                    mcu_poweroffdelay ();
                }
                p_dio_mmap->suspendtimer -= 3 ;
                if( p_dio_mmap->suspendtimer <= 0 ) {
                    dvr_log( "Suspend mode timeout!" );
                    mode_run();                                     // back to run normal
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_SHUTDOWN ) {                    // turn off mode, no keep power on 
                if( p_dio_mmap->dvrpid > 0 ) {
                    kill(p_dio_mmap->dvrpid, SIGTERM);
                }
                if( p_dio_mmap->glogpid > 0 ) {
                    kill(p_dio_mmap->glogpid, SIGTERM);
                }
                if( pid_smartftp > 0 ) {
                    smartftp_kill();
                }
                sync();
                if( runtime>modeendtime ) {     // system suppose to turn off during these time
                    // hard ware turn off failed. May be ignition turn on again? Reboot the system
                    dvr_log("Hardware shutdown failed. Try reboot by software!" );
                    p_dio_mmap->iomode=IOMODE_REBOOT ;
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_REBOOT ) {
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
                    watchdogtimeout=10 ;                    // 10 seconds watchdog time out
                    mcu_watchdogenable() ;
                    usewatchdog=0 ;                         // don't call watchdogenable again!
                    watchdogenabled=0 ;                     // don't kick watchdog ;
                    reboot_begin=1 ;                        // rebooting in process
                    modeendtime=runtime+20000 ;             // 20 seconds time out, watchdog should kick in already!
                }
                else if( runtime>modeendtime ) { 
                    // program should not go throught here,
                    mcu_reset();
                    system("/bin/reboot");                  // do software reboot
                    p_dio_mmap->iomode=IOMODE_QUIT ;                 // quit IOPROCESS
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_REINITAPP ) {
                dvr_log("IO re-initialize.");
                appfinish();
                appinit();
                p_dio_mmap->iomode = IOMODE_RUN ;
            }
            else if( p_dio_mmap->iomode==IOMODE_QUIT ) {
                break ;
            }
            else {
                // no good, enter undefined mode 
                dvr_log("Error! Enter undefined mode %d.", p_dio_mmap->iomode );
                p_dio_mmap->iomode = IOMODE_RUN ;              // we retry from mode 1
            }

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
                    p_dio_mmap->iomode=IOMODE_REBOOT ;
                    p_dio_mmap->dvrwatchdog=1 ;
                }
                
                static int nodatawatchdog = 0;
                if( (p_dio_mmap->dvrstatus & DVR_RUN) && 
                   (p_dio_mmap->dvrstatus & DVR_NODATA ) &&     // some camera not data in
                   p_dio_mmap->iomode==IOMODE_RUN )
                {
                    if( ++nodatawatchdog > 20 ) {   // 1 minute
                        buzzer( 10, 250, 250 );
                        // let system reset by watchdog
                        dvr_log( "One or more camera not working, system reset.");
                        p_dio_mmap->iomode=IOMODE_REBOOT ;
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
                    mcu_hdpoweron() ;
                }
                hdkeybounce = 0 ;

                if( hdinserted &&                      // hard drive inserted
                    p_dio_mmap->dvrpid > 0 &&
                    p_dio_mmap->dvrwatchdog >= 0 &&
                    (p_dio_mmap->dvrstatus & DVR_RUN) &&
                    (p_dio_mmap->dvrstatus & DVR_DISKREADY )==0 )        // dvrsvr is running but no disk detected
                {
                    p_dio_mmap->outputmap ^= HDLED ;
                    
                    if( ( p_dio_mmap->iomode==IOMODE_RUN ||
                        p_dio_mmap->iomode==IOMODE_SHUTDOWNDELAY ) &&
                        ++hdpower>hd_timeout*2 )                // 120 seconds, sometimes it take 100s to mount HD(CF)
                    {
                        dvr_log("Hard drive failed, system reset!");
                        buzzer( 10, 250, 250 );

                        // turn off HD led
                        p_dio_mmap->outputmap &= ~HDLED ;
                        mcu_hdpoweroff();                   // FOR NEW MCU, THIS WOULD TURN OFF SYSTEM POWER?!
                        hdpower=0;
                    }
                }
                else {
                    // turn on HD led
                    if( p_dio_mmap->iomode <= IOMODE_SHUTDOWNDELAY ) {
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
                if( hdkeybounce<10 ) {                          // wait for key lock stable
                    hdkeybounce++ ;
                    hd_dvrpid_save=0 ;
                }
                else if( hdkeybounce==10 ){
                    hdkeybounce++ ;
                    // suspend DVRSVR process
                    if( p_dio_mmap->dvrpid>0 ) {
                        hd_dvrpid_save=p_dio_mmap->dvrpid ;
                        dvr_log("HD key off, to suspend dvrsvr!");
                        kill(hd_dvrpid_save, SIGUSR1);         // request dvrsvr to turn down
                        // turn on HD LED
                        p_dio_mmap->outputmap |= HDLED ;
                    }
                }
                else if( hdkeybounce==11 ) {
                    if( p_dio_mmap->dvrpid <= 0 )  {
                        hdkeybounce++ ;
                        sync();
                        // umount disks
                        system("/davinci/dvr/umountdisks") ;
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
            
            if( p_dio_mmap->dvrpid>0 && (p_dio_mmap->dvrstatus & DVR_NODATA)!=0 && p_dio_mmap->iomode==IOMODE_RUN ) {
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

#ifdef PWII_APP
            // BIT 3: ERROR LED
            if( panelled & 2 ) {
                p_dio_mmap->pwii_output |= PWII_LED_ERROR ;
            }
            else {
                p_dio_mmap->pwii_output &= ~PWII_LED_ERROR ;
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
//                setnetwork();
            }
        }
        
        // adjust system time with RTC
        static unsigned int adjtime_timer ;
        if( p_dio_mmap->iomode==IOMODE_RUN ) {
            if( gpsvalid != p_dio_mmap->gps_valid ) {
                gpsvalid = p_dio_mmap->gps_valid ;
                if( gpsvalid ) {
                    time_syncgps () ;
                    adjtime_timer=runtime ;
                    gpsavailable = 1 ;
                }
                // sound the bell on gps data
                if( gpsvalid ) {
                    buzzer( 2, 300, 300 );
                }
                else {
                    buzzer( 3, 300, 300 );
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
    }
    
    appfinish();
    
    dvr_log( "DVR IO process ended!");
    return 0;
}

