
#include "dvr.h"

// OSD constants

#define _OSD_BASE	0x9000
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
capture ** cap_channel;

char Dvr264Header[40] = 
{
    '\x34', '\x48', '\x4B', '\x48', '\xFE', '\xB3', '\xD0', '\xD6',
    '\x08', '\x03', '\x04', '\x20', '\x00', '\x00', '\x00', '\x00',
    '\x03', '\x10', '\x02', '\x10', '\x01', '\x10', '\x10', '\x00',
    '\x80', '\x3E', '\x00', '\x00', '\x10', '\x02', '\x40', '\x01',
    '\x11', '\x10', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};

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
    m_signal_standard = 1 ;
    m_started = 0 ;
    loadconfig();

    m_headerlen = 40 ;
    mem_cpy32( m_header, Dvr264Header, 40 );
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
    
    sprintf(section, "camera%d", m_channel+1);
    m_attr.Enable = dvrconfig.getvalueint( section, "enable");
    tmpstr=dvrconfig.getvalue( section, "name");
    if( tmpstr.length()==0 )
        strcpy(m_attr.CameraName, section );
    else
        strncpy(m_attr.CameraName, tmpstr.getstring(), sizeof(m_attr.CameraName) );
    
    m_attr.Resolution=dvrconfig.getvalueint( section, "resolution");
    m_attr.RecMode=dvrconfig.getvalueint( section, "recordmode");
    m_attr.PictureQuality=dvrconfig.getvalueint( section, "quality");
    m_attr.FrameRate=dvrconfig.getvalueint( section, "framerate");
    
    // dummy trigger and osd field. these field are not used since we use web base configuration
    for(i=0; i<4; i++) {
        m_attr.SensorEn[i]=0;
        m_attr.Sensor[i]=0;
        m_attr.SensorOSD[i]=0;
    }
    
    // osd sensor
    for(i=0; i<32; i++) {
        sprintf(buf, "sensorosd%d", i+1);
        sensorosd=dvrconfig.getvalueint( section, buf );
        if( sensorosd>0 ) {
            m_sensorosd |= (1<<i);
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

    m_motionalarm = dvrconfig.getvalueint( section, "motionalarm" );
    m_motionalarm_pattern = dvrconfig.getvalueint( section, "motionalarmpattern" );
    m_signallostalarm = dvrconfig.getvalueint( section, "videolostalarm" );
    m_signallostalarm_pattern = dvrconfig.getvalueint( section, "videolostalarmpattern" );

    m_showgpslocation=dvrconfig.getvalueint( section, "showgpslocation" );
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
        
        if( m_channel<2 ) {                         // force first two channel to 768kbps
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
//    if( pcapframe->frametype == FRAMETYPE_KEYVIDEO ) {
//        time_now(&(pcapframe->frametime));
//    }

    m_working=10;
    m_streambytes+=pcapframe->framesize ;               // for bitrate calculation
    if( pcapframe->frametype == FRAMETYPE_264FILEHEADER ) {
        m_headerlen = pcapframe->framesize ;
        mem_cpy32( m_header, pcapframe->framedata, m_headerlen );
        return ;
    }
    if( pcapframe->frametype == FRAMETYPE_KEYVIDEO ) {
        struct hd_frame * pframe = (struct hd_frame *)pcapframe->framedata;
        if( (pframe->width_height>>16) > 570 ) {
            m_signal_standard = 2 ;         // PAL mode video
        }
        else {
            m_signal_standard = 1 ;         // assume NTSC
        }
    }    
    rec_onframe(pcapframe);
    net_onframe(pcapframe);
    screen_onframe(pcapframe); 
}

// periodic update procedure.
void capture::update(int updosd)
{
    if( updosd ) {
        // update OSD
        updateOSD();
    }
    if( m_oldsignal!=m_signal ) {
        if( dio_standby_mode == 0 ) {				// don't record video lost event in standby mode. (Requested by Harrison)
            dvr_log("Camera %d video signal %s.", m_channel, m_signal?"on":"lost");
        }
        m_oldsignal=m_signal ;
    }
}

static int  osdbrightness = 240 ;
static int  osdtranslucent = 0 ;
static int  osdtwinkle = 1 ;

#ifdef	MDVR_APP
void capture::updateOSD()
{
    char * k ;
    int i, j ;
    char osdbuf[128] ;
    struct hik_osd_type	osd ;
    
    if( !m_enable || m_remoteosd ) {
        return ;
    }
    
    osd.brightness = osdbrightness ;
    osd.translucent = osdtranslucent ;
    osd.twinkle = osdtwinkle  ;
    osd.lines = osdlines ;
    
    // prepare line 1
    i=0; 
    osd.osdline[0][i++]=8 ;           // x position
    osd.osdline[0][i++]=4 ;           // y position
    // Show Host Name
    k=g_hostname ;
    while( *k ) {
        osd.osdline[0][i++] = * k++ ;
    }
    osd.osdline[0][i++] = ' ' ;
    osd.osdline[0][i++] = ' ' ;
    
    // Show Camera name
    k=m_attr.CameraName;
    while( *k) {
        osd.osdline[0][i++] = * k++ ;
    }
    osd.osdline[0][i++] = ' ' ;
    osd.osdline[0][i++] = ' ' ;
    
    if( m_motion ){
        osd.osdline[0][i++] = '*' ;
    }
    else {
        osd.osdline[0][i++] = ' ' ;
    }
    
    osd.osdline[0][i++] = ' ' ;
    
    if( m_attr.GPS_enable && m_attr.ShowGPS && g_gpsspeed>-0.1) {
        if( m_attr.GPSUnit ) {			// kmh or mph
            sprintf( osdbuf, "%.1f km/h", g_gpsspeed * 1.852 );
        }
        else {
            sprintf( osdbuf, "%.1f mph", g_gpsspeed * 1.150779 );
        }
        k=osdbuf ;
        while( *k ) {
            osd.osdline[0][i++] = * k++ ;
        }
    }
    osd.osdline[0][i] = 0;          // null terminate
    
    // prepare line 2
    i=0;
    osd.osdline[1][i++]=8 ;           // x position
    osd.osdline[1][i++]=28 ;          // y position

    // date and time
    osd.osdline[1][i++]=_OSD_MONTH2 ;          // Month
    osd.osdline[1][i++]='/' ;          
    osd.osdline[1][i++]=_OSD_DAY ;          // Day
    osd.osdline[1][i++]='/' ;         
    osd.osdline[1][i++]=_OSD_YEAR4 ;          // 4 digit year
    osd.osdline[1][i++]=' ' ;          
    osd.osdline[1][i++]=_OSD_HOUR24 ;          // 24 hour
    osd.osdline[1][i++]=':' ;        
    osd.osdline[1][i++]=_OSD_MINUTE ;          // Minute
    osd.osdline[1][i++]=':' ;          
    osd.osdline[1][i++]=_OSD_SECOND ;          // Second
    osd.osdline[1][i++]=' ' ;         
    osd.osdline[1][i++]=' ' ;         
    
    // Show trigger name, if triggered
    for(j=0; j<num_sensors; j++) {
        if( m_sensorosd & (1<<j) ) {
            if( sensors[j]->value() ) {
                k=sensors[j]->name() ;
                while( *k ) {
                    osd.osdline[1][i++] = *k++ ;
                }
                osd.osdline[1][i++]=' ' ;
                if( i>120 ) {
                    break;
                }
            }
        }
    }
    
    osd.osdline[1][i]=0 ;
    
    // prepare line 3
    i=0;
    osd.osdline[2][i++]= osdlines3_x ;
    osd.osdline[2][i++]= osdlines3_y ;          
    
    if( m_attr.GPS_enable && m_attr.ShowGPS && m_showgpslocation ) {
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
            
            sprintf( osdbuf, "%d\xf8%02d\'%02d\"%c %d\xf8%02d\'%02d\"%c", 
                    ladeg, lamin, lasecond, north,
                    lodeg, lomin, losecond, east );
            k=osdbuf ;
            while( i<120 && *k ) {
                osd.osdline[2][i++] = * k++ ;
            }
            osd.osdline[2][i]=0 ;
            osd.lines++;
        }
    }
    
    setosd(&osd);
    return ;
}
#endif		// MDVR_APP


#ifdef TVS_APP

static int top_margin = 10 ;
static int bottom_margin = 10 ;
static int line_dist = 24 ;

void capture::updateOSD()
{
    char * k ;
    int i, j ;
    int line ;
    char osdbuf[128] ;
    struct hik_osd_type	osd ;
    int y_max ;
    
    if( !m_enable || m_remoteosd ) {
        return ;
    }

    if( m_signal_standard==2 ) {        // PAL signal?
        y_max = 572 ;
    }
    else {
        y_max = 480 ;
    }

            
    osd.brightness = osdbrightness ;
    osd.translucent = osdtranslucent ;
    osd.twinkle = osdtwinkle  ;
    
    // prepare first line, Date/time and license plate
    line=0 ;
    i=0; 
    osd.osdline[line][i++]=8 ;           // x position
    osd.osdline[line][i++]=top_margin ;  // y position
    
    // date and time
    osd.osdline[line][i++]=_OSD_MONTH2 ;          // Month
    osd.osdline[line][i++]='/' ;          
    osd.osdline[line][i++]=_OSD_DAY ;             // Day
    osd.osdline[line][i++]='/' ;         
    osd.osdline[line][i++]=_OSD_YEAR4 ;           // 4 digit year
    osd.osdline[line][i++]=' ' ;          
    osd.osdline[line][i++]=_OSD_HOUR24 ;          // 24 hour
    osd.osdline[line][i++]=':' ;        
    osd.osdline[line][i++]=_OSD_MINUTE ;          // Minute
    osd.osdline[line][i++]=':' ;          
    osd.osdline[line][i++]=_OSD_SECOND ;          // Second
    osd.osdline[line][i++]=' ' ;         
    osd.osdline[line][i++]=' ' ;         
    
    // show licenseplate
    if( m_show_licenseplate ) {
        sprintf( &osdbuf[i], "%20s", g_id2 );
        while( i<50 && osdbuf[i] ) {
            osd.osdline[line][i]=osdbuf[i] ;
            i++ ;
        }
    }

    osd.osdline[line][i]=0 ;

    // prepare line 2, GPS
    line++ ;
    i=0; 
    osd.osdline[line][i++]=8 ;             // x position
    osd.osdline[line][i++]=y_max - bottom_margin - line_dist*3; // y position
    
    double lati, longi, speed ;
    if( m_attr.GPS_enable && 
        gps_location(&lati, &longi, &speed) )
    {
        if( m_showgpslocation ) {
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
            sprintf( osdbuf, "%3d\xf8%02d\'%02d\"%c %3d\xf8%02d\'%02d\"%c", 
                    ladeg, lamin, lasecond, north,
                    lodeg, lomin, losecond, east );
            k=osdbuf ;
            while( i<30 && *k ) {
                osd.osdline[line][i++] = * k++ ;
            }
        }
        else {
            while( i<22 ) {
                osd.osdline[line][i++] = ' ' ;
            }
        }

        if( m_attr.ShowGPS ) {
            if( m_attr.GPSUnit ) {			// kmh or mph
                sprintf( osdbuf, "%11.1f km/h", speed * 1.852 );
            }
            else {
                sprintf( osdbuf, "%11.1f mph", speed * 1.150779 );
            }
            
            k=osdbuf ;
            while( *k && i<50 ) {
                osd.osdline[line][i++] = * k++ ;
            }
        }
    }
            
    osd.osdline[line][i] = 0;          // null terminate

    // prepare line 3, medallion and sensor
    line++ ;
    i=0; 
    osd.osdline[line][i++]=8 ;             // x position
    osd.osdline[line][i++]=y_max - bottom_margin - line_dist*2; // y position

    // show medallion
    if( m_show_medallion ) {
        sprintf( &osdbuf[i], "%s  ", g_id1 );
        while( i<50 && osdbuf[i] ) {
            osd.osdline[line][i]=osdbuf[i] ;
            i++ ;
        }
    }
    
    // Show sensors
    for(j=0; j<num_sensors; j++) {
        if( m_sensorosd & (1<<j) ) {
            if( sensors[j]->value() ) {
                k=sensors[j]->name() ;
                while( *k ) {
                    osd.osdline[line][i++] = *k++ ;
                }
                osd.osdline[line][i++]=' ' ;
                if( i>100 ) {
                    break;
                }
            }
        }
    }
    
    osd.osdline[line][i]=0 ;

    // prepare line 4, ivcs and camera name
    line++;
    i=0; 
    osd.osdline[line][i++]=8 ;              // x position
    osd.osdline[line][i++]= y_max - bottom_margin - line_dist; // y position
 
    // show IVCS and/or camera serial no
    sprintf( osdbuf, "%-20s %18s %c",
            m_show_ivcs?g_serial:" ", 
            m_show_cameraserial? m_attr.CameraName : " ",
            m_motion?'*':' ');
    
    k=osdbuf ;
    while( *k && i<50 ) {
        osd.osdline[line][i++] = * k++ ;
    }
    osd.osdline[line][i] = 0 ;

    line++;
    osd.lines = line ;

    setosd(&osd);
    return ;
}
#endif

#ifdef PWII_APP

static int top_margin = 18 ;
static int bottom_margin = 18 ;
static int line_dist = 24 ;

void capture::updateOSD()
{
    char * k ;
    int i, j ;
    int line ;
    char osdbuf[128] ;
    struct hik_osd_type	osd ;
    int y_max ;
    
    if( !m_enable || m_remoteosd ) {
        return ;
    }

    if( m_signal_standard==2 ) {        // PAL signal?
        y_max = 572 ;
    }
    else {
        y_max = 480 ;
    }
            
    osd.brightness = osdbrightness ;
    osd.translucent = osdtranslucent ;
    osd.twinkle = osdtwinkle  ;
    
    // prepare first line, Date/time
    line=0 ;
    i=0; 
    osd.osdline[line][i++]=8 ;           // x position
    osd.osdline[line][i++]=top_margin ;  // y position
    
    // date and time
    osd.osdline[line][i++]=_OSD_MONTH2 ;          // Month
    osd.osdline[line][i++]='/' ;          
    osd.osdline[line][i++]=_OSD_DAY ;             // Day
    osd.osdline[line][i++]='/' ;         
    osd.osdline[line][i++]=_OSD_YEAR4 ;           // 4 digit year
    osd.osdline[line][i++]=' ' ;          
    osd.osdline[line][i++]=_OSD_HOUR24 ;          // 24 hour
    osd.osdline[line][i++]=':' ;        
    osd.osdline[line][i++]=_OSD_MINUTE ;          // Minute
    osd.osdline[line][i++]=':' ;          
    osd.osdline[line][i++]=_OSD_SECOND ;          // Second
    osd.osdline[line][i++]=' ' ;         
    osd.osdline[line][i++]=' ' ;

    // show Police ID
    sprintf( osdbuf, "%20s",
            m_show_policeid?g_policeid:"");           // optional Police ID
    k=osdbuf ;
    while( *k && i<50 ) {
        osd.osdline[line][i++] = * k++ ;
    }
    osd.osdline[line][i] = 0 ;

    // prepare line 2, GPS
    line++ ;
    i=0;
    osd.osdline[line][i++]=8 ;             // x position
    osd.osdline[line][i++]=y_max - bottom_margin - line_dist*3; // y position
    
    double lati, longi, speed ;
    if( m_attr.GPS_enable && 
        gps_location(&lati, &longi, &speed) )
    {
        if( m_showgpslocation ) {
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
            sprintf( osdbuf, "%3d\xf8%02d\'%02d\"%c %3d\xf8%02d\'%02d\"%c", 
                    ladeg, lamin, lasecond, north,
                    lodeg, lomin, losecond, east );
            k=osdbuf ;
            while( i<30 && *k ) {
                osd.osdline[line][i++] = * k++ ;
            }
        }
        else {
            while( i<22 ) {
                osd.osdline[line][i++] = ' ' ;
            }
        }

        if( m_attr.ShowGPS ) {
            if( m_attr.GPSUnit ) {			// kmh or mph
                sprintf( osdbuf, "%11.1f km/h", speed * 1.852 );
            }
            else {
                sprintf( osdbuf, "%11.1f mph", speed * 1.150779 );
            }
            
            k=osdbuf ;
            while( *k && i<50 ) {
                osd.osdline[line][i++] = * k++ ;
            }
        }
    }
    osd.osdline[line][i] = 0;          // null terminate

    // prepare line 3, sensor
    line++ ;
    i=0; 
    osd.osdline[line][i++]=8 ;             // x position
    osd.osdline[line][i++]=y_max - bottom_margin - line_dist*2; // y position

    // Show sensors
    for(j=0; j<num_sensors; j++) {
        if( m_sensorosd & (1<<j) ) {
            if( sensors[j]->value() ) {
                k=sensors[j]->name() ;
                while( *k ) {
                    osd.osdline[line][i++] = *k++ ;
                }
                osd.osdline[line][i++]=' ' ;
                if( i>100 ) {
                    break;
                }
            }
        }
    }
    
    osd.osdline[line][i]=0 ;

    // prepare line 4, camera name
    line++;
    i=0; 
    osd.osdline[line][i++]=8 ;              // x position
    osd.osdline[line][i++]=y_max - bottom_margin - line_dist ; // y position
 
    // show VRI and camera serial no
    sprintf( osdbuf, "%19s %19s %c",
            m_show_vri?g_vri:" ",           // optional VRI(video referrence id)
            m_attr.CameraName ,
            m_motion?'*':' ');
    
    k=osdbuf ;
    while( *k && i<50 ) {
        osd.osdline[line][i++] = * k++ ;
    }
    osd.osdline[line][i] = 0 ;
    line++;

    osd.lines = line ;

    setosd(&osd);
    return ;
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
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->start();
    }
}

void cap_stop()
{
    int i; 
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->stop();
    }
}

void cap_restart(int channel)
{
    if( channel>=0 && channel<cap_channels ) {
        cap_channel[channel]->restart();
    }
}

void cap_capIframe(int channel)
{
    if( channel>=0 && channel<cap_channels ) {
        cap_channel[channel]->captureIFrame();
    }
}

// initial capture card, return channel numbers
void cap_init()
{
    int i;
    int videostandard ;
    int dvrchannels ;
    int enabled_channels ;
    int cap_ch ;
    int ilocal=0 ;		// local camera idx
    enabled_channels = 0 ;
    
    config dvrconfig(dvrconfigfile);
    
    cap_ch = dvrconfig.getvalueint("system", "totalcamera");
    
    // initialize local capture card (eagle32)
    videostandard = dvrconfig.getvalueint("system", "videostandard");
    dvrchannels=eagle32_init();
    
//    dvr_log("%d capture card (local) channels detected.", dvrchannels);
    if( cap_ch<=0 ) {
        cap_ch=dvrchannels ;
    }
    if( cap_ch> MAXCHANNEL ) {
        cap_ch=MAXCHANNEL ;
    }
    
    if( cap_ch > 0 ) {
        cap_channel = new capture * [cap_ch] ;
    }
    for (i = 0; i < cap_ch; i++ ) {
        char cameraid[16] ;
        int cameratype ;
        sprintf(cameraid, "camera%d", i+1 );
        cameratype = dvrconfig.getvalueint(cameraid, "type");	
        if( cameratype == 0 ) {			// local capture card
            if( ilocal<dvrchannels ) {
                cap_channel[i]=new eagle_capture(i, ilocal++);
            }
            else {
                cap_channel[i]=new eagle_capture(i, -1);		// dummy channel
            }
        }
        else if( cameratype == 1 ) {	// ip camera
            cap_channel[i] = new ipeagle32_capture(i);
        }
        else {
            cap_channel[i]=new eagle_capture(i, -1);		// dummy channel
        }
        
        if( cap_channel[i]->enabled() ) {
            enabled_channels++ ;
        }
    }
    
    cap_channels = i ;
    
//    if( enabled_channels<=0 ) {
//        printf("No camera available, DVR QUIT!\n");
//        exit(1);
//    }
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
    // un initialize local capture card
    eagle32_uninit ();
    dvr_log("Capture card uninitialized.");
}

