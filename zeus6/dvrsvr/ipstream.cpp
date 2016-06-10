/*
 * HD ip camera module
 */

#include "eagle8851/tme_sdk.h"
#include "eagle8851/tme_sdk_ip.h"

#include "dvr.h"

static void IPStreamReadCallBack(CALLBACK_DATA CallBackData,void* context)
{
	struct cap_frame capframe;
	
	if( CallBackData.size <= 0 || CallBackData.pBuf == NULL ) return ;
		
	switch(CallBackData.frameType){
    case FRAME_TYPE_AUDIO:
        capframe.frametype = FRAMETYPE_AUDIO ; 
        break;
    case FRAME_TYPE_VIDEO_P:
        capframe.frametype = FRAMETYPE_VIDEO ;
        break;
    case FRAME_TYPE_VIDEO_I:
        capframe.frametype = FRAMETYPE_KEYVIDEO ;
        break;
 
    default:
		// debug !
		printf("Unknown frame type: %d\n", CallBackData.frameType );
        return;
    }

	capframe.timestamp = CallBackData.mPTS;       //CallBackData.t_sec*1000+CallBackData.t_msec/1000;
	capframe.framesize = CallBackData.size ;
	capframe.framedata = NULL ;
	
    int ch = 0 ;
	for( ch=0; ch<cap_channels ; ch++ ) {
		capture * cap = cap_channel[ch] ;
		if( cap!=NULL && 
			cap->m_channel == CallBackData.channel &&
			cap->m_type == CAP_TYPE_HDIPCAM &&
			cap->m_start &&
			cap->m_enable )
		{
			if( cap->m_header.video_width == 0 ) {
				// get real resolution
				cap->m_header.video_width= GetIPVideoWidth(CallBackData.channel) ;
				cap->m_header.video_height= GetIPVideoHight(CallBackData.channel) ;
				cap->m_header.video_framerate = GetIPVideoFrameRate(CallBackData.channel) ;
			}
			if( capframe.framedata == NULL ) {
				capframe.framedata = (char *) mem_addref( CallBackData.pBuf, capframe.framesize ) ;
			}
			capframe.channel = cap->m_channel ;
			cap->onframe(&capframe);
		}
	}
	if( capframe.framedata != NULL ) {
		mem_free( capframe.framedata );
	}	
}

Ipstream_capture::Ipstream_capture( int channel) 
: capture(channel)
{
    config dvrconfig(dvrconfigfile);
    char cameraid[16] ;
    sprintf(cameraid, "camera%d", channel+1 );
    string IpUrl=dvrconfig.getvalue(cameraid, "stream_URL");

	RegisterIPStreamDataCallback(IPStreamReadCallBack,NULL,m_channel);
	SetIPAddress(IpUrl,m_channel);

}

void Ipstream_capture::start()
{
	if(m_enable && m_start==0){

		SetIPVideoResolution(m_channel,m_attr.Resolution);
		SetIPVideoFrameRate(m_channel,m_attr.FrameRate);
		SetIPVideoBitRate(m_channel,m_attr.Bitrate);
		SetIPVideoKeyframeInterval(m_channel,m_attr.key_interval);
		IPCameraEnable(m_channel);
		IPSystemStart(m_channel);
    
    	// read real resolution
    	m_header.video_width = 0 ; 			// do it on capture callback 
		
		// testing feature, route audio from other channels     
		if( m_audiochannel>=0 ) {
			eagle_startaudio( m_audiochannel );
		}

		m_start=1;
	}   
}

void Ipstream_capture::stop()
{
	if( m_start ) {
		IPSystemStop(m_channel);
		m_start=0;			// stopped
	}
}

int Ipstream_capture::vsignal() 
{
	return GetIPVideoSignal(m_channel); 
}	

void IpCamera_Init()
{
	InitIPSystem();	
	dvr_log("IP camera initialized.");	
}

void IpCamera_Uninit()
{
	FiniIPSystem();	
	dvr_log("IP camera done.");		
}

