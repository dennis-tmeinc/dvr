
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

char DvrFileHeader[40] =
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

class eagle368_capture : public capture {
protected:
    // Hik Eagle32 parameter
    int m_hikhandle;				// hikhandle = hikchannel+1
    int m_chantype ;
    int m_dspdatecounter ;

    char m_motiondata[256] ;
    int m_motionupd;				// motion status changed ;
public:
    eagle368_capture(int channel);
    ~eagle368_capture();

    int gethandle(){
        return m_hikhandle;
    }

    int getresolution(){
        return m_attr.Resolution;
    }

    virtual void getattr(struct DvrChannel_attr * pattr) {
        memcpy( pattr, &m_attr, sizeof(m_attr) );
    }
    virtual void setattr(struct DvrChannel_attr * pattr) {
        if( pattr->structsize == sizeof(struct DvrChannel_attr) && memcmp(pattr, &m_attr, pattr->structsize)!=0 ) {
            memcpy(&m_attr, pattr, pattr->structsize );
            m_enable = m_attr.Enable ;
        }
    }

    virtual void streamcallback( void * buf, int size, int frame_type);
    virtual void setosd( struct hik_osd_type * posd );

    // virtual function implements
    virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD
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
    : capture(channel)
{
    m_type=CAP_TYPE_HIKLOCAL;

    // hik eagle32 parameters
    m_hikhandle=channel ;
    m_chantype=MAIN_CHANNEL ;
    m_dspdatecounter=0 ;

    m_motion=0 ;
    m_motionupd = 1 ;
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

// stream call back
static void StreamReadCallBack(CALLBACK_DATA CallBackData,void* context)
{

    int channel=CallBackData.channel ;
    if( channel>=0 &&
        channel<cap_channels &&
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
    }

    if( capframe.frametype != FRAMETYPE_UNKNOWN ) {
        capframe.channel = m_channel ;
        capframe.framesize = size ;
        capframe.framedata = (char *) buf ;
        onframe(&capframe);
    }
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

/*
        if( m_attr.MotionSensitivity>=0 ) {
            res = SetMotionDetection(m_hikhandle,  m_attr.MotionSensitivity );
            res = EnalbeMotionDetection( m_hikhandle, 1);
        }
        else {
            EnalbeMotionDetection( m_hikhandle, 0 );
        }
*/

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

        res=EnalbeMotionDetection(m_hikhandle, 0);	// disable motion detection
        res=StopCodec(m_hikhandle, m_chantype);
        m_started = 0 ;
    }

    int i;
    for( i=0; i<cap_channels ;i++) {
        if( cap_channel[i] ) {
            cap_channel[i]->isstarted();
            return ;
        }
    }
    if( eagle368_systemstart ) {
        dvr_log( "Eagle368 System Stop!") ;
        SystemStop();
        eagle368_systemstart = 0 ;
        eagle368_systemstart_time = time_tick();
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
        if( m_dspdatecounter > g_timetick || (g_timetick-m_dspdatecounter)>30000 ) {
            struct dvrtime dvrt ;
            struct SYSTEMTIME nowt ;
            m_dspdatecounter = g_timetick ;
            time_now ( &dvrt );
            memset( &nowt, 0, sizeof(nowt));
            nowt.year = dvrt.year ;
            nowt.month = dvrt.month ;
            nowt.day = dvrt.day ;
            nowt.hour = dvrt.hour ;
            nowt.minute = dvrt.minute ;
            nowt.second = dvrt.second ;
            nowt.milliseconds = dvrt.milliseconds ;
//            SetDSPDateTime(m_hikhandle, &nowt);
        }
    }
}

void eagle368_capture::update(int updosd)
{
    if( m_started ) {
        if( m_motionupd ) {
            m_motionupd = 0 ;
            updosd=1;
        }
        capture::update( updosd );
    }
}

int eagle368_capture::getsignal()
{
    if( m_started ) {
        m_signal = GetVideoSignal(m_hikhandle);
    }
    return m_signal ;
}

int SetMotionDetection(int channel, int sensitity);

/*Notes for EnalbeMotionDetection
    Enable the Motion Detection for channel
    INPUTS:
    int channel: channel handle
    int enable: 1 enable,0 disable

*/
int EnalbeMotionDetection(int channel, int enable);
/*Notes for GetChannelMotionStatus
   Get the motion status for channel
   INPUTS:
   int channel: channel handle
*/
int GetChannelMotionStatus(int channel);

int eagle368_capture::getmotion()
{
    if( m_started ) {
        int motion = GetChannelMotionStatus(m_hikhandle);
        if( motion != m_motion ) {
            m_motion = motion ;
            m_motionupd = 1 ;
        }
    }
    return m_motion ;
}

// check if hik channel enabled ?
int eagle32_hikchanelenabled(int channel)
{
    if( channel < cap_channels ) {
        return cap_channel[channel]->enabled() ;
    }
    return 0 ;
}


void eagle_idle()
{
    int i;
    if(eagle368_systemstart_req!=eagle368_systemstart) {
        if( g_timetick-eagle368_systemstart_time > 8000 ) {     // delay 8 seconds between systemstart and systemstop, system would hang otherwise
            if(eagle368_systemstart_req){
                // start all channel
                for( i=0; i<cap_channels ;i++) {
                    if( cap_channel[i] ) {
                        cap_channel[i]->start();
                    }
                }
                dvr_log( "Eagle368 System Start!") ;
                SystemStart();
                eagle368_systemstart = eagle368_systemstart_req ;
                eagle368_systemstart_time = g_timetick ;
            }
            else {
                // stop all channel
                for( i=0; i<cap_channels ;i++) {
                    if( cap_channel[i] ) {
                        cap_channel[i]->stop();
                    }
                }
                dvr_log( "Eagle368 System Stop!") ;
                SystemStop();
                eagle368_systemstart = eagle368_systemstart_req ;
                eagle368_systemstart_time = g_timetick ;
            }
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
        cap_channels = (int)binfo.enc_chan ;

        if( cap_channels<0 || cap_channels>16 ) {
            cap_channels=0;
        }
        else {
            cap_channel = new capture * [cap_channels] ;
            //    dvr_log("%d capture card (local) channels detected.", dvrchannels);
            for (i = 0; i < cap_channels; i++ ) {
                cap_channel[i]=new eagle368_capture(i);

            }
            res=RegisterStreamDataCallback(StreamReadCallBack,NULL);
        }
    }

    eagle368_systemstart_req = eagle368_systemstart = 0  ;
    eagle368_systemstart_time = g_timetick-10000 ;  // make it ready to start

    return cap_channels ;
}

void eagle_uninit()
{
    int i;

    // stop eagle 368 capturing
    for( i=0; i<cap_channels ;i++) {
        if( cap_channel[i] ) {
            cap_channel[i]->stop();
        }
    }
    if(eagle368_systemstart) {
        SystemStop();
        eagle368_systemstart=0 ;
    }

    if( cap_channels > 0 ) {
        i=cap_channels ;
        cap_channels=0 ;
        while(i>0) {
            delete cap_channel[--i] ;
        }
        delete [] cap_channel ;
        cap_channel=NULL ;
        RegisterStreamDataCallback(NULL,NULL);
    }
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
