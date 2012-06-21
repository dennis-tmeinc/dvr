#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>

#ifndef __DRAW_H__
#define __DRAW_H__

#ifndef INT8
#define INT8    signed char
#endif
#ifndef INT16
#define INT16   signed short int
#endif
#ifndef INT32
#define INT32   signed int
#endif
#ifndef UINT8
#define UINT8   unsigned char
#endif
#ifndef UINT16
#define UINT16  unsigned short int
#endif
#ifndef UINT32
#define UINT32  unsigned int
#endif

struct BITMAP {
    int width ;
	int height ;
	int bits_per_pixel ;	// support 1, 24, 32 bits per pixel
	int bytes_per_line ;
	UINT8 * bits ;			// pointer to pixel data
} ;

// r, g, b, a all range from 0-255 
#define COLOR( r, g, b, a ) 	 ( (((UINT32)(r))<<16) | (((UINT32)(g))<<8) | (UINT32)(b) | (((UINT32)(a))<<24) )

// r, g, b, a componet
#define COLOR_R( color ) ( (UINT8)( (color)>>16 ) )
#define COLOR_G( color ) ( (UINT8)( (color)>>8  ) )
#define COLOR_B( color ) ( (UINT8)( (color)     ) )
#define COLOR_A( color ) ( (UINT8)( (color)>>24 ) )

#define DRAW_PIXELMODE_COPY	(0)
#define DRAW_PIXELMODE_BLEND (1)

// input
//       videoformat: 0=default, 1:NTSC, 2:PAL
// return 
//        0 : failed
//        1 : success
int draw_init(int videoformat);
// draw finish, clean up
void draw_finish();
int draw_screenwidth();				// return screen width
int draw_screenheight();			// return screen height
int draw_resetdrawarea();								// reset draw area to (0,0, screen_width, screen_height)
int draw_setdrawarea( int x, int y, int w, int h );		// set draw area, all drawing limited inside this area
int draw_refresh();		
void draw_setcolor( UINT32 color ) ;
UINT32 draw_getcolor();
void draw_setpixelmode( int pixelmode );
int draw_getpixelmode();
void draw_putpixel( int x, int y, UINT32 color );
#ifdef EAGLE34
void draw_putpixel_eagle34( int x, int y, UINT32 color );
#endif	
UINT32 draw_getpixel( int x, int y);
void draw_line(int x1, int y1, int x2, int y2 );
void draw_rect( int x, int y, int w, int h );
void draw_fillrect( int x, int y, int w, int h) ;
void draw_circle( int cx, int cy, int r );
void draw_fillcircle( int cx, int cy, int r );
int draw_openbmp( char * filename, struct BITMAP * bmp );
void draw_closebmp( struct BITMAP * bmp );
void draw_bitmap( struct BITMAP * bmp, int dx, int dy, int sx, int sy, int w, int h ) ;
void draw_stretchbitmap( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) ;
int draw_readbitmap(struct BITMAP * bmp, int x, int y, int w, int h );
int draw_fontwidth(struct BITMAP * font);
int draw_fontheight(struct BITMAP * font);
void draw_text( int dx, int dy, char * text, struct BITMAP * font);
void draw_text_ex( int dx, int dy, char * text, struct BITMAP * font, int fontw, int fonth);

#define draw_fillscreen() draw_fillrect( 0, 0, draw_screenwidth(), draw_screenheight() ) 

#endif
// __DRAW_H__
