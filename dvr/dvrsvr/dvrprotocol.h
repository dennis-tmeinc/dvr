
#ifndef __dvrprotocol_h__
#define __dvrprotocol_h__

#include "../cfg.h"

// network service
#define DVRPORT 15111

#define REQDVREX	0x7986a348
#define REQDVRTIME	0x7986a349
#define DVRSVREX	0x95349b63
#define DVRSVRTIME	0x95349b64
#define REQSHUTDOWN	0x5d26f7a3

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

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef UINT32
#define UINT32 unsigned int
#endif

// dvr system setup
struct system_stru {
//	char IOMod[80]  ;
    char productid[8] ;     // "TVS" or "MDVR" or "PWII"
    // for TVS/PWII system , 72 bytes
    char serial[24] ;       // TVS: ivcs, PWII: Unit#
    char ID1[24] ;          // TVS: medallion/vin#, PWII: Vehicle ID
    char ID2[24] ;          // TVS: licenseplate, PWII: District

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

// capture card structure
struct  DvrChannel_attr {
    int         Enable ;
    char        CameraName[64] ;
    int         Resolution;				// 0: CIF, 1: 2CIF, 2:DCIF, 3:4CIF, 4:QCIF
    int         RecMode;				// 0: Continue, 1: Trigger by sensor, 2: triger by motion, 3, trigger by sensor/motion, 4: Schedule
    int         PictureQuality;			// 0: lowest, 10: Highest
    int         FrameRate;
    int         Sensor[4] ;
    int         SensorEn[4] ;
    int         SensorOSD[4] ;
    int         PreRecordTime ;			// pre-recording time in seconds
    int         PostRecordTime ;		// post-recording time in seconds
    int         RecordAlarm ;			// Recording Signal
    int         RecordAlarmEn ;
    int         RecordAlarmPat ;		// 0: OFF, 1: ON, 2: 0.5s Flash, 3: 2s Flash, 4, 4s Flash
    int         VideoLostAlarm ;		// Video signal lost alarm
    int         VideoLostAlarmEn ;
    int         VideoLostAlarmPat ;
    int         MotionAlarm ;			// Motion Detect alarm
    int         MotionAlarmEn ;
    int         MotionAlarmPat ;
    int         MotionSensitivity;
    int         GPS_enable ;			// enable gps options
    int         ShowGPS ;
    int         GPSUnit ;				// GPS unit display, 0: English, 1: Metric
    int         BitrateEn ;				// Enable Bitrate Control
    int         BitMode;				// 0: VBR, 1: CBR
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
    int         res ;                   // reserved
} ;


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

#endif							// __dvrprotocol_h__
