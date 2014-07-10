/* --- Changes ---
 * 09/11/2009 by Harrison
 *   1. Added nodiskcheck
 *      to support temporary disk unmount for tab102
 *
 * 10/23/2009 by Harrison
 *   1. Added dio_hdpower()
 *      to support Hybrid disk
 *
 * 11/03/2009 by Harrison
 *   1. Update OSD when gps_valid changes from 1 to 0
 *
 * 11/20/2009 by Harrison
 *   1. Added semaphore for gps data.
 * 
 */

#include "dvr.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <errno.h>
#include <sys/sem.h>

#include "../ioprocess/diomap.h"

struct dio_mmap * p_dio_mmap ;
unsigned int dio_old_inputmap ;
int dio_standby_mode ;

struct gps {
  int		gps_valid ;
  double	gps_speed ;	// knots
  double	gps_direction ; // degree
  double  gps_latitude ;	// degree, + for north, - for south
  double  gps_longitud ;      // degree, + for east,  - for west
  double  gps_gpstime ;	// seconds from 1970-1-1 12:00:00 (utc)
};
int semid;

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
union semun {
  int val;
  struct semid_ds *buf;
  unsigned short int *array;
  struct seminfo *__buf;
};
#endif

static void prepare_semaphore()
{
  key_t unique_key;
  union semun options;

  unique_key = ftok("/dev/null", 'a');
  semid = semget(unique_key, 1, IPC_CREAT | IPC_EXCL | 0666);

  if (semid == -1) {
    if (errno == EEXIST) {
      semid = semget(unique_key, 1, 0);
      if (semid == -1) {
	fprintf(stderr, "semget error(%s)\n", strerror(errno));
	exit(1);
      }
    } else {
      fprintf(stderr, "semget error(%s)\n", strerror(errno));
      exit(1);
    }
  } else {
    options.val = 1;
    semctl(semid, 0, SETVAL, options);
  }
}

void get_gps_data(struct gps *g)
{
  struct sembuf sb = {0, -1, 0};

  if (!p_dio_mmap) {
    g->gps_valid = 0;
    return;
  }

  sb.sem_op = -1;
  semop(semid, &sb, 1);

  g->gps_valid = p_dio_mmap->gps_valid;
  g->gps_speed = p_dio_mmap->gps_speed;
  g->gps_direction = p_dio_mmap->gps_direction;
  g->gps_latitude = p_dio_mmap->gps_latitude;
  g->gps_longitud = p_dio_mmap->gps_longitud;
  g->gps_gpstime = p_dio_mmap->gps_gpstime;

  sb.sem_op = 1;
  semop(semid, &sb, 1);
}


int get_peak_data( float *fb, float *lr, float *ud )
{
  if( p_dio_mmap && p_dio_mmap->iopid ){
    *fb = p_dio_mmap->gforce_forward_d;
    *lr = p_dio_mmap->gforce_right_d;
    *ud = p_dio_mmap->gforce_down_d;
     return 1;
  }
  *fb = 0.0;
  *lr = 0.0;
  *ud = 0.0; 
  return 0;
}

int isPeakChanged()
{
   if( p_dio_mmap && p_dio_mmap->iopid ){
      return p_dio_mmap->gforce_changed;
   }
   return 0;
}
int dio_inputnum()
{
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        return p_dio_mmap->inputnum ;
    }
    return 0 ;
}

int dio_outputnum()
{
    if( p_dio_mmap && p_dio_mmap->iopid ) {
        return p_dio_mmap->outputnum ;
    }
    return 0 ;
}

int dio_input( int no )
{
    if( p_dio_mmap && p_dio_mmap->iopid && no>=0 && no<p_dio_mmap->inputnum ) {
        p_dio_mmap->dvrwatchdog = 0 ;
        return ((p_dio_mmap->inputmap)>>no)&1 ;
    }
    return 0 ;
}

void dio_output( int no, int v)
{
    if( p_dio_mmap && p_dio_mmap->iopid && no>=0 && no<p_dio_mmap->outputnum ) {
        if( v ) {
            p_dio_mmap->outputmap |= 1<<no ;
        }
        else {
            p_dio_mmap->outputmap &= ~(1<<no) ;
        }
    }
}

// return 1 for poweroff switch turned off

int dio_poweroff()
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
        return p_dio_mmap->poweroff ;
    }
    return (0);
}

int dio_lockpower()
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
        p_dio_mmap->lockpower = 1 ;
    }
    return (0);
}

int dio_unlockpower()
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
        p_dio_mmap->lockpower = 0;
    }
    return (0);
}

int dio_enablewatchdog()
{
    if( p_dio_mmap ){
        p_dio_mmap->dvrwatchdog = 0 ;
    }
    return (0);
}

int dio_disablewatchdog()
{
    if( p_dio_mmap ){
        p_dio_mmap->dvrwatchdog = -1 ;
    }
    return (0);
}

int dio_kickwatchdog()
{
    if( p_dio_mmap &&
       p_dio_mmap->dvrwatchdog>=0 )
    {
        p_dio_mmap->dvrwatchdog = 0 ;
    }
    return (0);
}

extern int g_nodiskcheck;
int dio_get_nodiskcheck()
{
  if( p_dio_mmap && p_dio_mmap->iopid ) {
    g_nodiskcheck = p_dio_mmap->nodiskcheck;
    return g_nodiskcheck;
  }

  return -1;
}

// checking io maps and dvr commands, return if io pins changed after last check
int dio_check()
{
    int res=0 ;
    if( p_dio_mmap && p_dio_mmap->iopid ){
      //dio_lock();

	g_nodiskcheck = p_dio_mmap->nodiskcheck;

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
            rec_stop();
        }
        else if( p_dio_mmap->dvrcmd == 4 ) {
            p_dio_mmap->dvrcmd = 0 ;
            dio_standby_mode=0;
            rec_start ();
        } 
        res = (dio_old_inputmap != p_dio_mmap->inputmap) ;
        dio_old_inputmap = p_dio_mmap->inputmap ;
        //dio_unlock();
    }
    return res ;
}

// set dvr status bits
int dio_setstate( int status ) 
{
    if( p_dio_mmap ){
       	p_dio_mmap->dvrstatus |= status ;
        return p_dio_mmap->dvrstatus ;
    }
    return 0 ;
}

// clear dvr status
int dio_clearstate( int status )
{
    if( p_dio_mmap ){
       	p_dio_mmap->dvrstatus &= ~status ;
        return p_dio_mmap->dvrstatus ;
    }
    return 0 ;
}

// set usb (don't remove) led
void dio_usb_led(int v)
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
		if( v ) {
        	p_dio_mmap->panel_led |= 1 ;
		}
		else {
        	p_dio_mmap->panel_led &= ~1 ;
		}
    }
}

// set error led
void dio_error_led(int v)
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
		if( v ) {
        	p_dio_mmap->panel_led |= 2 ;
		}
		else {
        	p_dio_mmap->panel_led &= ~2 ;
		}
    }
}

// set video lost led
void dio_videolost_led(int v)
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
		if( v ) {
        	p_dio_mmap->panel_led |= 4 ;
		}
		else {
        	p_dio_mmap->panel_led &= ~4 ;
		}
    }
}

// turn onoff all device power, (enter standby mode)
// onoff, 0: turn device off, others:device poweron
void dio_devicepower(int onoffmaps)
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
        p_dio_mmap->devicepower = onoffmaps ;
    }
}
int isInUSBretrieve()
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
       if( p_dio_mmap->mcu_cmd==10)
	 return 1;      
    }
    return 0;
}
int isInhbdcopying(){
    if( p_dio_mmap && p_dio_mmap->iopid ){
      return p_dio_mmap->ishybrid_copy;
    }
    return 0;
}

int isstandbymode()
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
      if( p_dio_mmap->current_mode>APPMODE_SHUTDOWNDELAY &&p_dio_mmap->current_mode<APPMODE_SHUTDOWN)
	return 1;
    }
    return 0; 
}

int old_gpsvalid;
double gps_speed(int *gpsvalid_changed)
{
    double speed = -1.0 ;

    if (p_dio_mmap == NULL || p_dio_mmap->glogpid <= 0)
      return speed;

    struct gps g;
    get_gps_data(&g);

    int new_gpsvalid = g.gps_valid;

    if (old_gpsvalid == new_gpsvalid) {
      *gpsvalid_changed = 0;
    } else {
      *gpsvalid_changed = 1;
    }
    old_gpsvalid = new_gpsvalid;

    if( new_gpsvalid ) {
        speed = g.gps_speed ;
    }
    return speed ;
}

int gps_location( double * latitude, double * longitude )
{
    if (p_dio_mmap == NULL || p_dio_mmap->glogpid <= 0)
      return 0;

    struct gps g;
    get_gps_data(&g);

    if( g.gps_valid ) {
        *latitude = g.gps_latitude ;
        *longitude = g.gps_longitud ;
        return 1;
    }
    return 0;
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
    
    if( p_dio_mmap && p_dio_mmap->iopid ){
        // wait MCU ready
        for( i=0;i<100; i++ ) {
            if( p_dio_mmap->rtc_cmd==0 ) break;
            if( p_dio_mmap->rtc_cmd<0 ) {
                p_dio_mmap->rtc_cmd=0;
                break;
            }
            usleep(1000);
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
            usleep(1000);
        }
    }
    if( res==0 ) {
        rtc_settime();                          // sync on board RTC
    }
    return res;
}

int isignitionoff()
{
  if( p_dio_mmap->current_mode==APPMODE_SHUTDOWNDELAY)
     return 1;
  return 0;
}
void dio_mcureboot()
{
   if( p_dio_mmap && p_dio_mmap->iopid ){
     p_dio_mmap->mcu_cmd =5; 
   }
}
int dio_hdpower(int on)
{
    int res=0;
    int i ;
    
    if( p_dio_mmap && p_dio_mmap->iopid ){
        // wait MCU ready
        for( i=0;i<100; i++ ) {
            if( p_dio_mmap->mcu_cmd==0 ) break;
            if( p_dio_mmap->mcu_cmd<0 ) {
                p_dio_mmap->mcu_cmd=0;
                break;
            }
            usleep(1000);
        }
        p_dio_mmap->mcu_cmd = on ? 2 : 1 ;
                // wait MCU ready
        for( i=0;i<200; i++ ) {
            if( p_dio_mmap->mcu_cmd==0 ) {
                res=1 ;                         // success
                break;
            }
            if( p_dio_mmap->mcu_cmd<0 ) {
                p_dio_mmap->mcu_cmd=0;
                break;
            }
            usleep(1000);
        }
    }

    return res;
}

void dio_setfileclose(int close)
{
    if(close){
       p_dio_mmap->fileclosed=1;
    } else {
       p_dio_mmap->fileclosed=0;
    }
}

void dio_hybridcopy(int on)
{
    if( p_dio_mmap && p_dio_mmap->iopid ){
        p_dio_mmap->ishybrid_copy=on ;
    }
}

void dio_init()
{
    int i;
    int fd ;
    void * p ;
    string iomapfile ;

    prepare_semaphore();

    config dvrconfig(dvrconfigfile);
    iomapfile = dvrconfig.getvalue( "system", "iomapfile");
    
    dio_old_inputmap = 0 ;
    dio_standby_mode = 0 ;
    
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
       // p_dio_mmap->dvrpid = 0 ;
        p_dio_mmap->usage-- ;
        p_dio_mmap->dvrcmd = 0 ;
        //p_dio_mmap->dvrstatus = 0 ;
       // p_dio_mmap->dvrwatchdog = -1 ;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}
