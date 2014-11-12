
#include "dvr.h"

static Ipstream_capture * Ip_capture_array[MAXCHANNEL] ;

static void IPStreamReadCallBack(CALLBACK_DATA CallBackData,void* context)
{
  
    int channel=CallBackData.channel;
#if 1  
    if(Ip_capture_array[channel]){
       Ip_capture_array[channel]->streamcallback(CallBackData);
    }
#else
   printf("channel:%d type:%d size:%d\n",CallBackData.channel,CallBackData.frameType,CallBackData.size);
#endif    
}

Ipstream_capture::Ipstream_capture( int channel) 
: capture(channel)
{
    config dvrconfig(dvrconfigfile);
    char cameraid[16] ;
    m_channel=channel;    
    sprintf(cameraid, "camera%d", channel+1 );
    m_IpUrl=dvrconfig.getvalue(cameraid, "stream_URL");
    m_ipchannel=channel;
    gettimeofday(&starttime, NULL);
    //printf("m_chanel:%d URL:%s cameraid:%s\n",m_ipchannel,m_IpUrl.getstring(),cameraid);
    m_type=1;
    
    m_enable = m_attr.Enable ;

    mCapture_Mutex=mutex_init;
}

Ipstream_capture::~Ipstream_capture()
{
    stop();  
    pthread_mutex_destroy(&mCapture_Mutex);
       
}
void Ipstream_capture::setosd( struct hik_osd_type * posd )
{
  
}

void Ipstream_capture::setCameraURL(string url)
{
     m_IpUrl=strdup(url.getstring());
}

void Ipstream_capture::streamcallback(CALLBACK_DATA CallBackData)
{
    int xframetype = FRAMETYPE_UNKNOWN ;
    struct cap_frame capframe;
    struct cap_frame_header  mframe ;
    char          osdData[600];
    int           osdLength=0;    
    void * buf;
    int size;
    int frame_type;
    struct timeval tm;
    
    buf=CallBackData.pBuf;
    size=CallBackData.size;
    frame_type=CallBackData.frameType;

    switch(frame_type){
    case FRAME_TYPE_AUDIO:
        xframetype = FRAMETYPE_AUDIO ; 
        break;
    case FRAME_TYPE_VIDEO_P:
        xframetype = FRAMETYPE_VIDEO ;
        break;
    case FRMAE_TYPE_VIDEO_I:
        xframetype = FRAMETYPE_KEYVIDEO ;
        break;
	
    default:
        return;
    }    
 //   printf("==channel:%d type:%d size:%d\n",CallBackData.channel,CallBackData.frameType,CallBackData.size);      
  // pthread_mutex_lock(&mCapture_Mutex);
   if(xframetype==FRAMETYPE_AUDIO){
     
 	capframe.channel = m_channel ;           
	capframe.framesize = size;
	capframe.frametype = xframetype ;
	capframe.framedata = (char *) mem_alloc(capframe.framesize);
	if( capframe.framedata == NULL ) {
	      return;
	}
	capframe.timestamp=CallBackData.mPTS;    //CallBackData.t_sec*1000+CallBackData.t_msec/1000;	
	memcpy(capframe.framedata,buf,size);		    	             
   } else {
	osdLength=0;
	//add osd
	if(mOSDNeedUpdate){
	  updateOSD1();
	  mOSDChanged=1;
	  mOSDNeedUpdate=0;
	}
	if(!mOSDChanged){
	    gettimeofday(&tm,NULL);      
	    if(tm.tv_sec!=mLastOSDTime){
		updateOSD1();
		mLastOSDTime=tm.tv_sec;
		mOSDChanged=1;
	    }
	}
	if(xframetype==FRAMETYPE_KEYVIDEO){		  
	  //copy the osd to
	    int i;
	    int bufNum=0; 
	    int size;
	    //copy osd header to framebuf
	    memcpy((void*)&osdData[bufNum],(void*)&mOsdHeadBuf,sizeof(mOsdHeadBuf));
	    bufNum+=sizeof(mOsdHeadBuf);
	    if(mOsdHeadBuf.wOSDSize>0){
		for(i=0;i<mOSDLineNum;++i){
		    size=mOSDLine[i].wLineSize+8;
		    memcpy((void*)&osdData[bufNum],(void*)&mOSDLine[i],size);
		    bufNum+=size;
		}//the end of for
	    }
	    osdLength=bufNum;
	    mOSDChanged=0;
	    //dvr_log("channel:%d osd added, size:%d",m_channel,bufNum);
	}  else if(mOSDChanged){
	//copy the osd to
	    int i;
	    int bufNum=0; 
	    int size;
	    //copy osd header to framebuf
	    memcpy((void*)&osdData[bufNum],(void*)&mOsdHeadBuf,sizeof(mOsdHeadBuf));
	    bufNum+=sizeof(mOsdHeadBuf);
	    if(mOsdHeadBuf.wOSDSize>0){
		for(i=0;i<mOSDLineNum;++i){
		    size=mOSDLine[i].wLineSize+8;
		    memcpy((void*)&osdData[bufNum],(void*)&mOSDLine[i],size);
		    bufNum+=size;
		}//the end of for
	    }
	    osdLength=bufNum;
	    mOSDChanged=0;
	    //dvr_log("channel:%d osd added, size:%d",m_channel,capframe.osdsize);            
	}     	
	capframe.channel = m_channel ;     
      
	capframe.framesize = size+osdLength;
	capframe.frametype = xframetype ;
	capframe.framedata = (char *) mem_alloc(capframe.framesize);
	if( capframe.framedata == NULL ) {
	  //  printf("no buffer for capframe\n");
	    if(xframetype == FRAMETYPE_KEYVIDEO){
		int i=0;
		for(i=0;i<5;++i){
		  usleep(100);
		  capframe.framedata = (char *) mem_alloc( capframe.framesize);
		  if(capframe.framedata)
		      break;
		}
	    }
	    if(capframe.framedata == NULL){      
	      return;
	    }
	}	
	capframe.timestamp= CallBackData.mPTS;      //CallBackData.t_sec*1000+CallBackData.t_msec/1000;
	
	if(osdLength>0){
	    memcpy((void*)capframe.framedata,(void*)&osdData[0],osdLength);
	    mem_cpy((void*)(capframe.framedata+osdLength), buf, size);
	 //   memcpy((void*)(capframe.framedata+osdLength), buf, size);	    
	} else {
	   // memcpy(capframe.framedata, buf, size); 
	    mem_cpy(capframe.framedata,buf,size);		    	   
	}  
   }
    onframe(&capframe);
    mem_free(capframe.framedata);
 //   printf("on frame done\n");

}

void Ipstream_capture::start()
{
  // printf("Ip stream Start:%d\n",m_channel);
   if(m_enable){
    // printf("URL:%s channel:%d\n",m_IpUrl.getstring(),m_ipchannel);  
 //   SetStreamURL(m_IpUrl.getstring(),m_ipchannel);
    SetIPVideoResolution(m_ipchannel,m_attr.Resolution);
    SetIPVideoFrameRate(m_ipchannel,m_attr.FrameRate);
    SetIPVideoBitRate(m_ipchannel,m_attr.Bitrate);
    SetIPVideoKeyframeInterval(m_ipchannel,m_attr.FrameRate);
    SetIPAddress(m_IpUrl.getstring(),m_ipchannel);
    IPCameraEnable(m_ipchannel);
    IPSystemStart(m_ipchannel);
    Ip_capture_array[m_ipchannel]=this;       
  }   
}

void Ipstream_capture::stop()
{
    int res ;
    Ip_capture_array[m_ipchannel]=NULL;
    if(m_enable){
      IPSystemStop(m_ipchannel);
    }
}


void Ipstream_capture::update(int updosd)
{
    capture::update( updosd );
}

int Ipstream_capture::getsignal()
{
    struct timeval curtime;
    gettimeofday(&curtime, NULL);
    m_signal=GetIPVideoSignal(m_channel); 
#if 0  
    if(!m_signal){
       if(curtime.tv_sec-starttime.tv_sec>60){
	  stop();
	  start();
	  starttime.tv_sec=curtime.tv_sec;
	  dvr_log("ip camera reconnected again");
       }
    } else {
      starttime.tv_sec=curtime.tv_sec;
    }
#endif    
    return m_signal ;
}

void IpCamera_Init()
{
    int i;
    InitIPSystem();
    for(i=0;i<MAXCHANNEL;++i){
      Ip_capture_array[i]=NULL;
      RegisterIPStreamDataCallback(IPStreamReadCallBack,NULL,i);
    }
}

void IpCamera_Uninit()
{
    int i; 
    FiniIPSystem();
    for(i=0;i<MAXCHANNEL;++i)
       Ip_capture_array[i]=NULL;    
}

