
#ifndef __dvr_h__
#define __dvr_h__

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
#include <sys/file.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

// common configure
#include "../cfg.h"

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef UINT32
#define UINT32 unsigned int
#endif

#ifndef DWORD
#define DWORD unsigned int
#endif

#ifndef WORD
#define WORD unsigned short
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif

// dvr memory allocations
#include "./memory.h"

#include "dvrprotocol.h"
#include "crypt.h"
#include "genclass.h"
#include "config.h"
#include "capture.h"
#include "netcapture.h"

#define SUPPORT_EVENTMARKER	1

#define DVRVERSION0		(2)
#define	DVRVERSION1		(0)
#define DVRVERSION2		(8)
#define	DVRVERSION3		(521)

// application variables, functions

#define APPQUIT 	(0)
#define APPUP   	(1)
#define APPDOWN 	(2)
#define APPRESTART	(3)

extern pthread_mutex_t mutex_init ;
extern volatile int app_state;
extern volatile int system_shutdown;
extern volatile float g_cpu_usage ;
extern volatile int g_lowmemory;
extern string g_servername ;
extern volatile int g_timetick ;            // global time tick ;
extern volatile int g_keyactive ;

extern unsigned char g_filekey[256] ;
extern const char g_264ext[] ;

// TVS related
extern char g_mfid[32] ;
extern int g_keycheck ;
extern char g_serial[64] ;
extern char g_id1[64] ;
extern char g_id2[64] ;

// PWII vri
extern char g_vri[128];
extern string g_policeidlistfile ;
extern char g_policeid[32];

int dvr_log(const char *fmt, ...);
void dvr_logkey( int op, struct key_data * key ) ;
//void dvr_lock();
//void dvr_unlock();

unsigned dvr_random();

// recording support
class rec_index {
protected:
    struct rec_index_item {
        int onoff ;
        int onofftime ;
    } * idx_array ;
    array <struct rec_index_item> m_array ;

public:
    rec_index(){}
    ~rec_index(){}
    void empty(){
        m_array.empty();
    }
    void addopen(int opentime);
    void addclose(int closetime);
    void savefile( char * filename );
    void readfile( char * filename );
    int getsize() {
        return m_array.size();
    }
    int getidx( int n, int * ponoff, int * ptime ) {
        if( n<0 || n>=m_array.size() )
            return 0;
        *ponoff=m_array[n].onoff ;
        *ptime=m_array[n].onofftime ;
        return 1 ;
    }
};

/*
    key file format
        1st_frame_milliseconds, 1st_frame_offset\n
        2nd_frame_milliseconds, 2nd_frame_offset\n
        ...
        Nth_frame_milliseconds, Nth_frame_offset\n
*/
// key frame index support
struct dvr_key_t {
    int ktime;				// key frame time, in milliseconds, from start of file(file time in filename)
    int koffset ;			// key frame offset
} ;

// dvrfile
class dvrfile {
protected:
    FILE * m_handle;

    int	   m_fileencrypt ;		// if file is encrypted?
    int    m_autodecrypt ;      // auto - decrypt when reading.
    int    m_initialsize;

    string m_filename ;
    int    m_openmode ;			// 0: read, 1: write
    DWORD  m_hdflag ;           // file header flag when reading

    struct dvrtime m_filetime ;	// file start time (as from file name)
    int   m_filesize ;          // file size in bytes

    int   m_filebuffersuggestsize;  // suggested buffer size
    int   m_filebuffersize ;        // real buffer size
    int   m_filebufferpos ;         // valid data size
    char *m_filebuffer ;

    int   m_filelen ;			// file length in seconds
    int	  m_filestart ;			// first frame offset

    // current frame info
    DWORD m_reftstamp ;			// referent frame time stamp
    int   m_reftime ;			// referent frame time (milliseconds from file time)

    int   m_framepos ;          // current frame position
    int   m_framesize ;         // current frame size
    int   m_frametype ;         // current frame type
    int   m_frametime ;         // current frame time (milliseconds from file time)
    int   m_frame_kindex ;

    array <struct dvr_key_t> m_keyarray ;

    int nextframe();
    int prevframe();

public:
    dvrfile() ;
    ~dvrfile();
    int open(const char *filename, char *mode = "r", int initialsize = 0);
    void close();
    int headersize() { return m_filestart; }
    int writeheader(void * buffer, size_t headersize) ;
    int readheader(void * buffer, size_t headersize) ;
    int read(void *buffer, size_t buffersize);
    int write(void *buffer, size_t buffersize);
    void setbufsize( int bufsize) ;
    void flushbuffer();

    int repair();							// repair a .264 file
    int repairpartiallock() ;               // repair a partial locked file
    int tell();
    int seek(int pos, int from = SEEK_SET);
    int truncate( int tsize );
    // seek to specified time,
    //   return 1 if seek inside the file.
    //	return 0 if seek outside the file, pointer set to begin or end of file
    int seek( dvrtime * seekto );

    int getframe();                 // advance to next frame and retrive frame info
    dvrtime frametime();			// return current frame time
    int frametype();                // return current frame type
    int framesize();                // return current frame size

    int prevkeyframe();
    int nextkeyframe();
    int readframe(void * framebuf, size_t bufsize);
    int writeframe(void * buffer, size_t size, int frametype, dvrtime * frametime);
    int isopen() {
        return m_handle != NULL;
    }
    int isencrypt() {
        return m_fileencrypt ;
    }
    int gethdflag() {               // get file header flag.
        return (int)m_hdflag ;
    }
    void autodecrypt(int decrypt) {
        m_autodecrypt=decrypt ;
    }
    void writekey();
    void readkey() ;

    static int rename(const char * oldfilename, const char * newfilename); // rename .264 filename as well .k file
    static int remove(const char * filename);		// remove .264 file as well .k file
    static int chrecmod(string & filename, char oldmod, char newmode);
    static int unlock(const char * filename) ; 		// unlock this .264 file
    static int lock(const char * filename) ;		// lock this .264 file
};

// file op
void file_init(config &dvrconfig);
void file_uninit();

// ptz service
void ptz_init(config &dvrconfig);
void ptz_uninit();
int ptz_msg( int channel, DWORD command, int param );

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
void  time_dvrtime_del( struct dvrtime *dvrt, int seconds);		// go back n seconds

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
    return (t1.year         == t2.year     &&
            t1.month        == t2.month    &&
            t1.day          == t2.day      &&
            t1.hour         == t2.hour     &&
            t1.minute       == t2.minute   &&
            t1.second       == t2.second   &&
            t1.milliseconds == t2.milliseconds );

}

inline int operator != (struct dvrtime & t1, struct dvrtime & t2 )
{
    return (t1.year         != t2.year   ||
            t1.month        != t2.month  ||
            t1.day          != t2.day    ||
            t1.hour         != t2.hour   ||
            t1.minute       != t2.minute ||
            t1.second       != t2.second ||
            t1.milliseconds != t2.milliseconds );
};

struct dvrtime operator + ( struct dvrtime & t, int seconds ) ;
struct dvrtime operator + ( struct dvrtime & t, double seconds ) ;
double operator - ( struct dvrtime &t1, struct dvrtime &t2 ) ;

// recording service
#define DEFMAXFILESIZE (100000000)
#define DEFMAXFILETIME (24*60*60)

extern string rec_basedir;
extern int rec_pause;           // pause recording temperary
extern volatile int rec_busy ;
extern int rec_watchdog ;      // recording watchdog
extern int rec_norecord ;
void rec_init(config &dvrconfig);
void rec_uninit();
void rec_onframe(struct cap_frame *pframe);
void rec_record(int channel);
void rec_prerecord(int channel);
void rec_postrecord(int channel);
void rec_lockstart(int channel);
void rec_lockstop(int channel);
int  rec_state(int channel);
int  rec_triggered(int channel);
int  rec_lockstate(int channel);
void rec_lock(time_t locktime);
void rec_unlock();
void rec_break();
void rec_update();
void rec_alarm();
void rec_start();
void rec_stop();
/*
 struct nfileinfo {
    int channel;
    struct dvrtime filetime ;
    int filelength ;
    char extension[4] ;                         // ".k" or ".264"
    int  filesize ;                             // option, can be 0
} ;
FILE * rec_opennfile(int channel, struct nfileinfo * nfi);
*/

// disk and .264 file directories
// get .264 file list by day and channel
extern struct dvrtime disk_earliesttime ;
int f264length(const char *filename);				// get file length from file name
int f264locklength(const char *filename);			// get file lock length from file name
int f264time(const char *filename, struct dvrtime *dvrt);
int f264channel(const char *filename);
char *basefilename(const char *fullpath);

class f264name : public string {
public:
        int operator < ( f264name & s2 ) {
            struct dvrtime t1, t2 ;
            f264time( getstring(), &t1 );
            f264time( s2.getstring(), &t2 );
            return t1<t2 ;
        }
        f264name & operator =(const char *str) {
            setstring(str);
            return *this;
        }
};

int disk_listday(array <f264name> & list, struct dvrtime * day, int channel);
void disk_getdaylist(array <int> & daylist, int channel);
int disk_unlockfile( dvrtime * begin, dvrtime * end );
void disk_check();
void disk_sync();
void disk_archive_start();
void disk_archive_stop();
int disk_archive_runstate();
int disk_stat(int * recordtime, int * lockfiletime, int * remaintime);
int disk_renew(char * newfilename, int add=1);
void disk_init(config &dvrconfig);
void disk_uninit();
extern volatile int disk_busy ;
extern int disk_error ;


// live video service
struct net_fifo {
    int bufsize;
    int loc;
    char *buf;
    struct net_fifo *next;
};

class live {
    protected:
        int m_channel;
        net_fifo * m_fifo ;
        void * m_framebuf ;
        int    m_framesize;

    public:
        live( int channel );
        ~live();
        void getstreamdata(void ** getbuf, int * getsize, int * frametype);
        void onframe(cap_frame * pframe);
};


// playback service
void play_init(config &dvrconfig);
void play_uninit();

class playback {
    protected:
        int m_channel;

        array <int> m_daylist ;			// available data day list
        int m_day ;						// current day, in bcd format
        array <f264name> m_filelist ;	// day file list
        int m_curfile ;					// current opened file, index of m_filelist

        dvrfile m_file ;				// current opened file, always opened
        struct dvrtime m_streamtime ;	// current frame time

        // pre-read buffer
        void * m_framebuf ;				// pre-read buffer
        int  m_framesize ;
        int  m_frametype ;

        int opennextfile();
        int close();
        void readframe();

        int m_autodecrypt ;              // decrypt video files

    public:
        playback( int channel, int decrypt=0 ) ;
        ~playback();
        int seek( struct dvrtime * seekto );
        void getstreamdata(void ** getbuf, int * getsize, int * frametype);
        void preread();
        int getstreamtime(dvrtime * dvrt) {
            *dvrt=m_streamtime ;
            return 1;
        }
        void getdayinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday, int lock=0 );
        void getlockinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday) {
            getdayinfo( dayinfo, pday, 1 );
        }

        DWORD getmonthinfo(dvrtime * month);				// return day info in bits, bit 0 as day 1
        int  getdaylist( array <int> & daylist );

        int getcurrentcliptime(struct dvrtime * begin, struct dvrtime * end) ;
        int getnextcliptime(struct dvrtime * begin, struct dvrtime * end) ;
        int getprevcliptime(struct dvrtime * begin, struct dvrtime * end) ;
        int fileheadersize() ;
        int readfileheader(char *hdbuf, int hdsize);
};

// network service
struct sockad {
    struct sockaddr addr;
    char padding[128];
    socklen_t addrlen;
};

// maximum live fifo size
extern int net_livefifosize;
// network active
extern int net_active ;
// trigger a new network wait cycle
void net_trigger();
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
int net_broadcast( char * interface, int port, void * msg, int msgsize );
int net_detectsmartserver();
int net_listen(int port, int socktype);
int net_connect(const char *ip, int port);
void net_clean(int sockfd);
int net_send(int sockfd, void * data, int datasize);
int net_recv(int sockfd, void * data, int datasize, int ustimeout=5000000);
int net_onframe(cap_frame * pframe);
// initialize network
void net_init(config &dvrconfig);
void net_uninit();

struct channel_fifo {
    int channel;
};

enum conn_type { CONN_NORMAL = 0, CONN_REALTIME, CONN_LIVESTREAM, CONN_JPEG };

#define closesocket(s) close(s)

struct key_data {
    char checksum[128] ;
    int size ;
    int version ;
    char tag[4] ;
    char usbid[32] ;
    char usbkeyserialid[512] ;
    char videokey[512] ;
    char manufacturerid[32] ;
    int counter ;
    int keyinfo_start ;
    int keyinfo_size ;
} ;

class dvrsvr {
    protected:
        static dvrsvr *m_head;
        dvrsvr *m_next;				// dvr list
        int m_sockfd;				// socket
        int m_shutdown;             // shutdown flag
        struct net_fifo *m_fifo;
        struct net_fifo *m_fifotail;
        int m_fifosize ;			// size of fifo
        int m_fifodrop ;            // drop fifo
        pthread_mutex_t m_mutex;
        void lock() {
            pthread_mutex_lock(&m_mutex);
        }

        void unlock() {
            pthread_mutex_unlock(&m_mutex);
        }

        playback * m_playback ;

        // dvr related struct
        int m_conntype;				// 0: normal, 1: realtime video stream, 2: answering
        int m_connchannel;

        struct dvr_req m_req;
        char *m_recvbuf;
        int m_recvlen;
        int m_recvloc;

        // TVS support
        int  m_keycheck ;           // 1: key has been checked

    private:
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
        int isfifo() {
            return m_fifo != NULL;
        }
        int socket() {
            return m_sockfd;
        }
        int  read();
        int  write();
        void cleanfifo();
        void down() {
            m_shutdown=1 ;
        }
        int isdown() {
            return (m_shutdown);
        }
        int onframe(cap_frame * pframe);

    protected:

        virtual void Send(void *buf, int bufsize);

        // request handlers
        void onrequest();
        void AnsOk();
        void AnsError();

        virtual void ReqRealTime();
        virtual void ChannelInfo();
        virtual void DvrServerName();
        virtual void GetSystemSetup();
        virtual void SetSystemSetup();
        virtual void GetChannelSetup();
        virtual void SetChannelSetup();
        virtual void HostName();
        virtual void GetChannelState();
        virtual void GetVersion();
        virtual void ReqEcho();
        virtual void ReqPTZ();
        virtual void ReqStreamOpen();
        virtual void ReqOpenLive();
        virtual void ReqStreamClose();
        virtual void ReqStreamSeek();
        virtual void ReqStreamFileHeader();
        virtual void ReqNextFrame();
        virtual void ReqNextKeyFrame();
        virtual void ReqPrevKeyFrame();
        virtual void ReqStreamGetData();
        virtual void Req2StreamGetDataEx();
        virtual void ReqStreamTime();
        virtual void ReqStreamDayInfo();
        virtual void ReqStreamMonthInfo();
        virtual void ReqStreamDayList();
        virtual void ReqLockInfo();
        virtual void ReqUnlockFile();
        virtual void Req2SetLocalTime();
        virtual void Req2GetLocalTime();
        virtual void Req2AdjTime();
        virtual void Req2SetSystemTime();
        virtual void Req2GetSystemTime();
        virtual void Req2GetZoneInfo();
        virtual void Req2SetTimeZone();
        virtual void Req2GetTimeZone();
        virtual void ReqSetled();
        virtual void ReqSetHikOSD();
        virtual void Req2GetChState();
        virtual void Req2GetSetupPage();
        virtual void Req2GetStreamBytes();
        virtual void Req2Keypad();
        virtual void Req2PanelLights();
        virtual void ReqSendData();
        virtual void ReqGetData();
        virtual void ReqCheckKey();
        virtual void ReqUsbkeyPlugin();
        virtual void Req2GetJPEG();

};

// DVR client side support

// open live stream from remote dvr
int dvr_openlive(int sockfd, int channel);

// get remote dvr system setup
// return 1:success
//        0:failed
int dvr_getsystemsetup(int sockfd, struct system_stru * psys);

// set remote dvr system setup
// return 1:success
//        0:failed
int dvr_setsystemsetup(int sockfd, struct system_stru * psys);

// get channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_getchannelsetup(int sockfd, int channel, struct DvrChannel_attr * pchannelattr);

// set channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_setchannelsetup (int sockfd, int channel, struct DvrChannel_attr * pchannelattr);

// set channel OSD
// return 1:success
//        0:failed
int dvr_sethikosd(int sockfd, int channel, struct hik_osd_type * posd);

// get channel status
//    return value:
//			bit 0: signal lost
//          bit 1: motion detected
int dvr_getchstate(int sockfd, int ch);

// set remote dvr time
// return 1: success, 0:failed
int dvr_settime(int sockfd, struct dvrtime * dvrt);

// get remote dvr time
// return 1: success, 0:failed
int dvr_gettime(int sockfd, struct dvrtime * dvrt);

// get remote dvr time in utc
// return 1: success, 0:failed
int dvr_getutctime(int sockfd, struct dvrtime * dvrt);

// get remote dvr time
// return 1: success, 0:failed
int dvr_setutctime(int sockfd, struct dvrtime * dvrt);

// adjust remote dvr time
// return 1: success, 0:failed
int dvr_adjtime(int sockfd, struct dvrtime * dvrt);

// set remote dvr time zone
// return 1: success, 0: failed
int dvr_settimezone(int sockfd, char * tzenv);

// open new clip file on remote dvr
int dvr_nfileopen(int sockfd, int channel, struct nfileinfo * nfi);

// close nfile on remote dvr
// return
//       1: success
//       0: failed
int dvr_nfileclose(int sockfd, int nfile);


// read nfile from remote dvr
// return actual readed bytes
int dvr_nfileread(int sockfd, int nfile, void * buf, int size);


// received UDP message
#define MAXMSGSIZE (32000)
int msg_onmsg(int msgfd, void *msgbuf, int msgsize, struct sockad *from);

extern int msgfd;

// event
void setdio(int onoff);
int  event_check();
void event_init( config &dvrconfig );
void event_uninit();
void event_run();
extern int event_tm ;          // To support PWII Trace Mark


// return true for ok
int dvr_getsystemsetup(struct system_stru * psys);
// return true for ok
int dvr_setsystemsetup(struct system_stru * psys);
// return true for ok
int dvr_getchannelsetup(int ch, struct DvrChannel_attr * pchannel, int attrsize);
// return true for ok
int dvr_setchannelsetup(int ch, struct DvrChannel_attr * pchannel, int attrsize);


enum e_keycode {
    VK_RETURN=0x0D, // ENTER key
    VK_ESCAPE=0x1B, // ESC key
    VK_SPACE=0x20,  // SPACEBAR
    VK_PRIOR,       // PAGE UP key
    VK_NEXT,        // PAGE DOWN key
    VK_END,         // END key
    VK_HOME,        // HOME key
    VK_LEFT,        // LEFT ARROW key
    VK_UP,          // UP ARROW key
    VK_RIGHT,       // RIGHT ARROW key
    VK_DOWN,        // DOWN ARROW key

    VK_MEDIA_NEXT_TRACK=0xB0, // Next Track key
    VK_MEDIA_PREV_TRACK,      // Previous Track key
    VK_MEDIA_STOP,            // Stop Media key
    VK_MEDIA_PLAY_PAUSE,      // Play/Pause Media key

    // pwii definition
    VK_EM   = 0xE0 ,    // EVEMT
    VK_LP,              // PWII, LP key (zoom in)
    VK_POWER,           // PWII, B/O
    VK_SILENCE,         // PWII, Mute
    VK_RECON,           // Turn on all record (force on)
    VK_RECOFF,          // Turn off all record
    VK_C1,              // PWII, Camera1
    VK_C2,              // PWII, Camera2
    VK_C3,              // PWII, Camera3
    VK_C4,              // PWII, Camera4
    VK_C5,              // PWII, Camera5
    VK_C6,              // PWII, Camera6
    VK_C7,              // PWII, Camera7
    VK_C8,              // PWII, Camera8
    VK_MUTE,            // PWII, Faked Mute button
    VK_SPKON            // PWII, Faked Speaker On button
} ;

#define VK_FRONT (VK_C1)
#define VK_REAR (VK_C2)
#define VK_TM   (VK_EM)

// digital io functions
//extern "C" {
int dio_inputnum();
int dio_outputnum();
int dio_input( int no );
void dio_output( int no, int v);
// return 1 for poweroff switch turned off
int dio_poweroff();
//int dio_lockpower();
//int dio_unlockpower();
int dio_enablewatchdog();
int dio_disablewatchdog();
int dio_kickwatchdog();
//void dio_devicepower(int onoff);
//void dio_usb_led(int v);
//void dio_error_led(int v);
//void dio_videolost_led(int v);
int dio_syncrtc();
int dio_check() ;
void dio_checkwifi();
int dio_setstate( int status ) ;
int dio_clearstate( int status ) ;
void dio_setchstat( int channel, int ch_state );
int dio_getgforce( float * gf, float * gr, float *gd );
int dio_mode_archive();
char * dio_getiomsg();
void dio_smartserveron(char * interface);

#ifdef PWII_APP
// return pwii media key event, 0=no key event, 1=key event
int dio_getpwiikeycode( int * keycode, int * keydown) ;
void dio_pwii_lpzoomin( int on );
#endif    // PWII_APP

void dio_init(config &dvrconfig);
void dio_uninit();
double gps_speed();
int gps_location( double * latitude, double * longitude, double * speed );
extern double g_gpsspeed ;
extern int dio_record;
extern int dio_capture;


//};

// sensor

class sensor_t {
  protected:
    string m_name ;
    int m_inputpin ;
    int m_inverted ;
    int m_value ;
    int m_eventmarker ; // 1=this sensor is a event marker
    int m_toggle ;     // set 1 if toggled.
  public:
    sensor_t(int n);
    char * name() {
        return (char *)m_name;
    }
    int check();
    int value() {
        return m_value ;
    }
    int toggle() {
        return m_toggle ;
    }
    int eventmarker() {
        return m_eventmarker ;
    }
} ;

extern sensor_t ** sensors ;
extern int num_sensors ;

class alarm_t {
    string m_name ;
    int m_outputpin ;
    int m_value ;
public:
    alarm_t(int n);
    ~alarm_t();

    char * name() {
        return (char *)m_name;
    }
    void clear();
    int setvalue(int v);
    void update();
} ;

extern alarm_t ** alarms ;
extern int num_alarms ;

void screen_init(config &dvrconfig);
void screen_uninit();
int screen_key( int keycode, int keydown ) ;
int screen_io(int usdelay=0);
int screen_setliveview( int channel );
int screen_menu(int level);
void screen_update();

#ifdef PWII_APP
extern int pwii_front_ch ;        // pwii front camera channel
extern int pwii_rear_ch ;         // pwii rear camera channel
#endif

//EAGLE SVR client

int dvr_screen_setmode(int sockfd, int videostd, int screennum);
int dvr_screen_live(int sockfd, int channel);
// speed=4: normal speed
int dvr_screen_play(int sockfd, int channel, int speed);
// input play back data
int dvr_screen_playinput(int sockfd, int channel, void * inputbuf, int inputsize);
// stop play this channel
int dvr_screen_stop(int sockfd, int channel);
// turn on/off audio channel
int dvr_screen_audio(int sockfd, int channel, int onoff);
// get screen width
int dvr_screen_width(int sockfd);
// get screen height
int dvr_screen_height(int sockfd);
// reset draw area (to full screen)
int dvr_draw_resetarea(int sockfd);
// set draw clip area
int dvr_draw_setarea(int sockfd, int x, int y, int w, int h);
int dvr_draw_refresh(int sockfd);
int dvr_draw_setcolor(int sockfd, UINT32 color);
UINT32 dvr_draw_getcolor(int sockfd);
int dvr_draw_setpixelmode(int sockfd, int pixelmode);
int dvr_draw_getpixelmode(int sockfd);
int dvr_draw_putpixel(int sockfd, int x, int y, UINT32 color);
UINT32 dvr_draw_getpixel(int sockfd, int x, int y);
UINT32 dvr_draw_line(int sockfd, int x1, int y1, int x2, int y2 );
int dvr_draw_rect(int sockfd, int x, int y, int w, int h );
int dvr_draw_fillrect(int sockfd, int x, int y, int w, int h) ;
int dvr_draw_circle(int sockfd, int cx, int cy, int r) ;
int dvr_draw_fillcircle(int sockfd, int cx, int cy, int r) ;
int dvr_draw_bitmap(int sockfd, struct BITMAP * bmp, int dx, int dy, int sx, int sy, int w, int h );
int dvr_draw_stretchbitmap(int sockfd, struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) ;
int dvr_draw_readbitmap(int sockfd, struct BITMAP * bmp, int x, int y, int w, int h );
int dvr_draw_setfont(int sockfd, struct BITMAP * font);
int dvr_draw_text( int sockfd, int dx, int dy, char * text);
int dvr_draw_textex( int sockfd, int dx, int dy, int fontw, int fonth, char * text);

int dvr_draw_refresh(int sockfd);
// show area (for eagle34, may remove in the fucture)
int dvr_draw_show(int sockfd, int id, int x, int y, int w, int h );
// hide area (for eagle34, may remove in the fucture)
int dvr_draw_hide(int sockfd, int id );

// Jpeg Capture
//  channel: capture card channel (0-3)
//  quality: 0=best, 1=better, 2=average
//  pic:     0=cif, 1=qcif, 2=4cif
int dvr_jpeg_capture(int sockfd, int channel, int quality, int pic);

// Start local capture ( for eagle368 )
int dvr_startcapture(int sockfd );
// Stop local capture ( for eagle368 )
int dvr_stopcapture(int sockfd );

// Init local shared memory
int dvr_shm_init(int sockfd, char * shmfile );
// close local shared memory
int dvr_shm_close(int sockfd);
// open live stream from local svr, over shared memory
int dvr_openliveshm(int sockfd, int channel);

#endif							// __dvr_h__
