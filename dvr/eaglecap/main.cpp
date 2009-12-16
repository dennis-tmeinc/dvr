
#include "dvr.h"

char g_hostname[128] ;
pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;

static pthread_mutex_t dvr_mutex;
int system_shutdown;

int app_state;				// APPQUIT, APPUP, APPDOWN, APPRESTART

int g_lowmemory ;

void dvr_lock()
{
    pthread_mutex_lock(&dvr_mutex);
}

void dvr_unlock()
{
    pthread_mutex_unlock(&dvr_mutex);
}

// lock with lock variable
void dvr_lock( void * lockvar, int delayus )
{
    dvr_lock();
    while (*(int *)lockvar) {
        dvr_unlock();
        usleep( delayus );
        dvr_lock();
    }
    (*(int *)lockvar)++ ;
    dvr_unlock();
}

void dvr_unlock( void * lockvar )
{
    dvr_lock();
    (*(int *)lockvar)-- ;
    dvr_unlock();
}

// get a random number 
unsigned dvr_random()
{
    FILE * fd ;
    unsigned ran ;
    fd = fopen("/dev/urandom", "r");
    if( fd ) {
        fread( &ran, 1, sizeof(ran), fd );
        fclose( fd );
    }
    return ran ;
}

int dvr_log( char * fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    vprintf( fmt, args );
    va_end( args );
    return 0 ;
}

int dvr_getsystemsetup(struct system_stru * psys)
{
    return 0 ;
}

int dvr_setsystemsetup(struct system_stru * psys)
{
    return 0 ;
}

int app_restart;
unsigned long app_signalmap=0;
int app_signal_ex=0;

void sig_handler(int signum)
{
    if( signum<32 ) {
        app_signalmap |= ((unsigned long)1)<<signum;
    }
    else {
        app_signal_ex=signum ;
    }
}

void sig_check()
{
    unsigned long sigmap = app_signalmap ;
    app_signalmap=0 ;
    if( sigmap == 0 ) {
        return ;
    }
    
    if( sigmap & (1<<SIGTERM) ) 
    {
        dvr_log("Signal <SIGTERM> captured.");
        app_state = APPQUIT ;
    }
    else if( sigmap & (1<<SIGQUIT) ) 
    {
        dvr_log("Signal <SIGQUIT> captured.");
        app_state = APPQUIT ;
    }
    else if( sigmap & (1<<SIGINT) ) 
    {
        dvr_log("Signal <SIGINT> captured.");
        app_state = APPQUIT ;
    }
    else if( sigmap & (1<<SIGUSR2) ) 
    {
        app_state = APPRESTART ;
    }
    else if( sigmap & (1<<SIGUSR1) ) 
    {
        app_state = APPDOWN ;
    }
    
    if( sigmap & (1<<SIGPIPE) ) 
    {
        dvr_log("Signal <SIGPIPE> captured.");
    }
    
    if( app_signal_ex ) {
        dvr_log("Signal %d captured.", app_signal_ex );
        app_signal_ex=0 ;
    }

}

static string pidfile ;
// one time application initialization
void app_init()
{
    static int app_start=0;
    string t ;
    string tz ;
    string tzi ;
    
    g_lowmemory=10000 ;

    // setup signal handler	
    signal(SIGQUIT, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);
    signal(SIGPOLL, sig_handler);
	// ignor these signal
    signal(SIGPIPE, SIG_IGN);		// ignor this

    if( app_start==0 ) {
        app_start=1 ;
    }
}

void app_exit()
{
    sync();
    dvr_log("Quit DVR.\n");
}

void do_init()
{
    // initial mutex
    memcpy( &dvr_mutex, &mutex_init, sizeof(mutex_init));
    
    dvr_log("Start initializing.");

    app_init();
    
    time_init();
    cap_init();
    net_init();

    cap_start();	// start capture 
}

void do_uninit()
{

    dvr_log("Start un-initializing.");
    cap_stop();		// stop capture

    net_uninit();
    cap_uninit();
    time_uninit();

    pthread_mutex_destroy(&dvr_mutex);
}

// dvr exit code
#define EXIT_NORMAL		(0)
#define EXIT_NOACT		(100)
#define EXIT_RESTART		(101)
#define EXIT_REBOOT		(102)
#define EXIT_POWEROFF		(103)

int main()
{
    unsigned int serial=0;
    int app_ostate;				// application status. ( APPUP, APPDOWN )
    struct timeval time1, time2 ;
    int    t_diff ;

    mem_init();
    
    app_ostate = APPDOWN ;
    app_state = APPUP ;
    
    while( app_state!=APPQUIT ) {
        if( app_state == APPUP ) {					// application up
            serial++ ;
            if( app_ostate != APPUP ) {
                do_init();
                app_ostate = APPUP ;
                gettimeofday(&time1, NULL);
            }
            if( (serial%400)==119 ){				// every 3 second
                // check memory
                if( mem_available () < g_lowmemory ) {
                    app_state = APPRESTART ;
                }
            }
            if( (serial%80)==3 ){					// every 1 second
                gettimeofday(&time2, NULL);
                t_diff = (int)(int)(time2.tv_sec - time1.tv_sec) ;
                if( t_diff<-100 || t_diff>100 )
                {
                }
                time1.tv_sec = time2.tv_sec ;
            }
//            screen_io(12500);							// do screen io
            usleep( 12500 );
        }
        else if (app_state == APPDOWN ) {			// application down
            if( app_ostate == APPUP ) {
                app_ostate = APPDOWN ;
                do_uninit();
            }
            usleep( 12500 );
        }
        else if (app_state == APPRESTART ) {
            if( app_ostate == APPUP ) {
                app_ostate = APPDOWN ;
                do_uninit();
            }
            app_state = APPUP ;
            usleep( 100 );
        }
        sig_check();
    }
    
    if( app_ostate==APPUP ) {
        app_ostate=APPDOWN ;
        do_uninit();
    }
    
    app_exit();
    mem_uninit ();
    
    if (system_shutdown) {              // requested system shutdown
        // dvr_log("Reboot system.");
        return EXIT_REBOOT ;
    }
    else {
        return EXIT_NOACT ;
    }
}
