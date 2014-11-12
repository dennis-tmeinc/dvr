
#include "dvr.h"

// OSD constants

#define _OSD_BASE		0x9000
#define	_OSD_YEAR4		_OSD_BASE+0
#define _OSD_YEAR2		_OSD_BASE+1
#define _OSD_MONTH3		_OSD_BASE+2
#define _OSD_MONTH2		_OSD_BASE+3
#define _OSD_DAY		_OSD_BASE+4
#define _OSD_WEEK3		_OSD_BASE+5
#define	_OSD_CWEEK1		_OSD_BASE+6
#define	_OSD_HOUR24		_OSD_BASE+7
#define	_OSD_HOUR12		_OSD_BASE+8
#define	_OSD_MINUTE		_OSD_BASE+9
#define _OSD_SECOND		_OSD_BASE+10
#define _OSD_DEGREESIGN '\xf8'

int cap_channels;
int cap_totalrate;
int cap_totalenabled;

capture ** cap_channel;

unsigned int g_ts_base;

// return true for ok
int dvr_getchannelsetup(int ch, struct DvrChannel_attr * pchannel,  int attrsize)
{
    if( attrsize == sizeof( struct DvrChannel_attr ) && ch>=0 && ch<cap_channels ) 
    {
        cap_channel[ch]->getattr( pchannel ) ;
        return 1 ;        
    }
    
    return 0 ;
}

// return true for ok. 
int dvr_setchannelsetup(int ch, struct DvrChannel_attr * pchannel, int attrsize)
{
    if( ch>=0 && ch<cap_channels &&
       attrsize == sizeof( struct DvrChannel_attr ) &&
       pchannel->structsize == attrsize ) 
    {
        cap_channel[ch]->setattr( pchannel ) ;
        cap_channel[ch]->restart();
        return 1 ;        
    }
    
    return 0 ;
}

capture::capture( int channel )
{
    m_type=CAP_TYPE_UNKNOWN;
    m_channel=channel ;
    m_motion=0;
    m_signal=1;			// assume signal is good at beginning
    m_oldsignal=1;
    m_enable=0 ;
    m_working=0 ;
    m_streambytes=0 ;
    m_remoteosd = 0 ;	// local OSD by default
    m_master=1;
    m_eventTriggerEnable=0;
    m_gforceCrashTriggerEnable=0;
    m_emk_send_enable=0;
    m_gforce_send_enable=0;       
    //add osd starts
//    mOsdHeadBuf=(OSDHeader*)osdData;
    mOsdHeadBuf.MagicNum[0]='T';
    mOsdHeadBuf.MagicNum[1]='X';
    mOsdHeadBuf.MagicNum[2]='T';
    mOsdHeadBuf.reserved[0]=0;
    mOsdHeadBuf.reserved[1]=0;
    mOsdHeadBuf.reserved[2]=0;
    mOsdHeadBuf.wOSDSize=0; 
    mOSDLineNum=0;
  //  gpsspeed=0;   
    mOSDChanged=0;
    mOSDChanged_Sub=0;
    mLastOSDTime=0;
    mOSDNeedUpdate=0;
    //add osd ends    
    loadconfig();
}

void capture::loadconfig()
{
    int i ;
    char section[20];
    char buf[20] ;
    int sensorosd ;
    string tmpstr;
    // loading osd bitmaps ;
    m_sensorosd=0 ;

    config dvrconfig(dvrconfigfile);
    
    m_attr.structsize = sizeof(m_attr);
    
    m_master = dvrconfig.getvalueint("system", "mastermode");
    
    sprintf(section, "camera%d", m_channel+1);
    m_attr.Enable = dvrconfig.getvalueint( section, "enable");
    tmpstr=dvrconfig.getvalue( section, "name");
    if( tmpstr.length()==0 )
        strcpy(m_attr.CameraName, section );
    else
        strcpy(m_attr.CameraName, tmpstr.getstring());
    
    m_attr.Resolution=dvrconfig.getvalueint( section, "resolution");
    m_attr.RecMode=dvrconfig.getvalueint( section, "recordmode");
    m_attr.PictureQuality=dvrconfig.getvalueint( section, "quality");
    m_attr.FrameRate=dvrconfig.getvalueint( section, "framerate");
    m_gforceCrashTriggerEnable=dvrconfig.getvalueint(section,"gforce_trigger_enable");
    
    m_emk_send_enable=dvrconfig.getvalueint(section,"emk_send_enable");
    m_gforce_send_enable=dvrconfig.getvalueint(section,"gforce_send_enable");    
  
    SecondStreamEn=dvrconfig.getvalueint( section, "second_enable");
    FrameRate_Second=dvrconfig.getvalueint( section, "framerate_second");  
    Resolution_Second=dvrconfig.getvalueint( section, "resolution_second");
    Liveview_stream=dvrconfig.getvalueint( section, "liveview_stream");
    SecondRecordingEn=dvrconfig.getvalueint( section, "secondrecording_enable");
    Bitrate_Second=dvrconfig.getvalueint( section, "bitrate_second"); 
    
    // triggering sensor and osd
    for(i=0; i<4; i++) {
        int trigger, sensorosd ;
        sprintf(buf, "trigger%d", i+1);
        trigger=dvrconfig.getvalueint( section, buf );
	if(i==0){
	   if(trigger)
	     m_eventTriggerEnable=1;
	}
        sprintf(buf, "sensorosd%d", i+1);
        sensorosd=dvrconfig.getvalueint( section, buf );
        if( trigger==0 && sensorosd==0 ) {
            m_attr.SensorEn[i]=0;
            m_attr.Sensor[i]=0;
            m_attr.SensorOSD[i]=0;
            continue;
        }
        else if( trigger>0 ){
            m_attr.SensorEn[i]=1;
            m_attr.Sensor[i]=trigger-1 ;
            m_attr.SensorOSD[i]=(sensorosd>0) ;
        }
        else {	// sensorosd>0 only
            m_attr.SensorEn[i]=0;
            m_attr.Sensor[i]=sensorosd-1 ;
            m_attr.SensorOSD[i]=1;
        }
    }
    
    m_attr.PreRecordTime=dvrconfig.getvalueint( section, "prerecordtime");
    m_attr.PostRecordTime=dvrconfig.getvalueint( section, "postrecordtime");
    
    // recording alarm
    m_attr.RecordAlarm=dvrconfig.getvalueint( section, "recordalarm");
    m_attr.RecordAlarmEn=dvrconfig.getvalueint( section, "recordalarmen");
    m_attr.RecordAlarmPat=dvrconfig.getvalueint( section, "recordalarmpattern");
    
    // video lost alarm
    m_attr.VideoLostAlarm=dvrconfig.getvalueint( section, "videolostalarm");
    m_attr.VideoLostAlarmEn=dvrconfig.getvalueint( section, "videolostalarmen");
    m_attr.VideoLostAlarmPat=dvrconfig.getvalueint( section, "videolostalarmpattern");
    
    // motion detection
    m_attr.MotionAlarm=dvrconfig.getvalueint( section, "motionalarm");
    m_attr.MotionAlarmEn=dvrconfig.getvalueint( section, "motionalarmen");
    m_attr.MotionAlarmPat=dvrconfig.getvalueint( section, "motionalarmpattern");
    m_attr.MotionSensitivity=dvrconfig.getvalueint( section, "motionsensitivity");
    
    // GPS
    m_attr.GPS_enable=dvrconfig.getvalueint( section, "gpsen");
    m_attr.ShowGPS=dvrconfig.getvalueint( section, "showgps");
    m_attr.GPSUnit=dvrconfig.getvalueint( section, "gpsunit");
    
    // bitrate
    m_attr.BitrateEn=dvrconfig.getvalueint( section, "bitrateen");
    m_attr.Bitrate=dvrconfig.getvalueint( section, "bitrate");
    m_attr.BitMode=dvrconfig.getvalueint( section, "bitratemode");
    
    // ptz address
    m_attr.ptz_addr=dvrconfig.getvalueint( section, "ptzaddr");

    // picture control
    m_attr.brightness=dvrconfig.getvalueint( section, "brightness");
    m_attr.contrast  =dvrconfig.getvalueint( section, "contrast");
    m_attr.saturation=dvrconfig.getvalueint( section, "saturation");
    m_attr.hue       =dvrconfig.getvalueint( section, "hue");

    m_attr.key_interval = dvrconfig.getvalueint( section, "key_interval" );
    
    m_attr.b_frames = dvrconfig.getvalueint( section, "b_frames" );
    m_attr.p_frames = dvrconfig.getvalueint( section, "p_frames" );
    
    m_attr.disableaudio = dvrconfig.getvalueint( section, "disableaudio" );    
    m_attr.showgpslocation=dvrconfig.getvalueint( section, "showgpslocation" );
    Tab102_enable = dvrconfig.getvalueint( "glog", "tab102b_enable");
    if(Tab102_enable)
       m_attr.ShowPeak=dvrconfig.getvalueint("glog","tab102b_showpeak");
    else
      m_attr.ShowPeak=0;
    
    // triggering sensor and osd
    for(i=0; i<32; i++) {
        sprintf(buf, "sensorosd%d", i+1);
        sensorosd=dvrconfig.getvalueint( section, buf );
        if( sensorosd>0 && sensorosd<32 ) {
            m_sensorosd |= (1<<(sensorosd-1));
        }
    }

       
    m_motionalarm = dvrconfig.getvalueint( section, "motionalarm" );
    m_motionalarm_pattern = dvrconfig.getvalueint( section, "motionalarmpattern" );
    m_signallostalarm = dvrconfig.getvalueint( section, "videolostalarm" );
    m_signallostalarm_pattern = dvrconfig.getvalueint( section, "videolostalarmpattern" );

    // reset some attr for special 2 version ()
    int videotype = dvrconfig.getvalueint( section, "videotype" );
    if( videotype>0 ) {
        if( videotype==1 ) {                        // best video
                m_attr.Resolution = 2 ;
                m_attr.FrameRate =30;
                m_attr.PictureQuality=6;
                m_attr.BitrateEn=1;
                m_attr.BitMode=0 ;
                m_attr.Bitrate=1000000;
                m_attr.key_interval=200 ;
        }
        else if( videotype==2 ) {                   // Good Video
                m_attr.Resolution = 2 ;
                m_attr.FrameRate =30;
                m_attr.PictureQuality=5;
                m_attr.BitrateEn=1;
                m_attr.BitMode=0 ;
                m_attr.Bitrate=768000;
                m_attr.key_interval=240 ;
        }
        else if( videotype==3 ) {                   // Best Picture
                m_attr.Resolution = 3 ;
                m_attr.FrameRate =12;
                m_attr.PictureQuality=8;
                m_attr.BitrateEn=1;
                m_attr.BitMode=0 ;
                m_attr.Bitrate=1000000;
                m_attr.key_interval=80 ;
        }
        else if( videotype==4 ) {                   // Better Picture
                m_attr.Resolution = 3 ;
                m_attr.FrameRate =15;
                m_attr.PictureQuality=6;
                m_attr.BitrateEn=1;
                m_attr.BitMode=0 ;
                m_attr.Bitrate=1000000;
                m_attr.key_interval=100 ;
        }
        else if( videotype==5 ) {                   // Good Picture
                m_attr.Resolution = 3 ;
                m_attr.FrameRate =20;
                m_attr.PictureQuality=4;
                m_attr.BitrateEn=1;
                m_attr.BitMode=0 ;
                m_attr.Bitrate=1000000;
                m_attr.key_interval=120 ;
        }
        else if( videotype==6 ) {                   // Long Storage Time
                m_attr.Resolution = 2 ;
                m_attr.FrameRate =15;
                m_attr.PictureQuality=5;
                m_attr.BitrateEn=1;
                m_attr.BitMode=0 ;
                m_attr.Bitrate=768000;
                m_attr.key_interval=180 ;
        }
        
        if( m_channel<2 ) {             // force first two channel to 768kbps
            if( m_attr.Bitrate>768000 ) {
                 m_attr.Bitrate=768000;
            }
        }
        
        if( dvrconfig.getvalueint( section, "cameratype" )==1 ) {                   // Exterial Camera? change resolution to 528x320
            if (m_attr.Resolution==3 ) {
                m_attr.Resolution=2 ;
            }
        }
    }
}

extern int screen_onframe( cap_frame * capframe );

void capture::onframe(cap_frame * pcapframe)
{

    if(pcapframe->frametype==FRAMETYPE_VIDEO||pcapframe->frametype == FRAMETYPE_KEYVIDEO){
	m_working=10;
    } 
    m_streambytes+=pcapframe->framesize ;               // for bitrate calculation
    if(pcapframe->frametype==FRAMETYPE_VIDEO_SUB||pcapframe->frametype == FRAMETYPE_KEYVIDEO_SUB){
         if(pcapframe->frametype==FRAMETYPE_VIDEO_SUB){
	    pcapframe->frametype=FRAMETYPE_VIDEO; 
	 } else {
	    pcapframe->frametype = FRAMETYPE_KEYVIDEO;
	 }
         if((Liveview_stream==LIVEVIEW_SUB)&&SecondStreamEn){
	    net_onframe(pcapframe);
	 }
	 
	 if(SecondRecordingEn){
	   // printf("second recording\n");
	    pcapframe->channel+=8; 
	    rec_onframe(pcapframe);  
	 }
	 
    } else if(pcapframe->frametype==FRAMETYPE_VIDEO||pcapframe->frametype == FRAMETYPE_KEYVIDEO){
         if((Liveview_stream==LIVEVIEW_MAIN)||!SecondStreamEn){
	    net_onframe(pcapframe);
	 }         
         rec_onframe(pcapframe); 
    } else if(pcapframe->frametype==FRAMETYPE_AUDIO){
	 net_onframe(pcapframe);
         rec_onframe(pcapframe);      
	 
	 if(SecondStreamEn&&SecondRecordingEn){
	    pcapframe->channel+=8; 
	    rec_onframe(pcapframe);  	     
	 }
    }
}

// periodic update procedure. (every 0.125 second)
void capture::update(int updosd)
{  
#if 0  
    if( updosd ) {
        // update OSD
        updateOSD();
    }
#else
    
    //static int mlastsecond=0;
    //int osdneed;
    //struct dvrtime newtime;
    //time_now(&newtime); 
    //if(newtime.second!=mlastsecond){
    //   osdneed=1;
    //   mlastsecond=newtime.second;
    //}
    
    if( updosd) {
        // update OSD
	//mOSDChanged=1;
	//dvr_log("channel:%d update OSD",m_channel);
	//isOSDChanged();
	mOSDNeedUpdate=1;
       // updateOSD1();
    } 
#endif
    if( m_oldsignal!=m_signal ) {
	if( !dio_poweroff() ) {				// don't record video lost event in standby mode. (Requested by Harrison)
	        dvr_log("Camera %d video signal %s.", m_channel, m_signal?"on":"lost");
	}
        m_oldsignal=m_signal ;
    }
  
}

#if 1
void capture::updateOSD1(){
    char * k ;
    int i, j ;
    int mLSize=0;
    int mOSDSize=0;
    char osdbuf[128] ;
    int  osdlines3_x = 160 ;
    int  osdlines3_y = 440 ;
    float  gps_speed=0.0;
    struct dvrtime newtime;
    time_now(&newtime);
    
   // printf("updateOSD1\n");
    mOSDLineNum=0;
    mOsdHeadBuf.wOSDSize=0;
////////////////////////////////////////
    mLSize=0;
    mOSDLine[mOSDLineNum].MagicNum[0]='s';
    mOSDLine[mOSDLineNum].MagicNum[1]='t';
    mOSDLine[mOSDLineNum].reserved[0]=0;
    mOSDLine[mOSDLineNum].reserved[1]=0;
    mOSDLine[mOSDLineNum].reserved[3]=0;
    mOSDLine[mOSDLineNum].reserved[4]=0;
    mOSDLine[mOSDLineNum].OSDText[0]=ALIGN_LEFT|ALIGN_TOP;
    mOSDLine[mOSDLineNum].OSDText[1]=8;
    mOSDLine[mOSDLineNum].OSDText[2]=4;
    mOSDLine[mOSDLineNum].OSDText[3]=0;
    mLSize=4;
    // Show Host Name
    k=g_hostname ;
    while( *k ) {
        mOSDLine[mOSDLineNum].OSDText[mLSize++] = * k++;
    }
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
    // Show Camera name
    k=m_attr.CameraName; //&cameraname[0];
     while( *k ) {
        mOSDLine[mOSDLineNum].OSDText[mLSize++] = * k++;
    }
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
    
    ///////////////////////////////////////////////////////////////
   // printf("GPS_enable:%d showgps:%d speed:%f\n",m_attr.GPS_enable,m_attr.ShowGPS,g_gpsspeed);
    if( m_attr.GPS_enable && m_attr.ShowGPS && g_gpsspeed>-0.1) {

        if( m_attr.GPSUnit ) {			// kmh or mph
	    gps_speed=g_gpsspeed * 1.852;
	    if(gps_speed<3.22)
	       gps_speed=0.0;
            sprintf( osdbuf, "%.1f km/h", gps_speed );
        }
        else {
	    gps_speed=g_gpsspeed * 1.150779;
	    if(gps_speed<2.0)
	      gps_speed=0.0;
            sprintf( osdbuf, "%.1f mph", gps_speed );
        }
        k=osdbuf ;
        while( *k ) {
            mOSDLine[mOSDLineNum].OSDText[mLSize++] = * k++;
        }
    }   
    ///////////////////////////////////////////////////////////////
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = '\n';
    mOSDLine[mOSDLineNum].wLineSize=mLSize;
    mOSDLineNum++;
    mOSDSize+=8+mLSize;
     //////////////////////////////////////////////////////
    //second OSD Line
    mOSDLine[mOSDLineNum].MagicNum[0]='s';
    mOSDLine[mOSDLineNum].MagicNum[1]='t';
    mOSDLine[mOSDLineNum].reserved[0]=0;
    mOSDLine[mOSDLineNum].reserved[1]=0;
    mOSDLine[mOSDLineNum].reserved[3]=0;
    mOSDLine[mOSDLineNum].reserved[4]=0;
    mOSDLine[mOSDLineNum].OSDText[0]=ALIGN_LEFT|ALIGN_TOP;
    mOSDLine[mOSDLineNum].OSDText[1]=8;
    mOSDLine[mOSDLineNum].OSDText[2]=28;
    mOSDLine[mOSDLineNum].OSDText[3]=0;
    mLSize=4;
    // date and time
    sprintf(osdbuf,
                "%02d/%02d/%04d %02d:%02d:%02d", 
                 newtime.month,
                 newtime.day,
                 newtime.year,
                 newtime.hour,
                 newtime.minute,
                 newtime.second
    );
    k=osdbuf ;
    while( *k ) {
       mOSDLine[mOSDLineNum].OSDText[mLSize++] = * k++;
    }
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
    mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
   
   // Show trigger name, if triggered
    for(j=0; j<num_sensors; j++) {
        if( m_sensorosd & (1<<j) ) {
            if( sensors[j]->value() ) {
                k=sensors[j]->name() ;
                while( *k ) {
                     mOSDLine[mOSDLineNum].OSDText[mLSize++] = * k++;
                }
                mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
                if( mLSize>120 ) {
                    break;
                }
            }
        }
    }

    mOSDLine[mOSDLineNum].OSDText[mLSize++] = '\n';
    mOSDLine[mOSDLineNum].wLineSize=mLSize;
    mOSDLineNum++;
    mOSDSize+=8+mLSize;
    ////////////////////////////////////////////////////////////////
    //third osd
   // printf("GPS_enable:%d showgpslocation:%d\n",m_attr.GPS_enable,m_attr.showgpslocation);
    if( m_attr.GPS_enable && m_attr.showgpslocation ) {
        double lati, longi ;
        if( gps_location (&lati, &longi) ) {	
            int east, north ;	  
	    mOSDLine[mOSDLineNum].MagicNum[0]='s';
	    mOSDLine[mOSDLineNum].MagicNum[1]='t';
	    mOSDLine[mOSDLineNum].reserved[0]=0;
	    mOSDLine[mOSDLineNum].reserved[1]=0;
	    mOSDLine[mOSDLineNum].reserved[3]=0;
	    mOSDLine[mOSDLineNum].reserved[4]=0;
	    mOSDLine[mOSDLineNum].OSDText[0]=ALIGN_LEFT|ALIGN_BOTTOM;
	    mOSDLine[mOSDLineNum].OSDText[1]=8 ;
	    mOSDLine[mOSDLineNum].OSDText[2]=8;
	    mOSDLine[mOSDLineNum].OSDText[3]=0;
	    mLSize=4;	  
	  
            if( lati>=0.0 ) {
                north='N' ;
            }
            else {
                lati=-lati ;
                north='S' ;
            }
            if( longi>=0.0 ) {
                east='E' ;
            }
            else {
                longi=-longi ;
                east='W' ;
            }

            int ladeg, lamin, lasecond ;
            int lodeg, lomin, losecond ;
            ladeg = (int) lati ;
            lati -= (double)ladeg ;
            lamin = (int)(lati*60) ;
            lati -= ((double)lamin)/60.0 ;
            lasecond = (int)(lati*3600) ;
            lodeg = (int) longi ;
            longi -= (double)lodeg ;
            lomin = (int)(longi*60) ;
            longi -= ((double)lomin)/60.0 ;
            losecond = (int)(longi*3600) ;
            
            sprintf( osdbuf, "%d\xba%02d\'%02d\"%c %d\xba%02d\'%02d\"%c", 
                    ladeg, lamin, lasecond, north,
                    lodeg, lomin, losecond, east );
            k=osdbuf ;
            while( *k ) {
                 mOSDLine[mOSDLineNum].OSDText[mLSize++] = * k++;
            }
            mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';

            mOSDLine[mOSDLineNum].OSDText[mLSize++] = '\n';
            mOSDLine[mOSDLineNum].wLineSize=mLSize;
            mOSDLineNum++;
            mOSDSize+=8+mLSize;
        }
    }
    //Forth Line
 
    //show G_FORCE 
    if(m_attr.ShowPeak &&isPeakChanged()){
        mLSize=0;
	mOSDLine[mOSDLineNum].MagicNum[0]='s';
	mOSDLine[mOSDLineNum].MagicNum[1]='t';
	mOSDLine[mOSDLineNum].reserved[0]=0;
	mOSDLine[mOSDLineNum].reserved[1]=0;
	mOSDLine[mOSDLineNum].reserved[3]=0;
	mOSDLine[mOSDLineNum].reserved[4]=0;
	mOSDLine[mOSDLineNum].OSDText[0]=ALIGN_LEFT|ALIGN_BOTTOM;
	mOSDLine[mOSDLineNum].OSDText[1]=8;
	mOSDLine[mOSDLineNum].OSDText[2]=32;
	mOSDLine[mOSDLineNum].OSDText[3]=0;
	mLSize=4;        
	//sprintf( osdbuf, "    F%5.2lf,R%5.2lf,D%5.2lf", g_fb, g_lr, g_ud);
	sprintf( osdbuf, "    %c%5.2lf,%c%5.2lf,%c%5.2lf",
		  g_fb>=0.0?'F':'B',
		  g_fb>=0.0?g_fb:-g_fb,
		  g_lr>=0.0?'R':'L',
		  g_lr>=0.0?g_lr:-g_lr, 
		  g_ud>=1.0?'D':'U',
		  g_ud>=1.0? (g_ud-1):(1-g_ud));
	k=osdbuf ;
	while( *k ) {
	  mOSDLine[mOSDLineNum].OSDText[mLSize++] = * k++;
	}       
        mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' ';
        mOSDLine[mOSDLineNum].OSDText[mLSize++] = ' '; 	
        mOSDLine[mOSDLineNum].OSDText[mLSize++] = '\n';
	mOSDLine[mOSDLineNum].wLineSize=mLSize;
	mOSDLineNum++;
	mOSDSize+=8+mLSize;             
    }  
    mOsdHeadBuf.wOSDSize=mOSDSize;
    return;  
}
#endif


// check alarm status relate to capture channel(every 0.125 second)
void capture::alarm()
{
    if( m_enable ) {
        // signal lost alarms
        if( getsignal()==0 &&
           m_signallostalarm_pattern > 0 && 
           m_signallostalarm>=0 && m_signallostalarm<num_alarms ) {
               alarms[m_signallostalarm]->setvalue(m_signallostalarm_pattern);
           }
        
        // motion alarms
        if( m_motion &&
           m_motionalarm_pattern > 0 && 
           m_motionalarm>=0 && m_motionalarm<num_alarms ) {
               alarms[m_motionalarm]->setvalue(m_motionalarm_pattern);
           }
    }
}

void cap_start()
{
    int i; 
   // dvr_log("cap_start");
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->start();
    }
    SystemStart();
#if 0    
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->updateOSD1 ();
    }
#endif    
   // dvr_log("cap_start done");
}

void cap_stop()
{
    int i; 
    for( i=0; i<cap_channels; i++) {
     //   printf("channel:%d stop\n",i);
        cap_channel[i]->stop();
	//printf("channel %d stopped\n",i);
    }
   // sleep(1);
  //  printf("start system stop\n");
    //sleep(20);
    SystemStop();
    
}

void cap_restart(int channel)
{
    if( channel>=0 && channel<cap_channels ) {
        cap_channel[channel]->restart();
    }
}

// initial capture card, return channel numbers
void cap_init()
{
    int i;
    int videostandard ;
    int dvrchannels ;
    int enabled_channels ;
    int totalrate=0;
    int cap_ch ;
    int ilocal=0 ;		// local camera idx
    enabled_channels = 0 ;
    
    config dvrconfig(dvrconfigfile);
    
    cap_ch = dvrconfig.getvalueint("system", "totalcamera");
    
    // initialize local capture card (eagle32)
    videostandard = dvrconfig.getvalueint("system", "videostandard");
    dvrchannels=eagle32_init();
    
    dvr_log("%d capture card (local) channels detected.", dvrchannels);
    if( cap_ch<=0 ) {
        cap_ch=dvrchannels ;
    }
    if( cap_ch> MAXCHANNEL ) {
        cap_ch=MAXCHANNEL ;
    }
    
    g_ts_base = time(NULL) - 11; /* used to make PTS */
    
    if( cap_ch > 0 ) {
        cap_channel = new capture * [cap_ch] ;
    }
    cap_totalrate=0;
    
    for (i = 0; i < cap_ch; i++ ) {
        char cameraid[16] ;
        int cameratype ;
	int enable;
	int secondenable;
	int secondrecordingenable;
	int bitrate;
	int secondbitrate;
        sprintf(cameraid, "camera%d", i+1 );
        cameratype = dvrconfig.getvalueint(cameraid, "type");	
	enable=dvrconfig.getvalueint( cameraid, "enable");
	secondenable=dvrconfig.getvalueint( cameraid, "second_enable");
	secondrecordingenable=dvrconfig.getvalueint( cameraid, "secondrecording_enable");
	
	bitrate=dvrconfig.getvalueint( cameraid, "bitrate");
	secondbitrate=dvrconfig.getvalueint( cameraid, "bitrate_second");
	if(enable){
	  totalrate+=bitrate; 
	  if(secondenable&&secondrecordingenable){
	     totalrate+=secondbitrate; 
	  }
	}
	
        if( cameratype == 0 ) {			// local capture card
            if( ilocal<dvrchannels ) {
                cap_channel[i]=new eagle_capture(i, ilocal++);
            }
            else {
                cap_channel[i]=new eagle_capture(i, -1);// dummy channel
            }
        }
        else if( cameratype == 1 ) {	// ip camera
            printf("ip camera:%d\n",i);
            cap_channel[i] = new ipeagle32_capture(i);
        }
        if( cap_channel[i]->enabled() ) {
            enabled_channels++ ;
        }
    }
    
    cap_channels = i ;
    cap_totalenabled=enabled_channels;
    cap_totalrate=(totalrate>>3)*0.8;
    if( enabled_channels<=0 ) {
        printf("No camera available, DVR QUIT!\n");
        exit(1);
    }
    dvr_log("Total %d cameras initialized.", enabled_channels);
}

// uninitial capture card
void cap_uninit()
{
    int i ;
    int cap_ch = cap_channels ;
    if( cap_channels > 0 ) {
        cap_channels=0 ;
        for (i = 0; i < cap_ch; i++) {
            delete cap_channel[i] ;
            cap_channel[i]=NULL ;
        }
        delete cap_channel ;
    }
    cap_channel = NULL ;
    cap_totalenabled=0;
    cap_totalrate=0;
    // un initialize local capture card
    eagle32_uninit ();
    dvr_log("Capture card uninitialized.");
}
