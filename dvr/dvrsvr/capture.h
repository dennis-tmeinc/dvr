

#ifndef __CAPTURE_H__
#define __CAPTURE_H__


// capture
// capture card structure

#ifndef DWORD
#define DWORD   unsigned int
#endif

#ifndef WORD
#define WORD   unsigned short
#endif

// dvr .264 file structure
// .264 file struct
struct hd_file {
    DWORD flag;					//      "4HKH", 0x484b4834
    DWORD res1[6];
    DWORD width_height;			// lower word: width, high word: height
    DWORD res2[2];
};

#ifdef EAGLE32
struct hd_frame {
    DWORD flag;					// 1
    DWORD serial;
    DWORD timestamp;
    DWORD res1;
    DWORD d_frames;				// sub frames number, lower 8 bits
    DWORD width_height;			// lower word: width, higher word: height
    DWORD res3[6];
    DWORD d_type;				// lower 8 bits; 3  keyframe, 1, audio, 4: B frame
    DWORD res5[3];
    DWORD framesize;
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

#endif

struct cap_frame {
    int channel;
    int framesize;
    int frametype;
    char * framedata;
};

extern int cap_channels;

void cap_start();
void cap_stop();
void cap_restart(int channel);
void cap_capIframe(int channel);
char * cap_fileheader(int channel);
void cap_init(config &dvrconfig);
void cap_uninit();

#define CAP_TYPE_UNKNOWN	(-1)
#define CAP_TYPE_HIKLOCAL	(0)
#define CAP_TYPE_HIKIP		(1)

class capture {
  protected:
    struct  DvrChannel_attr	m_attr ;	// channel attribute
    int m_type ;				// channel type, 0: local capture card, 1: eagle ip camera
    int m_channel;				// channel number
    int m_enable ;				// 1: enable, 0:disable
    int m_motion ;				// motion status ;
    int m_signal ;				// signal ok.
    int m_oldsignal ;			// signal ok.
    int m_signal_standard ;     // 1: ntsc, 2: pal
    unsigned m_streambytes;     // total stream bytes.

    int m_started ;             // 0: stopped, 1: started
    int m_working ;             // channel is working
    int m_remoteosd ;           // get OSD from network

    unsigned int m_sensorosd ;      // bit maps for sensor osd

    int m_motionalarm ;
    int m_motionalarm_pattern ;
    int m_signallostalarm ;
    int m_signallostalarm_pattern ;

    int m_showgpslocation ;

#ifdef  TVS_APP
    int m_show_medallion ;
    int m_show_licenseplate ;
    int m_show_ivcs ;
    int m_show_cameraserial ;
#endif

#ifdef PWII_APP
    int m_show_vri;
    int m_show_policeid ;
#endif

    int m_show_gforce ;

    int m_headerlen ;
    char m_header[256] ;

    // jpeg capture
    int m_jpeg_mode;            // -1: no capture, >=0 : capture resolution
    int m_jpeg_quality ;        // capture quality

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
    virtual int geteaglechannel() {
        return m_channel;
    }
    int enabled(){
        return m_enable;
    }
    int isworking(){
        if( m_started )
            return --m_working>0 ;
        else
            return 1 ;
    }
    int isstarted(){
        return m_started ;
    }
    int type(){
        return m_type ;
    }
    unsigned int streambytes() {
        return m_streambytes ;
    }
    void onframe(cap_frame * pcapframe);	// send frames to network or to recording
    char * getname(){
        return m_attr.CameraName ;
    }
    int getptzaddr() {
        return m_attr.ptz_addr;
    }
    void setremoteosd() { m_remoteosd=1; }
    void updateOSD ();          		// to update OSD text
    void alarm();                   	// update alarm relate to capture channel
    int getheaderlen(){return m_headerlen;}
    char * getheader(){return m_header;}

    virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD
    virtual void setosd( struct hik_osd_type * ){}
    virtual void start(){}				// start capture
    virtual void stop(){}				// stop capture
    virtual void restart(){				// restart capture
        stop();
        if( m_enable ) {
            start();
        }
    }
    virtual void captureIFrame(){        // force to capture I frame
    }
    // to capture one jpeg frame
    virtual int captureJPEG(int quality, int pic)
    {
        return 0 ;                     // not supported
    }
    virtual void docaptureJPEG()
    {
    }
    virtual int getsignal(){return m_signal;}	// get signal available status, 1:ok,0:signal lost
    virtual int getmotion(){return m_motion;}	// get motion detection status
};

extern capture * * cap_channel;

#ifndef NO_ONBOARD_EAGLE

extern int eagle32_channels ;
int eagle32_hikhandle(int channel);
int eagle32_hikchanelenabled(int channel);
int  eagle32_init(config &dvrconfig);
void eagle32_uninit();
// jpeg capture
void eagle_captureJPEG();

class eagle_capture : public capture {
protected:
    // Hik Eagle32 parameter
    int m_hikhandle;				// hikhandle = hikchannel+1
    int m_chantype ;
    int m_dspdatecounter ;

    char m_motiondata[256] ;
    int m_motionupd;				// motion status changed ;
    int m_codecrun ;                // codec running

public:
    eagle_capture(int channel, int hikchannel);
    ~eagle_capture();

    int gethandle(){
        return m_hikhandle;
    }

    virtual int geteaglechannel() {
        return m_hikhandle-1;
    }

    void streamcallback( void * buf, int size, int frame_type);

    int getresolution(){
        return m_attr.Resolution;
    }
    virtual void setosd( struct hik_osd_type * posd );

    // virtual function implements
    virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD
    virtual void start();
    virtual void stop();
    virtual void captureIFrame();       // force to capture I frame
    // to capture one jpeg frame
    virtual int captureJPEG(int quality, int pic) ;
    virtual void docaptureJPEG() ;
    virtual int getsignal();        // get signal available status, 1:ok,0:signal lost
};

#endif 	// NO_ONBOARD_EAGLE

#define EAGLESVRPORT (15159)

#endif  // __CAPTURE_H__
