
#ifndef __screen_h__
#define __screen_h__

// support screen service over tcp
void eagle_screen_setmode( int videostd, int screennum, unsigned char * displayorder );
void eagle_screen_live( int channel ) ;
void eagle_screen_playback( int channel, int speed ) ;
void eagle_screen_playbackinput( int channel, void * buffer, int bufsize );
void eagle_screen_stop( int channel );
void eagle_screen_audio( int channel, int onoff );

void eagle_screen_init();
void eagle_screen_uninit();

#endif							// __screen_h__
