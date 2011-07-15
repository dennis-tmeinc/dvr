
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

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "netdbg.h"
#include "mcu.h"
#include "gforce.h"
#include "diomap.h"
#include "iomisc.h"

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
    if( mcu_cmd( NULL, MCU_CMD_WRITERTC, 7, 
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

int time_rtctosys()
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
		return 1 ;
    }
	return 0 ;
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


void check_rtccmd()
{
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
		else if( p_dio_mmap->rtc_cmd == 11 ) {      // Ex command, save gforce crash data
			p_dio_mmap->rtc_cmd = 0 ;
			gforce_getcrashdata();
		}
		else if( p_dio_mmap->rtc_cmd == 15 ) {      // Ex command, calibrate gforce mount angle
			p_dio_mmap->rtc_cmd = 0 ;
			gforce_calibrate_mountangle(1);
		}
		else if( p_dio_mmap->rtc_cmd == 16 ) {      // Ex command, stop calibrate gforce mount angle
			p_dio_mmap->rtc_cmd = 0 ;
			gforce_calibrate_mountangle(0);
		}
		else {
			p_dio_mmap->rtc_cmd=-1;		// command error, (unknown cmd)
		}
	}
}	


void check_cpuusage()
{
	static unsigned int cpuusage_timer ;
	static int usage_counter=0 ;
	static float s_cputime=0.0, s_idletime=0.0 ;
	static float s_usage = 0.0 ;

	if( (runtime - cpuusage_timer)> 5000 ) {    // 5 seconds to monitor cpu usage
		cpuusage_timer=runtime ;

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
		
		if( s_usage>0.995 ) {
			if( ++usage_counter>12 ) {          // CPU keey busy for 1 minites
				buzzer( 10, 250, 250 );
				// let system reset by watchdog
				dvr_log( "CPU usage at 100%% for 60 seconds, reboot system.");
				p_dio_mmap->iomode=IOMODE_REBOOT ;
			}
		}
		else {
			usage_counter=0 ;
		}
	}
}


void check_temperature()
{
	int i ;
	static unsigned int temperature_timer ;
	if( (runtime - temperature_timer)> 10000 ) {    // 10 seconds to read temperature,
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
}

void check_watchdog()
{
	static unsigned int watchdog_timer = 0 ;
	if( usewatchdog && (runtime - watchdog_timer)> 3000 ) {    // 3 seconds to kick watchdog
		watchdog_timer = runtime ;
		mcu_watchdogkick();
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
    FILE * fpid = fopen(VAR_DIR"/dvrsvr.pid", "r");
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
    FILE * fpid = fopen(VAR_DIR"/dvrsvr.pid", "r");
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
    FILE * fpid = fopen(VAR_DIR"/glog.pid", "r");
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
    FILE * fpid = fopen(VAR_DIR"/glog.pid", "r");
    if( fpid ) {
        if( fscanf( fpid, "%d", &pid )==1 ) {
            kill( pid, SIGUSR2 );
        }
        fclose( fpid );
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

