
#include "eaglesvr.h"
#include <sys/ioctl.h>
#include <sys/timex.h>
#include <linux/rtc.h>

// time functions

int g_timetick=0 ;            // global time tick , to maintain a always count up time counter in ms (even between restart)
static time_t timetick_ref=0 ;

void time_init()
{
    struct timespec tp ;
    clock_gettime(CLOCK_MONOTONIC, &tp );
    timetick_ref = tp.tv_sec ;
}

void time_uninit()
{
}

// return ticks (milli-seconds) from time_init()
int time_tick()
{
    struct timespec tp ;
    // to gaurantee g_timetick always going up
    clock_gettime(CLOCK_MONOTONIC, &tp );
    g_timetick = (tp.tv_sec - timetick_ref)*1000+tp.tv_nsec/1000000 ;
    if( g_timetick<0 ) {                                             // timer overflow
        dvr_log("Reach timer limit, restarting.");
        timetick_ref = tp.tv_sec ;
        g_timetick=0;
        app_state = APPRESTART ;
    }
    return g_timetick ;
}

void time_now(struct dvrtime *dvrt)
{
    time_t t ;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    t = (time_t) current_time.tv_sec ;
    if( dvrt ) {
        struct tm stm ;
        localtime_r(&t, &stm);
        dvrt->year = stm.tm_year + 1900 ;
        dvrt->month = stm.tm_mon + 1 ;
        dvrt->day = stm.tm_mday ;
        dvrt->hour = stm.tm_hour ;
        dvrt->minute = stm.tm_min ;
        dvrt->second = stm.tm_sec ;
        dvrt->milliseconds=current_time.tv_usec/1000 ;
        dvrt->tz=0;
    }
}

time_t time_utctime(struct dvrtime *dvrt)
{
    time_t t ;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    t = (time_t) current_time.tv_sec ;
    if( dvrt ) {
        time_dvrtimeutc ( t, dvrt ) ;
        dvrt->milliseconds=current_time.tv_usec/1000 ;
        dvrt->tz=0;
    }
    return t ;
}

// set timezone with "TZ" environment
void time_settimezone(char * timezone)
{
    setenv("TZ", timezone, 1);
}


// set local time
int time_setlocaltime(struct dvrtime *dvrt)
{
    int res ;
    struct timeval tv ;
    tv.tv_usec=dvrt->milliseconds * 1000 ;
    tv.tv_sec=time_timelocal (dvrt) ;
    res=settimeofday( &tv, NULL );
    
    //	system("hwclock --systohc --utc");		// update time to RTC
    // update RTC
    time_setrtc();
    
    return res ;
}

// set utc time
int time_setutctime(struct dvrtime *dvrt)
{
    int res ;
    struct timeval tv ;
    
    tv.tv_usec=dvrt->milliseconds * 1000 ;
    tv.tv_sec=time_timeutc(dvrt);
    
    res=settimeofday( &tv, NULL );
    
    // update RTC
    time_setrtc();
    
    dvr_log("Set time: (UTC) %04d-%02d-%02d %02d:%02d:%02d.%03d",
            dvrt->year,
            dvrt->month,
            dvrt->day,
            dvrt->hour,
            dvrt->minute,
            dvrt->second,
            dvrt->milliseconds );
    return res ;
}

// request to adjust time
int time_adjtime(struct dvrtime *dvrt)
{
    int res ;
    struct timeval tv, adjtv ;
    signed long diffs, diffus ;
    
    gettimeofday(&tv, NULL );
    
    adjtv.tv_usec=dvrt->milliseconds * 1000 ;
    adjtv.tv_sec=time_timeutc(dvrt);
    
    diffs = (int)adjtv.tv_sec-(int)tv.tv_sec ;
    diffus = (int)adjtv.tv_usec - (int)tv.tv_usec ;
    
    if( diffs>=-2 && diffs<=2 ) {
        diffus+=diffs*1000000 ;
        if( diffus<2000000 && diffus>-2000000 ) {
            adjtv.tv_sec=0;
            adjtv.tv_usec=diffus ;
            adjtime( &adjtv, NULL );
            return 1;
        }
    }

    res=settimeofday( &adjtv, NULL );
    
    // restart capture ?!
    cap_stop () ;
    cap_start () ;
    
    // update RTC
    time_setrtc();
        
    dvr_log("adjtime: (UTC) %04d-%02d-%02d %02d:%02d:%02d.%03d",
            dvrt->year,
            dvrt->month,
            dvrt->day,
            dvrt->hour,
            dvrt->minute,
            dvrt->second,
            dvrt->milliseconds );
    
    return res ;
}


time_t time_dvrtime_init( struct dvrtime *dvrt,
                         int year, int month, int day, int hour, int minute, int second, int milliseconds )
{
    time_t t ;
    struct tm stm ;
    stm.tm_year = year - 1900 ;
    stm.tm_mon =  month - 1 ;
    stm.tm_mday = day ;
    stm.tm_hour = hour ;
    stm.tm_min =  minute ;
    stm.tm_sec =  second + milliseconds/1000 ;
    stm.tm_isdst=-1 ;		// mktime to automacit correct dst
    t=mktime( &stm );	// correct dst
    dvrt->year = stm.tm_year + 1900 ;
    dvrt->month = stm.tm_mon + 1 ;
    dvrt->day   = stm.tm_mday ;
    dvrt->hour = stm.tm_hour ;
    dvrt->minute = stm.tm_min ;
    dvrt->second = stm.tm_sec ;
    dvrt->milliseconds = milliseconds%1000 ;
    dvrt->tz=0;
    return t ;
}

time_t time_dvrtime_zerohour( struct dvrtime *dvrt )
{
    time_t t ;
    struct tm stm ;
    stm.tm_year = dvrt->year - 1900 ;
    stm.tm_mon = dvrt->month - 1 ;
    stm.tm_mday = dvrt->day ;
    stm.tm_hour = 0 ;
    stm.tm_min = 0 ;
    stm.tm_sec = 0 ;
    stm.tm_isdst=-1 ;		// mktime to automacit correct dst
    t=mktime( &stm );	// correct dst
    dvrt->year = stm.tm_year + 1900 ;
    dvrt->month = stm.tm_mon + 1 ;
    dvrt->day   = stm.tm_mday ;
    dvrt->hour = 0 ;
    dvrt->minute = 0 ;
    dvrt->second = 0 ;
    dvrt->milliseconds = 0 ;
    return t ;
}

time_t time_nextday( struct dvrtime *dvrt )
{
    time_t nday ;
    struct tm stm ;
    stm.tm_year = dvrt->year - 1900 ;
    stm.tm_mon = dvrt->month - 1 ;
    stm.tm_mday = dvrt->day+1 ;
    stm.tm_hour = 0 ;
    stm.tm_min = 0 ;
    stm.tm_sec = 0 ;
    stm.tm_isdst=-1 ;		// mktime to automacit correct dst
    nday=mktime( &stm );	// correct dst
    dvrt->year = stm.tm_year + 1900 ;
    dvrt->month = stm.tm_mon + 1 ;
    dvrt->day   = stm.tm_mday ;
    dvrt->hour = 0 ;
    dvrt->minute = 0 ;
    dvrt->second = 0 ;
    dvrt->milliseconds = 0 ;
    return nday ;
}

// difference in seconds (t1-t2)
int  time_dvrtime_diff( struct dvrtime *t1, struct dvrtime *t2)
{
    return ((int)time_timelocal (t1)-(int)time_timelocal (t2)) ;
}

// difference in seconds (t1-t2)
int  time_dvrtime_diffms( struct dvrtime *t1, struct dvrtime *t2 )
{
    int diff ;
    diff = time_dvrtime_diff(t1,t2);
    return diff*1000+t1->milliseconds-t2->milliseconds ;
}

// add seconds to dvrt
void  time_dvrtime_add( struct dvrtime *dvrt, int seconds)
{
    struct tm stm ;
    stm.tm_year = dvrt->year - 1900 ;
    stm.tm_mon = dvrt->month - 1 ;
    stm.tm_mday = dvrt->day ;
    stm.tm_hour = dvrt->hour ;
    stm.tm_min = dvrt->minute ;
    stm.tm_sec = dvrt->second + seconds + dvrt->milliseconds/1000 ;
    stm.tm_isdst=-1 ;		// mktime to automacit correct dst
    mktime( &stm );			// auto correct tm
    dvrt->year = stm.tm_year + 1900 ;
    dvrt->month = stm.tm_mon + 1 ;
    dvrt->day   = stm.tm_mday ;
    dvrt->hour = stm.tm_hour ;
    dvrt->minute = stm.tm_min ;
    dvrt->second = stm.tm_sec ;
    dvrt->milliseconds %= 1000 ;
}

// add milliseconds to t
void  time_dvrtime_addms( struct dvrtime *t, int ms)
{
    t->milliseconds+=ms%1000 ;
    time_dvrtime_add( t, ms/1000);
}

void  time_dvrtime_del( struct dvrtime *dvrt, int seconds)
{
    time_t t ;
    struct tm stm ;
    stm.tm_year = dvrt->year - 1900 ;
    stm.tm_mon = dvrt->month - 1 ;
    stm.tm_mday = dvrt->day ;
    stm.tm_hour = dvrt->hour ;
    stm.tm_min = dvrt->minute ;
    stm.tm_sec = dvrt->second + dvrt->milliseconds/1000 ;
    stm.tm_isdst=-1 ;		// mktime to automacit correct dst
    t=mktime( &stm );			// auto correct tm
    t-=seconds ;
    localtime_r( &t, &stm );
    dvrt->year = stm.tm_year + 1900 ;
    dvrt->month = stm.tm_mon + 1 ;
    dvrt->day   = stm.tm_mday ;
    dvrt->hour = stm.tm_hour ;
    dvrt->minute = stm.tm_min ;
    dvrt->second = stm.tm_sec ;
    dvrt->milliseconds %= 1000 ;
}

struct dvrtime operator + ( struct dvrtime & t, int seconds )
{
    struct dvrtime t1 ;
    t1=t ;
    time_dvrtime_add(&t1, seconds);
    return t1 ;
}

struct dvrtime operator + ( struct dvrtime & t, double seconds )
{
    int secondsint ;
    int secondsms ;
    struct dvrtime t1 ;
    secondsint = (int) seconds ;
    secondsms = (int)(seconds-secondsint) ;
    t1=t ;
    t1.milliseconds += secondsms ;
    secondsint+=(t1.milliseconds)/1000 ;
    t1.milliseconds%=1000 ;
    time_dvrtime_add(&t1, secondsint);
    return t1 ;
}

double operator - ( struct dvrtime &t1, struct dvrtime &t2 )
{
    return (double) ((int)time_timelocal (&t1)-(int)time_timelocal (&t2)) +
        (double)(t1.milliseconds-t2.milliseconds) / 1000.0 ;
}

time_t time_timelocal( struct dvrtime * dvrt);
time_t time_timeutc( struct dvrtime * dvrt);

struct dvrtime * time_dvrtimelocal( time_t t, struct dvrtime * dvrt)
{
    struct tm stm ;
    struct tm * ptm ;
    ptm = localtime_r(&t, &stm);
    dvrt->year = ptm->tm_year + 1900 ;
    dvrt->month = ptm->tm_mon + 1 ;
    dvrt->day = ptm->tm_mday ;
    dvrt->hour = ptm->tm_hour ;
    dvrt->minute = ptm->tm_min ;
    dvrt->second = ptm->tm_sec ;
    dvrt->milliseconds = 0 ;
    return dvrt ;
}

struct dvrtime * time_dvrtimeutc( time_t t, struct dvrtime * dvrt)
{
    struct tm stm ;
    struct tm * ptm ;
    ptm = gmtime_r(&t, &stm);
    dvrt->year = ptm->tm_year + 1900 ;
    dvrt->month = ptm->tm_mon + 1 ;
    dvrt->day = ptm->tm_mday ;
    dvrt->hour = ptm->tm_hour ;
    dvrt->minute = ptm->tm_min ;
    dvrt->second = ptm->tm_sec ;
    dvrt->milliseconds = 0 ;
    return dvrt ;
}

time_t time_timelocal( struct dvrtime * dvrt)
{
    struct tm stm ;
    stm.tm_year = dvrt->year - 1900 ;
    stm.tm_mon = dvrt->month - 1 ;
    stm.tm_mday = dvrt->day ;
    stm.tm_hour = dvrt->hour ;
    stm.tm_min = dvrt->minute ;
    stm.tm_sec = dvrt->second ;
    stm.tm_isdst=-1 ;		// mktime to automaticaly correct dst
    return mktime( &stm );
}

time_t time_timeutc( struct dvrtime * dvrt)
{
    struct tm stm ;
    stm.tm_year = dvrt->year - 1900 ;
    stm.tm_mon = dvrt->month - 1 ;
    stm.tm_mday = dvrt->day ;
    stm.tm_hour = dvrt->hour ;
    stm.tm_min = dvrt->minute ;
    stm.tm_sec = dvrt->second ;
    stm.tm_isdst=0 ;
    return timegm( &stm );
}

// update onboard rtc to system time
int time_setrtc()
{
//    dio_syncrtc ();                             // update time to RTC on MCU
    return 1;
}
