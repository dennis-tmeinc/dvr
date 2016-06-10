/* 
 * Local Analog Camera module
 */

#include "dvr.h"
#include "eagle8851/tme_sdk.h"

static int eagle_channels = 0 ;

// stream call back
static void StreamReadCallBack(CALLBACK_DATA CallBackData,void* context)
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
 
 /*        
    case FRAME_TYPE_SUBVIDEO_I:	
        capframe.frametype=FRAMETYPE_KEYVIDEO_SUB;
      break;
    case FRAME_TYPE_SUBVIDEO_P:
        capframe.frametype=FRAMETYPE_VIDEO_SUB;
      break;        
*/
 
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
			cap->m_enable && 
			cap->m_start )
		{
			if( capframe.frametype == FRAMETYPE_AUDIO ) {
				if( cap->m_audiochannel == CallBackData.channel ) {
					if( capframe.framedata == NULL ) {
						capframe.framedata = (char *) mem_addref( CallBackData.pBuf, capframe.framesize ) ;
					}
					capframe.channel = cap->m_channel ;
					cap->onframe(&capframe);
				}
			}
			else if( cap->m_phychannel == CallBackData.channel ) {
				if( cap->m_header.video_width == 0 ) {
					// get real resolution
					cap->m_header.video_width= GetVideoWidth(CallBackData.channel) ;
					cap->m_header.video_height= GetVideoHight(CallBackData.channel) ;
				}
				
				if( capframe.framedata == NULL ) {
					capframe.framedata = (char *) mem_addref( CallBackData.pBuf, capframe.framesize ) ;
				}
				capframe.channel = cap->m_channel ;
				cap->onframe(&capframe);
			}
		}
	}
	if( capframe.framedata != NULL ) {
		mem_free( capframe.framedata );
	}	
}

eagle_capture::eagle_capture( int channel ) 
: capture(channel)
{
    m_chantype=MAIN_CHANNEL ;
}

void eagle_capture::start()
{
    int res ;
    int brightness;
    int saturation;
    int contrast;

    if( m_enable && m_start==0 && m_phychannel>=0  && m_phychannel<eagle_channels ) {
        if( m_attr.disableaudio ) {
            res = SetStreamType(m_phychannel, m_chantype, STREAM_TYPE_VIDEO);
        }
        else {
            res = SetStreamType(m_phychannel, m_chantype,  STREAM_TYPE_AVSYN);
        }
        
        int QuantVal;
        if( m_attr.PictureQuality>10 ) {
            m_attr.PictureQuality = 10 ;
        }
        if( m_attr.PictureQuality<0 ) {
            m_attr.PictureQuality = 0 ;
        }
        QuantVal = 12+(10-m_attr.PictureQuality);
        res = SetDefaultQuant(m_phychannel, m_chantype, QuantVal)	; 
		// quality should be between 12 to 30

        m_attr.b_frames = 0;

		//  dvr_log("set IBP Mode");
        res= SetIBPMode(m_phychannel, m_chantype, m_attr.key_interval, m_attr.b_frames, m_attr.FrameRate);
        
        // setup bitrate control
        if( m_attr.BitrateEn ) {
            res = SetBitrateControl(m_phychannel, m_chantype, m_attr.Bitrate, ( m_attr.BitMode==0 )?MODE_VBR:MODE_CBR  );
        }
        else {
            res = SetBitrateControl(m_phychannel, m_chantype, 4000000, MODE_VBR  );
        }

        res = SetEncoderPictureFormat(m_phychannel, m_chantype, m_attr.Resolution);
 
        if( m_attr.MotionSensitivity>=0 ) {
            res = SetMotionDetection(m_phychannel,  m_attr.MotionSensitivity);
            res = EnalbeMotionDetection( m_phychannel, 1);
        }
        else {
            EnalbeMotionDetection( m_phychannel, 0 );
        }

        // set color contrl
#ifdef  ZEUS8

		SetSecondStreamEnable( m_phychannel, 0 );	
	
		SetVideoParam(m_phychannel, 
				m_attr.brightness * 25 , 				// 0 -- 255
				m_attr.contrast * 25,					// 0 -- 255
				m_attr.saturation*25,					// 0 -- 255
				  0);
#else
		SetVideoParam(m_phychannel, 
				(m_attr.brightness - 5) * 25 , 			// -128 --  +128
				(m_attr.contrast - 5 ) * 25,			// -128 --  +128
				m_attr.saturation*25,					// 0 -- 255
				  0);
                      
#endif
        
        StartCodec( m_phychannel, m_chantype );

		// read real resolution
    	m_header.video_width = 0 ; 			// do it on capture callback 
    	
        // route audio from other channel
        if( m_phychannel != m_audiochannel ) {
			eagle_startaudio( m_audiochannel ) ;
		}
        
        m_start = 1 ;		// started !
    }		
}

void eagle_capture::stop()
{
	if( m_start ) {
		StopCodec(m_phychannel, m_chantype);
		
		// audio from other channel
        if( m_phychannel != m_audiochannel ) {
			eagle_stopaudio( m_audiochannel ) ;
		}
		
		m_start = 0;
    }
}

int eagle_capture::vsignal()
{
	return GetVideoSignal(m_phychannel);
}

int eagle_capture::vmotion()
{
	return GetChannelMotionStatus(m_phychannel) ;
}

/****
  testing features here !!!!!!!!!!!!!!!
  route audio to another channel
*****/
  
// start audio channel only 
int eagle_startaudio( int phychannel )
{
	if( phychannel>=0 && phychannel<eagle_channels ) {
		// check if other channels are using the same physical channel?
		for( int ch=0; ch<cap_channels ; ch++ ) {
			capture * cap = cap_channel[ch] ;
			if( cap!=NULL && 
				cap->m_phychannel == phychannel ) {
					return 1;
			}
		}
		
		SetStreamType(phychannel, MAIN_CHANNEL, STREAM_TYPE_AVSYN);
		SetEncoderPictureFormat(phychannel, MAIN_CHANNEL, 0);
		SetIBPMode(phychannel, MAIN_CHANNEL, 200, 0, 2);
		SetBitrateControl(phychannel, MAIN_CHANNEL, 100000, MODE_VBR  );
        SetDefaultQuant(phychannel, MAIN_CHANNEL, 12);
        EnalbeMotionDetection( phychannel, 0 );
        return StartCodec( phychannel, MAIN_CHANNEL);	
	}
	return -1 ;	
}

int eagle_stopaudio( int phychannel )
{
	if( phychannel>=0 && phychannel<eagle_channels ) {
		
		// check if other channels are using the same physical channel?
		for( int ch=0; ch<cap_channels ; ch++ ) {
			capture * cap = cap_channel[ch] ;
			if( cap!=NULL && 
				cap->m_phychannel == phychannel ) {
					return 1;
			}
		}
				
		return StopCodec(phychannel, MAIN_CHANNEL);	
	}
	return -1 ;
}

void eagle_start()
{
    SystemStart();
}

void eagle_stop()
{
    SystemStop();
}

int eagle_init()
{
    if( eagle_channels>0 ) 
        return eagle_channels ;
    
    int i;
        
	i = InitSystem();
	if( i<0 ) {
		dvr_log("ZEUS board init failed!");
		return 0;
	}
    board_info binfo ;
    GetBoardInfo(&binfo);
    eagle_channels = binfo.enc_chan ;
    dvr_log("Total %d onboard channels detected.", eagle_channels);    
    
	for( i=0; i<eagle_channels; i++) {
	   RegisterStreamDataCallback(StreamReadCallBack, NULL,i);
	}

    return eagle_channels ;
}

void eagle_uninit()
{
    if( eagle_channels>0 ) {
		FiniSystem();
		eagle_channels=0;
	}
}

