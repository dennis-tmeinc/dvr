
#include "eaglesvr.h"

pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;
static pthread_mutex_t dvr_mutex;

int app_state;				// APPQUIT, APPUP, APPDOWN, APPRESTART

int g_lowmemory ;

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
    memset( psys, 0, sizeof(struct system_stru) );
    strcpy(  psys->productid, "EAGLE");
    strncpy(psys->ServerName, (char *)g_servername, sizeof(psys->ServerName));
    psys->cameranum = cap_channels;
    return 1 ;
}

int app_restart;
unsigned long app_signalmap=0;
int app_signal_ex=0;
const char g_servername[]="eaglesvr";

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
#ifdef EAGLE34
        app_state = APPDOWN ;       // APPQUIT make system reset in eagle34 board, so we just do APPDOWN
#else	// EAGLE34
        app_state = APPQUIT ;
#endif	// EAGLE34
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

// one time application initialization
void app_init()
{
    static int app_start=0;

    FILE * fid = NULL ;

    // make var directory ("/var/dvr") if it is not there.
    mkdir( VAR_DIR, 0777 );

    fid=fopen(VAR_DIR"/eaglesvr.pid", "w");
    if( fid ) {
        fprintf(fid, "%d", (int)getpid());
        fclose(fid);
    }

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
        dvr_log("Start EagleSvr.");
        app_start=1 ;
    }
}

void app_exit()
{
    remove(VAR_DIR"/eaglesvr.pid");
    dvr_log("Quit Eaglesvr.\n");
}

void do_init()
{
    dvr_log("Start initializing.");

    app_init();

    time_init();
    cap_init();
    screen_init();
    net_init();

//    cap_start();	// let ip cam to start the channel
}

void do_uninit()
{

    dvr_log("Start un-initializing.");
    cap_stop();		// stop capture

    net_uninit();
    screen_uninit();
    cap_uninit();
    time_uninit();

    // at this point, memory pool should be clean
    if( g_memused > 100 ) {
        dvr_log("Possible memory leak detected.");
    }

}

int main()
{
    int app_ostate;				// application status. ( APPUP, APPDOWN )

    // initial mutex
    memcpy( &dvr_mutex, &mutex_init, sizeof(mutex_init));

    mem_init();

    app_ostate = APPDOWN ;
    app_state = APPUP ;

    while( app_state!=APPQUIT ) {
        if( app_state == APPUP ) {					// application up
            if( app_ostate != APPUP ) {
                do_init();
                app_ostate = APPUP ;
            }
            time_tick();
            net_wait(1000) ;
#ifdef EAGLE368
            eagle_idle();
#endif
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
        }
        sig_check();
    }

    if( app_ostate==APPUP ) {
        app_ostate=APPDOWN ;
        do_uninit();
    }

    app_exit();
    mem_uninit ();
    // try un-init eagle board
    eagle_finish();

    pthread_mutex_destroy(&dvr_mutex);
    return 0 ;
}
