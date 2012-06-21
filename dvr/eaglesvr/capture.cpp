
#include "eaglesvr.h"

// OSD constants

#define _OSD_BASE	    0x9000
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
// degree symbol (Code page 437, 0xf8)
#define _OSD_DEGREESIGN '\xf8'

// for Eagle34 boards, OSD support exp
int _osd_vert_exp = 4 ;
int _osd_hori_exp = 4 ;

int cap_channels;
capture * * cap_channel;

#ifdef EAGLE32
char Dvr264Header[40] = 
{
    '\x34', '\x48', '\x4B', '\x48', '\xFE', '\xB3', '\xD0', '\xD6',
    '\x08', '\x03', '\x04', '\x20', '\x00', '\x00', '\x00', '\x00',
    '\x03', '\x10', '\x02', '\x10', '\x01', '\x10', '\x10', '\x00',
    '\x80', '\x3E', '\x00', '\x00', '\x10', '\x02', '\x40', '\x01',
    '\x11', '\x10', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};
#endif
#ifdef EAGLE34
char Dvr264Header[40] =
{
    '\x49', '\x4d', '\x4b', '\x48', '\x01', '\x01', '\x01', '\x00',
    '\x02', '\x00', '\x01', '\x00', '\x21', '\x72', '\x01', '\x0e',
    '\x80', '\x3e', '\x00', '\x00', '\x00', '\x7d', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};
#endif

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
    //    m_jpeg_mode = -1 ;
    //    m_jpeg_quality = 0 ;
    loadconfig();

    // default file header
    m_headerlen = 40 ;
    memcpy( m_header, Dvr264Header, 40 );
}

// simulated load config 
//   for eaglesvr, set to a resonable working parameter. All real parameter would come from IPcam
void capture::loadconfig()
{
    // loading osd bitmaps ;
    m_sensorosd=0 ;

    memset( &m_attr, 0, sizeof(m_attr) );
    m_attr.structsize = sizeof(m_attr);
    
    m_attr.Enable = 1;
    sprintf(m_attr.CameraName, "camera%d", m_channel);
    m_attr.Resolution=3; 	// 4CIF
    m_attr.RecMode=0;		// Not used
    m_attr.PictureQuality=5;
    m_attr.FrameRate=10;
    
    // bitrate
    m_attr.BitrateEn=1;
    m_attr.Bitrate=1000000;
    m_attr.BitMode=0;
    
    // picture control
    m_attr.brightness=5;
    m_attr.contrast  =5;
    m_attr.saturation=5;
    m_attr.hue       =5;

    m_attr.key_interval = 100;
}

void capture::onframe(cap_frame * pcapframe)
{
    if( !m_started )
        return ;
    m_working=10;										// capture channel watchdog
    m_streambytes+=pcapframe->framesize ;               // for bitrate calculation
    if( pcapframe->frametype == FRAMETYPE_264FILEHEADER ) {
        m_headerlen = pcapframe->framesize ;
        memcpy( m_header, pcapframe->framedata, pcapframe->framesize );
        return ;
    }
    if( net_onframe(pcapframe)<=0 ) {
        dvr_log( "No network connect, to stop channel %d ", m_channel );
        stop();
    }
}

// periodic update procedure.
void capture::update(int updosd)
{
    if( updosd ) {
        // update OSD
        //        updateOSD();
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

char * cap_fileheader(int channel)
{
    if( channel>=0 && channel<cap_channels ) {
        return cap_channel[channel]->getheader() ;
    }
    else {
        return Dvr264Header ;
    }
}

// initial capture card, return channel numbers
void cap_init()
{
    int i;
    cap_channels = eagle32_init();

    if( cap_channels<0 || cap_channels>16 ) {
        cap_channels = 0 ;
        return ;
    }
    cap_channel = new capture * [cap_channels] ;
    //    dvr_log("%d capture card (local) channels detected.", dvrchannels);
    for (i = 0; i < cap_channels; i++ ) {
        cap_channel[i]=new eagle_capture(i, i);
    }

}

// uninitial capture card
void cap_uninit()
{
    int i;
    cap_stop();
    if( cap_channels > 0 ) {
        i=cap_channels ;
        cap_channels=0 ;
        while(i>0) {
            delete cap_channel[--i] ;
        }
        delete [] cap_channel ;
        cap_channel=NULL ;
    }
    eagle32_uninit ();
}

