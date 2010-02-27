#include "dvr.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>

#include "../ioprocess/diomap.h"

struct dio_mmap * p_dio_mmap ;
unsigned int dio_old_inputmap ;
int dio_standby_mode ;

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
        while( atomic_swap( &(p_dio_mmap->lock), 1 ) ) {
            sleep(0);      // or sched_yield()
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
    int inputnum=0 ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        inputnum = p_dio_mmap->inputnum ;
    }
    dio_unlock();
    return inputnum ;
}

int dio_outputnum()
{
    int outputnum = 0 ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        outputnum = p_dio_mmap->outputnum ;
    }
    dio_unlock();
    return outputnum ;
}

int dio_input( int no )
{
    int v = 0 ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid && no>=0 && no<p_dio_mmap->inputnum ) {
        v = ((p_dio_mmap->inputmap)>>no)&1 ;
    }
    dio_unlock();
    return v ;
}

void dio_output( int no, int v)
{
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid && no>=0 && no<p_dio_mmap->outputnum ) {
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
    int s = 0 ;
    int rstart = 0 ;
    dio_lock();
    if( p_dio_mmap ){
#ifdef PWII_APP
        if( status == DVR_RECORD ) {
            if( (p_dio_mmap->dvrstatus & DVR_RECORD)==0 ) {
                // start recording
                struct dvrtime cliptime ;
                time_now(&cliptime) ;
                sprintf( g_vri, "%s_%02d%02d%02d%02d%02d", g_hostname,
                    cliptime.year%100,
                    cliptime.month,
                    cliptime.day,
                    cliptime.hour,
                    cliptime.minute
                    );
                memcpy( p_dio_mmap->pwii_VRI, g_vri, sizeof(p_dio_mmap->pwii_VRI) );
                rstart = 1 ;
            }
        }
#endif    
       	p_dio_mmap->dvrstatus |= status ;
        s = p_dio_mmap->dvrstatus ;
    }
    dio_unlock();
#ifdef PWII_APP    
    if( rstart ) {
        dvr_log( "Recording started, ID: %s", g_vri);
    }
#endif
    return s ;
}

// clear dvr status
int dio_clearstate( int status )
{
    int s = 0 ;
    int rstart = 1 ;

    dio_lock();
    if( p_dio_mmap ){
#ifdef PWII_APP
        if( status == DVR_RECORD ) {
            if( (p_dio_mmap->dvrstatus & DVR_RECORD) ) {
                // stop recording
                rstart = 0 ;
            }
        }
#endif    
       	p_dio_mmap->dvrstatus &= ~status ;
        s = p_dio_mmap->dvrstatus ;
    }
    dio_unlock();
#ifdef PWII_APP
    if( rstart==0 ) {
        dvr_log( "Recording stopped, ID: %s", g_vri);
    }
#endif    
    return s ;
}


// check if IO process busy (doing smartftp)
int dio_iobusy()
{
    int busy=0 ;
    dio_lock();
    if( p_dio_mmap ){
        busy = p_dio_mmap->iobusy ;
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
    static int key_timetick = 0 ;
    unsigned int xkey ;

    dio_lock();
    xkey=p_dio_mmap->pwii_buttons ^ pwiikey ;
    dio_unlock();

    if( xkey ) {
        key_timetick = g_timetick ;
        
        if( xkey & 1 ) {                    // bit 0: rew
            * keycode = (int) VK_MEDIA_PREV_TRACK ;
            * keydown = ((pwiikey&1)==0 );
            pwiikey ^= 1 ;
            return 1 ;
        }
        if( xkey & 2 ) {                     // bit 1: play/pause
            * keycode = (int) VK_MEDIA_PLAY_PAUSE ;
            * keydown = ((pwiikey&2)==0 );
            pwiikey ^= 2 ;
            return 1 ;
        }
        if( xkey & 4 ) {                     // bit 2: ff
            * keycode = (int) VK_MEDIA_NEXT_TRACK ;
            * keydown = ((pwiikey&4)==0 );
            pwiikey ^= 4 ;
            return 1 ;
        }
        if( xkey & 8 ) {                     // bit 3: ST/PWR
            * keycode = (int) VK_MEDIA_STOP ;
            * keydown = ((pwiikey&8)==0 );
            pwiikey ^= 8 ;
            return 1 ;
        }
        if( xkey & 0x10 ) {                     // bit 4: PR
            * keycode = (int) VK_PRIOR ;
            * keydown = ((pwiikey&0x10)==0 );
            pwiikey ^= 0x10 ;
            return 1 ;
        }
        if( xkey & 0x20 ) {                     // bit 5: NX
            * keycode = (int) VK_NEXT ;
            * keydown = ((pwiikey&0x20)==0 );
            pwiikey ^= 0x20 ;
            return 1 ;
        }
        if( xkey & 0x400 ) {                            // bit 10: tm
            * keycode = (int) VK_EM ;
            pwii_event_marker = * keydown = ((pwiikey&0x400)==0 );
            pwiikey ^= 0x400 ;
            dvr_log("TraceMark %s (%d).", pwii_event_marker==1?"pressed":"released", pwii_event_marker );
            return 1 ;
        }
        if( xkey & 0x800 ) {                            // bit 11: lp
//            * keycode = (int) VK_LP ;
            * keycode = (int) VK_POWER ;                // temperary use lp button to simulate power button 
            * keydown = ((pwiikey&0x800)==0 );
            pwiikey ^= 0x800 ;
            return 1 ;
        }
        if( xkey & 0x1000 ) {        // bit 12: blackout
			* keycode = (int) VK_POWER ;
            * keydown = ((pwiikey&0x1000)==0 );
			pwiikey ^= 0x1000 ;
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
        dio_lock();
        pwiikey = p_dio_mmap->pwii_buttons ;            // save key pad status
        dio_unlock();
    }
    else {                                              // no key
        // REC, C1 auto clear
        if( pwiikey & 0x300 ) {
            if( (g_timetick-key_timetick)>1200 || g_timetick<key_timetick ){
                dio_lock();
                if( p_dio_mmap->pwii_buttons & 0x100 ) {
                    p_dio_mmap->pwii_buttons &= ~0x100 ;		// auto clear
                }
                if( p_dio_mmap->pwii_buttons & 0x200 ) {
                    p_dio_mmap->pwii_buttons &= ~0x200 ;		// auto clear
                }
                dio_unlock();
            }
        }
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

// checking io maps and dvr commands, return if io pins changed after last check
int dio_check()
{
    int res=0 ;
    dio_lock();
    if( p_dio_mmap && p_dio_mmap->iopid ){
        // dvrcmd :  1: restart(resume), 2: suspend, 3: stop record, 4: start record
        if( p_dio_mmap->dvrcmd == 1 ) {
            p_dio_mmap->dvrcmd = 0 ;
            app_state = APPRESTART ;
            dio_standby_mode=0;
        }
        else if( p_dio_mmap->dvrcmd == 2 ) {
            p_dio_mmap->dvrcmd = 0 ;
            app_state = APPDOWN ;
        }
        else if( p_dio_mmap->dvrcmd == 3 ) {
            p_dio_mmap->dvrcmd = 0 ;
            dio_standby_mode=1;
            dio_unlock();
            rec_stop();
            dio_lock();
        }
        else if( p_dio_mmap->dvrcmd == 4 ) {
            p_dio_mmap->dvrcmd = 0 ;
            dio_standby_mode=0;
            dio_unlock();
            rec_start ();
            dio_lock();
        }
        res = (dio_old_inputmap != p_dio_mmap->inputmap) ;
#ifdef    PWII_APP
        res = res || pwii_event_marker ;
#endif
        dio_old_inputmap = p_dio_mmap->inputmap ;
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
        p_dio_mmap->dvrstatus = 0 ;
        p_dio_mmap->dvrwatchdog = -1 ;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}
