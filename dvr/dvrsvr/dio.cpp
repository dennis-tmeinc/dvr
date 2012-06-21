#include "dvr.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>

// #define DEBUGGING

#include "../ioprocess/diomap.h"

static unsigned int dio_inputmap ;
int dio_record;
int dio_capture;

int dio_inputnum()
{
    // atomic op, no lock nessissary
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        return p_dio_mmap->inputnum ;
    }
    return 0;
}

int dio_outputnum()
{
    // atomic op, no lock nessissary
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        return p_dio_mmap->outputnum ;
    }
    return 0 ;
}

int dio_input( int no )
{
    return ((dio_inputmap)>>no)&1 ;
}

void dio_output( int no, int v)
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        if( v ) {
            p_dio_mmap->outputmap |= 1<<no ;
        }
        else {
            p_dio_mmap->outputmap &= ~(1<<no) ;
        }
    }
    dio_unlock();
}

// return 1 for poweroff switch turned off
/*
int dio_poweroff()
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
        return p_dio_mmap->poweroff ;
    }
    return (0);
}
*/

int dio_lockpower()
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        p_dio_mmap->lockpower = 1 ;
    }
    dio_unlock();
    return (0);
}

int dio_unlockpower()
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        p_dio_mmap->lockpower = 0;
    }
    dio_unlock();
    return (0);
}

int dio_enablewatchdog()
{
    dio_lock();
    if( p_dio_mmap ){
#ifdef  DEBUGGING
        p_dio_mmap->dvrwatchdog = -1 ;
#else
        p_dio_mmap->dvrwatchdog = 0 ;
#endif		
    }
    dio_unlock();
    return (0);
}

int dio_disablewatchdog()
{
    dio_lock();
    if( p_dio_mmap ){
        p_dio_mmap->dvrwatchdog = -1 ;
    }
    dio_unlock();
    return (0);
}

int dio_kickwatchdog()
{
    dio_lock();
    if( p_dio_mmap &&
       p_dio_mmap->dvrwatchdog>=0 )
    {
        p_dio_mmap->dvrwatchdog = 0 ;
    }
    dio_unlock();
    return (0);
}

// set dvr status bits
int dio_setstate( int status ) 
{
#ifdef PWII_APP
    int rstart = 0 ;
#endif    
    dio_lock();
    if( p_dio_mmap ){
#ifdef PWII_APP
        if( (status & DVR_LOCK)!=0 &&
            (p_dio_mmap->dvrstatus & DVR_LOCK)==0 ) 
        {
                // start recording
                struct dvrtime cliptime ;
                time_now(&cliptime) ;
                sprintf( g_vri, "%s-%02d%02d%02d%02d%02d", 
                        (char *)g_servername,
                        cliptime.year%100,
                        cliptime.month,
                        cliptime.day,
                        cliptime.hour,
                        cliptime.minute
                        );
                memcpy( p_dio_mmap->pwii_VRI, g_vri, sizeof(p_dio_mmap->pwii_VRI) );
                rstart = 1 ;
        }
#endif    
       	p_dio_mmap->dvrstatus |= status ;
    }
    dio_unlock();
#ifdef PWII_APP    
    if( rstart ) {
        dvr_log( "Recording started, VRI: %s", g_vri);
    }
#endif
    return 0 ;
}

// clear dvr status
int dio_clearstate( int status )
{
#ifdef PWII_APP
    int rstart = 1 ;
#endif    
    dio_lock();
    if( p_dio_mmap ){
#ifdef PWII_APP
        if( status == DVR_LOCK &&
            (p_dio_mmap->dvrstatus & DVR_LOCK)!=0 ) 
        {
            // stop recording
            rstart = 0 ;
            g_vri[0]=0 ;
            p_dio_mmap->pwii_VRI[0]=0 ;
        }
#endif    
       	p_dio_mmap->dvrstatus &= ~status ;
    }
    dio_unlock();
#ifdef PWII_APP
    if( rstart==0 ) {
        dvr_log( "Recording stopped.");
    }
#endif    
    return 0 ;
}

// check if io mode allow archiving
int dio_mode_archive()
{
    int iomode = 0 ; ;
    if( p_dio_mmap ){
        iomode = p_dio_mmap->iomode ;
    }
    if( iomode == IOMODE_ARCHIVE ||
        iomode == IOMODE_RUN ||
        iomode == IOMODE_SHUTDOWNDELAY )
    {
        return 1 ;
    }
    else {
        return 0 ;
    }
}

// set video channel status
// parameter :  channel = camera channle
//              ch_state = channel status ,  bit 0: signal, bit 1: recording, bit 2: motion, 3: video data
void dio_setchstat( int channel, int ch_state )
{
    if( channel < 8 ) {
        dio_lock();
        p_dio_mmap->camera_status[channel] = ch_state ;
        dio_unlock();
    }
}

#ifdef    PWII_APP

int pwii_front_ch ;         // pwii front camera channel
int pwii_rear_ch ;          // pwii real camera channel

// return 1 : key event, 0: no key event 
int dio_getpwiikeycode( int * keycode, int * keydown)
{
    static struct key_map_t {
        unsigned int key_bit ;
        unsigned int key_code ;
    } keymap[] = {
        { PWII_BT_REW, (int) VK_MEDIA_PREV_TRACK },
        { PWII_BT_PP,  (int) VK_MEDIA_PLAY_PAUSE },
        { PWII_BT_FF,  (int) VK_MEDIA_NEXT_TRACK },
        { PWII_BT_ST,  (int) VK_MEDIA_STOP },
        { PWII_BT_PR,  (int) VK_PRIOR },
        { PWII_BT_NX,  (int) VK_NEXT},
        { PWII_BT_C1,  (int) VK_FRONT },
        { PWII_BT_C2,  (int) VK_REAR },
        { PWII_BT_TM,  (int) VK_TM },
        { PWII_BT_LP,  (int) VK_LP},
        { PWII_BT_BO,  (int) VK_POWER },
        { PWII_BT_SPKMUTE,(int) VK_MUTE },
        { PWII_BT_SPKON,  (int) VK_SPKON },
        {0,0}
    } ;
    static unsigned int dio_pwii_bt_s ;

    unsigned int xkey ;
    unsigned int dio_pwii_bt_n ;
    if( p_dio_mmap ) {
        dio_pwii_bt_n = p_dio_mmap->pwii_buttons ;
    }
    else {
        dio_pwii_bt_n = 0 ;
    }
    xkey = dio_pwii_bt_s ^ dio_pwii_bt_n ;

    if( xkey ) {
        int i;
        for( i = 0 ; i<32 ; i++ ) {
            if( keymap[i].key_bit==0 ) break;
            if( xkey & keymap[i].key_bit ) {
                * keycode = keymap[i].key_code ;
                * keydown = (dio_pwii_bt_n&keymap[i].key_bit)!=0 ;
                dio_pwii_bt_s ^=  keymap[i].key_bit ;
                return 1 ;
            }
        }
        dio_pwii_bt_s = dio_pwii_bt_n ;
    }
    return 0 ;
}

void dio_pwii_lcd( int lcdon )
{
    if( p_dio_mmap ) {
        dio_lock();
        if( lcdon ) {
            p_dio_mmap->pwii_output |= PWII_POWER_LCD ;
        }
        else {
            p_dio_mmap->pwii_output &= ~PWII_POWER_LCD ;
        }
        dio_unlock();
    }
}

void dio_pwii_standby( int standby )
{
    if( p_dio_mmap ) {
        dio_lock();
        if( standby ) {
            p_dio_mmap->pwii_output |= PWII_POWER_STANDBY ;
        }
        else {
            p_dio_mmap->pwii_output &= ~PWII_POWER_STANDBY ;
        }
        dio_unlock();
    }
}

void dio_pwii_lpzoomin( int on )
{
    if( p_dio_mmap ) {
        dio_lock();
        if( on ) {
            p_dio_mmap->pwii_output |= PWII_LP_ZOOMIN ;
        }
        else {
            p_dio_mmap->pwii_output &= ~PWII_LP_ZOOMIN ;
        }
        dio_unlock();
    }
}

#endif    // PWII_APP

void dio_smartserveron(char * interface)
{
    int log=0 ;
    if( p_dio_mmap ){
        dio_lock();
        if( p_dio_mmap->smartserver==0 ) {
            p_dio_mmap->smartserver=1;
            strncpy( p_dio_mmap->smartserver_interface, interface, sizeof(p_dio_mmap->smartserver_interface) );
            log=1 ;
        }
        dio_unlock();
    }
    if( log ) {
        dvr_log( "Smart server detected!" );
    }
}

void dio_checkwifi()
{
    if( p_dio_mmap ){
        if( p_dio_mmap->iomode==IOMODE_DETECTWIRELESS ) {
            // probe smartserver
            net_detectsmartserver();
        }
    }
}

// checking io maps and dvr commands, return if io pins changed after last check
int dio_check()
{
    int res=0 ;

    if( p_dio_mmap && p_dio_mmap->iopid ){
        int iocmd ;
        dio_lock();
        iocmd = p_dio_mmap->dvrcmd ;
        p_dio_mmap->dvrcmd = 0 ;
        dio_unlock();
        // dvrcmd :  1: restart(resume), 2: suspend, 3: stop record, 4: start record, 5: start archiving
        if( iocmd ) {
            res=1 ;
            switch (iocmd) {
            case 1:
                app_state = APPRESTART ;
                break;
            case 2:
                app_state = APPDOWN ;
                break;
            case 3:
                rec_stop();
                break;
            case 4:
                rec_start();
                break;
            case 5:
                dio_lock();
                p_dio_mmap->dvrstatus |= DVR_ARCH ;
                dio_unlock();
                disk_archive_start();
                break;
            case 6:
                // disk_archive_stop();         // 2011-12-19. Archiving is runing all the time
                break;
            case 7:				// update on screen message
                // screen_update();
                break;
            default:
                res=0 ;
                break;
            }
        }

        dio_lock();
        dio_record = ( p_dio_mmap->iomode == IOMODE_RUN || p_dio_mmap->iomode == IOMODE_SHUTDOWNDELAY ) ;
        if( dio_inputmap != p_dio_mmap->inputmap ) {
            dio_inputmap = p_dio_mmap->inputmap ;
            res = 1 ;
        }
        dio_unlock();
    }

    return res ;
}

char * dio_getiomsg()
{
    char * iomsg=NULL ;
    dio_lock();
    if( p_dio_mmap ){
        iomsg = p_dio_mmap->iomsg ;
    }
    dio_unlock();
    return iomsg ;
}

// set usb (don't remove) led
void dio_usb_led(int v)
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        if( v ) {
            p_dio_mmap->panel_led |= 1 ;
        }
        else {
            p_dio_mmap->panel_led &= ~1 ;
        }
    }
    dio_unlock();
}

// set error led
void dio_error_led(int v)
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        if( v ) {
            p_dio_mmap->panel_led |= 2 ;
        }
        else {
            p_dio_mmap->panel_led &= ~2 ;
        }
    }
    dio_unlock();
}

// set video lost led
void dio_videolost_led(int v)
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        if( v ) {
            p_dio_mmap->panel_led |= 4 ;
        }
        else {
            p_dio_mmap->panel_led &= ~4 ;
        }
    }
    dio_unlock();
}

// turn onoff all device power, (enter standby mode)
// onoff, 0: turn device off, others:device poweron
void dio_devicepower(int onoffmaps)
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        p_dio_mmap->devicepower = onoffmaps ;
    }
    dio_unlock();
}

// return 0: no g-force data, 1: new g-force data, 2: existing g-force data
int dio_getgforce( float * gf, float * gr, float *gd )
{
    static int gforceupdtime ;
    static int gforceserialno ;
    int v=0 ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->gforce_serialno ) {
        if( gforceserialno !=  p_dio_mmap->gforce_serialno ) {
            v=1 ;
            gforceupdtime=g_timetick ;
            gforceserialno = p_dio_mmap->gforce_serialno ;
        }
        else if( (g_timetick-gforceupdtime)<5000 ) {
            v=2 ;
        }
        if( v ) {
            *gf = p_dio_mmap->gforce_forward ;
            *gr = p_dio_mmap->gforce_right ;
            *gd = p_dio_mmap->gforce_down ;
        }
    }
    dio_unlock();
    return v;
}

int gps_location( double * latitude, double * longitude, double * speed )
{
    int v = 0 ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->glogpid>0 && p_dio_mmap->gps_valid ) {
        *latitude = p_dio_mmap->gps_latitude ;
        *longitude = p_dio_mmap->gps_longitud ;
        *speed = p_dio_mmap->gps_speed ;
        v = 1 ;
    }
    dio_unlock();
    return v;
}

void rtc_settime()
{
    struct dvrtime dvrt ;
    time_utctime(&dvrt);
    struct tm rtctime ;
    int hrtc = open("/dev/rtc", O_WRONLY );
    if( hrtc>0 ) {
        memset( &rtctime, 0, sizeof(rtctime));
        rtctime.tm_year   = dvrt.year-1900 ;
        rtctime.tm_mon    = dvrt.month-1 ;
        rtctime.tm_mday   = dvrt.day ;
        rtctime.tm_hour   = dvrt.hour ;
        rtctime.tm_min    = dvrt.minute ;
        rtctime.tm_sec    = dvrt.second ;
        ioctl( hrtc, RTC_SET_TIME, &rtctime ) ;
        close( hrtc );
    }
}

// sync system time to mcu(rtc)
int dio_syncrtc()
{
    int res=0;
    int i ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        // wait MCU ready
        for( i=0;i<100; i++ ) {
            if( p_dio_mmap->rtc_cmd==0 ) break;
            if( p_dio_mmap->rtc_cmd<0 ) {
                p_dio_mmap->rtc_cmd=0;
                break;
            }
            dio_unlock();
            usleep(1000);
            dio_lock();
        }
        p_dio_mmap->rtc_cmd = 3 ;
                // wait MCU ready
        for( i=0;i<200; i++ ) {
            if( p_dio_mmap->rtc_cmd==0 ) {
                res=1 ;                         // success
                break;
            }
            if( p_dio_mmap->rtc_cmd<0 ) {
                p_dio_mmap->rtc_cmd=0;
                break;
            }
            dio_unlock();
            usleep(1000);
            dio_lock();
        }
    }
    dio_unlock();
    if( res==0 ) {
        rtc_settime();                          // sync on board RTC
    }
    return res;
}


void dio_init(config &dvrconfig)
{
    string iomapfile ;
    
    dio_inputmap = 0 ;
    dio_record = 1 ;
    p_dio_mmap=NULL ;
    
#ifdef    PWII_APP
    pwii_front_ch = dvrconfig.getvalueint( "pwii", "front");
    pwii_rear_ch = dvrconfig.getvalueint( "pwii", "rear");
    if( pwii_rear_ch == pwii_front_ch ) {
        pwii_rear_ch = pwii_front_ch+1 ;
    }
#endif
    iomapfile = dvrconfig.getvalue( "system", "iomapfile");
    if( iomapfile.length()==0 ) {
        return ;						// no DIO.
    }
    if( dio_mmap( iomapfile )==NULL ) {
        dvr_log( "IO module not started!");
        return ;
    }
    dio_lock();
    p_dio_mmap->lockpower = 0 ;
    p_dio_mmap->dvrpid = getpid () ;
    p_dio_mmap->dvrwatchdog = -1 ;
    p_dio_mmap->usage++ ;
    // initialize dvrsvr communications
    p_dio_mmap->dvrcmd = 0 ;
    p_dio_mmap->dvrstatus = DVR_RUN ;
    dio_unlock();

    // wait io module ready
    if( p_dio_mmap->iomode==0 ) {
        dvr_log( "Wait IO module to be ready!");
        while( p_dio_mmap->iomode==0 ) {
            sleep(1);
        }
    }
    return ;
}

void dio_uninit()
{
    if( p_dio_mmap ) {
        dio_unlockpower();
        dio_lock();
        p_dio_mmap->dvrpid = 0 ;
        p_dio_mmap->usage-- ;
        p_dio_mmap->dvrcmd = 0 ;
        p_dio_mmap->dvrstatus &= ~DVR_RUN ;
        p_dio_mmap->dvrwatchdog = -1 ;
        dio_unlock();
        dio_munmap();
    }
}
