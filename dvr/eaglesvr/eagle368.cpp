
#include "eaglesvr.h"

#include "eagle368/tme_sdk.h"

// file header for eagle368 (.266 file)
struct File_format_info {

   unsigned int file_fourcc;
   unsigned short file_version;
   unsigned short device_type;
   unsigned short file_format;
   unsigned short video_codec;
   unsigned short audio_codec;
   unsigned char audio_channels;
   unsigned char audio_bits_per_sample;
   unsigned int audio_sample_rate;
   unsigned int audio_bit_rate;
   unsigned short video_width;
   unsigned short video_height;
   unsigned short video_framerate;
   unsigned short reserverd;
   unsigned int reserverd1;
   unsigned int reserverd2;
};

const char DvrFileHeader[40] =
{
    '\x46', '\x45', '\x4d', '\x54', '\x01', '\x00', '\x01', '\x00',
    '\x01', '\x00', '\x01', '\x00', '\x01', '\x00', '\x01', '\x10',
    '\x40', '\x1f', '\x00', '\x00', '\x80', '\x3e', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};

static int  eagle368_systemstart ;
static int  eagle368_systemstart_req ;
static int  eagle368_systemstart_time ;

class eagle368_capture : public icapture {
protected:
    // Hik Eagle32 parameter
    int m_channel;				// channel number
    int m_chantype ;            // MAIN_CHANNEL
    int m_hikhandle;			// hikhandle = channel (used for for API call)

    struct  DvrChannel_attr	m_attr ;	// channel attribute
    int m_enable ;				// 1: enable, 0:disable
    int m_motion ;				// motion status ;
    int m_signal ;				// signal ok.
    int m_oldsignal ;           // signal ok.
    int m_signal_standard ;     // 1: ntsc, 2: pal

    int m_started ;             // 0: stopped, 1: started

    int m_headerlen ;
    char m_header[256] ;

public:
    eagle368_capture(int channel);
    ~eagle368_capture();

    int gethandle(){
        return m_hikhandle;
    }

    int getresolution(){
        return m_attr.Resolution;
    }

    virtual int enabled(){
        return m_enable;
    }
    virtual int isstarted(){
        return ( m_started ) ;
    }

    virtual int getheaderlen(){return m_headerlen;}
    virtual char * getheader(){return m_header;}

    virtual void setattr(struct DvrChannel_attr * pattr) {
        if( pattr->structsize == sizeof(struct DvrChannel_attr) && memcmp(pattr, &m_attr, pattr->structsize)!=0 ) {
            memcpy(&m_attr, pattr, pattr->structsize );
            m_enable = m_attr.Enable ;
        }
    }

    virtual void streamcallback( void * buf, int size, int frame_type);
    virtual void setosd( struct hik_osd_type * posd );

    // virtual function implements
    virtual void start();
    virtual void stop();
    virtual void captureIFrame();       // force to capture I frame
    // to capture one jpeg frame
    virtual void captureJPEG(int quality, int pic) ;
    //    virtual void docaptureJPEG() ;
    virtual int getsignal();        // get signal available status, 1:ok,0:signal lost
    virtual int getmotion();    	// get motion detection status

};

eagle368_capture::eagle368_capture( int channel )
{
    m_channel = channel ;

    m_signal=1;			// assume signal is good at beginning
    m_oldsignal=1;
    m_enable=0 ;
    m_signal_standard = 1 ;
    m_started = 0 ;
    // default file header
    m_headerlen = 0;

    //    m_jpeg_mode = -1 ;
    //    m_jpeg_quality = 0 ;

    // set default m_attr
    memset( &m_attr, 0, sizeof(m_attr) );
    m_attr.structsize = sizeof(m_attr);

    m_attr.Enable = 0;
    sprintf(m_attr.CameraName, "camera%d", m_channel);
    m_attr.Resolution=3; 	// 4CIF
    m_attr.RecMode=0;		// Not used
    m_attr.PictureQuality=5;
    m_attr.FrameRate=10;

    // bitrate
    m_attr.BitrateEn=1;
    m_attr.Bitrate=1000000;
    m_attr.BitMode=0;

    // picture control
    m_attr.brightness=5;
    m_attr.contrast  =5;
    m_attr.saturation=5;
    m_attr.hue       =5;

    m_attr.key_interval = 100;

    // hik eagle32 parameters
    m_hikhandle=channel ;
    m_chantype=MAIN_CHANNEL ;

    m_motion=0 ;
    m_started = 0 ;                 // no started.

    // default file header
    m_headerlen = 40 ;
    memcpy( m_header, DvrFileHeader, 40 );

    // default .266 file header
    struct File_format_info * ffi = (struct File_format_info *)&m_header ;
    ffi->video_framerate = m_attr.FrameRate ;
    ffi->video_height = 480 ;
    ffi->video_width = 720 ;
}

eagle368_capture::~eagle368_capture()
{
    stop();
}

int cap_channels;
icapture * * cap_channel;

// nul stream call back
static void Eagle368_NulStreamCallBack(CALLBACK_DATA ,void* )
{
}

// stream call back
static void Eagle368_StreamCallBack(CALLBACK_DATA CallBackData,void* context)
{

    int channel=CallBackData.channel ;
    if( eagle368_systemstart &&
        eagle368_systemstart_req &&
        channel>=0 &&
        channel<cap_channels &&
        cap_channel != NULL &&
        cap_channel[channel] )
    {
        cap_channel[channel]->streamcallback(CallBackData.pBuf,
                                            CallBackData.size,
                                            CallBackData.frameType );
    }
}

void eagle368_capture::streamcallback(
        void * buf,
        int size,
        int frame_type)
{
    if( !m_started ) {      // not started?
        return ;
    }

    struct File_format_info * ffi = (struct File_format_info *)&m_header ;
    if( ffi->video_height == 0 ) {
        ffi->video_width = GetVideoWidth(m_hikhandle) ;
        ffi->video_height = GetVideoHight(m_hikhandle) ;
        dvr_log( "Ch %d : %dx%d", m_channel, ffi->video_width, ffi->video_height );
    }

    struct cap_frame capframe;
    capframe.frametype = FRAMETYPE_UNKNOWN  ;
    switch(frame_type){
    case FRAME_TYPE_AUDIO:
        capframe.frametype = FRAMETYPE_AUDIO ;
        break ;
    case FRAME_TYPE_VIDEO_P:
        capframe.frametype = FRAMETYPE_VIDEO ;
        break;
    case FRMAE_TYPE_VIDEO_I:
        capframe.frametype = FRAMETYPE_KEYVIDEO ;
        break;
    default:
        dvr_log( "Unknown frame captured!" );
        return ;
    }

    capframe.channel = m_channel ;
    capframe.framesize = size ;
    capframe.framedata = (char *)buf;
    onframe(&capframe);
}

void eagle368_capture::start()
{
    int v ;
    int res ;
    if( m_enable && (!m_started) ) {

        if( m_attr.disableaudio ) {
            res = SetStreamType(m_hikhandle, m_chantype, STREAM_TYPE_VIDEO);
        }
        else {
            res = SetStreamType(m_hikhandle, m_chantype,  STREAM_TYPE_AVSYN);
        }

        // set picture quality
        v = m_attr.PictureQuality ;
        if( v>10 ) {
            v = 10 ;
        }
        if( v<0 ) {
            v = 0 ;
        }
        v = 12+(10-v);
        res = SetDefaultQuant(m_hikhandle, m_chantype, v);       // quality should be between 12 to 30

        // set key frame intervel, (b frames = 0 )
        v = m_attr.key_interval ;
        if( v <=0 ) {
            v = 3 * m_attr.FrameRate ;
        }
        if( v<10 || v>100 ) {
            v=12 ;
        }
        res= SetIBPMode(m_hikhandle, m_chantype, v, 0, m_attr.FrameRate);

        // setup bitrate control
        if( m_attr.BitrateEn ) {
            res = SetBitrateControl(m_hikhandle, m_chantype, m_attr.Bitrate, ( m_attr.BitMode==0 )?MODE_VBR:MODE_CBR  );
        }
        else {
            res = SetBitrateControl(m_hikhandle, m_chantype, 4000000, MODE_VBR  );
        }

        // setup picture resolution
        const static picture_format fmt[] = {
            ENC_CIF,
            ENC_2CIF,
            ENC_DCIF,
            ENC_4CIF,
            ENC_QCIF,
            ENC_D1
        } ;
        v =  m_attr.Resolution ;
        if(  v < 0 ) {
            v = 0 ;
        }
        else if( v > 5 ) {
            v = 5 ;
        }
        res = SetEncoderPictureFormat(m_hikhandle, m_chantype, fmt[v] );

        // setup color parameter
        SetVideoParam(m_hikhandle,
                      m_attr.brightness*25,
                      m_attr.contrast*25,
                      m_attr.saturation*25,
                      m_attr.hue*36-180 );


        if( m_attr.MotionSensitivity>=0 ) {
            res = SetMotionDetection(m_hikhandle,  m_attr.MotionSensitivity );
            res = EnalbeMotionDetection( m_hikhandle, 1);
        }
        else {
            EnalbeMotionDetection( m_hikhandle, 0 );
        }

        // Start codec
        StartCodec( m_hikhandle, m_chantype );

        // start encoder
        m_started = 1 ;

        struct File_format_info * ffi = (struct File_format_info *)&m_header ;
        ffi->video_height = 0 ;

        dvr_log("Cap channel %d started!", m_channel );
    }
}

void eagle368_capture::stop()
{
    int res ;
    if( m_started ) {

        dvr_log("Cap channel %d stopped!", m_channel );

//        res=EnalbeMotionDetection(m_hikhandle, 0);	// disable motion detection
        res=StopCodec(m_hikhandle, m_chantype);
        m_started = 0 ;
    }
}

void eagle368_capture::captureIFrame()
{
//    if( m_started ) {
//        CaptureIFrame( m_hikhandle, m_chantype );
//    }
}

// to capture one jpeg frame
void eagle368_capture::captureJPEG(int quality, int pic)
{
}

void eagle368_capture::setosd( struct hik_osd_type * posd )
{
    //	EnableOSD(m_hikhandle, 0);		// disable OSD
    int i;
    WORD * osdformat[8] ;
    if( m_started && posd!=NULL ) {
        for( i=0; i<posd->lines; i++) {
            osdformat[i]=&(posd->osdline[i][0]) ;
        }
        SetOSDDisplayMode(m_hikhandle,
                          posd->brightness,
                          posd->translucent,
                          posd->twinkle,
                          posd->lines,
                          osdformat );
        EnableOSD(m_hikhandle, 1);		// enable OSD
    }
}

int eagle368_capture::getsignal()
{
    if( m_started ) {
        m_signal = GetVideoSignal(m_hikhandle);
    }
    return m_signal ;
}

int eagle368_capture::getmotion()
{
    if( m_started ) {
        m_motion = GetChannelMotionStatus(m_hikhandle);
    }
    return m_motion ;
}

void eagle_idle()
{
    if(eagle368_systemstart_req!=eagle368_systemstart) {
        int i;
        int timetick = time_tick();
        if( timetick-eagle368_systemstart_time > 6000 ) {     // delay 6 seconds between systemstart and systemstop, system would hang otherwise
            if(eagle368_systemstart_req){
                // start all channel
                for( i=0; i<cap_channels ;i++) {
                    if( cap_channel[i] ) {
                        cap_channel[i]->start();
                    }
                }
                dvr_log( "SystemStart()...") ;
                SystemStart();
                dvr_log( "SystemStart() finished!");
            }
            else {
                // stop all channel
                for( i=0; i<cap_channels ;i++) {
                    if( cap_channel[i] ) {
                        cap_channel[i]->stop();
                    }
                }
                dvr_log( "SystemStop()...") ;
                SystemStop();
                dvr_log( "SystemStop() finished.") ;
            }
            eagle368_systemstart = eagle368_systemstart_req ;
            eagle368_systemstart_time = timetick ;
        }
    }
}

void eagle368_startcapture()
{
    eagle368_systemstart_req = 1 ;
}

void eagle368_stopcapture()
{
    eagle368_systemstart_req = 0 ;
}

static int eagle_InitSystem = 0;

int eagle_init()
{
    int res ;
    int i;
    int channels ;
    board_info binfo ;

    if( eagle_InitSystem==0 ) {
        res = InitSystem();
        if( res<0 ) {
            printf("Board init failed!\n");
            //            return 0;
        }
        eagle_InitSystem=1 ;
    }

    if( GetBoardInfo(&binfo)==0 ) {
        channels = (int)binfo.enc_chan ;

        if( channels<0 || channels>16 ) {
            channels=0;
        }
        else {
            cap_channel = new icapture * [channels] ;
            //    dvr_log("%d capture card (local) channels detected.", dvrchannels);
            for (i = 0; i < channels; i++ ) {
                cap_channel[i]=new eagle368_capture(i);
            }
            res=RegisterStreamDataCallback(Eagle368_StreamCallBack,NULL);
        }
    }

    eagle368_systemstart_req = eagle368_systemstart = 0  ;
    eagle368_systemstart_time = time_tick()-10000 ;  // make it ready to start

    cap_channels=channels ;

    return cap_channels ;
}

void eagle_uninit()
{
    int i;
    int channels;

    if( cap_channel==NULL ) {
        return ;
    }
    channels = cap_channels ;
    cap_channels = 0 ;          // to stop callback

    // register a nul call back
    RegisterStreamDataCallback(Eagle368_NulStreamCallBack,NULL);

    // stop eagle 368 capturing
    for( i=0; i<channels ;i++) {
        if( cap_channel[i] ) {
            cap_channel[i]->stop();
        }
    }
    if(eagle368_systemstart) {
        SystemStop();
        eagle368_systemstart=0 ;
    }

    // delete  eagle 368 channel
    for( i=0; i<channels ;i++) {
        if( cap_channel[i] ) {
            delete cap_channel[i];
        }
    }
    delete [] cap_channel ;
    cap_channel=NULL ;
}

void eagle_finish()
{
    eagle_uninit();

    if(	eagle_InitSystem ) {
        FiniSystem(); 			// will this work?
        eagle_InitSystem=0 ;
        dvr_log("eagle_finish\n");
    }
}