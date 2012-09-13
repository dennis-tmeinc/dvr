
#ifndef __eaglesvr_h__
#define __eaglesvr_h__

// #define DBG

// standard headers
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <fnmatch.h>
#include <termios.h>
#include <stdarg.h>

#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../cfg.h"
#include "../dvrsvr/dvrprotocol.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/config.h"

// dvr memory allocation
#include "../dvrsvr/memory.h"

#define EAGLESVRPORT (15159)

int dvr_log(char *fmt, ...);

// used types
typedef unsigned int DWORD;
typedef unsigned short int WORD;
typedef unsigned char BYTE;

// application variables, functions

#define APPQUIT 	(0)
#define APPUP   	(1)
#define APPDOWN 	(2)
#define APPRESTART	(3)

extern pthread_mutex_t mutex_init ;
extern int app_state;
extern int system_shutdown;
extern float g_cpu_usage ;
extern int g_lowmemory;
extern int g_timetick ;            // global time tick ;


//void dvr_lock();
//void dvr_unlock();

unsigned dvr_random();

struct cap_frame {
    int channel;
    int framesize;
    int frametype;
    char * framedata;
};

// capture

extern int cap_channels;

void cap_start();
void cap_stop();
void cap_init();
void cap_uninit();

struct hik_osd_type {
    int brightness ;
    int translucent ;
    int twinkle ;
    int lines ;
    WORD osdline[8][128] ;
} ;

class capture {
protected:
    struct  DvrChannel_attr	m_attr ;	// channel attribute
    int m_channel;				// channel number
    int m_enable ;				// 1: enable, 0:disable
    int m_motion ;				// motion status ;
    int m_signal ;				// signal ok.
    int m_oldsignal ;           // signal ok.
    int m_signal_standard ;     // 1: ntsc, 2: pal

    int m_started ;             // 0: stopped, 1: started

    int m_headerlen ;
    char m_header[256] ;

public:
    capture(int channel) {
        m_channel = channel ;
    }

    virtual ~capture(){}

    int enabled(){
        return m_enable;
    }
    int isstarted(){
        return ( m_started ) ;
    }

    void onframe(cap_frame * pcapframe);	// send frames to network or to recording
    char * getname(){
        return m_attr.CameraName ;
    }

    virtual int getheaderlen(){return m_headerlen;}
    virtual char * getheader(){return m_header;}

    virtual void getattr(struct DvrChannel_attr * pattr)=0;
    virtual void setattr(struct DvrChannel_attr * pattr)=0;

    virtual void setosd( struct hik_osd_type * posd ){}
    virtual void start(){}				// start capture
    virtual void stop(){}				// stop capture
    virtual void restart(){				// restart capture
        stop();
        if( m_enable ) {
            start();
        }
    }
    virtual	void streamcallback( void * buf, int size, int frame_type)=0 ;	// live stream call back

    virtual void captureIFrame(){        // force to capture I frame
    }
    // to capture one jpeg frame
    virtual void captureJPEG(int quality, int pic)=0 ;

    virtual int getsignal(){return 0;}	// get signal available status, 1:ok,0:signal lost
    virtual int getmotion(){return 0;}	// get motion detection status
};

extern capture * * cap_channel;

void eagle_idle();
int  eagle_init();
void eagle_uninit();
void eagle_finish();
// jpeg capture
void eagle_captureJPEG();

#ifdef EAGLE368
void eagle368_startcapture();
void eagle368_stopcapture();
#endif

struct dayinfoitem {
    int ontime  ;		// seconds of the day
    int offtime ;		// seconds of the day
} ;

void time_init();
void time_uninit();
void time_settimezone(char * timezone);
int  time_gettimezone(char * timezone);
void time_inittimezone();

// struct used for synchronize dvr time. (sync time with ip camera)
struct dvrtimesync {
    unsigned int tag ;
    struct dvrtime dvrt ;
    char tz[128] ;
} ;

void time_now(struct dvrtime *dvrt);
int time_tick();
time_t time_utctime(struct dvrtime *dvrt);
int time_setlocaltime(struct dvrtime *dvrt);
int time_setutctime(struct dvrtime *dvrt);
int time_adjtime(struct dvrtime *dvrt);

time_t time_dvrtime_init( struct dvrtime *dvrt,
              int year=2000,
              int month=1,
              int day=1,
              int hour=0,
              int minute=0,
              int second=0,
              int milliseconds=0 );
time_t time_dvrtime_zerohour( struct dvrtime *dvrt );
time_t time_dvrtime_nextday( struct dvrtime *dvrt );

int  time_dvrtime_diff( struct dvrtime *t1, struct dvrtime *t2) ;	// difference in seconds
int  time_dvrtime_diffms( struct dvrtime *t1, struct dvrtime *t2 ); // difference in milliSecond
void  time_dvrtime_add( struct dvrtime *t, int seconds);				// add seconds to t
void  time_dvrtime_addms( struct dvrtime *t, int ms);				// add milliseconds to t
void  time_dvrtime_del( struct dvrtime *dvrt, int seconds);

// convert between dvrtime and time_t
struct dvrtime * time_dvrtimelocal( time_t t, struct dvrtime * dvrt);
struct dvrtime * time_dvrtimeutc( time_t t, struct dvrtime * dvrt);
time_t time_timelocal( struct dvrtime * dvrt);
time_t time_timeutc( struct dvrtime * dvrt);
int time_readrtc(struct dvrtime * dvrt);
int time_setrtc();

inline int operator > ( struct dvrtime & t1, struct dvrtime & t2 )
{
    if(      t1.year   != t2.year )   return (t1.year   > t2.year);
    else if( t1.month  != t2.month )  return (t1.month  > t2.month);
    else if( t1.day    != t2.day )    return (t1.day    > t2.day);
    else if( t1.hour   != t2.hour )   return (t1.hour   > t2.hour);
    else if( t1.minute != t2.minute ) return (t1.minute > t2.minute);
    else if( t1.second != t2.second ) return (t1.second > t2.second);
    else return (t1.milliseconds > t2.milliseconds);
}

inline int operator < ( struct dvrtime & t1, struct dvrtime & t2 )
{
    if(      t1.year   != t2.year )   return (t1.year   < t2.year);
    else if( t1.month  != t2.month )  return (t1.month  < t2.month);
    else if( t1.day    != t2.day )    return (t1.day    < t2.day);
    else if( t1.hour   != t2.hour )   return (t1.hour   < t2.hour);
    else if( t1.minute != t2.minute ) return (t1.minute < t2.minute);
    else if( t1.second != t2.second ) return (t1.second < t2.second);
    else return (t1.milliseconds > t2.milliseconds);
}

inline int operator == ( struct dvrtime & t1, struct dvrtime & t2 )
{
    return ( (t1.year         == t2.year  )  &&
             (t1.month        == t2.month )  &&
             (t1.day          == t2.day   )  &&
             (t1.hour         == t2.hour  )  &&
             (t1.minute       == t2.minute ) &&
             (t1.second       == t2.second ) &&
             (t1.milliseconds == t2.milliseconds) );
}

inline int operator != (struct dvrtime & t1, struct dvrtime & t2 )
{
    return ( (t1.year   != t2.year)   ||
             (t1.month  != t2.month)  ||
             (t1.day    != t2.day)    ||
             (t1.hour   != t2.hour)   ||
             (t1.minute != t2.minute) ||
             (t1.second != t2.second) ||
             (t1.milliseconds != t2.milliseconds) );
}

struct dvrtime operator + ( struct dvrtime & t, int seconds ) ;
struct dvrtime operator + ( struct dvrtime & t, double seconds ) ;
double operator - ( struct dvrtime &t1, struct dvrtime &t2 ) ;

// live video service
struct net_fifo {
    int bufsize;
    int loc;
    char *buf;
    struct net_fifo *next;
};

struct sockad {
    struct sockaddr addr;
    char padding[128];
    socklen_t addrlen;
};

// network active
int net_wait(int timeout_ms);
// trigger a new network wait cycle
int net_sendok(int fd, int tout);
int net_recvok(int fd, int tout);
void net_message();
int net_sendmsg( char * dest, int port, const void * msg, int msgsize );
void net_dprint( char * fmt, ... ) ;
#ifdef NETDBG
#define NET_DPRINT net_dprint
#else
#define NET_DPRINT
#endif

#define closesocket(s) close(s)

int net_broadcast( char * interface, int port, void * msg, int msgsize );
int net_detectsmartserver();
int net_listen(int port, int socktype);
int net_connect(char *ip, int port);
void net_close(int sockfd);
void net_clean(int sockfd);
int net_send(int sockfd, void * data, int datasize);
int net_recv(int sockfd, void * data, int datasize);
int net_onframe(cap_frame * pframe);
// initialize network
void net_init();
void net_uninit();

extern void * net_shm_heap ;

enum conn_type { CONN_NORMAL = 0, CONN_REALTIME, CONN_LIVESTREAM, CONN_LIVESTREAM2 };

class dvrsvr {
protected:
    static dvrsvr *m_head;
    dvrsvr *m_next;		// dvr list
    int m_sockfd;		// socket
    int m_shutdown;             // shutdown flag
    struct net_fifo *m_fifo;
    struct net_fifo *m_fifotail;
    int m_fifosize ;		// size of fifo
    int m_fifodrop ;            // drop fifo

    pthread_mutex_t m_mutex;
    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }

    // dvr related struct
    int m_conntype;				// 0: normal, 1: realtime video stream, 2: answering
    int m_connchannel;
    int m_headersent ;          // postponed header sending

    // receiving buffer
    struct dvr_req m_req;
    char *m_recvbuf;
    int m_recvlen;
    int m_recvloc;

    struct BITMAP * m_fontcache ;	// font cache for drawing

    void send_fifo(char *buf, int bufsize, int loc = 0);

public:
    dvrsvr(int fd);
    virtual ~dvrsvr();
    static dvrsvr *head() {
        return m_head;
    }

    dvrsvr *next() {
        return m_next;
    }
    int hasfifo() {
        return m_fifo != NULL;
    }
    int socket() {
        return m_sockfd;
    }
    int  read();
    int  write();
    void cleanfifo();
    void cleanfifotail();

    virtual void Send(void *buf, int bufsize);
    void down() {
        m_shutdown=1 ;
    }
    int isdown() {
        return (m_shutdown);
    }
    int onframe(cap_frame * pframe);

    void AnsOk();
    void AnsError();

    // request handlers
    void onrequest();

    void ReqRealTime();
    void ReqChannelInfo();
    void GetChannelSetup();
    void SetChannelSetup();
    void GetChannelState();
    void ReqEcho();
    void ReqOpenLive();
    void ReqOpenLiveShm();
    void ReqInitShm();
    void Req2SetLocalTime();
    void Req2GetLocalTime();
    void Req2AdjTime();
    void Req2SetSystemTime();
    void Req2GetSystemTime();
    void Req2SetTimeZone();
    void ReqSetHikOSD();
    void Req2GetChState();
    void Req2GetJPEG();

    // EagleSVR supports
    void ReqScreenSetMode();
    void ReqScreenLive();
    void ReqScreenPlay();
    void ReqScreenInput();
    void ReqScreenStop();
    void ReqScreenAudio();
    void ReqGetScreenWidth();
    void ReqGetScreenHeight();
    void ReqDrawSetArea();
    void ReqDrawRefresh();
    void ReqDrawSetColor();
    void ReqDrawGetColor();
    void ReqDrawSetPixelMode();
    void ReqDrawGetPixelMode();
    void ReqDrawPutPixel();
    void ReqDrawGetPixel();
    void ReqDrawLine();
    void ReqDrawRect();
    void ReqDrawFill();
    void ReqDrawCircle();
    void ReqDrawFillCircle();
    void ReqDrawBitmap();
    void ReqDrawStretchBitmap();
    void ReqDrawReadBitmap();
    void ReqDrawSetFont();
    void ReqDrawText();
    void ReqDrawTextEx();

    // EAGLE368 Specific
    void ReqStartCapture();
    void ReqStopCapture();
};

#endif							// __eaglesvr_h__
