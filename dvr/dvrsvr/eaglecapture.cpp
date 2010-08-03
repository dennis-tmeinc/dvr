
#include "dvr.h"
#ifdef EAGLE32
#include "eagle32/davinci_sdk.h"
#endif
#ifdef EAGLE34
#include "eagle34/davinci_sdk.h"
#endif

//static WORD eagle32_tsadjust = 0 ;
#define MAX_EAGLE_CHANNEL   (4)

int eagle32_channels ;

static eagle_capture * eagle_capture_array[MAX_EAGLE_CHANNEL] ;

eagle_capture::eagle_capture( int channel, int hikchannel ) 
: capture(channel)
{
    m_enable = 0 ;
    m_type=CAP_TYPE_HIKLOCAL;
    
    // hik eagle32 parameters
    m_hikhandle=hikchannel+1 ;			// handle = channel+1
    m_chantype=MAIN_CHANNEL ;
    m_dspdatecounter=0 ;
    
    m_motion=0 ;
    m_motionupd = 1 ;

    m_started = 0 ;                 // no started.
    
    if( hikchannel<0 ) {
        return ;
    }
#ifdef  EAGLESVR
    m_enable = 1 ;
#else    
    m_enable = m_attr.Enable ;
#endif
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
    handle--;
    if( handle>=0 && 
        handle<MAX_EAGLE_CHANNEL &&
        eagle_capture_array[handle] ) 
    {
        eagle_capture_array[handle]->streamcallback( buf, size, frame_type );
    }
}		
#endif


#ifdef EAGLE34
static void StreamReadCallBack(CALLBACK_DATA CallBackData,void* context)
{
    int handle=CallBackData.channel-1 ;
    if( handle>=0 && 
        handle<MAX_EAGLE_CHANNEL &&
        eagle_capture_array[handle] ) 
    {
        eagle_capture_array[handle]->streamcallback(CallBackData.pBuf, 
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
    int xframetype ;
    struct cap_frame capframe;

    if( dio_record || net_active ) {            // record or send frame only when necessary. 
        xframetype = FRAMETYPE_UNKNOWN ;
#ifdef EAGLE32    
        if( frame_type==FRAME_TYPE_AUDIO ) {
            xframetype = FRAMETYPE_AUDIO ;
        }
        else if( frame_type==FRAME_TYPE_VIDEO_P || 
            frame_type==FRAME_TYPE_VIDEO_BP ||
            frame_type==FRAME_TYPE_VIDEO_SUB_BP || 
            frame_type==FRAME_TYPE_VIDEO_SUB_P ) 
        {
            xframetype = FRAMETYPE_VIDEO ;
        }
        else if( frame_type==FRAME_TYPE_VIDEO_SUB_I || 
            frame_type==FRAME_TYPE_VIDEO_I ) 
        {
            struct hd_frame * pframe = (struct hd_frame *)buf;
            if( (((pframe->width_height)>>16)&0xfff)%40 == 0 ) {
                m_signal_standard = 1 ;         // assume NTSC when height would be 120, 320, 240, 480
            }
            else {
                m_signal_standard = 2 ;         // PAL mode video
            }
            xframetype = FRAMETYPE_KEYVIDEO ;
            if( m_motion>0 ) {
                if( --m_motion==0 ) {
                    m_motionupd=1 ;
                }
            }
        }
        else if( frame_type<FRAME_TYPE_HEADER && frame_type>FRAME_TYPE_JPEG_IMG && frame_type==6 ) {	// error!
            return ;
        }
        else if( frame_type==FRAME_TYPE_HEADER || frame_type==FRAME_TYPE_SUB_HEADER ) {
            if( frame_type==FRAME_TYPE_HEADER ) {
                memcpy( Dvr264Header, buf, size );
            }
            xframetype = FRAMETYPE_264FILEHEADER ;
        }
        else if( frame_type==FRAME_TYPE_MD_RESULT ) {
            // analyze motion data
            if(  motionanalyze( (unsigned int *)buf, size/sizeof(unsigned int) ) > 2 ) {
                if( m_motion==0 ) {
                    m_motionupd = 1 ;
                }
                m_motion=2 ;
            }
            return ;
        }
#endif

#ifdef EAGLE34    
        switch(frame_type){
            case FRAME_TYPE_AUDIO_PS:
                xframetype = FRAMETYPE_AUDIO ; 
                break;
            case FRAME_TYPE_VIDEO_P_PS:
                xframetype = FRAMETYPE_VIDEO ;
                break;
            case FRAME_TYPE_VIDEO_I_PS:

/*  not neccessory, API GetVideoParam did this job already 
                 
                // to detect video size, and signal standard (NTSC/PAL)
                unsigned char * fbuf = (unsigned char *)buf ;
                int fsz = (unsigned) size ;
                while( fsz>20 ) {
                    if( fbuf[0]==0 &&
                        fbuf[1]==0 &&
                        fbuf[2]==1 )        // start code
                    {
                        if( fbuf[3]==0xba ) {   // packet start
                            fsz-=20 ;
                            fbuf+=20 ;
                        }
                        else {
                            if( fbuf[3]==0xbc ) {   // stream map (key frame)
                                fsz = (((unsigned int)fbuf[8])<<8) +
                                +(((unsigned int)fbuf[9])) ;
                                fbuf+=fsz+10 ;          // bypass program descriptor
                                if( fbuf[3]==0xe0 ) {   // descriptor for 0xe0 (video)
                                    int vsize = (((unsigned int)fbuf[12])<<8) +
                                        +(((unsigned int)fbuf[13])) ;
                                    int hsize = (((unsigned int)fbuf[14])<<8) +
                                        +(((unsigned int)fbuf[15])) ;
                                    if( hsize%40 == 0 ) {
                                        m_signal_standard = 1 ;         // assume NTSC when height would be 120, 320, 240, 480
                                    }
                                    else {
                                        m_signal_standard = 2 ;         // PAL mode video
                                    }
                                }
                                break;
                            }
                            int pksize = (((unsigned int)fbuf[4])<<8) +
                                +(((unsigned int)fbuf[5])) + 
                                6 ;                         // sync (4b) + size (2b)
                            fsz-=pksize ;
                            fbuf+=pksize ;
                        }
                    }
                    else {
                        break;
                    }
                }
*/
                
                xframetype = FRAMETYPE_KEYVIDEO ;
                if( m_motion>0 ) {
                    if( --m_motion==0 ) {
                        m_motionupd=1 ;
                    }
                }
                break;
            case FRAME_TYPE_HEADER:
            case FRAME_TYPE_SUB_HEADER:
                m_headerlen = size ;
                memcpy( m_header, buf, size );
                if( frame_type==FRAME_TYPE_HEADER ) {
                    memcpy( Dvr264Header, buf, size );
                }
                return;
            case FRAME_TYPE_MD_RESULT:
                // analyze motion data
                if(  motionanalyze( (unsigned int *)buf, size/sizeof(unsigned int) ) > 2 ) {
                    if( m_motion==0 ) {
                        m_motionupd = 1 ;
                    }
                    m_motion=2 ;
                }
                return ;
            default:
                dvr_log( "Unknown frame captured!" );
                return;
        }
#endif

        capframe.channel = m_channel ;
        capframe.framesize = size ;
        capframe.frametype = xframetype ;

        capframe.framedata = (char *) mem_alloc( capframe.framesize );
        if( capframe.framedata ) {
            mem_cpy32(capframe.framedata, buf, size ) ;
            
        /* 
#ifdef EAGLE32         
        // change time stamp
        if( xframetype == FRAMETYPE_AUDIO ||
            xframetype == FRAMETYPE_KEYVIDEO ||
            xframetype == FRAMETYPE_VIDEO ) 
        {
            // replace hik time stamp.
            struct hd_frame * pframe  = (struct hd_frame *) capframe.framedata ;
            if( eagle32_tsadjust ==0 ) {
                struct timeval tv ;
                gettimeofday(&tv, NULL);
                eagle32_tsadjust = (tv.tv_sec%86400)*64 + tv.tv_usec * 64 / 1000000 - pframe->timestamp ;
            }
            pframe->timestamp += eagle32_tsadjust ;
        }
#endif  // EAGLE32            
        */

            // send frame
            onframe(&capframe);
            mem_free(capframe.framedata);
        }
    }
}

void eagle_capture::start()
{
    int res ;
    stop();
    if( m_enable ) {
        
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
        }
        if( m_attr.key_interval<12 || m_attr.key_interval>400 ) {
            m_attr.key_interval=12 ;
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
        
        // start encoder
        if( m_hikhandle>0 &&
            m_hikhandle<=MAX_EAGLE_CHANNEL ) 
        {
            eagle_capture_array[m_hikhandle-1]=this ;
        }

        StartCodec( m_hikhandle, m_chantype );

        m_started = 1 ;
        
        // Update OSD
        updateOSD();
    }		
}

void eagle_capture::stop()
{
    int res ;
    if( m_started ) {
        res=EnalbeMotionDetection(m_hikhandle, 0);	// disable motion detection
        res=StopCodec(m_hikhandle, m_chantype);
        if( m_hikhandle>0 &&
            m_hikhandle<=MAX_EAGLE_CHANNEL ) 
        {
            eagle_capture_array[m_hikhandle-1]=NULL ;
        }
        m_started = 0 ;
    }
}

void eagle_capture::captureIFrame()
{
    if( m_hikhandle>0 ) {
        CaptureIFrame( m_hikhandle, m_chantype );
    }
}

int jpeg_quality ;
int jpeg_mode ;
int jpeg_size ;

// to capture one jpeg frame
void eagle_capture::captureJPEG()
{
    if( m_hikhandle>0 ) {
        unsigned int imgsize = jpeg_size ;
        unsigned char * img = (unsigned char *)mem_alloc( imgsize );
        if( img ) {
            if( GetJPEGImage(m_hikhandle, jpeg_quality, jpeg_mode, img, &imgsize )==0 ) {
                struct cap_frame capframe;
                capframe.channel = m_channel ;
                capframe.framesize = imgsize ;
                capframe.frametype = FRAMETYPE_JPEG ;
                capframe.framedata = (char *) img ;
                onframe( &capframe );
            }
        }
        mem_free( img );
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
        if( m_dspdatecounter > g_timetick || (g_timetick-m_dspdatecounter)>60000 ) {
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
            updateOSD();
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
    

static int eagle32_sysinit=0 ;

int eagle32_init()
{
    int res ;
    int i;
    
    config dvrconfig(dvrconfigfile);
    
    struct board_info binfo ;
    if( eagle32_sysinit==0 ) {
        res = InitSystem();
        if( res<0 ) {
            printf("Board init failed!\n");
//            return 0;
        }
        eagle32_sysinit=1 ;
    }
   
    for( i=0; i<MAX_EAGLE_CHANNEL; i++) {
        eagle_capture_array[i]=NULL ;
    }

    eagle32_channels = 0 ;

    if( GetBoardInfo(&binfo)==0 ) {
        eagle32_channels = (int)binfo.enc_chan ;

        if( eagle32_channels<0 ) {
            eagle32_channels=0;
        }
        if( eagle32_channels>MAX_EAGLE_CHANNEL ) {
            eagle32_channels = MAX_EAGLE_CHANNEL ;
        }
        if( eagle32_channels > 0 ) {
            res=RegisterStreamDataCallback(StreamReadCallBack,NULL);
        }
    }


    jpeg_quality = dvrconfig.getvalueint( "system", "jpeg_quality" );
    jpeg_mode = dvrconfig.getvalueint( "system", "jpeg_mode" );
    jpeg_size = dvrconfig.getvalueint( "system", "jpeg_size" );
    if( jpeg_size<=0 ) {
        jpeg_size=500000 ;
    }
    
    return eagle32_channels ;
}

void eagle32_uninit()
{
    int i ;
    eagle32_channels=0 ;
    RegisterStreamDataCallback(NULL,NULL);
    for( i=0; i<MAX_EAGLE_CHANNEL; i++) {
        eagle_capture_array[i]=NULL ;
    }
/*
     if( eagle32_sysinit==1 ) {
        FiniSystem();
        eagle32_sysinit=0 ;
    }
*/
}

