#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <sys/stat.h>

#include "../cfg.h"

#include "../eaglesvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/config.h"

#include "mcu.h"
#include "gforce.h"
#include "diomap.h"
#include "iomisc.h"
#include "yagf.h"
#include "cdc.h"

#ifdef PWII_APP
int pwii_tracemark_inverted=0 ;
int pwii_rear_ch ;
int pwii_front_ch ;

unsigned int pwii_keytime ;
unsigned int pwii_keyreltime ;
unsigned int pwii_outs = 0 ;

// Set C1/C2 LED and set external mic
void mcu_pwii_setc1c2()
{
    // C1 LED (front)
    dio_lock();
    if( p_dio_mmap->camera_status[pwii_front_ch] & 2 ) {         // front camera recording?
        if( (p_dio_mmap->pwii_output & PWII_LED_C1 ) == 0 ) {
            p_dio_mmap->pwii_output |= PWII_LED_C1 ;
            dio_unlock();
            // turn on zoomcamera led
            zoomcam_led(1) ;
            // turn on mic
            mcu_cmd(NULL, MCU_CMD_MICON) ;
            dio_lock();
        }
    }
    else {
        if( (p_dio_mmap->pwii_output & PWII_LED_C1) != 0 ) {
            p_dio_mmap->pwii_output &= (~PWII_LED_C1) ;
            dio_unlock();
            // turn off zoomcamera led
            zoomcam_led(0) ;
            // turn off mic
            mcu_cmd(NULL, MCU_CMD_MICOFF);
            dio_lock();
        }
    }

    // C2 LED
    if( p_dio_mmap->camera_status[pwii_rear_ch] & 2 ) {         // rear camera recording?
        p_dio_mmap->pwii_output |= PWII_LED_C2 ;
    }
    else {
        p_dio_mmap->pwii_output &= (~PWII_LED_C2) ;
    }
    dio_unlock();
}

// update PWII outputs, include leds and powers
void mcu_pwii_output()
{
    unsigned int pwii_xouts ;
    unsigned int xouts ;

    mcu_pwii_setc1c2()  ;       // check c1 c2 led.
    pwii_xouts = p_dio_mmap->pwii_output ;

    if( (p_dio_mmap->iomode == IOMODE_RUN || p_dio_mmap->iomode == IOMODE_SHUTDOWNDELAY)
        && (pwii_xouts & PWII_POWER_BLACKOUT) )
    {
        pwii_xouts &= ~( PWII_LED_POWER |
                         PWII_LED_C1 |
                         PWII_LED_C2 |
                         PWII_LED_MIC |
                         PWII_LED_ERROR |
                         PWII_POWER_LCD ) ;
    }

    xouts = pwii_outs ^ pwii_xouts ;
    pwii_outs = pwii_xouts ;

    if( xouts & PWII_POWER_BLACKOUT ) {         // Black out bit
        mcu_pwii_cmd(NULL, PWII_CMD_STANDBY, 1, (pwii_outs&PWII_POWER_BLACKOUT)!=0 );
        if( (pwii_outs&PWII_POWER_BLACKOUT)==0 ) {
            p_dio_mmap->pwii_output |= PWII_LED_POWER ;			// out of blackout also turn on POWER LED
        }
    }

    static int s_led_power_refresh_time ;
    if( (xouts & PWII_LED_POWER) || runtime-s_led_power_refresh_time > 10000 ) {
        // BIT 4: POWER LED
        mcu_pwii_cmd(NULL, PWII_CMD_LEDPOWER, 1, ((pwii_outs&PWII_LED_POWER)?1:0) );
        s_led_power_refresh_time = runtime ;
    }

    static int s_led_c1_refresh_time ;
    if( (xouts & PWII_LED_C1) || runtime-s_led_c1_refresh_time>10000 ) {
        mcu_pwii_cmd(NULL, PWII_CMD_C1, 1, ((pwii_outs&PWII_LED_C1)?1:0) );
        s_led_c1_refresh_time = runtime ;
    }

    static int s_led_c2_refresh_time ;
    if( (xouts & PWII_LED_C2) || runtime-s_led_c2_refresh_time>10000 ) {
        mcu_pwii_cmd(NULL, PWII_CMD_C2, 1, ((pwii_outs&PWII_LED_C2)?1:0) );
        s_led_c2_refresh_time = runtime ;
    }

    static int s_led_mic_refresh_time ;
    if( (xouts & PWII_LED_MIC) || runtime-s_led_mic_refresh_time>10000 ) {
        mcu_pwii_cmd(NULL, PWII_CMD_LEDMIC, 1, ((pwii_outs&PWII_LED_MIC)?1:0) );
        s_led_mic_refresh_time = runtime ;
    }

    if( xouts & PWII_LP_ZOOMIN ) {
        mcu_camera_zoom( pwii_outs & PWII_LP_ZOOMIN ) ;
    }

    if( xouts & PWII_LED_ERROR ) {           // bit 3: ERROR LED
        if((pwii_outs&PWII_LED_ERROR)!=0) {
            if( p_dio_mmap->pwii_error_LED_flash_timer>0 ) {
                mcu_pwii_cmd(NULL, PWII_CMD_LEDERROR, 2, 2, p_dio_mmap->pwii_error_LED_flash_timer );
            }
            else {
                mcu_pwii_cmd(NULL, PWII_CMD_LEDERROR, 2, 1, 0 );
            }
        }
        else {
            mcu_pwii_cmd(NULL, PWII_CMD_LEDERROR, 2, 0, 0 );
        }
    }

    if( xouts & PWII_POWER_LCD ) {          	// LCD  power
        mcu_pwii_cmd(NULL, PWII_CMD_LCD, 1, ((pwii_outs&PWII_POWER_LCD)!=0) );
    }

    if( xouts & PWII_POWER_ANTENNA ) {          // BIT 8: GPS antenna power
        mcu_pwii_cmd(NULL, PWII_CMD_POWER_GPSANT, 1, ((pwii_outs&PWII_POWER_ANTENNA)!=0) );
    }

    if( xouts & PWII_POWER_GPS ) {         // BIT 9: GPS POWER
        mcu_pwii_cmd(NULL, PWII_CMD_POWER_GPS, 1, ((pwii_outs&PWII_POWER_GPS)!=0) );
    }

    if( xouts & PWII_POWER_RF900 ) {          // BIT 10: RF900 POWER
        mcu_pwii_cmd(NULL, PWII_CMD_POWER_RF900, 1, ((pwii_outs&PWII_POWER_RF900)!=0) );
    }

    if( xouts & PWII_POWER_WIFI ) {          // BIT 13: WIFI power
        mcu_pwii_cmd(NULL, PWII_CMD_POWER_WIFI, 1, ((pwii_outs&PWII_POWER_WIFI)!=0) );
    }

}

// auto release keys REC, C2, TM
void pwii_keyautorelease()
{
    void mode_poweroff();
    if( p_dio_mmap->pwii_buttons != 0 ) {
        if( (runtime-pwii_keytime) > pwii_keyreltime ) {
            p_dio_mmap->pwii_buttons = 0 ;
            p_dio_mmap->iomsg[0]=0 ;
        }
        if( p_dio_mmap->poweroff && (p_dio_mmap->pwii_buttons & PWII_BT_POWER) ) {
            if( (runtime-pwii_keytime) > 3000 ) {			// POWER/ST button pressed for 3 seconds
                mode_poweroff();
                pwii_keytime=runtime ;
            }
        }
    }
}

void mcu_pwii_init()
{
    pwii_outs = mcu_pwii_ouputstatus() ;
    p_dio_mmap->pwii_output =
            PWII_LED_POWER |
            PWII_POWER_ANTENNA |
            PWII_POWER_GPS |
            PWII_POWER_RF900 |
            PWII_POWER_LCD ;
}

void  mcu_pwii_input(char * ibuf)
{
    mcu_pwii_cdcfailed = 0 ;                        // clear pwii cdc error!
    pwii_keytime = runtime ;
    pwii_keyreltime = 20000 ;                       // all keys will be auto-released in 20 sec, or set bellow
    switch( ibuf[3] ) {
    case PWII_INPUT_REC :                        // Front Camera (REC) button
        p_dio_mmap->pwii_buttons = PWII_BT_C1 ;  // bit 8: front camera
        pwii_keyreltime = 1000 ;         		// auto release in 1 sec
        if( (p_dio_mmap->pwii_output&PWII_POWER_STANDBY)==0 ) {       // not in standby mode
            mcu_response( ibuf, 1, ((p_dio_mmap->pwii_output&PWII_LED_C1)!=0) );  // bit 0: c1 led
        }
        else {
            mcu_response( ibuf, 1, 0 );  // bit 0: c1 led
        }
        break;

    case PWII_INPUT_C2 :                         // Back Seat Camera (C2) Starts/Stops Recording
        p_dio_mmap->pwii_buttons = PWII_BT_C2 ;      // bit 9: back camera
        pwii_keyreltime = 1000 ;         			// auto release in 1 sec
        if( (p_dio_mmap->pwii_output&PWII_POWER_STANDBY)==0 ) {       // not in standby mode
            mcu_response( ibuf, 1, ((p_dio_mmap->pwii_output&PWII_LED_C2)!=0) );  // bit 1: c2 led
        }
        else {
            mcu_response( ibuf, 1, 0 );  // bit 0: c1 led
        }
        break;

    case PWII_INPUT_TM :                                 // TM Button
        if( ibuf[5] ) {
            p_dio_mmap->pwii_buttons = PWII_BT_TM ;     // bit 10: tm button
        }
        else {
            pwii_keyreltime = 500 ;         	// extend TM key release 0.5 second. (allow dvrsvr to response)
        }
        mcu_response( ibuf );

        break;

    case PWII_INPUT_LP :                                 // LP button
        mcu_response( ibuf );
        if( ibuf[5] ) {
            p_dio_mmap->pwii_buttons = PWII_BT_LP ;     // bit 11: LP button
        }
        else {
            pwii_keyreltime = 500 ;         	// extend LP key release 0.5 second.
        }
        break;

    case PWII_INPUT_BO :                                 // BIT 12:  blackout, 1: pressed, 0: released, auto release
        if( ibuf[5] ) {
            p_dio_mmap->pwii_buttons = PWII_BT_BO ;    // bit 12: blackout
        }
        else {
            p_dio_mmap->pwii_buttons = 0 ;
        }
        mcu_response( ibuf );
        break;

    case PWII_INPUT_MEDIA :                              // Video play Control
        p_dio_mmap->pwii_buttons = (unsigned char) (~ibuf[5]) ;
        mcu_response( ibuf );
        break;

    case PWII_INPUT_BOOTUP :                         // CDC ask for boot up ready
        mcu_response( ibuf, 1, 1  );                 // possitive response

        mcu_pwii_bootupready() ;					//  re-send boot up event. (Have to do this, because possitive response do not work on newer CDC

        pwii_outs = mcu_pwii_ouputstatus();

        dvr_log("CDC Plug-in Event!");

        break;

    case PWII_INPUT_SPEAKERSTATUS :
        mcu_response( ibuf );                 		// possitive response
        if( ibuf[5] ) {
            p_dio_mmap->pwii_buttons = PWII_BT_SPKON ;

            dvr_log("Speaker On Event!");

        }
        else {
            p_dio_mmap->pwii_buttons = PWII_BT_SPKMUTE ;

            dvr_log("Speaker Mute Event!" );

        }
        pwii_keyreltime = 3000 ;         			// auto release in 3 sec
        mcu_response( ibuf );                 		// possitive response

        break;

    default :
        netdbg_print("Unknown message from PWII MCU.\n");
        break;

    }
}

int mcu_pwii_cdcfailed ;

// Send command to pwii and check responds
// return
//       size of response
//       0 if failed
int mcu_pwii_cmd(char * rsp, int cmd, int datalen, ...)
{
    int res ;
    va_list v ;
    if( mcu_pwii_cdcfailed ) {  // don't retry on failed cdc, could be not connected
        return 0 ;
    }
    va_start( v, datalen );
    res = mcu_cmd_va( ID_CDC, rsp, cmd, datalen, v );
    if( res<=0 ) {
        dvr_log( "CDC communication lost!" ) ;
        mcu_pwii_cdcfailed = 1 ;
        return 0 ;
    }
    return res ;
}

int mcu_pwii_bootupready()
{
    if( mcu_pwii_cmd(NULL, PWII_CMD_BOOTUPREADY)>0 ) {
        return 1 ;
    }
    mcu_pwii_cdcfailed = 1 ;
    return 0 ;
}

// get pwii MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_pwii_version(char * version)
{
    int versionlen ;
    char rsp[MCU_MAX_MSGSIZE] ;
    if( mcu_pwii_cmd(rsp, PWII_CMD_VERSION)>0 ) {
        versionlen = rsp[0]-6 ;
        memcpy( version, &rsp[5], versionlen );
        version[versionlen]=0 ;
        return versionlen;
    }
    return 0 ;
}

unsigned int mcu_pwii_ouputstatus()
{
    unsigned int outputmap = 0 ;
    char rsp[MCU_MAX_MSGSIZE] ;
    if( mcu_pwii_cmd(rsp, PWII_CMD_OUTPUTSTATUS, 2, 0, 0 )>0 ) {
        if( rsp[6] & 1 ) outputmap|=PWII_LED_C1 ;        // C1 LED
        if( rsp[6] & 2 ) outputmap|=PWII_LED_C2 ;        // C2 LED
        if( rsp[6] & 4 ) outputmap|=PWII_LED_MIC ;           // MIC
        if( rsp[6] & 0x10 ) outputmap|=PWII_LED_ERROR ;      // Error
        if( rsp[6] & 0x20 ) outputmap|=PWII_LED_POWER ;      // POWER LED
        if( rsp[6] & 0x40 ) outputmap|=PWII_LED_BO ;  	 	 // BO_LED
        if( rsp[6] & 0x80 ) outputmap|=PWII_LED_BACKLIGHT ;  // Backlight LED

        if( rsp[5] & 1 ) outputmap|=PWII_POWER_LCD ;      // LCD POWER
        if( rsp[5] & 2 ) outputmap|=PWII_POWER_GPS ;      // GPS POWER
        if( rsp[5] & 4 ) outputmap|=PWII_POWER_RF900 ;    // RF900 POWER
    }
    return outputmap ;
}

// Check CDC speaker volume
// return 0: speaker off, 1: speaker on
int mcu_pwii_speakervolume()
{
    char rsp[MCU_MAX_MSGSIZE] ;
    if( mcu_pwii_cmd(rsp, PWII_CMD_SPEAKERVOLUME, 0 )>0 ) {
        return rsp[5] ;
    }
    return 0 ;
}

#endif  // PWII_APP
