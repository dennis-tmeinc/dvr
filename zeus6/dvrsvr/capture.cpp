
#include <sys/types.h>

#include "dvr.h"
   
#ifdef  ZEUS8

static char Dvr264Header[40] = 
{
    '\x46', '\x45', '\x4d', '\x54', '\x01', '\x00', '\x04', '\x00',
    '\x01', '\x00', '\x01', '\x00', '\x02', '\x00', '\x01', '\x08',
    '\x40', '\x1f', '\x00', '\x00', '\x00', '\xfa', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};

#else

static char Dvr264Header[40] = 
{
    '\x46', '\x45', '\x4d', '\x54', '\x01', '\x00', '\x01', '\x00',
    '\x01', '\x00', '\x01', '\x00', '\x01', '\x00', '\x01', '\x10',
    '\x40', '\x1f', '\x00', '\x00', '\x80', '\x3e', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};

#endif
   
int cap_channels;
capture ** cap_channel;
    
// standard resolution table
struct res_table_t {
	int w, h ;
} ;

#define DEF_WIDTH	(720)
#define DEF_HEIGHT	(480)

static res_table_t restable[] = {
	{360,240},
	{720,240},
	{528,320},
	{720,480},
	{176,120},
	{1280,720},
	{1920,1080}
};

#define RESTABLE_SIZE (sizeof(restable)/sizeof(restable[0]))

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
    m_working=0 ;
    m_streambytes=0 ;
    m_master=1;
    
    // physical channel, -1: not used
    m_phychannel = -1 ;		// not yet enabled
	m_audiochannel = -1 ;
    
    loadconfig();
    
    // initial default file header
    memcpy( &m_header, Dvr264Header, 40 );
	if(m_attr.Resolution >= 0 && m_attr.Resolution < RESTABLE_SIZE ) {
		m_header.video_width=restable[m_attr.Resolution].w;
		m_header.video_height=restable[m_attr.Resolution].h ;
	}
	else {
		m_header.video_width=DEF_WIDTH;
		m_header.video_height=DEF_HEIGHT;
	}
	m_header.video_framerate = m_attr.FrameRate ;
    
	m_enable = m_attr.Enable ;
	m_start = 0 ;			// not started
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
    
    // triggering sensor and osd
    for(i=0; i<4; i++) {
        int trigger, sensorosd ;
        sprintf(buf, "trigger%d", i+1);
        trigger=dvrconfig.getvalueint( section, buf );
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
    if( m_attr.key_interval < 5  || m_attr.key_interval > 500 ) {
		m_attr.key_interval = 3*m_attr.FrameRate  ;			// default 3 seconds
	}
    
    m_attr.b_frames = dvrconfig.getvalueint( section, "b_frames" );
    m_attr.p_frames = dvrconfig.getvalueint( section, "p_frames" );
    
    m_attr.disableaudio = dvrconfig.getvalueint( section, "disableaudio" );    
    m_attr.showgpslocation=dvrconfig.getvalueint( section, "showgpslocation" );
    m_attr.Tab102_enable = dvrconfig.getvalueint( "glog", "tab102b_enable");
    if(m_attr.Tab102_enable)
       m_attr.ShowPeak=dvrconfig.getvalueint("glog","tab102b_showpeak");
    else
      m_attr.ShowPeak=0;
    
    // triggering sensor and osd
    for(i=0; i<32; i++) {
        sprintf(buf, "sensorosd%d", i+1);
        sensorosd=dvrconfig.getvalueint( section, buf );
        if( sensorosd>0 && sensorosd<32 ) {
            m_sensorosd |= 1<<i;
        }
    }
       
    m_motionalarm = dvrconfig.getvalueint( section, "motionalarm" );
    m_motionalarm_pattern = dvrconfig.getvalueint( section, "motionalarmpattern" );
    m_signallostalarm = dvrconfig.getvalueint( section, "videolostalarm" );
    m_signallostalarm_pattern = dvrconfig.getvalueint( section, "videolostalarmpattern" );
    
#ifdef TVS_APP
   m_show_medallion=dvrconfig.getvalueint( section, "show_medallion" );
   m_show_licenseplate=dvrconfig.getvalueint( section, "show_licenseplate" );
   m_show_ivcs=dvrconfig.getvalueint( section, "show_ivcs" );
   m_show_cameraserial=dvrconfig.getvalueint( section, "show_cameraserial" );
#endif

#ifdef PWII_APP
    m_show_vri=dvrconfig.getvalueint( section, "show_vri" );
    m_show_policeid=dvrconfig.getvalueint( section, "show_policeid" );
#endif
	m_attr.ShowPeak=dvrconfig.getvalueint(section,"show_gforce");
	
	m_type = dvrconfig.getvalueint(section,"type");

	// audio route support
	tmpstr=dvrconfig.getvalue( section, "audiochannel");
	if( tmpstr.length()>0 ) {
		sscanf((char*)tmpstr, "%d", &m_audiochannel);
	}

	if( m_type == CAP_TYPE_HIKLOCAL ) {
		m_phychannel = dvrconfig.getvalueint(section,"channel") ;
		if( m_audiochannel < 0 )
			m_audiochannel = m_phychannel ;
	}
	
}
    
void capture::gpsframe()
{
    struct cap_frame capframe;
    char        * gpsData;
    char          gpsLength=0;
    struct gps    mGPS;

	//add the gps data if gps data is available
	get_gps_data(&mGPS);
	if(mGPS.gps_valid){

		gpsData=(char *)mem_alloc(1024);
	
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
		int tempLength = sprintf(pGps,"01,%09.6f%c%010.6f%c%.1fD%06.2f",
			      mGPS.gps_latitude<0.0000000 ? -mGPS.gps_latitude : mGPS.gps_latitude ,
			      mGPS.gps_latitude<0.0000000 ? 'S' : 'N' ,
			      mGPS.gps_longitud<0.0000000 ? -mGPS.gps_longitud: mGPS.gps_longitud,
			      mGPS.gps_longitud<0.0000000 ? 'W' : 'E' ,
			      (float)(mGPS.gps_speed * 1.852),      // in km/h.
			      (float)mGPS.gps_direction 		  
		);
		pGps[tempLength]='\0';
		pGps[tempLength+1]='\0';
		pGps[tempLength+2]='\0';
		pGps[tempLength+3]='\0';
		tempLength=((tempLength+3)>>2)<<2;

		unsigned short *mlength=(unsigned short*)&gpsData[6];
		*mlength=tempLength;
		gpsLength=tempLength+8;	

		capframe.channel = m_channel ;
		capframe.framesize = gpsLength ;
		capframe.frametype = FRAMETYPE_GPS ;
		capframe.framedata = (char *) gpsData;
		onframe(&capframe);
		
		mem_free( gpsData );		
	}
}
 
void capture::osdframe( int keyosd ) 
{
    struct cap_frame capframe;
    char	* osdData;
    int		osdLength ;    

    osdData = (char *)mem_alloc(2048);
    
	osdLength = updateOSD( osdData ) ;

	capframe.channel = m_channel ;     
	capframe.framesize = osdLength ;
	if( keyosd ) {
		capframe.frametype = FRAMETYPE_KEYOSD ;
	}
	else {
		capframe.frametype = FRAMETYPE_OSD ;
	}
	capframe.framedata = osdData;
	onframe(&capframe);
	
	mem_free(osdData);
}

extern int screen_onframe( cap_frame * capframe );

void capture::onframe(cap_frame * pcapframe)
{
	pcapframe->channel = m_channel ;
    m_streambytes+=pcapframe->framesize ;               // for bitrate calculation
    
	// to generate OSD frames, would be different on HIK channel and HDIP channel
	if( pcapframe->frametype == FRAMETYPE_KEYVIDEO ) {
		// mark channel is working
		m_working=20;

		m_osdtime = time(NULL) ;
		// insert key osd frame
		osdframe(1);

		// insert GPS frame. (removed)
		//gpsframe();
	}
	else if( pcapframe->frametype == FRAMETYPE_VIDEO  ) 
	{
		// mark channel is working
		m_working=20;

		time_t t = time(NULL);
		if( t!= m_osdtime ) {
			// generate OSD frame
			osdframe(0);
			m_osdtime = t ;
		}
	}
	
    net_onframe(pcapframe);
    rec_onframe(pcapframe);  
}

// periodic update procedure. (every 0.125 second)
// 		to update  signal, motion, osd , etc
void capture::update()
{  
	if( event_serialno % 13 == 0 ) {
		int v = vsignal();
		if( m_signal != v ) {
			m_signal = v ;
			if( !dio_standby_mode ) {				// don't record video lost event in standby mode. (Requested by Harrison)
				dvr_log("Camera %d video signal %s.", m_channel+1, m_signal?"on":"lost");
			}		
		}
		
		v = vmotion();
		if( m_motion != v ) {
			m_motion = v ;
//			if( v ) {
//					dvr_log("Camera %d motion detected!", m_channel+1 );
//			}
		}
		
	}; 
}
		
#ifdef PWII_APP

static int osd_line(char * line, char *text, int align, int posx, int posy)
{
	int lsize ;
	line[0] = 's' ;
	line[1] = 't' ;
	line[2] = 0 ;
	line[8] = align ;
	line[9] = posx ;
	line[10] = posy ;
	line[11] = 0 ;
	
	lsize = strlen(text);
	memcpy( line+12, text, lsize);
	if(lsize&1) {
		line[ lsize+12 ] = ' ' ; 	// fill with blank
		lsize++ ;
	}
	
	*(short *)(line+6) = lsize+4 ;
	return lsize+12 ;
}

int capture::updateOSD( char * osd ) 
{
	int i, j, l;
	char * osdline ;
    char osdbuf[256] ;	
	
	osdline = osd+8 ;			// first osdline
	
    // first OSD Line, date/time and Police id
    struct dvrtime newtime;
    time_now(&newtime);    
    sprintf(osdbuf,
                "%02d/%02d/%04d %02d:%02d:%02d", 
                 newtime.month,
                 newtime.day,
                 newtime.year,
                 newtime.hour,
                 newtime.minute,
                 newtime.second ) ;
    osdline+=osd_line( osdline, osdbuf, ALIGN_LEFT|ALIGN_TOP, 8, 8 );
    
    // first OSD Line right, Police id
    sprintf(osdbuf,"%s", m_show_policeid?g_policeid:"" ) ;
    osdline+=osd_line( osdline, osdbuf, ALIGN_RIGHT|ALIGN_TOP, 16, 8 );

	// second line, GPS
	if( m_attr.GPS_enable ) {
		osdbuf[0]=0 ;
		if( m_attr.showgpslocation ) {
			double lati, longi ;
			if( gps_location (&lati, &longi) ) {	
				int east, north ;	  
		  
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
            
            sprintf( osdbuf, "%d\xba%02d\'%02d\"%c %d\xba%02d\'%02d\"%c ", 
                    ladeg, lamin, lasecond, north,
                    lodeg, lomin, losecond, east );
			}
	   }
	         
		if( m_attr.ShowGPS && g_gpsspeed>-0.1 ) {
			float  gps_speed=0.0;
     		l = strlen(osdbuf);
		   if( m_attr.GPSUnit ) {			// kmh or mph
				gps_speed=g_gpsspeed * 1.852;
				if(gps_speed<3.22)
					gps_speed=0.0;

				sprintf( osdbuf+l, "%.1f km/h", gps_speed );
		   }
		   else {
				gps_speed=g_gpsspeed * 1.150779;
				if(gps_speed<2.0)
					gps_speed=0.0;

				sprintf( osdbuf+l, "%.1f mph", gps_speed );
			}           	
	   }
		osdline+=osd_line( osdline, osdbuf, ALIGN_LEFT|ALIGN_TOP, 8, 36 );
	}
	
	// line 2 right, g-force value
	osdbuf[0]=0 ;
    if(m_attr.ShowPeak && isPeakChanged()){
		sprintf( osdbuf, "%c%5.2lf,%c%5.2lf,%c%5.2lf  ",
		  g_fb>=0.0?'F':'B',
		  g_fb>=0.0?g_fb:-g_fb,
		  g_lr>=0.0?'R':'L',
		  g_lr>=0.0?g_lr:-g_lr, 
		  g_ud>=1.0?'D':'U',
		  g_ud>=1.0? (g_ud-1):(1-g_ud));
    }
   	osdline+=osd_line( osdline, osdbuf, ALIGN_RIGHT|ALIGN_TOP, 16, 36 );

    // 3rd line, sensors
    l=0;
    if( m_motion ) {
		osdbuf[l++]='*' ;
		osdbuf[l++]=' ' ;
	}
	if( event_tm ) {
		l+=sprintf( osdbuf+l, "TraceMark " );
	}
    for(j=0; j<num_sensors; j++) {
        if( m_sensorosd & (1<<j) ) {
            if( sensors[j]->value() ) {
            	 l+=sprintf( osdbuf+l, "%s ", sensors[j]->name() );
            }
        }
    }
    osdbuf[l] = 0 ;
    osdline+=osd_line( osdline, osdbuf, ALIGN_LEFT|ALIGN_BOTTOM, 8, 36 );

    // line 4, camera name
    sprintf( osdbuf, "%s", m_attr.CameraName );
    osdline+=osd_line( osdline, osdbuf, ALIGN_LEFT|ALIGN_BOTTOM, 8, 8 );

    // line 4 right, VRI
	sprintf( osdbuf, "%s", m_show_vri?g_vri:" " );           // optional VRI(video referrence id)
	osdline+=osd_line( osdline, osdbuf, ALIGN_RIGHT|ALIGN_BOTTOM, 16, 8 );

	// osd header
	osd[0] = 'T' ;
	osd[1] = 'X' ;
	osd[2] = 'T' ;
	osd[3] = 0 ;
	
	* (short *)(osd+6) = (osdline - osd) - 8 ;
	
	return (osdline - osd) ;
}

#endif   // PWII_APP

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

void cap_restart(int channel)
{
    if( channel>=0 && channel<cap_channels ) {
        cap_channel[channel]->restart();
    }
}

void cap_start()
{
    int i; 

    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->start();
    }
    
    // start eagle (zeus) board here
    eagle_start();
}

void cap_stop()
{
    int i; 
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->stop();
    }
    
    // stop eagle (zeus) board here
    eagle_stop();
}

// initial capture card, return channel numbers
void cap_init()
{
    int i;
    int videostandard ;
    int enabled_channels = 0;
    
    config dvrconfig(dvrconfigfile);
    
    cap_channels = dvrconfig.getvalueint("system", "totalcamera");

    // initialize local capture card (eagle32)
    videostandard = dvrconfig.getvalueint("system", "videostandard");
   
    if( cap_channels<0 || cap_channels > MAXCHANNEL ) {
        cap_channels=MAXCHANNEL ;
    }
    
    g_ts_base = time(NULL) - 11; /* used to make PTS */
    
#ifdef SUPPORT_IPCAM    
    // un init IP camera
    IpCamera_Init();
#endif       
    
    if( cap_channels > 0 ) {
        cap_channel = new capture * [cap_channels] ;
    }
    
    for (i = 0; i < cap_channels; i++ ) {
        char cameraid[16] ;
        int cameratype ;
        sprintf(cameraid, "camera%d", i+1 );
        cameratype = dvrconfig.getvalueint(cameraid, "type");	
        
		if( cameratype == CAP_TYPE_HIKIP ) {	// eagle ip camera
            cap_channel[i] = new ipeagle32_capture(i);
        }
#ifdef SUPPORT_IPCAM    
		else if( cameratype == CAP_TYPE_HDIPCAM ) {		// standard HD ip camera 
            cap_channel[i] = new Ipstream_capture(i);
        }
#endif                
        else {		// by default: CAP_TYPE_HIKLOCAL
			cap_channel[i] = new eagle_capture(i);
		}

        if( cap_channel[i]->enabled() ) {
            enabled_channels++ ;
        }
    }
    
    if( enabled_channels<=0 ) {
		dvr_log("No camera been setup!!!");
    }
    else {
		dvr_log("Total %d cameras in use, %d camera(s) enabled.",  cap_channels, enabled_channels);
	}
}

// uninitial capture card
void cap_uninit()
{
    int i ;
    if( cap_channels > 0 ) {
		int cap_ch = cap_channels ;
        cap_channels=0 ;
        for (i = 0; i < cap_ch; i++) {
            delete cap_channel[i] ;
            cap_channel[i]=NULL ;
        }
        delete cap_channel ;
    }
    cap_channel = NULL ;
    
#ifdef SUPPORT_IPCAM    
    // un init IP camera
    IpCamera_Uninit();
#endif    

    dvr_log("Capture card uninitialized.");
}
