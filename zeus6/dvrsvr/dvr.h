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
#include <malloc.h>

#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/times.h>

#include "../cfg.h"

#include "mpegps.h"

#include "crypt.h"
#include "genclass.h"
#include "cfg.h"

#define SUPPORT_EVENTMARKER	1

#define DVRVERSION0		(2)
#define	DVRVERSION1		(0)
#define DVRVERSION2		(8)
#define	DVRVERSION3		(521)

#define VERSION0                (1)
#define VERSION1                (0)
#define VERSION2                (10)
#define VERSION3                (921)

#define USENEWVERSION            1

#define ALIGN_LEFT             0x01
#define ALIGN_RIGHT            0x02
#define ALIGN_TOP              0x04
#define ALIGN_BOTTOM           0x08

// used types
typedef unsigned long int DWORD;
typedef unsigned short int WORD;
typedef unsigned char BYTE;

#define MAXCHANNEL		16

// application variables, functions

#define APPQUIT 	(0)
#define APPUP   	(1)
#define APPDOWN 	(2)
#define APPRESTART	(3)

extern pthread_mutex_t mutex_init ;
extern char dvrconfigfile[];
extern int app_state;
extern int system_shutdown;
extern int g_lowmemory;
extern char g_hostname[] ;
extern char g_usbid[] ;

extern int    g_cpu_usage ;	// cpu usage , 0-100 ;

// TVS related

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

extern unsigned char g_filekey[] ;
extern char g_mfid[32] ;
extern int g_keycheck ;
extern char g_serial[64] ;
extern char g_id1[64] ;
extern char g_id2[64] ;

// generic mutex initializer
extern pthread_mutex_t mutex_init ;

// pwii
extern char g_servername[256] ;
extern char g_vri[128] ;
extern char g_policeid[] ;
extern string g_policeidlistfile ; // Police ID list filename

void dvr_logkey( int op, struct key_data * key ) ;

extern int turndiskon_power;
extern int powerNum;

// memory allocation
extern int g_memfree;  			// free memory in kb
extern int g_memdirty; 			// dirty memory 

void *mem_alloc(int size);
void mem_free(void *pmem);
void mem_dropcache() ;
void *mem_addref(void *pmem, int memsize=0);
int mem_refcount(void *pmem);
int mem_check(void *pmem);
int mem_size(void * pmem);
int mem_available();
void mem_init();
void mem_uninit();

int  dvr_log(const char *str, ...);
int  dvr_logd(char *str, ...);
void dvr_lock();
void dvr_unlock();

// lock variable
void dvr_lock( void * lockvar, int delayus=10000 ) ;
void dvr_unlock( void * lockvar );

unsigned dvr_random();
void dvr_cleanlog(char* logfilename);

// time service
struct dvrtime {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int milliseconds;
	int tz;
};

struct dvrtimeex {
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned short milliseconds;
};

// dvr .264 file structure
// .264 file struct
struct hd_file {
	DWORD flag;	//      "4HKH", 0x484b4834
	DWORD res1[6];
	DWORD width_height;	// lower word: width, high word: height
	DWORD res2[2];
};

struct hd_frame {
	DWORD flag;					// 1
	DWORD serial;
	DWORD timestamp;
	DWORD res1;
	DWORD d_frames;		// sub frames number, lower 8 bits
	DWORD width_height;	// lower word: width, higher word: height
	DWORD res3[6];
	DWORD d_type;				// lower 8 bits; 3  keyframe, 1, audio, 4: B frame
	DWORD res5[3];
	DWORD framesize;
};

// .266 file header (40bytes)
struct File_format_info {
   unsigned int file_fourcc;
   unsigned short file_version;
   unsigned short device_type;			// ZEUS6: 1, ZEUS8: 4
   unsigned short file_format;
   unsigned short video_codec;
   unsigned short audio_codec;			// 1: ulaw, 2: alaw
   unsigned char audio_channels;
   unsigned char audio_bits_per_sample;		
   unsigned int audio_sample_rate;		// 8000
   unsigned int audio_bit_rate;
   unsigned short video_width;
   unsigned short video_height;
   unsigned short video_framerate;
   unsigned short reserverd;
   unsigned int reserverd1;
   unsigned int reserverd2;
};

#define HD264_FRAMEWIDTH(hd)  	  ( ((hd).width_height) & 0x0ffff)
#define HD264_FRAMEHEIGHT(hd)     ( (((hd).width_height)>>16)&0x0ffff)
#define HD264_FRAMESUBFRAME(hd)   ( ((hd).d_frames)&0x0ff)
#define HD264_FRAMETYPE(hd)       ( ((hd).d_type)&0x0ff)

struct hd_subframe {
	DWORD d_flag ;				// lower 16 bits as flag, audio: 0x1001  B frame: 0x1005
	DWORD res1[3];
	DWORD framesize;			// 0x50 for audio
};

#define HD264_SUBFLAG(subhd)      ( ((subhd).d_flag)&0x0ffff )

#define FRAMETYPE_UNKNOWN	(0)
#define FRAMETYPE_KEYVIDEO	(1)
#define FRAMETYPE_VIDEO		(2)
#define FRAMETYPE_AUDIO		(3)
#define FRAMETYPE_JPEG		(4)

// new frame type ?
#define FRAMETYPE_KEYVIDEO_SUB  (5)
#define FRAMETYPE_VIDEO_SUB     (6)

#define FRAMETYPE_264FILEHEADER	(10)
#define FRAMETYPE_OSD		(11)			// TXT
#define FRAMETYPE_GPS		(12)			// GPS
#define FRAMETYPE_OTHER		(20)

enum REC_STATE {
    REC_STOP,           // no recording
    REC_PRERECORD,      // pre-recording
    REC_RECORD,         // recording
    REC_LOCK,           // recording lock file
};

struct rec_fifo {
    struct rec_fifo *next;
    struct dvrtime time;        // captured time
    int frametype;              // a key frame
    char *buf;
    int bufsize;
    DWORD timestamp;
    REC_STATE rectype;          // recording type
    int swappos ;				// swap position on swap file, -1: swap request, 0: no swap,(in mem), >0 : swap out
};

struct cap_frame {
	int channel;
	int framesize;
	int frametype;
    DWORD timestamp;
	char *framedata;
};

struct cap_frame_header{
	int channel;
	int framesize;
	int frametype;
    DWORD timestamp;
};

// for playback
struct frame_info {
    int frametype;
    int framesize;
    char * framebuf ;
    struct dvrtime frametime;        
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
    int ktime;		// key frame time, in milliseconds, from start of file(file time in filename)
    int koffset ;	// key frame offset
} ;

typedef struct {
    unsigned char MagicNum[4];
    unsigned char reserved[2];
    unsigned short wOSDSize;
} OSDHeader;

typedef struct {
     unsigned char MagicNum[2]; 
     unsigned char reserved[4];  
     unsigned short wLineSize;
}OSDLINE_HEADER;

typedef struct {
 	unsigned char mAlignMode;
	unsigned char mXoffset;
	unsigned char mYoffset;
	unsigned char res;     
}OSDLINE_TXT_HEADER;

typedef struct {
    unsigned char MagicNum[2];
    unsigned char reserved[4];
    unsigned short wLineSize;
    unsigned char  OSDText[128];
} OSDLine;


// dvrfile
class dvrfile {
  protected:
    FILE * m_khandle;
    
    int    m_handle ;
    char * m_writebuf ; 
    int    m_writebufsize ;
    int    m_writepos ;
    
    struct hd_file m_fileheader ;
	int    m_fileencrypt ;		// if file is encrypted?

	string m_filename ;
	int    m_openmode ;			// 0: read, 1: write

	struct dvrtime m_filetime ;	// file start time
	int    m_lastframetime ;	// lastframetime, when writing
	
	int   m_filesize ;
        
	int   m_curframe ;			// current reading key frame index
        
	array <struct dvr_key_t> m_keyarray ;

  public:
	 dvrfile() ;
	~dvrfile() ;
	
	// generate dvrfile path, caller should delete the string
	static char * makedvrfilename( struct dvrtime * filetime, int channel, int lock, int filelen = 0 ) ;
	
	char * getfilename() {
		return (char *)m_filename ;
	}
	
	int open(const char *filename, char *mode );
	int writeopen(const char *filename, char *mode,void* pheader);
	void close();
	int writeheader(void * buffer, size_t headersize) ;
	int readheader(void * buffer, size_t headersize) ;
	int read(void *buffer, size_t buffersize);
	int write(void *buffer, size_t buffersize);
	void flush();
	int repair(); // repair a .264 file
    int repairpartiallock() ;               // repair a partial locked file
	int tell();
	int seek(int pos, int from = SEEK_SET);
    int truncate( int tsize );
	// seek to specified time,
	//   return 1 if seek inside the file.
	//	return 0 if seek outside the file, pointer set to begin or end of file
	int seek( dvrtime * seekto );
    int frameinfo();                // read frame info
    int getnextbyte(unsigned char* mbyte);
    int skipnextnbytes(int n);
	dvrtime frametime();			// return current frame time
	//int frametype();                // return current frame type
	//int framesize();                // return current frame size
    int frametypeandsize(int* size);
    frame_info * getframeinfo();
    
    struct dvrtime * getfiletime(){
		return &m_filetime ;
	}
    
   	int readkey( array <struct dvr_key_t> &keyarray );
	void writekey( array <struct dvr_key_t> &keyarray ) ;

	int prevkeyframe();
	int nextkeyframe();
	//int readframe(struct cap_frame *pframe);
	int readframe(void * framebuf, size_t bufsize);
	int readframe( frame_info * fi );
	int writeframe(unsigned char * buf, int size, int frametype, dvrtime * frametime);
	int writeframe( rec_fifo * frame ) ;
	int isopen() {
		// return m_handle != NULL;
		return m_handle > 0 ;
	}
	int isencrypt() {
		return m_fileencrypt ;
	}
	int isframeencrypt() {
	    return m_fileencrypt;
	}
    DWORD getfileheader() ;

	static int rename(const char * oldfilename, const char * newfilename); // rename .264 filename as well .k file
	static int remove(const char * filename);// remove .264 file as well .k file
	static int breakLfile( char * filename, struct dvrtime * ltime );	// break N file into n+l
	
};

void file_sync();
void file_init();
void file_uninit();

// capture
// capture card structure

struct  DvrChannel_attr {
    int         Enable ;
    char        CameraName[64] ;
    int         Resolution;	// 0: CIF, 1: 2CIF, 2:DCIF, 3:4CIF, 4:QCIF
    int         RecMode;	// 0: Continue, 1: Trigger by sensor, 2: triger by motion, 3, trigger by sensor/motion, 4: Schedule
    int         PictureQuality;			// 0: lowest, 10: Highest
    int         FrameRate;
    int         Sensor[4] ;
    int         SensorEn[4] ;
    int         SensorOSD[4] ;
    int         PreRecordTime ;			// pre-recording time in seconds
    int         PostRecordTime ;		// post-recording time in seconds
    int         RecordAlarm ;			// Recording Signal
    int         RecordAlarmEn ;
    int         RecordAlarmPat ;               // 0: OFF, 1: ON, 2: 0.5s Flash, 3: 2s Flash, 4, 4s Flash
    int         VideoLostAlarm ;		// Video signal lost alarm
    int         VideoLostAlarmEn ;
    int         VideoLostAlarmPat ;
    int         MotionAlarm ;			// Motion Detect alarm
    int         MotionAlarmEn ;
    int         MotionAlarmPat ;
    int         MotionSensitivity;		
    int         GPS_enable ;			// enable gps options
    int         ShowGPS ;
    int         GPSUnit ;		// GPS unit display, 0: English, 1: Metric
    int         BitrateEn ;		// Enable Bitrate Control
    int         BitMode;		// 0: VBR, 1: CBR
    int         Bitrate;
    int         ptz_addr ;
    int         structsize;             // structure size
    int         brightness;             // 1-9, 0: use default value
    int         contrast;               // 1-9, 0: use default value
    int         saturation;             // 1-9, 0: use default value
    int         hue;                    // 1-9, 0: use default value
    int         key_interval ;          // key frame interval, default 100
    int         b_frames ;              // b frames number, default 2
    int         p_frames ;              // p frames number, not used
    int         disableaudio ;          // 1: no audio
    int         showgpslocation ;       // show gps location on osd
    int         Tab102_enable;
    int         ShowPeak;
} ;

extern int cap_channels;

void cap_start();
void cap_stop();
void cap_restart(int channel);
void cap_init();
void cap_uninit();

#define CAP_TYPE_UNKNOWN	(-1)
#define CAP_TYPE_HIKLOCAL	(0)
#define CAP_TYPE_HIKIP		(1)
#define CAP_TYPE_HDIPCAM	(2)

struct hik_osd_type {
    int brightness ;
    int translucent ;
    int twinkle ;
    int lines ;
    WORD osdline[8][128] ;
} ;

extern unsigned int g_ts_base;

class capture {
public:
    int m_channel;				// logic channel number
    int m_enable ;				// 1: enable, 0:disable
    int m_type ;				// channel type, 0: local capture card, 1: eagle ip camera

	int m_phychannel ;			// local physical channel
	int m_audiochannel ;		// physical audiochannel
	int m_start ;				// started?
	
	File_format_info m_header ;	// file header

protected:    
    struct  DvrChannel_attr	m_attr ;	// channel attribute
    
    int m_motion ;				// motion status ;
    int m_signal ;				// signal ok.
    
    unsigned long m_streambytes; // total stream bytes.
    time_t m_osdtime ;
        
    int m_working ;             // channel is working
        
    unsigned int m_sensorosd ;      // bit maps for sensor osd
	     
#ifdef TVS_APP
    int m_show_medallion;
    int m_show_licenseplate;
    int m_show_ivcs;
    int m_show_cameraserial;
#endif

#ifdef PWII_APP
    int m_show_vri;
    int m_show_policeid;
#endif
    
    int m_motionalarm ;
    int m_motionalarm_pattern ;
    int m_signallostalarm ;
    int m_signallostalarm_pattern ;
    int m_master;

  public:
	capture(int channel) ;
    virtual ~capture(){}

	void loadconfig() ;
    void getattr(struct DvrChannel_attr * pattr) {
        memcpy( pattr, &m_attr, sizeof(m_attr) );
    }
    void setattr(struct DvrChannel_attr * pattr) {
        if( pattr->structsize == sizeof(struct DvrChannel_attr) ) {
            memcpy(&m_attr, pattr, sizeof(m_attr) );
        }
    }
    int getchannel() {
	    return m_channel;
    }
    int enabled(){
	    return m_enable;
    }
    int isworking(){
        return --m_working>0 ;
    }
    int type(){
	    return m_type ;
    }
    unsigned long streambytes() {
        return m_streambytes ;
    }
    void onframe(cap_frame * pcapframe);// send frames to network or to recording
	char * getname(){ 
		return m_attr.CameraName ;
	}
	int getptzaddr() {
		return m_attr.ptz_addr;
	}
	
	int updateOSD( char * osd ) ;

	virtual void update();				// periodic update procedure, updosd: require to update OSD

	void setremoteosd() {}				// obsoleted function 
	//void updateOSD ();          		// to update OSD text
   	virtual void setosd( struct hik_osd_type * posd ){};
	
	// to generate gps data frame
	virtual void gpsframe() ;
	// to generate a OSD frame for eagle channel
	virtual void osdframe() ;	

	void alarm();                   // update alarm relate to capture channel
	virtual void start(){};				// start capture
	virtual void stop(){};				// stop capture
	virtual void restart(){				// restart capture
		stop();
		if( m_enable ) {
			start();
		}
	}
	
	int getsignal(){return m_signal;}	// get signal available status, 1:ok,0:signal lost
    int getmotion(){return m_motion;}	// get motion detection status
    
    virtual int getFrameRate() {
		return m_attr.FrameRate ;
	}
	
protected:
	// retrive signal status
	virtual int vsignal(){
		return 0 ;
	};   	
	// retrive motion status
	virtual int vmotion(){
		return 0 ;
	}

};

extern capture ** cap_channel;

// eagle (zeus) board interface
int eagle_startaudio( int phychannel );
int eagle_stopaudio( int phychannel );
void eagle_start();
void eagle_stop();
int  eagle_init();
void eagle_uninit();

// Local channel
class eagle_capture : public capture {
public:
	int m_chantype ;

public:
	eagle_capture(int channel);
   	
	// virtual function implements
	virtual void start();
	virtual void stop();

protected:
	// retrive signal status
	virtual int vsignal() ;
	// retrive motion status
	virtual int vmotion() ;
};

class ipeagle32_capture : public capture {
protected:
	int m_sockfd ;		//  control socket for ip camera
	int m_streamfd ;	//  stream socket for ip camera
	string m_ip ;		//  remote camera ip address
	int m_port ;		//  remote camera port
	int m_ipchannel ; 	//  remote camera channel
	struct hd_file m_hd264 ;	// .264 file header
	pthread_t m_streamthreadid ;	// streaming thread id
	int m_state;		//  thread running state, 0: stop, 1: running, 2: started but not connected.
	int m_updcounter ;
public:
	ipeagle32_capture(int channel);

	void streamthread();
	int  connect();

// virtual functions
	virtual void start();				// start capture
	virtual void stop();				// stop capture
	virtual void setosd( struct hik_osd_type * posd );
} ;

// IP HD camera
class Ipstream_capture : public capture {
public:
	Ipstream_capture(int channel);
	
	// virtual function implements
	virtual void start();
	virtual void stop();
 	
protected:
	// retrive signal status
	virtual int vsignal() ;
};

// Init/Uninit HD ip camera
void IpCamera_Init();
void IpCamera_Uninit();

// ptz service
void ptz_init();
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

time_t time_now(struct dvrtime *dvrt);
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

// convert between dvrtime and time_t
struct dvrtime * time_dvrtimelocal( time_t t, struct dvrtime * dvrt);
struct dvrtime * time_dvrtimeutc( time_t t, struct dvrtime * dvrt);
time_t time_timelocal( struct dvrtime * dvrt);
time_t time_timeutc( struct dvrtime * dvrt);
// get a replace hik card time stamp
DWORD time_hiktimestamp() ;

// appliction up time in seconds
int time_uptime();

int time_readrtc(struct dvrtime * dvrt);
int time_setrtc();

// retrive milliseconds
int time_gettick();

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

extern int rec_busy ;
extern int rec_pause;           // pause recording temperary
extern int rec_watchdog ;      // recording watchdog
extern int gHBD_Recording;

void rec_init();
void rec_uninit();
void rec_onframe(struct cap_frame *pframe);
void rec_record(int channel);
void rec_prerecord(int channel);
void rec_postrecord(int channel);
void rec_lockstart(int channel);
void rec_lockstop(int channel);
int  rec_state(int channel);
int  rec_state2(int channel);
int  rec_forcestate(int channel);
int  rec_lockstate(int channel);
int  rec_staterec(int channel);
int  rec_forcechannel(int channel);

void rec_lock(time_t locktime);
void rec_unlock();
void rec_break();
void rec_update();
void rec_alarm();
void rec_start();
void rec_stop();

#ifdef APP_PWZ5
void rec_pwii_force_rec( int c, int rec );
void rec_pwii_toggle_rec( int c );
#endif

int  fileclosed();
struct nfileinfo {
    int channel;
    struct dvrtime filetime ;
    int filelength ;
    char extension[4] ;                         // ".k" or ".264"
    int  filesize ;                             // option, can be 0
} ;
FILE * rec_opennfile(int channel, struct nfileinfo * nfi);

// disk and .264 file directories
// get .264 file list by day and channel
extern struct dvrtime disk_earliesttime ;
int f264length(const char *filename);				// get file length from file name
int f264channel(const char *filename);				// get file channel number
int f264locklength(const char *filename);			// get file lock length from file name
int f264time(const char *filename, struct dvrtime *dvrt);
char *basefilename(const char *fullpath);

class f264name : public string {
public:
	int operator < ( f264name & s2 ) {
		struct dvrtime t1, t2 ;
		f264time( getstring(), &t1 );
		f264time( s2.getstring(), &t2 );
		return t1<t2 ;
	}

	f264name & operator =(char *str) {
		set(str);
		return *this;
	}
};

struct pw_diskinfo {
	int mounted ;		// boolean	
	int totalspace ;	// total space in MB
	int freespace ;		// free space in MB
	int reserved ;		// reserved space (5%)
	int full ;			// disk full
	int l_len, n_len ;	// L file length, and N file length
	string disk ;		// disk mount point
} ;

extern struct pw_diskinfo pw_disk[3] ;

void disk_listday(array <f264name> & list, struct dvrtime * day, int channel);
void disk_getdaylist(array <int> & daylist, int channel=-1);
int disk_unlockfile( dvrtime * begin, dvrtime * end );
void disk_check();
int disk_renew(char * newfilename);
void disk_rescan();			// to rescan disk N/L file ration
void disk_init(int);
void disk_uninit(int);
int do_disk_check();
char * disk_getbasedisk( int locked );
char * disk_getlogdisk();


// live video service
struct net_fifo {
    struct net_fifo *next;
    char *buf;
    int bufsize;
    int loc;
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

void play_init();
void play_uninit();

class playback {
    protected:
        int m_channel;
        
        array <int> m_daylist ;			// available data day list
        
        int m_day ;						// BCD date of current file list
        array <f264name> m_filelist ;	// day file list
        int m_curfile ;					// current opened file, index of m_filelist
        
        dvrfile m_file ;				// current opened file, always opened
        struct dvrtime m_streamtime ;	// current frame time        
        
        int opennextfile();
        int close();
        void readframe();
        
    public:
        playback( int channel ) ;
        ~playback();
        int seek( struct dvrtime * seekto );
        void getstreamdata(void ** getbuf, int * getsize, int * frametype);
//        void getstreamdataex(void ** getbuf, int * getsize, int * frametype);
		int  getframetypeandsize(int * pframetype, struct dvrtime * ptime);
		void getframedata(void * framebuf, int framesize);
		int readframe( frame_info * fi );

        int getstreamtime(dvrtime * dvrt);
        void getdayinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday);
        void getlockinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday);
        DWORD getmonthinfo(dvrtime * month);// return day info in bits, bit 0 as day 1
        int  getdaylist( int * * daylist ) ;
        DWORD getcurrentfileheader();
	int  getfileheader(void * buffer,size_t headersize);
	int getplaychannel(){return m_channel;};
};

// network service

#define DVRPORT 15114
#define REQDVREX	0x7986a348
#define REQDVRTIME	0x7986a349
#define DVRSVREX	0x95349b63
#define DVRSVRTIME	0x95349b64
#define REQSHUTDOWN	0x5d26f7a3
#define REQMSG		0x5373cb4a
#define DVRMSG		0x774f9a31

struct sockad {
    struct sockaddr addr;
    char padding[128];
    socklen_t addrlen;
};

// maximum live fifo size
extern int net_livefifosize;
// network active
extern int net_active ;		
extern int net_liveview;
extern int net_playarc;
void net_lock();
void net_unlock();
// trigger a new network wait cycle
void net_trigger();
int net_sendok(int fd, int tout);
int net_recvok(int fd, int tout);
void net_message();
int net_listen(int port, int socktype);
int net_connect(const char *ip, int port);
int net_peer(int sockfd);
void net_clean(int sockfd);
int net_send(int sockfd, void * data, int datasize);
int net_recv(int sockfd, void * data, int datasize);
int net_onframe(cap_frame * pframe);
// initialize network
void net_init();
void net_uninit();


// protocol type for send/receive DVR data
#define PROTOCOL_LOGINFO       (1)        // receiving log inforamtion
#define PROTOCOL_TIME          (2)        // send time info to DVR
#define PROTOCOL_TIME_BEGIN    (3)        // send a begining time
#define PROTOCOL_TIME_END      (4)        // send a ending time
#define PROTOCOL_PTZ           (5)        // send PTZ command
#define PROTOCOL_POWERCONTROL  (6)        // set DVR power status
#define	PROTOCOL_KEYDATA       (7)		 // send and check TVS/PWII key data to dvr
#define PROTOCOL_TVSKEYDATA	   (PROTOCOL_KEYDATA)
//#define PROTOCOL_KEYLOG		   (8)       // receive key log (TVS/PWII) data
//#define PROTOCOL_TVSLOG		   (PROTOCOL_KEYLOG)
#define PROTOCOL_PWEVENT	   (9)

// PWII get channel status
#define PROTOCOL_PW_GETSTATUS   	(1001)
#define PROTOCOL_PW_GETPOLICEIDLIST	(1002)
#define PROTOCOL_PW_SETPOLICEID		(1003)
#define PROTOCOL_PW_GETVRILISTSIZE	(1004)
#define PROTOCOL_PW_GETVRILIST		(1005)
#define PROTOCOL_PW_SETVRILIST		(1006)
#define PROTOCOL_PW_GETSYSTEMSTATUS	(1007)
#define PROTOCOL_PW_GETWARNINGMSG	(1008)
#define PROTOCOL_PW_SETCOVERTMODE	(1009)
#define PROTOCOL_PW_GETDISKINFO		(1010)
#define PROTOCOL_PW_GETEVENTLIST	(1011)

// dvr net service
struct dvr_req {
    int reqcode;
    int data;
    int reqsize;
};

struct dvr_ans {
    int anscode;
    int data;
    int anssize;
};

struct channel_fifo {
    int channel;
};

enum reqcode_type {
    REQOK =	1,
    REQREALTIME, REQREALTIMEDATA,
    REQCHANNELINFO, REQCHANNELDATA,
    REQSWITCHINFO, REQSHAREINFO,
    REQPLAYBACKINFO, REQPLAYBACK,
    REQSERVERNAME,
    REQAUTH, REQGETSYSTEMSETUP,
    REQSETSYSTEMSETUP, REQGETCHANNELSETUP,
    REQSETCHANNELSETUP, REQKILL,
    REQSETUPLOAD, REQGETFILE, REQPUTFILE,
    REQHOSTNAME, REQRUN,
    REQRENAME, REQDIR, REQFILE,
    REQGETCHANNELSTATE, REQSETTIME, REQGETTIME,
    REQSETLOCALTIME, REQGETLOCALTIME,
    REQSETSYSTEMTIME, REQGETSYSTEMTIME,
    REQSETTIMEZONE, REQGETTIMEZONE,
    REQFILECREATE, REQFILEREAD,
    REQFILEWRITE, REQFILESETPOINTER, REQFILECLOSE,
    REQFILESETEOF, REQFILERENAME, REQFILEDELETE,
    REQDIRFINDFIRST, REQDIRFINDNEXT, REQDIRFINDCLOSE,
    REQFILEGETPOINTER, REQFILEGETSIZE, REQGETVERSION,
    REQPTZ, REQCAPTURE,
    REQKEY, REQCONNECT, REQDISCONNECT,
    REQCHECKID, REQLISTID, REQDELETEID, REQADDID, REQCHPASSWORD,
    REQSHAREPASSWD, REQGETTZIENTRY,
    REQECHO,

    REQ2BEGIN =	200,
    REQSTREAMOPEN, REQSTREAMOPENLIVE, REQSTREAMCLOSE,
    REQSTREAMSEEK, REQSTREAMGETDATA, REQSTREAMTIME,
    REQSTREAMNEXTFRAME, REQSTREAMNEXTKEYFRAME, REQSTREAMPREVKEYFRAME,
    REQSTREAMDAYINFO, REQSTREAMMONTHINFO, REQLOCKINFO, REQUNLOCKFILE, REQSTREAMDAYLIST,
    REQOPENLIVE,
    REQ2ADJTIME,
    REQ2SETLOCALTIME, REQ2GETLOCALTIME, REQ2SETSYSTEMTIME, REQ2GETSYSTEMTIME,
    REQ2GETZONEINFO, REQ2SETTIMEZONE, REQ2GETTIMEZONE,
    REQSETLED,
    REQSETHIKOSD,                                               // Set OSD for hik's cards (225)
    REQ2GETCHSTATE,
    REQ2GETSETUPPAGE,
    REQ2GETSTREAMBYTES,
    REQ2GETSTATISTICS,
    REQ2KEYPAD,
    REQ2PANELLIGHTS,
    REQ2GETJPEG,
    REQ2STREAMGETDATAEX,
    REQ2SYSTEMSETUPEX,
    REQ2GETGPS,
    REQSTREAMFILEHEADER,

    REQ3BEGIN = 300,
    REQSENDDATA,
    REQGETDATA,
    REQCHECKKEY,
    REQUSBKEYPLUGIN, // plug-in a new usb key. (send by plug-in autorun program)

    // for communicate in or between eagle boards
    REQ5BEGIN = 500,

    // Screen setup
    REQSCREEENSETMODE,
    REQSCREEENLIVE, REQSCREENPLAY, REQSRCEENINPUT, REQSCREENSTOP, REQSCREENAUDIO,

    REQGETSCREENWIDTH, REQGETSCREENHEIGHT,
    REQDRAWREFRESH, REQDRAWSETAREA,
    REQDRAWSETCOLOR, REQDRAWGETCOLOR,
    REQDRAWSETPIXELMODE, REQDRAWGETPIXELMODE,
    REQDRAWPUTPIXEL, REQDRAWGETPIXEL,
    REQDRAWLINE, REQDRAWRECT,
    REQDRAWFILL, REQDRAWCIRCLE, REQDRAWFILLCIRCLE,
    REQDRAWBITMAP, REQDRAWSTRETCHBITMAP,
    REQDRAWSETFONT, REQDRAWTEXT, REQDRAWTEXTEX,
    REQDRAWREADBITMAP,

    REQ5STARTCAPTURE, REQ5STOPCAPTURE, REQINITSHM, REQCLOSESHM, REQOPENLIVESHM,

    REQMAX
};

enum anscode_type {
    ANSERROR =1, ANSOK,
    ANSREALTIMEHEADER, ANSREALTIMEDATA, ANSREALTIMENODATA,
    ANSCHANNELHEADER, ANSCHANNELDATA,
    ANSSWITCHINFO,
    ANSPLAYBACKHEADER, ANSSYSTEMSETUP, ANSCHANNELSETUP, ANSSERVERNAME,
    ANSGETCHANNELSTATE,
    ANSGETTIME, ANSTIMEZONEINFO, ANSFILEFIND, ANSGETVERSION, ANSFILEDATA,
    ANSKEY, ANSCHECKID, ANSLISTID, ANSSHAREPASSWD, ANSTZENTRY,
    ANSECHO,

    ANS2BEGIN =	200,
    ANSSTREAMOPEN, ANSSTREAMDATA,ANSSTREAMTIME,
    ANSSTREAMDAYINFO, ANSSTREAMMONTHINFO, ANSLOCKINFO, ANSSTREAMDAYLIST,
    ANS2RES1, ANS2RES2, ANS2RES3,                        	// reserved, don't use
    ANS2TIME, ANS2ZONEINFO, ANS2TIMEZONE, ANS2CHSTATE, ANS2SETUPPAGE, ANS2STREAMBYTES,
    ANS2JPEG, ANS2STREAMDATAEX, ANS2SYSTEMSETUPEX, ANS2GETGPS,
    ANSSTREAMFILEHEADER,

    ANS3BEGIN = 300,
    ANSSENDDATA,
    ANSGETDATA,

    // for communicate between processes or eagle boards
    ANS5BEGIN = 500,
    ANSGETSCREENWIDTH, ANSGETSCREENHEIGHT,
    ANSDRAWGETPIXELMODE, ANSDRAWGETCOLOR, ANSDRAWGETPIXEL, ANSDRAWREADBITMAP,

    ANSSTREAMDATASHM,

    ANSMAX
};

enum conn_type { CONN_NORMAL = 0, CONN_REALTIME, CONN_LIVESTREAM };

#define closesocket(s) ::close(s)

class dvrsvr {
	public:
		static dvrsvr * head;
		dvrsvr *m_next;		// dvr list
	
    protected:
        int m_sockfd;		// socket
        
        struct net_fifo * m_fifo ;
        
        playback * m_playback ;
        live	 * m_live ;
        int      mIframe;
        
        // dvr related struct
        int m_conntype;				// 0: normal, 1: realtime video stream, 2: answering
        int m_connchannel;
        
        struct dvr_req m_req;
        char *m_recvbuf;
        int m_recvlen;
        int m_recvloc;
        
        int m_keycheck ;
        
        FILE * m_nfilehandle ;

    public:
        dvrsvr(int fd);
        ~dvrsvr();
        
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
        void send_fifo(void *buf, int bufsize);
		void Send(void *buf, int bufsize);
        void close();
		int isclose() {
			return (m_sockfd <= 0);
		}
        int onframe(cap_frame * pframe);
        
        // request handlers
        void onrequest();
        void DefaultReq();
        
        void ReqOK();
        void ReqRealTime();
        void ChannelInfo();
        void DvrServerName();
        void GetSystemSetup();
        void SetSystemSetup();
        void GetChannelSetup();
        void SetChannelSetup();
        void ReqKill();
        void SetUpload();
        void HostName();
        void FileRename();
        void FileCreate();
        void FileRead();
        void FileWrite();
        void FileClose();
        void FileSetPointer();
        void FileGetPointer();
        void FileGetSize();
        void FileSetEof();
        void FileDelete();
        void DirFindFirst();
        void DirFindNext();
        void DirFindClose();
        void GetChannelState();
        void SetDVRSystemTime();
        void GetDVRSystemTime();
        void SetDVRLocalTime();
        void GetDVRLocalTime();
        void SetDVRTimeZone();
        void GetDVRTimeZone();
        void GetDVRTZIEntry();
        void GetVersion();
        void ReqEcho();
        void ReqPTZ();
        void ReqAuth();
        void ReqKey();
        void ReqDisconnect();
        void ReqCheckId();
        void ReqListId();
        void ReqDeleteId();
        void ReqAddId();
        void ReqSharePasswd();
        void ReqStreamOpen();
        void ReqStreamOpenLive();
		void ReqOpenLive();	
        void ReqStreamClose();
        void ReqStreamSeek();
        void ReqNextFrame();
        void ReqNextKeyFrame();
        void ReqPrevKeyFrame();
        void ReqStreamGetData();
        void ReqStreamGetDataEx();
        void ReqStreamTime();
        void ReqStreamDayInfo();
        void ReqStreamMonthInfo();
        void ReqStreamDayList();
        void ReqLockInfo();
        void ReqUnlockFile();
        void Req2SetLocalTime();
        void Req2GetLocalTime();
        void Req2AdjTime();
        void Req2SetSystemTime();
        void Req2GetSystemTime();
        void Req2GetZoneInfo();
        void Req2SetTimeZone();
        void Req2GetTimeZone();
        void ReqSetled();
        void ReqSetHikOSD();
        void Req2GetChState();
        void Req2GetSetupPage();
        void Req2GetStreamBytes();
		void ReqSendData();
		void ReqGetData();
        
        // TVS
        void ReqCheckKey();
        
#if defined (PWII_APP)
		void Req2Keypad();
		void Req2PanelLights();
#endif


};

// DVR client side support

// open live stream from remote dvr
int dvr_openlive(int sockfd, int channel, struct hd_file * hd264);

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

// received UDP message
#define MAXMSGSIZE (32000)
int msg_onmsg(void *msgbuf, int msgsize, struct sockad *from);
void msg_clean();
void msg_init();
void msg_uninit();

// event 
extern int event_serialno ;

void setdio(int onoff);
void event_key( int keycode, int keydown );		// vk event
void event_check();
void event_init();
void event_uninit();
void event_run();

// trace mark event
extern int event_tm ;
extern int event_tm_time ;

// system setup
struct system_stru {
	char IOMod[80]  ;
	char ServerName[80] ;
	int cameranum ;
	int alarmnum ;
	int sensornum ;
	int breakMode ;
	int breakTime ;
	int breakSize ;
	int minDiskSpace ;
	int shutdowndelay ;
	char autodisk[32] ;
	char sensorname[16][32];
	int sensorinverted[16];
	int eventmarker_enable ;
	int eventmarker ;
	int eventmarker_prelock ;
	int eventmarker_postlock ;
	char ptz_en ;
	char ptz_port ;			// COM1: 0
	char ptz_baudrate ;		// times of 1200
	char ptz_protocol ;		// 0: PELCO "D"   1: PELCO "P"
	int  videoencrpt_en ;	// enable video encryption
	char videopassword[32] ;
	int  rebootonnoharddrive ; // seconds to reboot if no harddrive detected!
	char res[84] ;
};

// return true for ok
int dvr_getsystemsetup(struct system_stru * psys);
// return true for ok
int dvr_setsystemsetup(struct system_stru * psys);
// return true for ok
int dvr_getchannelsetup(int ch, struct DvrChannel_attr * pchannel, int attrsize);
// return true for ok
int dvr_setchannelsetup(int ch, struct DvrChannel_attr * pchannel, int attrsize);
	

// digital io functions
//extern "C" {

struct gps {
  int		gps_valid ;
  double	gps_speed ;	// knots
  double	gps_direction ; // degree
  double  gps_latitude ;	// degree, + for north, - for south
  double  gps_longitud ;      // degree, + for east,  - for west
  double  gps_gpstime ;	// seconds from 1970-1-1 12:00:00 (utc)
};

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
int dio_setstate( int status ) ;
int dio_clearstate( int status ) ;
void dio_init();
void dio_uninit();
int dio_hdpower(int on);
void dio_mcureboot();
int isignitionoff();
int dio_curmode();
int dio_runmode();
void get_gps_data(struct gps *g);
double gps_speed(int *gpsvalid_changed);
int gps_location( double * latitude, double * longitude );
extern double g_gpsspeed ;
extern int dio_standby_mode ;
int dio_get_nodiskcheck();
int dio_get_temperature( int idx ) ;
void dio_hybridcopy(int on);
void dio_setfileclose(int close);
void dio_set_camera_status(int camera, unsigned int status, unsigned long streambytes );
void dio_covert_mode( int covert );

// return gps knots to mph
float dio_get_gps_speed();
// get gforce value
int dio_get_gforce( float *fb, float *lr, float *ud );

int get_peak_data(float *fb,float *lr,float *ud);
int isPeakChanged();
int isInUSBretrieve();
int isInhbdcopying();
int isstandbymode();

#ifdef PWII_APP
// return pwii media key event, 0=no key event, 1=key event
int dio_getpwiikeycode( int * keycode, int * keydown) ;
void dio_pwii_lpzoomin( int on );
void dio_pwii_mic_off();
void dio_pwii_mic_on();
void dio_pwii_emg_off();
#endif    // PWII_APP

extern float g_fb;
extern float g_lr;
extern float g_ud;
//};
// sensor

class sensor_t {
protected:	
	string m_name ;
	int m_inputpin ;
	int m_inverted ;
	int m_xvalue ;
	int m_value ;
    int m_eventmarker ; // 1=this sensor is a event marker
public:
	sensor_t(int n);
	char * name() {
		return m_name.getstring();
	}
	int check();
	void resetxvalue() {
		m_xvalue = m_value ;
	}
	int xvalue() {
		return m_xvalue ;
	}
	int value() {
		return m_value ;
	}
    int eventmarker() {
        return m_eventmarker ;
    }	
    int iseventmarker() {
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
		return m_name.getstring();
	}
	void clear();
	int setvalue(int v);
	void update();
} ;

extern alarm_t ** alarms ;
extern int num_alarms ;


void screen_init();
void screen_uninit();
int screen_io(int usdelay);


#ifdef PWII_APP
	// vri functions
	
// log new vir	
void vri_log( char * vri );	
// get buffer size required to retrieve vri items
int vri_getlistsize( int * itemsize );
// retrieve vri list
int vri_getlist( char * buf, int bufsize );
// update vri list items
void vri_setlist( char * buf, int bufsize );
	
	
#endif  // PWII_APP	

#endif							// __dvr_h__
