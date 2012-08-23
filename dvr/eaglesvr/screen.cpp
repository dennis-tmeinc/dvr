
// screen process

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include "draw.h"
#include "eaglesvr.h"

#ifdef EAGLE32
#include "eagle32/davinci_sdk.h"
#endif

#ifdef EAGLE34
#include "eagle34/davinci_sdk.h"
#endif

#ifdef EAGLE368
#include "eagle368/tme_sdk.h"
#endif

#ifndef MAIN_OUTPUT
#define MAIN_OUTPUT  (0)
#endif

static int ScreenStandard = STANDARD_NTSC ;
static int ScreenNum ;

// decode speed, 0=fastest, 3=fast, 4=normal, 5=slow, 6=slowest, >=7 error
#define DECODE_SPEED_FASTEST    (0)
#define DECODE_SPEED_FAST       (3)
#define DECODE_SPEED_NORMAL     (4)
#define DECODE_SPEED_SLOW       (5)
#define DECODE_SPEED_SLOWEST    (6)

#define SCREEN_PLAYMODE_STOP	(0)
#define SCREEN_PLAYMODE_LIVE	(1)
#define SCREEN_PLAYMODE_DECODE	(2)


class videoscreen {
    int m_channel ;
    int m_playmode ; 	// 0: stopped, 1: liveview, 2: decoder(playback)
    int m_decodehandle ;
    int m_decodespeed ;
public:
    videoscreen(int channel) {
        m_channel=channel ;
        m_playmode=SCREEN_PLAYMODE_LIVE ;			// assume it is in liveview mode after capture card initialized
        m_decodehandle = -1 ;	// no decode handle
        m_decodespeed = DECODE_SPEED_NORMAL ;
        stop();					// stop decoder
    }
    ~videoscreen() {
        stop();				// stop decoder
    }

    void startliveview() {
        stop();

        dvr_log( "Screen channle %d start liveview.", m_channel );

#ifdef EAGLE368
        SetPreviewScreen(m_channel,1);                      // third  parameter turn on/off specified channel
        SetPreviewAudio(m_channel,1);                       // turn on/off audio ?
#else   // EAGLE34/EAGLE34
        SetPreviewScreen(MAIN_OUTPUT,m_channel+1,1);        // third  parameter turn on/off specified channel
        SetPreviewAudio(MAIN_OUTPUT,m_channel+1,1);         // turn on/off audio ?
#endif
        m_playmode = SCREEN_PLAYMODE_LIVE ;
    }

    void startdecode() {
        stop();

        dvr_log( "Screen channle %d start decode.", m_channel );

#if defined(EAGLE32) || defined( EAGLE34 )
        // replace channel to 0, because it is the only channel support playback
        dvr_log( "Mapped screen channle %d.", m_channel%ScreenNum  );

        SetDecodeScreen(MAIN_OUTPUT, m_channel%ScreenNum+1, 1);
        SetDecodeAudio(MAIN_OUTPUT, m_channel%ScreenNum+1, 1);
        m_decodehandle = StartDecode(MAIN_OUTPUT, m_channel%ScreenNum+1, 1, cap_fileheader(0));
        m_decodespeed = DECODE_SPEED_NORMAL ;
        SetDecodeSpeed(m_decodehandle, m_decodespeed);
        m_playmode = SCREEN_PLAYMODE_DECODE ;
#endif
    }

    void stop() {

        if( m_playmode==SCREEN_PLAYMODE_DECODE ) {
#if defined(EAGLE32) || defined( EAGLE34 )
            if( m_decodehandle>=0 ) {
                StopDecode( m_decodehandle );
                m_decodehandle = -1 ;
            }
            SetDecodeScreen(MAIN_OUTPUT, m_channel%ScreenNum+1, 0);
            SetDecodeAudio(MAIN_OUTPUT, m_channel%ScreenNum+1, 0);
#endif
            dvr_log( "Screen channle %d stop decode.", m_channel );

        }
        else {
            // stop live view
#if defined(EAGLE32) || defined( EAGLE34 )
            SetPreviewScreen(MAIN_OUTPUT,m_channel+1,0);          // third  parameter turn on/off specified channel
            SetPreviewAudio(MAIN_OUTPUT,m_channel+1,0);           // turn on/off audio ?
#endif
#ifdef EAGLE368
            SetPreviewScreen(m_channel,0);
            SetPreviewAudio(m_channel,0);
#endif
            dvr_log( "Screen channle %d stop live view.", m_channel );

        }
        m_playmode = SCREEN_PLAYMODE_STOP ;
    }

    // 1: turn audio on, 0: turn audio off
    void setaudio( int onoff ) {
        if( m_playmode==1 ) {
#ifdef EAGLE368
            SetPreviewAudio(m_channel, onoff );
#else
            SetPreviewAudio(MAIN_OUTPUT,m_channel+1, onoff );
#endif
        }
        else if( m_playmode==2 ) {
#if defined(EAGLE32) || defined( EAGLE34 )
            SetDecodeAudio(MAIN_OUTPUT,m_channel+1, onoff );
#endif
        }
    }

    int islive() {
        return (m_playmode == SCREEN_PLAYMODE_LIVE) ;
    }

    int isdecode() {
        return (m_playmode == SCREEN_PLAYMODE_DECODE) ;
    }

    // set decode speed. 0
    // decode speed, 0=fastest, 3=fast, 4=normal, 5=slow, 6=slowest, >=7 error
    void decodespeed( int speed ) {
#if defined(EAGLE32) || defined( EAGLE34 )
        if( m_decodehandle>=0 ) {
            m_decodespeed = speed ;
            SetDecodeSpeed(m_decodehandle, m_decodespeed);
        }
#endif
    }

    // input decode data
    void decodeinput( void * inputbuff, int bufsize ) {

        // dvr_log( "Screen channle %d input %d bytes.", m_channel, bufsize );
#if defined(EAGLE32) || defined( EAGLE34 )
        if( m_decodehandle>=0 ) {
            InputAvData(m_decodehandle, inputbuff, bufsize );
        }
#endif
    }
} ;

videoscreen ** screen_vs = NULL ;

// set screen mode
//    parameters:
//			videostd:  1: NTSC, 2: PAL
//          screennum: live/decoder screen numbers
//          displayorder: screen layout. support  1 , 2, 4 (2x2), 6 (3x3), 8 (?), 9 (3x3 ), 16 (4x4)
void screen_setmode( int videostd, int screennum, unsigned char * displayorder )
{
    int i;
    // setup preview display screen
    ScreenNum = screennum ;
    if( ScreenNum<0 ) {
        ScreenNum=1  ;
    }
    else if( ScreenNum>cap_channels ) {
        ScreenNum=cap_channels ;
    }

    // NTSC : 1  , PAL : 2
    ScreenStandard = videostd ;

#if defined(EAGLE32)||defined(EAGLE34)
    int res = 0 ;
    SetOutputFormat( ScreenStandard ) ;
    // clear display
    ClrDisplayParams(MAIN_OUTPUT,0);
    res=SetDisplayParams(MAIN_OUTPUT, ScreenNum);       //  set screen format, support 1 , 2, 4 (2x2), 6 (3x3), 8 (?), 9 (3x3 ), 16 (4x4)
    res=SetDisplayOrder(MAIN_OUTPUT, displayorder );    // select screen channel order, also turn on all preview channels
#endif

#ifdef EAGLE368
    SetDisplayParams(ScreenNum);
#endif

    if( screen_vs ) {
        for( i=0; i<cap_channels; i++ ) {
            if( screen_vs[i] ) {
                screen_vs[i]->stop();
            }
        }
    }
    // select Video output format to NTSC and initial drawing surface
    draw_init(ScreenStandard);
}

// initialize screen
void screen_init()
{
    int i;
    unsigned char displayorder[16] ;
    for(i=0; i<16; i++) {
        displayorder[i]=i ;
    }
    screen_setmode( STANDARD_NTSC, 1, displayorder );

    screen_vs = new videoscreen * [cap_channels] ;
    for( i=0; i<cap_channels; i++ ) {
        screen_vs[i]=new videoscreen(i);
    }
}

// screen finish, clean up
void screen_uninit()
{
    int i;
    if( screen_vs ) {
        for( i=0; i<cap_channels; i++ ) {
            delete screen_vs[i] ;
        }
        delete [] screen_vs ;
        screen_vs = NULL ;
    }
    draw_finish();
}


// support screen service over tcp

void screen_live( int channel )
{
    if( ScreenNum==1 ) {        // single screen
        int ch ;
        for (ch=0; ch<cap_channels; ch++ ) {
            if( ch!=channel ) {
                screen_vs[ch]->stop();
            }
        }
    }
    if( channel>=0 && channel<cap_channels ) {
        if( !screen_vs[channel]->islive() ) {
            screen_vs[channel]->startliveview();
        }
    }
}

void screen_playback( int channel, int speed )
{
    if( ScreenNum==1 ) {        // single screen
        int ch ;
        for (ch=0; ch<cap_channels; ch++ ) {
            if( ch!=channel ) {
                screen_vs[ch]->stop();
            }
        }
    }
#if defined(EAGLE32) || defined(EAGLE34)
    if( channel>=0 && channel<cap_channels ) {
        if( !screen_vs[channel]->isdecode() ) {
            screen_vs[channel]->startdecode();
        }
        screen_vs[channel]->decodespeed(speed);
    }
#endif
}

void screen_playbackinput( int channel, void * buffer, int bufsize )
{
#if defined(EAGLE32) || defined(EAGLE34)
    if( channel>=0 && channel<cap_channels ) {
        if( screen_vs[channel]->isdecode() ) {
            screen_vs[channel]->decodeinput(buffer, bufsize);
        }
    }
#endif
}

void screen_stop( int channel )
{
    if( channel>=0 && channel<cap_channels ) {
        screen_vs[channel]->stop();
    }
}

void screen_audio( int channel, int onoff )
{
    if( channel>=0 && channel<cap_channels ) {
        screen_vs[channel]->setaudio( onoff ) ;
    }
}

