
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
    m_streambytes=0 ;
    m_remoteosd = 0 ;	// local OSD by default
    m_signal_standard = 1 ;
    m_started = 0 ;
    // default file header
    m_headerlen = 0;

    //    m_jpeg_mode = -1 ;
    //    m_jpeg_quality = 0 ;

    // loading osd bitmaps ;
    m_sensorosd=0 ;

    // set default m_attr
    memset( &m_attr, 0, sizeof(m_attr) );
    m_attr.structsize = sizeof(m_attr);

    m_attr.Enable = 0;
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

capture::~capture()
{
    stop();
}

void capture::onframe(cap_frame * pcapframe)
{
    if( !m_started )
        return ;
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

/*
//  capture started by network
void cap_start()
{
    int i;
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->start();
    }
}
*/

void cap_stop()
{
    int i;
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->stop();
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
        return NULL ;
    }
}

// initial capture card, return channel numbers
void cap_init()
{
    cap_channels = eagle_init();
}

// uninitial capture card
void cap_uninit()
{
    eagle_uninit ();
}

