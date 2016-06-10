
/*
 * 
 * 
 *   mcucmd.cpp
 *       strip out some simple mcu commands from ioprocess.cpp
 * 
 */
  
 
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

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"
#include "mcu.h"

int dvr_log(const char *fmt, ...);

static char bcd(int v)
{
    return (char)(((v/10)%10)*0x10 + v%10) ;
}

int mcu_bootupready(int* D1)
{
	static int mcuready = 0 ;
    int i ;
    if( mcuready ) {
        return 1 ;
    }

printf("BOOT RDY?\n");    
    
    char * rsp = mcu_cmd( 0x11, 1, 1 ) ;
    if( rsp ) {
        int rlen = rsp[0] - 6 ;
        char status[200] ;
        char tst[10] ;
        status[0]=0 ;
        for( i=0; i<rlen; i++ ) {
            sprintf(tst, " %02x", (unsigned int)(unsigned char)rsp[5+i] );
            strcat( status, tst );
        }
        dvr_log( "MCU ready with status :%s", status );
        mcuready = 1 ;
        
        // refer to bill's code
        *D1 = (rsp[6] == 0x80 ) ;
        return 1 ;
    }
    else {
        dvr_log( "MCU not ready!" );
	}
    return 0 ;
}

extern unsigned int iosensor_inverted ;

int mcu_iosensorrequest()
{
	char * rsp = mcu_cmd(0x14) ;
	if( rsp ) {
		  //  printf("inside io request:%x %x %x %x %x %x %x %x\n",responds[0],responds[1],responds[2],responds[3],responds[4], \
		  //        responds[5],responds[6], responds[7]);		  
		unsigned int out_sensor=iosensor_inverted & 0xffff;
		unsigned int res_sensor=rsp[5]|(rsp[6]<<8);
		if(out_sensor == res_sensor){
			return 0; 
		}
		return 1;
	}
	return 0;	
}

int mcu_iosensor_send()
{
    mcu_sendcmd( 0x13, 2, (iosensor_inverted & 0xff), (iosensor_inverted>>8)&0xff ) ;
	return 0 ;
}

// return bcd value
static int getbcd(unsigned char c)
{
    return (int)(c/16)*10 + c%16 ;
}

int mcu_readcode()
{
	char * rsp = mcu_cmd(0x41);
	if( rsp ) {
		struct tm ttm ;
		time_t tt ;
		memset( &ttm, 0, sizeof(ttm) );
		ttm.tm_year = getbcd(rsp[12]) + 100 ;
		ttm.tm_mon  = getbcd(rsp[11]) - 1;
		ttm.tm_mday = getbcd(rsp[10]) ;
		ttm.tm_hour = getbcd(rsp[8]) ;
		ttm.tm_min  = getbcd(rsp[7]) ;
		ttm.tm_sec  = getbcd(rsp[6]) ;
		ttm.tm_isdst= -1 ;
		tt=timegm(&ttm);
		if( (int)tt>0 ) {
			localtime_r(&tt, &ttm);
			dvr_log( "MCU shutdown code: %02x at %04d-%02d-%02d %02d:%02d:%02d",
				(int)rsp[5] ,
				ttm.tm_year+1900,
				ttm.tm_mon+1,
				ttm.tm_mday,
				ttm.tm_hour,
				ttm.tm_min,
				ttm.tm_sec );

		}
		return 1 ;
	}
    return 0 ;
}

// digital output
int mcu_dio_output( unsigned int outputmap )
{
	mcu_sendcmd( 0x31, 1, outputmap );
	return 0 ;
}

// digital input
int mcu_dio_input()
{
	char * rsp = mcu_cmd( 0x1d );
	if( rsp ) {
		return (int)((unsigned char)rsp[5]+256*(unsigned int)(unsigned char)rsp[6]) ; ;
	}
	return 0 ;
}

int mcu_reboot()
{
	mcu_dio_output( 0 ) ;
	mcu_sendcmd( 0x12 );
	return 0 ;
}

// get MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_version(char * version)
{
	mcu_dio_output( 0 ) ;
	char * rsp = mcu_cmd(0x2d);
	if( rsp ) {
		int sz = rsp[0] ;
		memcpy( version, &rsp[5], sz-6 );
		version[sz-6]=0 ;
		return sz-6 ;
	}
	return 0 ;
}

int mcu_camera_zoomin(int zoomin)
{
	if( zoomin ) {
		dvr_log("Camera zoom in.");
		mcu_cmd(0x1f);
	}
	else {        
		dvr_log("Camera zoom out.");
		mcu_cmd(0x20);
	}
	return 1;
}

void mcu_camera_nightmode( int night )
{
	if( night ) {
		dvr_log("Camera night mode on.");
		mcu_cmd_target(4, 0x24, 1, 3 );
	}
	else {        
		dvr_log("Camera night mode off.");
		mcu_cmd_target(4, 0x24, 1, 0 );
	}
}

int mcu_battery_check( int * voltage )
{
	char * rsp = mcu_cmd(0x48);
	if( rsp ) {
//		dvr_log("Battery status :%d", rsp[5] );
		if( voltage ) {
			*voltage = (int)((unsigned char)rsp[7]+256*(unsigned int)(unsigned char)rsp[6]) ; 
		}
		return (int)rsp[5] ;
	}
	return -1 ;
}

// set pan camera mode, 0: pan cam, 1: zoom cam
int mcu_set_pancam( int cameramode )
{
	dvr_log("Select %s.", cameramode?"Zoom Cam":"Pan Cam");
	mcu_cmd(0x57,1,cameramode);
}

int mcu_campower()
{
	dvr_log("CAM2-6 Power.");
	mcu_cmd(0x4b);
	mcu_cmd(0x4c);
	return 0;
}

//  ----------------------------

// 3 LEDs On Panel
//   parameters:
//      led:  0= USB flash LED, 1= Error LED, 2 = Video Lost LED
//      flash: 0=off, 1=flash
void mcu_led(int led, int flash)
{
	mcu_sendcmd( 0x2f, 2, led, flash );
}

// Device Power
//   parameters:
//      device:  0= GPS, 1= Slave Eagle boards, 2=Network switch
//      poweron: 0=poweroff, 1=poweron
void mcu_devicepower(int device, int poweron )
{
	if( mcu_cmd( 0x2e, 2, device, (poweron!=0) ) ) {
		dvr_log( "Device %d %s.", device, poweron?"on":"off");
	}
}

// return 1: success
//        0: failed
int mcu_w_rtc(time_t tt)
{
	struct tm t ; 
	gmtime_r( &tt, &t);
	if( mcu_cmd( 7, 7,
		bcd( t.tm_sec ),
		bcd( t.tm_min ),
		bcd( t.tm_hour ),
		bcd( t.tm_wday ),
		bcd( t.tm_mday ),
		bcd( t.tm_mon + 1 ),
		bcd( t.tm_year ) ) )
	{ 
		  return 1;
	}
	else {
		return 0 ;
	}
}

// return time_t: success
//             -1: failed
int mcu_r_rtc( struct tm * ptm,time_t * rtc )
{
	struct tm ttm ;
    time_t tt ;
	char * responds = mcu_cmd( 0x06 ) ;
	if( responds ) {
		memset( &ttm, 0, sizeof(ttm) );
		ttm.tm_year = getbcd(responds[11]) + 100 ;
		ttm.tm_mon  = getbcd(responds[10]) - 1;
		ttm.tm_mday = getbcd(responds[9]) ;
		ttm.tm_hour = getbcd(responds[7]) ;
		ttm.tm_min  = getbcd(responds[6]) ;
		ttm.tm_sec  = getbcd(responds[5]) ;
		ttm.tm_isdst= -1 ;

		if( ttm.tm_year>110 && ttm.tm_year<150 &&
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
				if(rtc){
					*rtc=tt;
				}
				return (int)tt ;
			}
		}
	}
    dvr_log("Read clock from MCU failed:06");    
    return 0;
}

void mcu_readrtc()
{
    time_t t ;
    struct tm ttm ;
    int ret;
    ret = mcu_r_rtc(&ttm,&t) ;
    if( ret ) {
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

extern int mcupowerdelaytime;
static int delay_inc = 20 ;

// set more mcu power off delay (keep power alive), (called every few seconds)
void mcu_poweroffdelay()
{
	// 08 command is very confusing in documents. ????
	//   08 described as a msg from MCU not a command
	// I just keep using older version
	int delay ;
	if( mcupowerdelaytime < 80 ) {
        delay=delay_inc;
    }
    else {
        delay=0 ;
    }
	char * rsp = mcu_cmd( 0x08, 2, 0, delay);
	if( rsp ) {
		mcupowerdelaytime = ((unsigned)(rsp[5]))*256+((unsigned)rsp[6]) ;
#ifdef MCU_DEBUG        
		printf("mcu delay %d s\n", mcupowerdelaytime );
#endif   
	}
}

void mcu_poweroffdelay_N(int delay)
{
	// 08 command is very confusing in documents. ????
	//   08 described as a msg from MCU not a command
	// I just keep using older version
	char * rsp = mcu_cmd( 0x08, 2, (delay >> 8) & 0xff, delay & 0xff );
	if( rsp ) {
		mcupowerdelaytime = ((unsigned)(rsp[5]))*256+((unsigned)rsp[6]) ;
#ifdef MCU_DEBUG        
		printf("mcu delay %d s\n", mcupowerdelaytime );
#endif   
	}
}

void mcu_setwatchdogtime(int timeout)
{
	mcu_cmd( 0x19, 2, timeout/256, timeout%256 );
}

void mcu_watchdogenable()
{
	extern int usewatchdog ;
	extern int watchdogtimeout ;
	if( usewatchdog ) {
		mcu_cmd( 0x19, 2, watchdogtimeout/256, watchdogtimeout%256 );
		mcu_cmd( 0x1a );
		dvr_log("mcu_watchdogenable");    
	}
	else {
		mcu_cmd( 0x1b );
	}
}

int mcu_watchdogdisable()
{
	char * responds = mcu_cmd(0x1b);
	if( responds ) {
		return responds[5] ;
	}
	return 0 ;
}

void mcu_watchdogkick()
{
	mcu_sendcmd(0x18);
}

// get io board temperature
int mcu_iotemperature()
{
    char * responds = mcu_cmd( 0x0b );
    if( responds ) {
	    return (int)responds[5] ;
	}
   
    return -128;
}

// get hd temperature
int mcu_hdtemperature(int *hd1,int *hd2)
{
	char * responds = mcu_cmd( 0x0c );
	if( responds ) {
		*hd1=(int)(signed char)responds[5] ;
		*hd2=(int)(signed char)responds[7] ;
		return 0 ;
	}
	else {
		return -1 ;
	}
}

void mcu_poe_power( int power )
{
	dvr_log("POE Power: %d", power);
	mcu_cmd(0x3a, 1, power);
}

void mcu_poepoweron()
{
	mcu_poe_power(1);
}

void mcu_poepoweroff()
{
	mcu_poe_power(0);
}

void mcu_wifipower(int power)
{
	dvr_log("MCU WiFi Power: %d", power);
	mcu_cmd(0x38, 1, power);
}

void mcu_wifipoweron()
{
	mcu_wifipower(1);
}

void mcu_wifipoweroff()
{
	mcu_wifipower(0);
}

void mcu_motioncontrol_enable()
{
    dvr_log("mcu_motioncontrol_enable.");
	mcu_cmd(0x13, 1, 1);
}

void mcu_motioncontrol_disable()
{
    dvr_log("mcu_motioncontrol_disable.");
	mcu_cmd(0x13, 1, 0);
}

void mcu_hdpoweron()
{
	dvr_log( "HD power on.");
    mcu_cmd( 0x28 );
}

void mcu_hdpoweroff()
{
    dvr_log( "HD power off.");
    mcu_cmd( 0x29 );
}

void mcu_mic_on( int mic )
{
	int cmd ;
	if( mic==0 ) {
		cmd = 0x21 ;
	}
	else {
		cmd = 0x52 ;
	}
    mcu_cmd( cmd );
	dvr_log("Turn on mic %d.", mic+1);

}

void mcu_mic_off( int mic )
{
	int cmd ;
	if( mic==0 ) {
		cmd = 0x22 ;
	}
	else {
		cmd = 0x53 ;
	}
    mcu_cmd( cmd );

	dvr_log("Turn off mic %d.", mic+1);
}

// command 5b, 
void mcu_mic_ledoff()
{
	dvr_log("All mic turn off.");
    mcu_cmd( 0x5b );
}

static int convertmode = 0 ;

// covert mode ( turn off LEDS on ip cameras )
void mcu_covert( int coverton )
{
	if( coverton != 0 ) {
		dvr_log( "Enter covert mode." );
		mcu_cmd(0x3d, 1, 1 );
		mcu_cmd(0x3c, 1, 0 );
		convertmode = 1 ;
	}
	else {
		dvr_log( "Leave covert mode." );
		mcu_cmd(0x3d, 1, 0 );
		convertmode = 0 ;
	}
}

// turn amber led on / off
void mcu_amberled(int ledon)
{
	int amber = ledon && (!convertmode) ;
	mcu_cmd(0x5d, 1, amber);
}


// to receive "Enter a command" , success: return 1, failed: return 0
static int mcu_recv_enteracommand()
{
	int r ;
    int res = 0 ;
    char enteracommand[200] ;
    while( mcu_dataready(100000) ) {
		r = mcu_read( enteracommand, sizeof(enteracommand), 1000000, 100000 ) ;
		if( r>0 ) {
			enteracommand[r]=0 ;
			if( strcasestr( enteracommand, "Enter a command" )) {
				res=1 ;
			}
		}
    }
    return res ;
}

// 
int mcu_reset()
{
    int retry=0;
    for(retry=0;retry<3;++retry){
	if( mcu_sendcmd( 0 ) ) {
	    if( mcu_dataready (30000000) ) {
		if( mcu_recv_enteracommand() ) {
		    return 1;
		}
	    }
	}
    }
    dvr_log("mcu_reset:00");
    return 0 ;
}


