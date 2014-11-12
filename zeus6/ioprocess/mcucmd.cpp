
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

int mcu_bootupready(int* D1)
{
	static int mcuready = 0 ;
    int i ;
    char rsp[MCU_MAX_MSGSIZE] ;
    if( mcuready ) {
        return 1 ;
    }
    if( mcu_cmd(rsp, 0x11, 1, 0 )>0 ) {
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
    return 0 ;
}

extern unsigned int iosensor_inverted ;

int mcu_iosensorrequest()
{
	// door input status check
    char responds[MCU_MAX_MSGSIZE] ;
    if( mcu_cmd(responds, 0x14)>0 ) {
		  //  printf("inside io request:%x %x %x %x %x %x %x %x\n",responds[0],responds[1],responds[2],responds[3],responds[4], \
		  //        responds[5],responds[6], responds[7]);		  
		unsigned int out_sensor=iosensor_inverted & 0xffff;
		unsigned int res_sensor=responds[5]|(responds[6]<<8);
		if(out_sensor == res_sensor){
			return 0; 
		}
		return 1;
	}
	return 0;	
}

int mcu_iosensor_send()
{
	char responds[MCU_MAX_MSGSIZE] ;
    if( mcu_cmd(responds, 0x13, 2, (iosensor_inverted & 0xff), (iosensor_inverted>>8)&0xff )>0 ) {
		return 1 ;
	}
	return 0 ;
}

// return bcd value
static int getbcd(unsigned char c)
{
    return (int)(c/16)*10 + c%16 ;
}

int mcu_readcode()
{
    char responds[MCU_MAX_MSGSIZE] ;
    if( mcu_cmd(responds, 0x41)>0 ) {
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
				(int)responds[5] ,
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
	char responds[MCU_MAX_MSGSIZE] ;
    if( mcu_cmd(responds, 0x31, 1, (int)(unsigned char)outputmap ) > 0 ) {
		return 1 ;
	}
	return 0 ;
}

// digital input
int mcu_dio_input()
{
	char responds[MCU_MAX_MSGSIZE] ;
    if( mcu_cmd(responds, 0x1d ) > 0 ) {
		return (int)((unsigned char)responds[5]+256*(unsigned int)(unsigned char)responds[6]) ; ;
	}
	return 0 ;
}

int mcu_reboot()
{
	char responds[MCU_MAX_MSGSIZE] ;
	mcu_dio_output( 0 ) ;
    if( mcu_cmd(responds, 0x12) > 0 ) {
		return 1 ;
	}
	return 0 ;
}

// get MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_version(char * version)
{
	char responds[MCU_MAX_MSGSIZE] ;
	mcu_dio_output( 0 ) ;
    if( mcu_cmd(responds, 0x2d) > 0 ) {
		int sz = responds[0] ;
		memcpy( version, &responds[5], sz-6 );
		version[sz-6]=0 ;
		return sz-6 ;
	}
	return 0 ;
}

int mcu_camera_zoomin(int zoomin)
{
	char responds[MCU_MAX_MSGSIZE] ;
	int cmd ;
	if( zoomin ) cmd=0x1f ;
	else         cmd=0x20 ;
	
    if( mcu_cmd(responds, cmd) > 0 ) {
		return 1;
	}
	return 0 ;
}

int mcu_battery_check( int * voltage )
{
	char responds[MCU_MAX_MSGSIZE] ;
    if( mcu_cmd(responds, 0x48) > 0 ) {
		dvr_log("Battery status :%d", responds[5] );
		if( voltage ) {
			*voltage = (int)((unsigned char)responds[7]+256*(unsigned int)(unsigned char)responds[6]) ; ;
		}
		return (int)responds[5] ;
	}
	return -1 ;
}

// set pan camera mode, 0: pan cam, 1: zoom cam
int mcu_set_pancam( int cameramode )
{
	char responds[MCU_MAX_MSGSIZE] ;
    return mcu_cmd(responds, 0x57, 1, cameramode) > 0 ;
}

int mcu_poe_power( int onoff )
{
	char responds[MCU_MAX_MSGSIZE] ;
	int on ;
	if( onoff ) on=1 ;
	else        on=0 ;
	
    if( mcu_cmd(responds, 0x3a, 1, on ) > 0 ) {
		return 1;
	}
	return 0 ;
}

int mcu_campower()
{
	char responds[MCU_MAX_MSGSIZE] ;
	mcu_cmd(responds, 0x4b);
	mcu_cmd(responds, 0x4c);
	return 0;
}

