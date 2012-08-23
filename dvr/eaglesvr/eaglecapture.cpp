
#include "eaglesvr.h"

#ifdef EAGLE32
#include "eagle32/davinci_sdk.h"
#endif
#ifdef EAGLE34
#include "eagle34/davinci_sdk.h"
#endif

#ifdef EAGLE32
char Dvr264Header[40] =
{
    '\x34', '\x48', '\x4B', '\x48', '\xFE', '\xB3', '\xD0', '\xD6',
    '\x08', '\x03', '\x04', '\x20', '\x00', '\x00', '\x00', '\x00',
    '\x03', '\x10', '\x02', '\x10', '\x01', '\x10', '\x10', '\x00',
    '\x80', '\x3E', '\x00', '\x00', '\x10', '\x02', '\x40', '\x01',
    '\x11', '\x10', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};
#endif
#ifdef EAGLE34
char Dvr264Header[40] =
{
    '\x49', '\x4d', '\x4b', '\x48', '\x01', '\x01', '\x01', '\x00',
    '\x02', '\x00', '\x01', '\x00', '\x21', '\x72', '\x01', '\x0e',
    '\x80', '\x3e', '\x00', '\x00', '\x00', '\x7d', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};
#endif


class eagle_capture : public capture {
protected:
    // Hik Eagle32 parameter
    int m_hikhandle;				// hikhandle = hikchannel+1
    int m_chantype ;
    int m_dspdatecounter ;

    char m_motiondata[256] ;
    int m_motionupd;				// motion status changed ;

public:
    eagle_capture(int channel);
    ~eagle_capture();

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
            if( m_started ) {
                restart();
            }
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
};


eagle_capture::eagle_capture( int channel )
: capture(channel)
{
    m_type=CAP_TYPE_HIKLOCAL;

    // hik eagle32 parameters
    m_hikhandle=channel+1 ;			// handle = channel+1
    m_chantype=MAIN_CHANNEL ;
    m_dspdatecounter=0 ;

    m_motion=0 ;
    m_motionupd = 1 ;
    m_started = 0 ;                 // no started.
    m_enable = 1 ;

    // default file header
    m_headerlen = 40 ;
    memcpy( m_header, Dvr264Header, 40 );
}

eagle_capture::~eagle_capture()
{
    stop();
}

// analyze motion data package
static int motionanalyze( unsigned int * mddata, int line )
{
    int i, j, motion ;
    unsigned int m;
    motion=0 ;
    if( line>15 )
        line=15 ;
    for( i=0; i<line; i++) {
        m = *mddata++ ;
        for(j=0;j<22;j++) {	// 22 blocks in a raw
            if( m&1 ) {
                motion++ ;
            }
            m>>=1 ;
        }
    }
    return (motion);
}

// stream call back
#ifdef EAGLE32
static void StreamReadCallBack(	int handle,
                               void * buf,
                               int size,
                               int frame_type,
                               void * context )
{
    int channel = handle-1 ;
    if( channel>=0 &&
        channel<cap_channels &&
        cap_channel[channel] )
    {
        cap_channel[channel]->streamcallback( buf, size, frame_type );
    }
}
#endif


#ifdef EAGLE34
static void StreamReadCallBack(CALLBACK_DATA CallBackData,void* context)
{
    int channel=CallBackData.channel-1 ;
    if( channel>=0 &&
       channel<cap_channels &&
       cap_channel[channel] )
    {
        cap_channel[channel]->streamcallback(CallBackData.pBuf,
                                                    CallBackData.size,
                                                    CallBackData.frameType );
    }
}
#endif

void eagle_capture::streamcallback(
                                   void * buf,
                                   int size,
                                   int frame_type)
{
    if( !m_started ) {      // not started?
        return ;
    }
    struct cap_frame capframe;
    capframe.frametype = FRAMETYPE_UNKNOWN  ;
    if( m_started ) {            // record or send frame only when necessary.
        switch(frame_type){
#ifdef EAGLE32
            case FRAME_TYPE_AUDIO:
#endif
#ifdef EAGLE34
            case FRAME_TYPE_AUDIO_PS:
#endif
                capframe.frametype = FRAMETYPE_AUDIO ;
                break ;
#ifdef EAGLE32
            case FRAME_TYPE_VIDEO_P:
            case FRAME_TYPE_VIDEO_SUB_P:
            case FRAME_TYPE_VIDEO_BP:
            case FRAME_TYPE_VIDEO_SUB_BP:
#endif
#ifdef EAGLE34
            case FRAME_TYPE_VIDEO_P_PS:
#endif
                capframe.frametype = FRAMETYPE_VIDEO ;
                break;
#ifdef EAGLE32
            case FRAME_TYPE_VIDEO_I:
            case FRAME_TYPE_VIDEO_SUB_I:
                struct hd_frame * pframe = (struct hd_frame *)buf;
                if( (((pframe->width_height)>>16)&0xfff)%40 == 0 ) {
                    m_signal_standard = 1 ;         // assume NTSC when height would be 120, 320, 240, 480
                }
                else {
                    m_signal_standard = 2 ;         // PAL mode video
                }
#endif
#ifdef EAGLE34
            case FRAME_TYPE_VIDEO_I_PS:
#endif
                capframe.frametype = FRAMETYPE_KEYVIDEO ;
                if( m_motion>0 ) {
                    if( --m_motion==0 ) {
                        m_motionupd=1 ;
                    }
                }
                break;
            case FRAME_TYPE_HEADER:
            case FRAME_TYPE_SUB_HEADER:
                capframe.frametype = FRAMETYPE_264FILEHEADER ;
                break;
            case FRAME_TYPE_MD_RESULT:
                // analyze motion data
                if(  motionanalyze( (unsigned int *)buf, size/sizeof(unsigned int) ) > 2 ) {
                    if( m_motion==0 ) {
                        m_motionupd = 1 ;
                    }
                    m_motion=2 ;
                }
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
}

void eagle_capture::start()
{
    int res ;
    if( m_enable && (!m_started) ) {

        dvr_log("Cap channel %d started!", m_channel );

        if( m_attr.disableaudio ) {
            res = SetStreamType(m_hikhandle, m_chantype,  STREAM_TYPE_VIDOE);
        }
        else {
            res = SetStreamType(m_hikhandle, m_chantype,  STREAM_TYPE_AVSYN);
        }

        int QuantVal;
        if( m_attr.PictureQuality>10 ) {
            m_attr.PictureQuality = 10 ;
        }
        if( m_attr.PictureQuality<0 ) {
            m_attr.PictureQuality = 0 ;
        }
        QuantVal = 12+(10-m_attr.PictureQuality);
        res = SetDefaultQuant(m_hikhandle, m_chantype, QuantVal)	; // quality should be between 12 to 30

        // set frame mode
        if( m_attr.key_interval<=0 ) {
            m_attr.key_interval = 3 * m_attr.FrameRate ;
            if( m_attr.key_interval<12 || m_attr.key_interval>400 ) {
                m_attr.key_interval=12 ;
            }
        }

        if( m_attr.b_frames <=0  && m_attr.b_frames > 10 ) {
            m_attr.b_frames = 2;
        }

        if( m_attr.p_frames <=0  && m_attr.p_frames > 10 ) {
            m_attr.p_frames = 1;
        }

        res= SetIBPMode(m_hikhandle, m_chantype, m_attr.key_interval, m_attr.b_frames, m_attr.p_frames, m_attr.FrameRate);

        // setup bitrate control
        if( m_attr.BitrateEn ) {
            res = SetBitrateControl(m_hikhandle, m_chantype, m_attr.Bitrate, ( m_attr.BitMode==0 )?MODE_VBR:MODE_CBR  );
        }
        else {
            res = SetBitrateControl(m_hikhandle, m_chantype, 4000000, MODE_VBR  );
        }

        const static picture_format fmt[5] = {
            ENC_CIF,
            ENC_2CIF,
            ENC_DCIF,
            ENC_4CIF,
            ENC_QCIF
        } ;
        if( m_attr.Resolution < 0 ) {
            m_attr.Resolution = 0 ;
        }
        else if( m_attr.Resolution>4 ) {
            m_attr.Resolution = 3 ;
        }
        res = SetEncoderPictureFormat(m_hikhandle, m_chantype, fmt[m_attr.Resolution] );

        if( m_attr.MotionSensitivity>=0 ) {
            RECT rect ;
            rect.left = 5 ;
            rect.top = 5 ;
            rect.right = 696 ;
            rect.bottom = 475 ;
            res = SetMotionDetection(m_hikhandle,  m_attr.MotionSensitivity, 2, 0, &rect, 1 );
            res = EnalbeMotionDetection( m_hikhandle, 1);
        }
        else {
            EnalbeMotionDetection( m_hikhandle, 0 );
        }

        // set color contrl
        SetVideoParam(m_hikhandle,
                      128-80+m_attr.brightness*16,
                      128-80+m_attr.contrast*16,
                      128-120+m_attr.saturation*24,
                      128-20+m_attr.hue*4 );

        int b, c, s, h ;
        video_standard v ;
        res = GetVideoParam(m_hikhandle, &v, &b, &c, &s, &h );
        m_signal_standard = (int)v ;

//        eagle32_tsadjust=0 ;

        StartCodec( m_hikhandle, m_chantype );

        // start encoder
        m_started = 1 ;
    }
}

void eagle_capture::stop()
{
    int res ;
    if( m_started ) {

        dvr_log("Cap channel %d stopped!", m_channel );

        res=EnalbeMotionDetection(m_hikhandle, 0);	// disable motion detection
        res=StopCodec(m_hikhandle, m_chantype);
        m_started = 0 ;
    }
}

void eagle_capture::captureIFrame()
{
    if( m_started ) {
        CaptureIFrame( m_hikhandle, m_chantype );
    }
}

// this is call from main thread to do actural jpeg capture
/*
void eagle_captureJPEG()
{
    int ch ;
    for( ch=0; ch<cap_channels ; ch++ ) {
        cap_channel[ch]->docaptureJPEG();
    }
}
*/

struct st_jpegcap {
    int channel ;
    int handle;
    int quality ;
    int pic ;
};

void * thread_capjpeg(void *param)
{
    struct st_jpegcap * pjpeg_parameter = (struct st_jpegcap *)param ;
    // set to low priority ?
    nice(10);
    usleep(10);
    unsigned int bufsize = 256*1024 ;
    unsigned char * jpegbuf = (unsigned char *) mem_alloc(bufsize) ;
    if( jpegbuf ) {
        if( GetJPEGImage(pjpeg_parameter->handle, pjpeg_parameter->quality, pjpeg_parameter->pic, jpegbuf, &bufsize)==0 ) {
            // adjust/add jpeg tag
            int begin, end, i ;
            begin = 0 ;
            end = bufsize ;
            // look for jpeg file ending tag
            for( i=0; i<200 ; i++ ) {
                if( jpegbuf[end-i-1] == 0xd9 &&
                    jpegbuf[end-i-2] == 0xff )
                {
                    end -= i ;
                    break;
                }
            }
            // look for jpeg file begin tag
            for( i=0; i<200; i++ ) {
                if( jpegbuf[i]==0xff &&
                    jpegbuf[i+1]==0xd8 &&
                    jpegbuf[i+2]==0xff )
                {
                    begin=i ;
                    break;
                }
            }

            struct cap_frame capframe;
            capframe.channel = pjpeg_parameter->channel ;
            capframe.framesize = end-begin ;
            capframe.frametype = FRAMETYPE_JPEG ;
            capframe.framedata = (char *) mem_alloc( capframe.framesize );
            if( capframe.framedata ) {
                mem_cpy32(capframe.framedata, &jpegbuf[begin], capframe.framesize ) ;
                cap_channel[capframe.channel]->onframe(&capframe);
                mem_free(capframe.framedata);
            }
        }
        mem_free(jpegbuf);
    }
    delete pjpeg_parameter ;
    return NULL ;
}

// to capture one jpeg frame
void eagle_capture::captureJPEG(int quality, int pic)
{
    if( m_enable && m_hikhandle>0 ) {
        pthread_t jpegthreadid ;
        struct st_jpegcap * pjpeg_parameter = new struct st_jpegcap ;
        pjpeg_parameter->handle = m_hikhandle ;
        pjpeg_parameter->quality = quality ;
        pjpeg_parameter->pic = pic ;
        pjpeg_parameter->channel = m_channel ;
        if( pthread_create(&jpegthreadid, NULL, thread_capjpeg, (void *)pjpeg_parameter ) == 0 ) {
            pthread_detach( jpegthreadid );
        }
        else {
            delete pjpeg_parameter ;
        }
    }
}

void eagle_capture::setosd( struct hik_osd_type * posd )
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
            SetDSPDateTime(m_hikhandle, &nowt);
        }
    }
}

void eagle_capture::update(int updosd)
{
    if( m_started ) {
        if( m_motionupd ) {
            m_motionupd = 0 ;
            updosd=1;
        }
        capture::update( updosd );
    }
}

int eagle_capture::getsignal()
{
    int res ;
    int sig ;
    if( m_started ) {
        sig = m_signal ;
        res=GetVideoSignal(m_hikhandle, &m_signal);
// this method doesn't work (on Eagle32, but works on Eagle34), can't get signal standard by GetVideoParam()
#ifdef EAGLE34
        if( sig!=m_signal ) {
            int b, c, s, h ;
            video_standard v ;
            res = GetVideoParam(m_hikhandle, &v, &b, &c, &s, &h );
            m_signal_standard = (int)v ;
        }
#endif
    }
    return m_signal ;
}

// convert channel (camera number) to hik handle
int eagle32_hikhandle(int channel)
{
    if( channel < cap_channels ) {
        if( cap_channel[channel]->type()==0 ) {     // local capture card
            return ((eagle_capture *)cap_channel[channel])->gethandle() ;
        }
    }
    return 0 ;
}


// check if hik channel enabled ?
int eagle32_hikchanelenabled(int channel)
{
    if( channel < cap_channels ) {
        return cap_channel[channel]->enabled() ;
    }
    return 0 ;
}

static int eagle_InitSystem=0 ;

int eagle_init()
{
    int res ;
    int i;
    board_info binfo ;

    if( eagle_InitSystem==0 ) {
        res = InitSystem();
        if( res<0 ) {
            dvr_log("Board init failed!\n");
            // return 0;
        }
        eagle_InitSystem=1 ;
    }

    eagle_uninit();

    if( GetBoardInfo(&binfo)==0 ) {
        cap_channels = (int)binfo.enc_chan ;

        if( cap_channels<0 || cap_channels>16 ) {
            cap_channels=0;
        }
        else {
            cap_channel = new capture * [cap_channels] ;
            //    dvr_log("%d capture card (local) channels detected.", dvrchannels);
            for (i = 0; i < cap_channels; i++ ) {
                cap_channel[i]=new eagle_capture(i);
            }
            res=RegisterStreamDataCallback(StreamReadCallBack,NULL);
        }
    }

    return cap_channels ;
}

void eagle_uninit()
{
    int i;
    cap_stop();
    if( cap_channels > 0 ) {
        i=cap_channels ;
        cap_channels=0 ;
        while(i>0) {
            delete cap_channel[--i] ;
            cap_channel[--i]=NULL ;
        }
        delete [] cap_channel ;
        cap_channel=NULL ;
        RegisterStreamDataCallback(NULL,NULL);
    }
}

void eagle_finish()
{
    if(	eagle_InitSystem ) {
        FiniSystem(); 			// Works eagle368 (did not hang ~~)
        eagle_InitSystem=0 ;
        dvr_log("eagle_finish\n");
    }
}
