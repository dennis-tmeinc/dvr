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

#include "../eaglesvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/config.h"
#include "netdbg.h"
#include "mcu.h"
#include "cdc.h"
#include "gforce.h"
#include "diomap.h"
#include "iomisc.h"
#include "yagf.h"

// options
int restartonswitch ;		// restart if switch turn on from standby
int standbyhdoff = 0 ;
int usewatchdog = 0 ;
int hd_timeout = 120 ;       // hard drive ready timeout
unsigned int runtime ;
int disable_wifiupload ;	// disable wifi uploading
int disable_archive ;		// disable archiving to arch disk

int hdlock=0 ;								// HD lock status
int hdinserted=0 ;

char dvriomap[100] = VAR_DIR"/dvriomap" ;
const char pidfile[] = VAR_DIR"/ioprocess.pid" ;

int shutdowndelaytime ;
int wifidetecttime ;
int uploadingtime ;
int archivetime ;
int standbytime ;

// unsigned int outputmap ;	// output pin map cache
const char logfile[]=VAR_DIR"/dvrlogfile";
char temp_logfile[128] ;
int watchdogenabled=0 ;
int watchdogtimeout=30 ;
int gpsvalid = 0 ;

unsigned int panelled=0 ;
unsigned int devicepower=0;

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
    char lbuf[512];
    FILE *flog=NULL;
    time_t ti;
    int l;
    va_list ap ;

    // check logfile, it must be a symlink to hard disk
    if( readlink(logfile, lbuf, 512)>10 ) {
        flog = fopen(logfile, "a");
    }

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
    if (flog) {
        fprintf(flog, "%s\n", lbuf);
        fclose( flog ) ;
        return 1;
    }

    flog = fopen(temp_logfile, "a");
    if (flog) {
        fprintf(flog, "%s *\n", lbuf);
        fclose(flog);
    }

    return 0 ;
}

static time_t starttime=0 ;
void initruntime()
{
    struct timespec tp ;
    clock_gettime(CLOCK_MONOTONIC, &tp );
    starttime=tp.tv_sec ;
}

// return runtime in mili-seconds
unsigned int getruntime()
{
    struct timespec tp ;
    clock_gettime(CLOCK_MONOTONIC, &tp );
    return (unsigned int)(tp.tv_sec-starttime)*1000 + tp.tv_nsec/1000000 ;
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
        execl(APP_DIR"/setnetwork", "setnetwork", NULL);
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
        execl(APP_DIR"/mountdisks", "mountdisks", NULL);
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
        execl(APP_DIR"/umountdisks", "umountdisks", NULL);
        exit(0);    // should not return to here
    }
}

// bring up wifi adaptor
static int wifi_run=0 ;
void wifi_up()
{
    system( APP_DIR"/wifi_up" );
    // turn on wifi power. mar 04-2010
    dio_lock();
    p_dio_mmap->pwii_output |= PWII_POWER_WIFI ;
    // DEVICE_POWER_POEPOWER always on
    //    	p_dio_mmap->devicepower |= DEVICE_POWER_POEPOWER ;		// USE POE for wifi device
    dio_unlock();
    wifi_run=1 ;
    netdbg_print("Wifi up\n");
}

// bring down wifi adaptor
void wifi_down()
{
    if( wifi_on ) {
        return ;
    }
    if( wifi_run ) {
        system( APP_DIR"/wifi_down" );
        // turn off wifi power
        dio_lock();
        p_dio_mmap->pwii_output &= ~PWII_POWER_WIFI ;
        // DEVICE_POWER_POEPOWER always on
        //    	p_dio_mmap->devicepower &= ~DEVICE_POWER_POEPOWER ;		// USE POE for wifi device
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
    dio_lock();
    buzzer_timer = getruntime();
    buzzer_count = times ;
    buzzer_on = 0 ;
    buzzer_ontime = ontime ;
    buzzer_offtime = offtime ;
    dio_unlock();
}

// run buzzer, runtime: runtime in ms
static void buzzer_run( int runtime )
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
int smartftp_arch ;     // do smartftp archived files
int smartftp_src ;
char disk_dir[260] ;
char disk_archdir[260] ;

// src == 0 : do "disk", src==1 : do "arch"
void smartftp_start(int src=0)
{
    // Wifi status
    int     smartserver ;        // smartserver detected
    char    smartserver_interface[32] ;

    dio_lock();
    smartserver = p_dio_mmap->smartserver ;
    strncpy( smartserver_interface, p_dio_mmap->smartserver_interface, sizeof( smartserver_interface ) );
    dio_unlock();
    if( smartserver == 0 ) {
        dvr_log( "Smartftp not started, no smartserver detected." );
        return ;		// not smartserver detected
    }

    smartftp_src = src ;
    dvr_log( "Start smart server uploading. (%s)", (src==0)?"disks":"arch" );
    pid_smartftp = fork();
    if( pid_smartftp==0 ) {     // child process
        char hostname[128] ;
        //        char mountdir[250] ;

        // get BUS name
        gethostname(hostname, 128) ;

        // be nice to other processes
        nice(10);

        //      system(APP_DIR"/setnetwork");  // reset network, this would restart wifi adaptor
        if( src==0 ) {
            execl( APP_DIR"/smartftp", "smartftp",
                   smartserver_interface,
                   hostname,
                   "247SECURITY",
                   disk_dir,
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
            execl( APP_DIR"/smartftp", "smartftp",
                   smartserver_interface,
                   hostname,
                   "247SECURITY",
                   disk_archdir,
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

int io_firstrun=0 ;

// return
//        0 : failed
//        1 : success
int appinit()
{
    FILE * pidf ;
    int fd ;
    char * p ;
    config dvrconfig(CFG_FILE);
    string v ;
    int i;

    // setup netdbg
    if( dvrconfig.getvalueint( "debug", "ioprocess" ) ) {
        i = dvrconfig.getvalueint( "debug", "port" ) ;
        v = dvrconfig.getvalue( "debug", "host" ) ;
        netdbg_init( v, i );
    }

    // setup time zone
    string tz ;
    string tzi ;
    tz=dvrconfig.getvalue( "system", "timezone" );
    if( tz.length()>0 ) {
        tzi=dvrconfig.getvalue( "timezones", tz );
        if( tzi.length()>0 ) {
            p=strchr(tzi, ' ' ) ;
            if( p ) {
                *p=0;
            }
            p=strchr(tzi, '\t' ) ;
            if( p ) {
                *p=0;
            }
            setenv("TZ", tzi, 1);
        }
        else {
            setenv("TZ", tz, 1);
        }
    }

    v = dvrconfig.getvalue( "system", "temp_logfile");
    if (v.length() > 0){
        strncpy( temp_logfile, v, sizeof(temp_logfile));
    }

    v = dvrconfig.getvalue( "system", "iomapfile");
    char * iomapfile = v;
    if( iomapfile && strlen(iomapfile)>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }

    // create io map file
    fd = open( dvriomap, O_RDWR ) ;
    if( fd<0 ) {    // file not exist
        fd = open( dvriomap, O_RDWR | O_CREAT, S_IRWXU);
        io_firstrun=1 ;
        dvr_log("IO first time running!");
    }

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

    if( io_firstrun ) {
        memset( p_dio_mmap, 0, sizeof( struct dio_mmap ) );
    }

    p_dio_mmap->usage++;                           // add one usage

    if( p_dio_mmap->iopid > 0 ) { 				// another ioprocess running?
        pid_t pid = p_dio_mmap->iopid ;
        // kill it
        if( kill( pid,  SIGTERM )==0 ) {
            // wait for it to quit
            p_dio_mmap->iomode=0 ;
            waitpid( pid, NULL, 0 );
            sleep(2);
        }
        else {
            p_dio_mmap->iomode=0 ;
            sleep(5);
        }
    }
    p_dio_mmap->iomode=0 ;

    // initialize shared memory
    p_dio_mmap->inputnum = dvrconfig.getvalueint( "io", "inputnum");
    if( p_dio_mmap->inputnum<=0 || p_dio_mmap->inputnum>32 )
        p_dio_mmap->inputnum = MCU_INPUTNUM ;
    p_dio_mmap->outputnum = dvrconfig.getvalueint( "io", "outputnum");
    if( p_dio_mmap->outputnum<=0 || p_dio_mmap->outputnum>32 )
        p_dio_mmap->outputnum = MCU_OUTPUTNUM ;

    p_dio_mmap->panel_led = 0;
    p_dio_mmap->devicepower = DEVICE_POWER_RUN;		// assume all device is power on

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
            FILE * pwiiversionfile=fopen(VAR_DIR"/pwiiversion", "w");
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
        strcpy( zoomcam_dev, v );
    }
    zoomcam_baud = dvrconfig.getvalueint("io", "zoomcam_baudrate");
    if( zoomcam_baud<2400 || zoomcam_baud>115200 ) {
        mcu_baud=DEFSERIALBAUD ;
    }

    zoomcam_enable = dvrconfig.getvalueint("io", "zoomcam_enable");

#endif // PWII_APP

    if( g_syncrtc ) {
        if( time_rtctosys()== 0 ) {	// failed ?
            // try syn system time back to RTC
            time_syncrtc();
        }
    }

#ifdef SUPPORT_YAGF
    yagf_init( dvrconfig );
#else
    gforce_init( dvrconfig );
#endif

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
    if( shutdowndelaytime<5 ) {
        shutdowndelaytime=5 ;
    }
    else if(shutdowndelaytime>3600*12 ) {
        shutdowndelaytime=3600*12 ;
    }

    restartonswitch = dvrconfig.getvalueint( "system", "restartonswitch");

    standbytime = dvrconfig.getvalueint( "system", "standbytime");
    if(standbytime<0 ) {
        standbytime=0 ;
    }
    else if( standbytime>43200 ) {
        standbytime=43200 ;
    }

    wifidetecttime = dvrconfig.getvalueint( "system", "wifidetecttime");
    if(wifidetecttime<10 || wifidetecttime>600 ) {
        wifidetecttime=60 ;
    }

    uploadingtime = dvrconfig.getvalueint( "system", "uploadingtime");
    if(uploadingtime<=0 || uploadingtime>43200 ) {
        uploadingtime=1800 ;
    }

    archivetime = dvrconfig.getvalueint( "system", "archivetime");
    if(archivetime<=0 || archivetime>14400 ) {
        archivetime=1800 ;
    }

    strcpy( disk_dir, dvrconfig.getvalue("system", "mountdir"));
    strcpy( disk_archdir, dvrconfig.getvalue("system", "archivedir"));

    smartftp_arch = dvrconfig.getvalueint( "system", "archive_upload");

    disable_wifiupload = dvrconfig.getvalueint( "system", "disable_wifiupload");
    disable_archive = dvrconfig.getvalueint( "system", "disable_archive");

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

    mcu_watchdogdisable();

#ifdef SUPPORT_YAGF
    yagf_finish();
#else
    gforce_finish();
#endif

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
    int omode = p_dio_mmap->iomode ;
    glog_resume();
    if( restartonswitch ) {
        if( omode >= IOMODE_ARCHIVE ) {
            p_dio_mmap->iomode=IOMODE_REBOOT ;
            dvr_log("Power on switch, restarting system. (mode %d)", p_dio_mmap->iomode);
        }
    }
    else {
        dvr_log("Power on switch, set to running mode. (mode %d)", p_dio_mmap->iomode);
        if( omode > IOMODE_SHUTDOWNDELAY ) {
            dvrsvr_resume() ;
        }
    }
    dio_lock();
    p_dio_mmap->devicepower=DEVICE_POWER_RUN ;    // turn on all devices power
    p_dio_mmap->iomsg[0]=0 ;
    p_dio_mmap->iomode=IOMODE_RUN ;    // back to run normal
    dio_unlock();
}

void mode_archive()
{
    if( disable_archive ) {
        p_dio_mmap->iomode = IOMODE_ARCHIVE ;
        modeendtime = runtime ;    						// timeout
    }
    else {
        dio_lock();
        strcpy( p_dio_mmap->iomsg, "Archiving, Do not remove CF Card.");
        p_dio_mmap->iomode=IOMODE_ARCHIVE  ;
        p_dio_mmap->dvrcmd = 5 ;
        dio_unlock();
        modeendtime = runtime+archivetime*1000 ;
        dvr_log("Enter archiving mode. (mode %d).", p_dio_mmap->iomode);
#ifdef PWII_APP
        zoomcam_close();
#endif
    }
}

void mode_detectwireless()
{
    if( disable_wifiupload ) {
        p_dio_mmap->iomode = IOMODE_DETECTWIRELESS ;
        modeendtime = runtime ;    						// timeout
    }
    else {
        wifi_up();                                      // turn on wifi
        dio_lock();
        p_dio_mmap->smartserver=0 ;         // smartserver detected
        p_dio_mmap->iomode = IOMODE_DETECTWIRELESS ;
        strcpy( p_dio_mmap->iomsg, "Detecting Smart Server!");
        p_dio_mmap->dvrcmd = 7 ;
        dio_unlock();
        modeendtime = runtime + wifidetecttime * 1000 ;
        dvr_log("Start detecting smart server. (mode %d).", p_dio_mmap->iomode);
    }
}

void mode_upload()
{
    if( disable_wifiupload ) {
        return ;
    }

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
    p_dio_mmap->iomsg[0]=0 ;
    p_dio_mmap->iomode=IOMODE_STANDBY ;

#ifdef PWII_APP
    p_dio_mmap->pwii_output &= ~(
                                   PWII_LED_C1 |
                                   PWII_LED_C2 |
                                   PWII_LED_MIC |
                                   PWII_POWER_LCD ) ;
    p_dio_mmap->pwii_output |= PWII_POWER_BLACKOUT ;         // STANDBY mode ( to turn ff backlights )
#endif

    dio_unlock();
    modeendtime = runtime+standbytime*1000 ;
    dvr_log("Enter standby mode. (mode %d)", p_dio_mmap->iomode);
}

void mode_poweroff()			// request to power off
{
    sync();
    dio_lock();
    if( p_dio_mmap->iomode != IOMODE_POWEROFF ) {
        p_dio_mmap->iomode=IOMODE_POWEROFF ;
        dvr_log( "Power button pushed, shutdown system.");
        // shutdown message
        strcpy( p_dio_mmap->iomsg, "System shutdown, please wait...");
        p_dio_mmap->pwii_output |= PWII_LED_POWER ;
        modeendtime = runtime+10000 ;			// 10 seconds timeout and force shutdown
        p_dio_mmap->dvrcmd = 7 ;
    }
    dio_unlock();
}


void mode_shutdown()
{
    // start shutdown
    p_dio_mmap->devicepower=0 ;    // turn off all devices power
    p_dio_mmap->iomode=IOMODE_SHUTDOWN ;    // turn off mode
    modeendtime = runtime+90000 ;
    dvr_log("Standby timeout, system shutdown. (mode %d).", p_dio_mmap->iomode );
    buzzer( 5, 250, 250 );
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

    printf("%s started!\n", argv[0]);

    //    signal(SIGUSR1, sig_handler);
    //    signal(SIGUSR2, sig_handler);

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
                mcu_watchdogenable (delay) ;
                sleep(delay+20) ;
                mcu_reset();
                return 1;
            }
            else if( strcasecmp( argv[i], "-fwreset" )==0 ) {
                mcu_sendcmd( MCU_CMD_REBOOT ) ;			// only works on PWZEUS
                //				if( !mcu_reboot() ) {
                //	                mcu_reset();
                //				}
                return 0;
            }
        }
    }

    // setup signal handler
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
    p_dio_mmap->devicepower = DEVICE_POWER_RUN ;

    if( io_firstrun && gforce_crashdata_enable )  {
        gforce_getcrashdata();                      // iomode is set to IOMODE_REINITAPP
    }
    p_dio_mmap->iomode = IOMODE_RUN ;

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

        // do digital output
        mcu_doutput();

#ifdef PWII_APP
        // update PWII outputs
        mcu_pwii_output();
        pwii_keyautorelease() ;     // auto release pwii key pad (REC, C2, TM)
#endif

        // rtc command check
        check_rtccmd();

        // check cpu usage
        check_cpuusage();

        // check temperature
        check_temperature();

        // kick watchdog
        check_watchdog();

        static unsigned int appmode_timer ;
        if( (runtime - appmode_timer)> 3000 ) {    // 3 seconds mode timer
            appmode_timer=runtime ;

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

                if( p_dio_mmap->dvrpid>0 &&
                    (p_dio_mmap->dvrstatus & DVR_DISKREADY) )
                {
                    if( gforce_crashdatalen>0 )  {
                        gforce_savecrashdata();
                    }
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
                        if( (p_dio_mmap->dvrstatus & DVR_SENSOR ) &&
                            (p_dio_mmap->dvrstatus & DVR_RECORD ) )
                        {
                            // extend shutdown delay time
                            modeendtime = runtime+5 ;
                        }
                        else {
                            // start wifi detecting
                            mode_detectwireless();
                        }
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

                    if( runtime>modeendtime ) {
                        dvr_log( "No smartserver detected!" );
                        // enter archiving mode
                        wifi_down();
                        mode_archive();
                    }
                    else if( p_dio_mmap->smartserver ) {
                        if( smartftp_disable==0 ) {
                            mode_upload();
                        }
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

                    if( runtime>modeendtime &&
                        p_dio_mmap->dvrpid>0 &&
                        p_dio_mmap->dvrcmd==0 &&
                        (p_dio_mmap->dvrstatus & DVR_ARCH ) )
                    {
                        // try to stop archiving
                        p_dio_mmap->dvrcmd=6 ;
                    }
                    else if( p_dio_mmap->dvrpid<=0 ||
                             ( (p_dio_mmap->dvrstatus & DVR_RECORD )==0 &&
                               (p_dio_mmap->dvrstatus & DVR_ARCH )==0 ) )
                    {
                        // do gforce crashdata collection here. (any other choice?)
                        if( gforce_crashdata_enable )  {
                            gforce_getcrashdata();
                            // save data right away, of data may get lost if power turn off
                            if( gforce_crashdatalen>0 )  {
                                gforce_savecrashdata();
                            }
                        }
                        // enter standby mode
                        mode_standby();
                    }
                }
                else {
                    p_dio_mmap->dvrcmd=6 ;
                    mode_run();                                     // back to run normal
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_STANDBY ) {         // standby
                if( p_dio_mmap->poweroff == 0 ) {
#ifdef PWII_APP
                    // pwii jump out of standby
                    dio_lock();
                    p_dio_mmap->pwii_output &= ~PWII_POWER_BLACKOUT ;	// standby off
                    p_dio_mmap->pwii_output |= PWII_POWER_LCD ;      	// LCD on
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
                    p_dio_mmap->pwii_output |= PWII_LED_POWER ;

                    if( p_dio_mmap->dvrpid>0 &&
                        (p_dio_mmap->dvrstatus & DVR_RUN) &&
                        (p_dio_mmap->dvrstatus & DVR_NETWORK) )
                    {
                        p_dio_mmap->devicepower=DEVICE_POWER_RUN ;    // turn on all devices power as runing mode
                        if( disk_mounted==0 ) {
                            mountdisks();
                        }
                    }
                    else {
                        p_dio_mmap->devicepower=DEVICE_POWER_STANDBY ;   // turn off all devices power as standby mode
                        if( disk_mounted ) {
                            dvr_log("Unmount disks on standby mode.") ;
                            umountdisks();
                        }
                    }

                    // turn off HD power ?
                    if( standbyhdoff ) {
                        p_dio_mmap->outputmap &= ~HDLED ;   // turn off HDLED
                        if( hdpower ) {
                            hdpower=0;
                            mcu_hdpoweroff ();				// warning , this may turn off system power
                        }
                    }

                    if( runtime>modeendtime  ) {
                        // start shutdown
                        mode_shutdown();
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
                    mcu_watchdogenable(3) ;                 // let watchdog time out in 3 seconds
                    usewatchdog=0 ;                         // don't kick watchdog
                    reboot_begin=1 ;                        // rebooting in process
                    modeendtime=runtime+20000 ;             // 20 seconds time out, watchdog should kick in already!
                }
                else if( runtime>modeendtime ) {
                    // program should not go throught here,
                    mcu_reset();
                    system("/bin/reboot");                  // do software reboot
                    p_dio_mmap->iomode=IOMODE_QUIT ;        // quit IOPROCESS
                }
            }
            else if( p_dio_mmap->iomode==IOMODE_REINITAPP ) {
                dvr_log("IO re-initialize.");
                appfinish();
                appinit();
                p_dio_mmap->iomode = IOMODE_RUN ;
            }
            else if( p_dio_mmap->iomode==IOMODE_POWEROFF ) {	// to force power off, only on ignition is off
                sync();
                if( p_dio_mmap->glogpid > 0 ) {
                    kill(p_dio_mmap->glogpid, SIGTERM);
                }
                if( p_dio_mmap->dvrpid > 0 && runtime<modeendtime  ) {
                    kill(p_dio_mmap->dvrpid, SIGTERM);
                }
                else {
                    sync();
                    mcu_reset();							// use mcu_reset to force POWER off
                }
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
            if( usewatchdog &&
                p_dio_mmap->dvrpid>0 &&
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
        if( (runtime - hd_timer)> 500 ) {    // 0.5 seconds to check hard drive and power button
            hd_timer=runtime ;

            // check HD plug-in state
            if( hdlock && hdkeybounce<10 ) {          // HD lock on
                if( hdpower==0 ) {
                    // turn on HD power
                    mcu_hdpoweron() ;
                }
                hdkeybounce = 0 ;

                if( hdinserted &&                      // hard drive inserted
                    usewatchdog &&
                    p_dio_mmap->dvrpid > 0 &&
                    p_dio_mmap->dvrwatchdog >= 0 &&
                    (p_dio_mmap->dvrstatus & DVR_RUN) &&
                    (p_dio_mmap->dvrstatus & DVR_DISKREADY )==0 )        // dvrsvr is running but no disk detected
                {
                    p_dio_mmap->outputmap ^= HDLED ;

                    if( ( p_dio_mmap->iomode==IOMODE_RUN ||
                          p_dio_mmap->iomode==IOMODE_SHUTDOWNDELAY ) &&
                        ++hdpower>hd_timeout*2 )                		// 120 seconds, sometimes it take 100s to mount HD(CF)
                    {
                        dvr_log("Hard drive failed, system reset!");
                        buzzer( 10, 250, 250 );

                        // turn off HD led
                        p_dio_mmap->outputmap &= ~HDLED ;
                        mcu_hdpoweroff();                   // FOR NEW MCU, THIS WOULD TURN OFF SYSTEM POWER?! (this stop working since 2011-11-15)
                        hdpower=0;
                        // added on 2011-11-18, force system to reboot.
                        p_dio_mmap->iomode=IOMODE_REBOOT ;
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
                        dvr_log( "HDD key off, unmount disks.");
                        system(APP_DIR"/umountdisks") ;
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
                        mcu_devicepower(i, (p_dio_mmap->devicepower & (1<<i))!=0 );
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
            if( (runtime - adjtime_timer)> 1800000 ||
                (runtime < adjtime_timer) ||
                adjtime_timer == 0 )
            {
                // call adjust time every 30 minute
                adjtime_timer=runtime ;
                if( gpsavailable ) {
                    if( gpsvalid ) {
                        time_syncgps();
                    }
                }
                else if( g_syncrtc ) {
                    time_syncmcu();
                }
            }
        }
    }

    appfinish();

    dvr_log( "DVR IO process ended!");

    printf("%s End!\n", argv[0]);
    return 0;
}

