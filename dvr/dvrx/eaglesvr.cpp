
#include "dvr.h"

pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;

static pthread_mutex_t dvr_mutex;

int app_state;				// APPQUIT, APPUP, APPDOWN, APPRESTART
int g_keycheck=0 ;
char g_hostname[]="EAGLE" ;
string logfile ;
int g_lowmemory ;
char dvrconfigfile[] = "" ;
void dvr_lock()
{
    pthread_mutex_lock(&dvr_mutex);
}

void dvr_unlock()
{
    pthread_mutex_unlock(&dvr_mutex);
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

// write log to log file. 
// return 
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(char *fmt, ...)
{
    va_list ap ;
    va_start( ap, fmt );
    printf("eaglesvr:");
    vprintf(fmt, ap );
    printf("\n");
    va_end( ap );
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
}

void app_exit()
{
}
 
void do_init()
{
   
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

    // at this point, memory pool should be clean
    if( g_memused > 100 ) {
        dvr_log("Unsolved memory leak, request to restart system" );
        sleep(60);
        app_state = APPQUIT ;
    }

    time_uninit();

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

    // initial mutex
    memcpy( &dvr_mutex, &mutex_init, sizeof(mutex_init));
    
    mem_init();
    
    app_ostate = APPDOWN ;
    app_state = APPUP ;
    
    while( app_state!=APPQUIT ) {
        if( app_state == APPUP ) {					// application up
            serial++ ;
            if( app_ostate != APPUP ) {
                do_init();
                app_ostate = APPUP ;
            }
            time_tick();
            sleep(1);                               // will wake up on signal.
        }
        else if (app_state == APPDOWN ) {			// application down
            if( app_ostate == APPUP ) {
                app_ostate = APPDOWN ;
                do_uninit();
            }
            sleep(1);                               // will wake up on signal.
        }
        else if (app_state == APPRESTART ) {
            app_state = APPUP ;
            if( app_ostate == APPUP ) {
                do_uninit();
                app_ostate = APPDOWN ;
            }
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

    pthread_mutex_destroy(&dvr_mutex);

    return EXIT_NOACT ;
}
