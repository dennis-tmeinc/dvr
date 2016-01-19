
#include "dvr.h"

int    eagle32_channels = 0 ;
static WORD eagle32_tsadjust = 0 ;
#define MAX_EAGLE_CHANNEL   (8)
#define SAMPLE_RATE 8000

static eagle_capture * eagle_capture_array[MAX_EAGLE_CHANNEL] ;


//#define DIRECT_SAVE
eagle_capture::eagle_capture( int channel, int hikchannel ) 
: capture(channel)
{
    m_enable = 0 ;
    m_type=CAP_TYPE_HIKLOCAL;
    
    // hik eagle32 parameters
    // m_hikhandle=hikchannel+1 ;	// handle = channel+1
    m_hikhandle=hikchannel ;
    m_chantype=MAIN_CHANNEL ;
    m_dspdatecounter=0 ;
    
    m_motion=0 ;
    m_motionupd = 1 ;
    
    m_state = 0 ;
    
    if( hikchannel<0 ) {
        return ;
    }
    m_enable = m_attr.Enable ;
#ifdef DIRECT_SAVE    
    fpVideo=NULL;
#endif  
    gettimeofday(&starttime, NULL);
    signalchecked=0;
    bytes_since_audioref=0;
    keyframedroped=0;
    mCapture_Mutex=mutex_init;
    timerclear(&lastvideo);
}

eagle_capture::~eagle_capture()
{
    stop();  
    pthread_mutex_destroy(&mCapture_Mutex);
    
#ifdef DIRECT_SAVE      
    if(fpVideo){
      fclose(fpVideo);
    }
    fpVideo=NULL; 
#endif    
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
/*
static void StreamReadCallBack(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}*/

static void StreamReadCallBack0(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

static void StreamReadCallBack1(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

static void StreamReadCallBack2(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

static void StreamReadCallBack3(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

static void StreamReadCallBack4(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

static void StreamReadCallBack5(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

static void StreamReadCallBack6(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

static void StreamReadCallBack7(CALLBACK_DATA CallBackData,void* context)
{
    int handle;
    handle=CallBackData.channel;
   // if(handle==0)
    //printf("type:%d size=%d channel:%d\n",CallBackData.frameType,CallBackData.size,handle);
    if( eagle_capture_array[handle] ) {
        eagle_capture_array[handle]->streamcallback(CallBackData);
    }
}

void eagle_capture::streamcallback(CALLBACK_DATA CallBackData)
{
    int xframetype = FRAMETYPE_UNKNOWN ;
    struct cap_frame capframe;
    struct cap_frame_header  mframe ;
    char          osdData[600];
    int           osdLength=0; 
    char          gpsData[256];
    char          gpsLength=0;
    struct gps    mGPS;
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
    case FRAME_TYPE_VIDEO_I:
        xframetype = FRAMETYPE_KEYVIDEO ;
        if( m_motion>0 ) {
            if( --m_motion==0 ) {
                m_motionupd=1 ;
            }
        }
        break;
    default:
        return;
    }
#if 1
    if(xframetype != FRAME_TYPE_AUDIO){
	if(GetChannelMotionStatus(m_hikhandle)){
	       // printf("motion\n");
		if( m_motion==0 ) {
		    m_motionupd = 1 ;
		}
		m_motion=2 ;      
	}
    } 
#endif    
   
#if 0
    if(m_channel==0){
        static struct timeval tm;
        gettimeofday(&tm, NULL);
	printf("sec:%d usec:%d\n",tm.tv_sec,tm.tv_usec);
	if(xframetype == FRAMETYPE_KEYVIDEO)
	{ 
	  printf("type=key size=%d sec:%d msec:%d\n",size,CallBackData.t_sec,CallBackData.t_msec);
	} else if(xframetype==FRAMETYPE_VIDEO)
	{
	  printf("type=video size=%d sec:%d msec:%d\n",size,CallBackData.t_sec,CallBackData.t_msec); 
	} else if(xframetype==FRAMETYPE_AUDIO){
	  printf("type=audio size=%d sec:%d msec:%d\n",size,CallBackData.t_sec,CallBackData.t_msec); 
	}
    }
   return;
#endif    

#ifdef DIRECT_SAVE 
    if(fpVideo){
	  fwrite(buf,1,size,fpVideo);
	  if(ftell(fpVideo)>200*1024*1024){
	    fclose(fpVideo);
	    fpVideo=NULL;
	  }	  
    }  
    if(!fpVideo){
       if (rec_basedir.length() <= 0) {	
            return;
        }    
        char newfilename[512];
	dvrtime mTime;
	time_now(&mTime); 
	int l;
       // make date directory
        sprintf(newfilename, "%s/%04d%02d%02d", 
                rec_basedir.getstring(),
                mTime.year,
                mTime.month,
                mTime.day );
        mkdir(newfilename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
        
        // make channel directory
        l = strlen(newfilename);
        sprintf(newfilename + l, "/CH%02d", m_channel);
        mkdir(newfilename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
        
        // make new file name
        l = strlen(newfilename);
        sprintf(newfilename + l,
                "/CH%02d_%04d%02d%02d%02d%02d%02d.264", 
                m_channel,
                mTime.year,
                mTime.month,
                mTime.day,
                mTime.hour,
                mTime.minute,
                mTime.second);	
       fpVideo=fopen(newfilename,"wb");
    }    
   return;
#endif  
#if 0
   if(xframetype==FRAMETYPE_VIDEO)
   { 
      if(keyframedroped)
	return;
   }
#endif   
  // pthread_mutex_lock(&mCapture_Mutex);
   if(xframetype==FRAMETYPE_AUDIO){
     
 	capframe.channel = m_channel ;           
	capframe.framesize = size;
	capframe.frametype = xframetype ;
	capframe.framedata = (char *) mem_alloc(capframe.framesize);
	if( capframe.framedata == NULL ) {
	      return;
	}
	capframe.timestamp=CallBackData.mPTS;     //CallBackData.t_sec*1000+CallBackData.t_msec/1000;	
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
	    //add the gps data if gps data is available
	    get_gps_data(&mGPS);
	    if(mGPS.gps_valid){
	        dvrtime mTime;
	        time_now(&mTime); 
		//dvr_log("lat:%f long:%f speed:%f direction:%f",mGPS.gps_latitude,
		//  mGPS.gps_longitud,mGPS.gps_speed,mGPS.gps_direction
		//);
	
		//gps Tag
		gpsData[0]='G';
		gpsData[1]='P';
		gpsData[2]='S';
		gpsData[3]='\0';
		//reserverd 2 bytes
		gpsData[4]='\0';
		gpsData[5]='\0';
			
		//GPS length;
		char* pGps=&gpsData[8];
		sprintf(pGps,"01,%09.6f%c%010.6f%c%.1fD%06.2f",
			      mGPS.gps_latitude<0.0000000 ? -mGPS.gps_latitude : mGPS.gps_latitude ,
			      mGPS.gps_latitude<0.0000000 ? 'S' : 'N' ,
			      mGPS.gps_longitud<0.0000000 ? -mGPS.gps_longitud: mGPS.gps_longitud,
			      mGPS.gps_longitud<0.0000000 ? 'W' : 'E' ,
			      (float)(mGPS.gps_speed * 1.852),      // in km/h.
			      (float)mGPS.gps_direction 		  
		);

		int tempLength=strlen(pGps);	
		pGps[tempLength]='\0';
		pGps[tempLength+1]='\0';
		pGps[tempLength+2]='\0';
		pGps[tempLength+3]='\0';
		tempLength=((tempLength+3)>>2)<<2;
		//dvr_log("%s",pGps);	
		//dvr_log("gps length:%d",tempLength);					
		//gpsData[6]=(tempLength>>8)&0xff;
		//gpsData[7]=tempLength;
		unsigned short *mlength=(unsigned short*)&gpsData[6];
		*mlength=tempLength;
		gpsLength=tempLength+8;	
		//tempLength=(int)gpsData[6]<<8|gpsData[7];
		//dvr_log("gps length1:%d",*mlength);			
	    }
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
      
	capframe.framesize = size+osdLength+gpsLength;
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
#if 0	      
	      if(xframetype == FRAMETYPE_KEYVIDEO){
		keyframedroped=1;
	      }
#endif	      
	      return;
	    }
	}
#if 0	
	if(xframetype == FRAMETYPE_KEYVIDEO){
	    keyframedroped=0;
	}
#endif	
	capframe.timestamp=CallBackData.mPTS;       //CallBackData.t_sec*1000+CallBackData.t_msec/1000;
	
	if(gpsLength>0){
	    memcpy((void*)capframe.framedata,(void*)&gpsData,gpsLength);
	}
	if(osdLength>0){
	    memcpy((void*)(capframe.framedata+gpsLength),(void*)&osdData[0],osdLength);
	    mem_cpy((void*)(capframe.framedata+gpsLength+osdLength), buf, size);
	 //   memcpy((void*)(capframe.framedata+osdLength), buf, size);	    
	} else {
	   // memcpy(capframe.framedata, buf, size); 
	    mem_cpy(capframe.framedata+gpsLength,buf,size);		    	   
	}  
   }
  // pthread_mutex_unlock(&mCapture_Mutex);
#if 0
    if(fpVideo){
          //fwrite(buf,1,size,fpVideo);
	  fwrite(capframe.framedata,1,capframe.framesize,fpVideo);
	  if(ftell(fpVideo)>200*1024*1024){
	    fclose(fpVideo);
	    fpVideo=NULL;
	  }	  
    }  
    if(!fpVideo){
       if (rec_basedir.length() <= 0) {	
	    mem_free(capframe.framedata);  
            return;
        }    
        char newfilename[512];
	dvrtime mTime;
	time_now(&mTime); 
	int l;
       // make date directory
        sprintf(newfilename, "%s/%04d%02d%02d", 
                rec_basedir.getstring(),
                mTime.year,
                mTime.month,
                mTime.day );
        mkdir(newfilename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
        
        // make channel directory
        l = strlen(newfilename);
        sprintf(newfilename + l, "/CH%02d", m_channel);
        mkdir(newfilename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
        
        // make new file name
        l = strlen(newfilename);
        sprintf(newfilename + l,
                "/CH%02d_%04d%02d%02d%02d%02d%02d.264", 
                m_channel,
                mTime.year,
                mTime.month,
                mTime.day,
                mTime.hour,
                mTime.minute,
                mTime.second);	
       fpVideo=fopen(newfilename,"wb");
    }
   mem_free(capframe.framedata);   
   return;
#endif    
    
//  memcpy( capframe.framedata, buf, size ) ;
    // send frame
  //  printf("on frame\n");
    onframe(&capframe);
    mem_free(capframe.framedata);
 //   printf("on frame done\n");

}

void eagle_capture::start()
{
    int res ;
//    stop();
    int brightness;
    int saturation;
    int contrast;
    if( m_enable ) {
        if( m_attr.disableaudio ) {
            res = SetStreamType(m_hikhandle, m_chantype, STREAM_TYPE_VIDEO);
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
        res = SetDefaultQuant(m_hikhandle, m_chantype, QuantVal)	; 
       // quality should be between 12 to 30

        m_attr.b_frames = 0;

		//  dvr_log("set IBP Mode");
        res= SetIBPMode(m_hikhandle, m_chantype, m_attr.key_interval, m_attr.b_frames, m_attr.FrameRate);
        
	//dvr_log("set Bitrate Control");
        // setup bitrate control
        if( m_attr.BitrateEn ) {
	   // dvr_log("set Bitrate Control");
            res = SetBitrateControl(m_hikhandle, m_chantype, m_attr.Bitrate, ( m_attr.BitMode==0 )?MODE_VBR:MODE_CBR  );
        }
        else {
            res = SetBitrateControl(m_hikhandle, m_chantype, 4000000, MODE_VBR  );
        }

#if 0
        const static picture_format fmt[5] = {
            ENC_CIF,
            ENC_2CIF,
            ENC_DCIF,
            ENC_4CIF,
            ENC_QCIF
        } ;
#endif
#if 0
       const static picture_format fmt[3] = {
            ENC_CIF,
            ENC_D1,
            ENC_HALF_D1,
        } ;
#endif
        if( m_attr.Resolution < 0 ) {
            m_attr.Resolution = 0 ;
        }
        else if( m_attr.Resolution>4 ) {
            m_attr.Resolution = 3 ;
        }
    //    dvr_log("set picture resolution");
        res = SetEncoderPictureFormat(m_hikhandle, m_chantype, m_attr.Resolution);
 
#if 0	
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
#endif        
#if 1
        if( m_attr.MotionSensitivity>=0 ) {
            res = SetMotionDetection(m_hikhandle,  m_attr.MotionSensitivity);
            res = EnalbeMotionDetection( m_hikhandle, 1);
        }
        else {
            EnalbeMotionDetection( m_hikhandle, 0 );
        }
#endif
        // set color contrl
//	dvr_log("SetVideoParam");
#if 0
        SetVideoParam(m_hikhandle, 
                      128-80+m_attr.brightness*16, 
                      128-80+m_attr.contrast*16,
                      128-120+m_attr.saturation*24,
                      128-20+m_attr.hue*4 );
#else 

        switch(m_attr.brightness)
	{
	  case 0:
	    brightness = -125;
	    break;
	  case 1:
	    brightness = -100;
	    break;
	  case 2:
	    brightness = -75;
	    break;
	  case 3:
	    brightness = -50;
	    break;
	  case 4:
	    brightness = -25;
	    break;
	  case 5:
	    brightness = 0;
	    break;
	  case 6:
	    brightness = 25;
	    break;
	  case 7:
	    brightness = 50;
	    break;
	  case 8:
	    brightness = 75;
	    break;
	  case 9:
	    brightness = 100;
	    break;
	  case 10:
	    brightness = 125;
	    break;
	  default:
	    brightness = 0;
	    break;	  
	}
	switch(m_attr.contrast)
	{
	  case 0:
	    contrast = -125;
	    break;
	  case 1:
	    contrast = -100;
	    break;
	  case 2:
	    contrast = -75;
	    break;
	  case 3:
	    contrast = -50;
	    break;
	  case 4:
	    contrast = -25;
	    break;
	  case 5:
	    contrast = 0;
	    break;
	  case 6:
	    contrast = 25;
	    break;
	  case 7:
	    contrast = 50;
	    break;
	  case 8:
	    contrast = 75;
	    break;
	  case 9:
	    contrast = 100;
	    break;
	  case 10:
	    contrast = 125;
	    break;
	  default:
	    contrast = 0;
	    break;	  
	}
	
	switch(m_attr.saturation)
	{
	case 0:
	    saturation = 0;
	    break;
	  case 1:
	    saturation = 25;
	    break;
	  case 2:
	    saturation = 50;
	    break;
	  case 3:
	    saturation = 75;
	    break;
	  case 4:
	    saturation = 100;
	    break;
	  case 5:
	    saturation = 128;
	    break;
	  case 6:
	    saturation = 150;
	    break;
	  case 7:
	    saturation = 175;
	    break;
	  case 8:
	    saturation = 200;
	    break;
	  case 9:
	    saturation = 225;
	    break;
	  case 10:
	    saturation = 250;
	    break;
	  default:
	    saturation = 128;
	    break;	  	  
	}
	
        SetVideoParam(m_hikhandle, 
                      brightness, 
                      contrast,
                      saturation,
                      0);
#endif      
#if 0
        struct SYSTEMTIME nowt ;
        struct dvrtime dvrt ;
        time_now ( &dvrt );
        memset( &nowt, 0, sizeof(nowt));
        nowt.year = dvrt.year ;
        nowt.month = dvrt.month ;
        nowt.day = dvrt.day ;
        nowt.hour = dvrt.hour ;
        nowt.minute = dvrt.minute ;
        nowt.second = dvrt.second ;
        nowt.milliseconds = dvrt.milliseconds ;
        
    //    SetDSPDateTime(m_hikhandle, &nowt);
#endif    
        eagle32_tsadjust=0 ;
        
        // start encoder
        eagle_capture_array[m_hikhandle]=this ;
	//dvr_log("start codec:%d",m_hikhandle);
        StartCodec( m_hikhandle, m_chantype );

        // Update OSD
    //    updateOSD();
    }		
}

void eagle_capture::stop()
{
    int res ;
  //  res=EnalbeMotionDetection(m_hikhandle, 0);	// disable motion detection
    if( m_enable ){
      //  printf("stop channel:%d",m_hikhandle);
	res=StopCodec(m_hikhandle, m_chantype);
	//printf("StopCodec return %d\n",res);
	eagle_capture_array[m_hikhandle]=NULL ;
    }
}

void eagle_capture::setosd( struct hik_osd_type * posd )
{
    //	EnableOSD(m_hikhandle, 0);		// disable OSD
    int i;
    WORD * osdformat[8] ;
    for( i=0; i<8; i++) {
        osdformat[i]=&(posd->osdline[i][0]) ;
    }
#if 1    
    SetOSDDisplayMode(m_hikhandle, 
                      posd->brightness,
                      posd->translucent,
                      posd->twinkle,
                      posd->lines,
                      osdformat );
    EnableOSD(m_hikhandle, 1);		// enable OSD
#endif    
}

void eagle_capture::update(int updosd)
{
    if( ++m_dspdatecounter>500 ) {
        eagle32_tsadjust=0 ;
        m_dspdatecounter=0 ;
    }
    if( m_motionupd ) {
        m_motionupd = 0 ;
        updosd=1;
    }
    capture::update( updosd );
}

int eagle_capture::getsignal()
{
    int res=1;
    struct timeval time1;
    if(signalchecked){
      if(m_enable){
         m_signal=GetVideoSignal(m_hikhandle);
      } 
    }else {
       gettimeofday(&time1, NULL);
       int diff=time1.tv_sec-starttime.tv_sec;
       if(diff>5){
	  if(m_enable){
	    m_signal=GetVideoSignal(m_hikhandle);
	  }
	  signalchecked=1;
       } else {
	   m_signal=1;  
       }
    }
    return m_signal ;
}

int eagle32_init()
{
    if( eagle32_channels>0 ) 
        return eagle32_channels ;
    
    int res ;
    int i;
    board_info binfo ;
    static int sysinit=0 ;
    eagle32_channels=0 ;    
    if( sysinit==0 ) {
       // dvr_log("start init system");
        res = InitSystem();
        if( res<0 ) {
            printf("Board init failed!\n");
            return 0;
        }
        sysinit=1 ;
    }    
  //  dvr_log("get board info");
    res = GetBoardInfo(&binfo);
    eagle32_channels = binfo.enc_chan ;
    
    if( eagle32_channels<=0 || eagle32_channels>MAX_EAGLE_CHANNEL )
        eagle32_channels=0 ;
    if( eagle32_channels > 0 ) {
        for( i=0; i<eagle32_channels; i++) {
            eagle_capture_array[i]=NULL ;
        }
        res=RegisterStreamDataCallback(StreamReadCallBack0,NULL,0);
        res=RegisterStreamDataCallback(StreamReadCallBack1,NULL,1);
        res=RegisterStreamDataCallback(StreamReadCallBack2,NULL,2);
        res=RegisterStreamDataCallback(StreamReadCallBack3,NULL,3);
        res=RegisterStreamDataCallback(StreamReadCallBack4,NULL,4);
        res=RegisterStreamDataCallback(StreamReadCallBack5,NULL,5);
        res=RegisterStreamDataCallback(StreamReadCallBack6,NULL,6);
        res=RegisterStreamDataCallback(StreamReadCallBack7,NULL,7);	
    }
    
    // reset time stamp adjustment
    eagle32_tsadjust = 0 ;
    return eagle32_channels ;
}

void eagle32_uninit()
{
    int i ;
    for( i=0; i<MAX_EAGLE_CHANNEL; i++) {
        eagle_capture_array[i]=NULL ;
    }
#if 0
    if( eagle32_channels>0 ) {
        eagle32_channels=0 ;
      	FiniSystem();
    }
#else
//    SystemStop();
#endif    
}

