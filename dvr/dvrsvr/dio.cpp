#include "dvr.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>

#include "../ioprocess/diomap.h"

struct dio_mmap * p_dio_mmap ;
unsigned int dio_old_inputmap ;
int dio_standby_mode ;

/*
   share memory lock implemented use atomic swap

    operations between lock() and unlock() should be quick enough and only memory access only.
 
*/ 

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

void dio_lock()
{
    if( p_dio_mmap ) {
        int c=0;
        while( atomic_swap( &(p_dio_mmap->lock), 1 ) ) {
            if( c++<20 ) {
                sched_yield();
            }
            else {
                // yield too many times ?
                usleep(1);
            }
        }
    }
}

void dio_unlock()
{
    if( p_dio_mmap ) {
        p_dio_mmap->lock=0;
    }
}

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
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        return ((p_dio_mmap->inputmap)>>no)&1 ;
    }
    return 0 ;
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
        p_dio_mmap->dvrwatchdog = 0 ;
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
        if( status == DVR_LOCK &&
            (p_dio_mmap->dvrstatus & DVR_LOCK)==0 ) 
        {
                // start recording
                struct dvrtime cliptime ;
                time_now(&cliptime) ;
                sprintf( g_vri, "%s-%02d%02d%02d%02d%02d", g_hostname,
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
            (p_dio_mmap->dvrstatus & DVR_LOCK) ) 
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

// check if IO process busy (doing smartftp)
int dio_iobusy()
{
    int busy=0 ;
    dio_lock();
    if( p_dio_mmap ){
        busy = ( p_dio_mmap->iomode == IOMODE_UPLOADING ) ;
    }
    dio_unlock();
    return busy ;
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

int pwii_event_marker ;     // event marker for PWII rear view mirror
int pwii_front_ch ;         // pwii front camera channel
int pwii_rear_ch ;          // pwii real camera channel

// return 1 : key event, 0: no key event 
int dio_getpwiikeycode( int * keycode, int * keydown)
{
    void rec_pwii_toggle_rec_front() ;
    void rec_pwii_toggle_rec_rear() ;

    static unsigned int pwiikey = 0 ;
    unsigned int nkey=0 ;
    unsigned int xkey ;

    if( p_dio_mmap ) {
//    dio_lock();
        // this is atomic op already
        nkey=p_dio_mmap->pwii_buttons ;
//    dio_unlock();
    }
    
    xkey=pwiikey^nkey ;

    if( xkey ) {
        
        if( xkey & 1 ) {                    // bit 0: rew
            * keycode = (int) VK_MEDIA_PREV_TRACK ;
            * keydown = ((nkey&1)!=0 );
            pwiikey ^= 1 ;
            return 1 ;
        }
        if( xkey & 2 ) {                     // bit 1: play/pause
            * keycode = (int) VK_MEDIA_PLAY_PAUSE ;
            * keydown = ((nkey&2)!=0 );
            pwiikey ^= 2 ;
            return 1 ;
        }
        if( xkey & 4 ) {                     // bit 2: ff
            * keycode = (int) VK_MEDIA_NEXT_TRACK ;
            * keydown = ((nkey&4)!=0 );
            pwiikey ^= 4 ;
            return 1 ;
        }
        if( xkey & 8 ) {                     // bit 3: ST/PWR
            * keycode = (int) VK_MEDIA_STOP ;
            * keydown = ((nkey&8)!=0 );
            pwiikey ^= 8 ;
            return 1 ;
        }
        if( xkey & 0x10 ) {                     // bit 4: PR
            * keycode = (int) VK_PRIOR ;
            * keydown = ((nkey&0x10)!=0 );
            pwiikey ^= 0x10 ;
            return 1 ;
        }
        if( xkey & 0x20 ) {                     // bit 5: NX
            * keycode = (int) VK_NEXT ;
            * keydown = ((nkey&0x20)!=0 );
            pwiikey ^= 0x20 ;
            return 1 ;
        }
        if( xkey & 0x800 ) {                            // bit 11: lp
            * keycode = (int) VK_LP ;
            * keydown = ((nkey&0x800)!=0 );
            pwiikey ^= 0x800 ;
            return 1 ;
        }
        if( xkey & 0x1000 ) {        // bit 12: blackout
			* keycode = (int) VK_POWER ;
            * keydown = ((nkey&0x1000)!=0 );
			pwiikey ^= 0x1000 ;
			return 1 ;
        }
        if( xkey & 0x400 ) {                            // bit 10: tm
			pwiikey ^= 0x400 ;
			* keycode = (int) VK_EM ;
            pwii_event_marker = * keydown = ((nkey&0x400)!=0 );
            if ( pwii_event_marker ){
                dvr_log("TraceMark pressed!");
            }
			return 1 ;
        }
        if( xkey & 0x100 ) {        // bit 8: front camera rec
            if( (pwiikey & 0x100 )==0 ) {
                rec_pwii_toggle_rec_front() ;
                dvr_log("REC pressed!");
            }
        }
        if( xkey & 0x200 ) {        // bit 9: back camera rec
            if( (pwiikey & 0x200 )==0 ) {
                rec_pwii_toggle_rec_rear() ;
                dvr_log("C2 pressed!");
            }
        }
        pwiikey = nkey ;
    }
    return 0 ;
}

void dio_pwii_lcd( int lcdon )
{
    if( p_dio_mmap ) {
        dio_lock();
        if( lcdon ) {
            p_dio_mmap->pwii_output |= (1<<11) ;
        }
        else {
            p_dio_mmap->pwii_output &= ~(1<<11) ;
        }
        dio_unlock();
    }
}

void dio_pwii_standby( int standby )
{
    if( p_dio_mmap ) {
        dio_lock();
        if( standby ) {
            p_dio_mmap->pwii_output |= (1<<12) ;
        }
        else {
            p_dio_mmap->pwii_output &= ~(1<<12) ;
        }
        dio_unlock();
    }
}

#endif    // PWII_APP

void dio_smartserveron()
{
    int log=0 ;
    if( p_dio_mmap ){
        dio_lock();
        if( p_dio_mmap->smartserver==0 ) {
            p_dio_mmap->smartserver=1;
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
    int smsdetect=0 ;       // smart server detection requested?

    if( p_dio_mmap && p_dio_mmap->iopid ){
        dio_lock();
        // smartserver probe ?
        if( p_dio_mmap->wifi_req!=0 && p_dio_mmap->smartserver==0 ) {
            smsdetect=1 ;
        }
        dio_unlock();
    }

    // probe smart server
    if( smsdetect ) {
        net_broadcast("rausb0", 49954, (void *)"lookingforsmartserver", 21 );
    }
}

// checking io maps and dvr commands, return if io pins changed after last check
int dio_check()
{
    int res=0 ;

    if( p_dio_mmap && p_dio_mmap->iopid ){
        dio_lock();
        if( p_dio_mmap->dvrcmd ) {
            // dvrcmd :  1: restart(resume), 2: suspend, 3: stop record, 4: start record
            if( p_dio_mmap->dvrcmd == 1 ) {
                app_state = APPRESTART ;
                dio_standby_mode=0;
            }
            else if( p_dio_mmap->dvrcmd == 2 ) {
                app_state = APPDOWN ;
            }
            else if( p_dio_mmap->dvrcmd == 3 ) {
                dio_unlock();
                rec_stop();
                dio_lock();
            }
            else if( p_dio_mmap->dvrcmd == 4 ) {
                dio_unlock();
                rec_start ();
                dio_lock();
            }
            p_dio_mmap->dvrcmd = 0 ;
        }
        if( p_dio_mmap->iomode > IOMODE_SHUTDOWNDELAY || p_dio_mmap->iomode<IOMODE_QUIT ) {
            if( dio_standby_mode == 0 ) {
                dio_unlock();
                rec_stop();
                dio_lock();
                dio_standby_mode=1;
            }
        }
        else {
            if( dio_standby_mode ) {
                dio_unlock();
                rec_start();
                dio_lock();
                dio_standby_mode=0;
            }
        }
        
        res = (dio_old_inputmap != p_dio_mmap->inputmap) ;
#ifdef    PWII_APP
        res = ( res || pwii_event_marker );
#endif
        dio_old_inputmap = p_dio_mmap->inputmap ;
        dio_unlock();
    }

    return res ;
}

int dio_getiomsg( char * oldmsg )
{
    int res = 0 ;
    dio_lock();
    if( p_dio_mmap ){
        if( strcmp( oldmsg, p_dio_mmap->iomsg )!=0 ) {
            strcpy( oldmsg, p_dio_mmap->iomsg );
            res=1 ;
        }
    }
    dio_unlock();
    return res ;
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

int dio_getgforce( float * gfb, float * glr, float *gud )
{
    int v=0 ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->glogpid>0 && p_dio_mmap->gforce_serialno ) {
        *gfb = p_dio_mmap->gforce_forward ;
        *glr = p_dio_mmap->gforce_right ;
        *gud = p_dio_mmap->gforce_down ;
        v = 1 ;
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


void dio_init()
{
    int i;
    int fd ;
    void * p ;
    string iomapfile ;
    config dvrconfig(dvrconfigfile);
    iomapfile = dvrconfig.getvalue( "system", "iomapfile");
    
    dio_old_inputmap = 0 ;
    dio_standby_mode = 0 ;
#ifdef PWII_APP
    pwii_event_marker = 0 ;
#endif        
    p_dio_mmap=NULL ;
    if( iomapfile.length()==0 ) {
        return ;						// no DIO.
    }
    for( i=0; i<10; i++ ) {				// retry 10 times
        fd = open(iomapfile.getstring(), O_RDWR );
        if( fd>0 ) {
            break ;
        }
        sleep(1);
    }
    if( fd<=0 ) {
        dvr_log( "IO module not started!");
        return ;
    }

#ifdef    PWII_APP
    pwii_front_ch = dvrconfig.getvalueint( "pwii", "front");
    pwii_rear_ch = dvrconfig.getvalueint( "pwii", "rear");
    if( pwii_rear_ch == pwii_front_ch ) {
        pwii_rear_ch = pwii_front_ch+1 ;
    }
#endif
    
    p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );                                // don't need fd to use memory map.
    if( p==(void *)-1 || p==NULL ) {
        dvr_log( "IO memory map failed!");
        return ;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    for( i=0; i<10; i++ ) {
        if( p_dio_mmap->iopid ) {
            p_dio_mmap->lockpower = 0 ;
            p_dio_mmap->dvrpid = getpid () ;
            p_dio_mmap->dvrwatchdog = 0 ;
            p_dio_mmap->usage++ ;
            // initialize dvrsvr communications 
            p_dio_mmap->dvrcmd = 0 ;
            p_dio_mmap->dvrstatus = DVR_RUN ;
            return ;		// success 
        }
        sleep(1);			// wait for 10 seconds
    }
    munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    p_dio_mmap = NULL ;
    return ;
}

void dio_uninit()
{
    if( p_dio_mmap ) {
        dio_unlockpower();
        p_dio_mmap->dvrpid = 0 ;
        p_dio_mmap->usage-- ;
        p_dio_mmap->dvrcmd = 0 ;
//        p_dio_mmap->dvrstatus = 0 ;
        p_dio_mmap->dvrwatchdog = -1 ;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}
