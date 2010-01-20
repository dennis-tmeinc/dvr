
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
    m_sensorosd = 1 ;

    memset( &m_attr, 0, sizeof(m_attr));
    m_attr.structsize = sizeof(m_attr);
    m_attr.Enable = 1;
    m_attr.FrameRate=1;
    m_attr.Resolution=3;
    m_attr.PictureQuality=5;
    m_attr.MotionAlarmEn=1 ;
    m_attr.MotionSensitivity=3 ;
    m_attr.BitrateEn=1 ;
    m_attr.BitMode=0 ;
    m_attr.Bitrate=1000000;
    m_attr.structsize=sizeof(m_attr);
    m_attr.brightness=5;
    m_attr.contrast=5;   
    m_attr.saturation=5; 
    m_attr.hue=5; 

}

extern int screen_onframe( cap_frame * capframe );

void capture::onframe(cap_frame * pcapframe)
{
//    if( pcapframe->frametype == FRAMETYPE_KEYVIDEO ) {
//        time_now(&(pcapframe->frametime));
//    }
    m_working=10;
    m_streambytes+=pcapframe->framesize ;               // for bitrate calculation
    net_onframe(pcapframe);
}

// periodic update procedure. (every 0.125 second)
void capture::update(int updosd)
{
}

void capture::updateOSD()
{
    return ;
}

// check alarm status relate to capture channel(every 0.125 second)
void capture::alarm()
{
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

// initial capture card, return channel numbers
void cap_init()
{
    int i;
    int dvrchannels ;
    int enabled_channels ;
    int cap_ch ;
    int ilocal=0 ;		// local camera idx
    enabled_channels = 0 ;
    
    // initialize local capture card (eagle32)
    dvrchannels=eagle32_init();
    cap_ch = dvrchannels ;
    
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
        cameratype = 0;	
        if( cameratype == 0 ) {			// local capture card
            if( ilocal<dvrchannels ) {
                cap_channel[i]=new eagle_capture(i, ilocal++);
            }
            else {
                cap_channel[i]=new eagle_capture(i, -1);		// dummy channel
            }
        }
        else {
            cap_channel[i]=new eagle_capture(i, -1);		// dummy channel
        }
        
        if( cap_channel[i]->enabled() ) {
            enabled_channels++ ;
        }
    }
    
    cap_channels = i ;
    
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

