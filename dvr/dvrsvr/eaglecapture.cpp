
#include "dvr.h"
#include "eagle32/davinci_sdk.h"

int    eagle32_channels = 0 ;
//static WORD eagle32_tsadjust = 0 ;
#define MAX_EAGLE_CHANNEL   (8)
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
    m_enable = m_attr.Enable ;
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
static void StreamReadCallBack(	int handle,
                               void * buf,
                               int size,
                               int frame_type,
                               void * context ) 
{
    if( eagle_capture_array[handle-1] ) {
        eagle_capture_array[handle-1]->streamcallback( buf, size, frame_type );
    }
}		

void eagle_capture::streamcallback( 
                                   void * buf,
                                   int size,
                                   int frame_type)
{
    int xframetype = FRAMETYPE_UNKNOWN ;
    struct cap_frame capframe;
    
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
    
    capframe.channel = m_channel ;
    capframe.framesize = size ;
    capframe.frametype = xframetype ;
    
    capframe.framedata = (char *) mem_alloc( capframe.framesize );
    if( capframe.framedata == NULL ) {
        return ;
    }
    mem_cpy32(capframe.framedata, buf, size ) ;
/*
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
*/
    // send frame
    onframe(&capframe);
    mem_free(capframe.framedata);

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
        eagle_capture_array[m_hikhandle-1]=this ;
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
        eagle_capture_array[m_hikhandle-1]=NULL ;
        m_started = 0 ;
    }
}

void eagle_capture::captureIFrame()
{
    CaptureIFrame( m_hikhandle, m_chantype );
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
//    int sig ;
    if( m_started ) {
//        sig = m_signal ;
        res=GetVideoSignal(m_hikhandle, &m_signal);
/* this method doesn't work, can't get signal standard by GetVideoParam()        
        if( sig!=m_signal ) {
            int b, c, s, h ;
            video_standard v ;
            res = GetVideoParam(m_hikhandle, &v, &b, &c, &s, &h );
            m_signal_standard = (int)v ;
            updateOSD();
        }
*/         
    }
    return m_signal ;
}

int eagle32_init()
{
    int res ;
    int i;
    struct board_info binfo ;
    static int sysinit=0 ;
    eagle32_channels=0 ;
    if( sysinit==0 ) {
        res = InitSystem();
        if( res<0 ) {
            printf("Board init failed!\n");
//            return 0;
        }
        
        //		// ********************************** testing, mount CF card
        //		EnableATA ();
        //		//	EnableFLASH ();
        //		mkdir("/dvrdisks/CF", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        //		mount("/dev/hda1", "/dvrdisks/CF", "vfat", 0, NULL);
        
        sysinit=1 ;
    }
    res = GetBoardInfo(&binfo);
    eagle32_channels = binfo.enc_chan ;
    
    if( eagle32_channels<=0 || eagle32_channels>MAX_EAGLE_CHANNEL )
        eagle32_channels=0 ;
    if( eagle32_channels > 0 ) {
        for( i=0; i<MAX_EAGLE_CHANNEL; i++) {
            eagle_capture_array[i]=NULL ;
        }
        res=RegisterStreamDataCallback(StreamReadCallBack,NULL);
    }
    
    // reset time stamp adjustment
//    eagle32_tsadjust = 0 ;
    
    return eagle32_channels ;
}

void eagle32_uninit()
{
    int i ;
    for( i=0; i<MAX_EAGLE_CHANNEL; i++) {
        eagle_capture_array[i]=NULL ;
    }
    if( eagle32_channels>0 ) {
        eagle32_channels=0 ;
        //		FiniSystem();		// FiniSystem cause InitSystem fail!
    }
}

