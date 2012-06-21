#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>

#include "../lzmasdk/lzma.h"
#include "../cfg.h"

#include "dvr.h"
#include "draw.h"

static UINT32 draw_drawcolor ;
static UINT32 draw_pixelmode ;		// 0: copy, 1: blend
static struct BITMAP * draw_font_cache ;

struct BITMAPFILEHEADER_TYPE
{
	 UINT16 bfType;
//	 UINT32 bfSize;
	 UINT16 bfSizeLow ;
	 UINT16 bfSizeHigh ;
	 UINT16 bfReserve1;
	 UINT16 bfReserve2;
//	 UINT32 bfOffBits;
	 UINT16 bfOffBitsLow ;
	 UINT16 bfOffBitsHigh ;
} ;

struct BITMAPFILEHEADER
{
	 UINT32 bfSize;
	 UINT16 bfReserve1;
	 UINT16 bfReserve2;
	 UINT32 bfOffBits;
} ;

struct BITMAPINFOHEADER
{
	 UINT32 bitSize;
	 UINT32 biWidth;
	 UINT32 biHeight;
	 UINT16 biPlanes;	
	 UINT16 biBitCount;	
	 UINT32 biCompression;	
	 UINT32 biSizeImage;
	 UINT32 biXPlosPerMeter;
	 UINT32 biYPlosPerMeter;	
	 UINT32 biClrUsed;		
	 UINT32 biClrImportant;	
} ;


#ifndef MAIN_OUTPUT
#define MAIN_OUTPUT  (0)
#endif

#define DRAW_W (704)
#define DRAW_H (576)

static int draw_w ;
static int draw_h ;

extern int screen_sockfd ;

// input
//       videoformat: 0=default, 1:NTSC, 2:PAL
// return 
//        0 : failed
//        1 : success
int draw_init(int videoformat)
{
	draw_w = dvr_screen_width(screen_sockfd);
	draw_h = dvr_screen_height(screen_sockfd);
	draw_font_cache = NULL ;
	return 1;
}

// draw finish, clean up
void draw_finish()
{
	draw_setfont(NULL);
}

int draw_screenwidth()
{
	return draw_w  ;
}

int draw_screenheight()
{
	return draw_h ;
}

int draw_setdrawarea( int x, int y, int w, int h )
{
	dvr_draw_setarea(screen_sockfd, x, y, w, h);
	return 0;
}

int draw_refresh()
{
	dvr_draw_refresh(screen_sockfd);
    return 0;
}

void draw_setcolor( UINT32 color ) 
{
	draw_drawcolor = color ;
	dvr_draw_setcolor(screen_sockfd, color);
}

UINT32 draw_getcolor()
{
	return draw_drawcolor ;
}

void draw_setpixelmode( int pixelmode )
{
	draw_pixelmode = pixelmode ; 
	dvr_draw_setpixelmode(screen_sockfd, pixelmode);
}

int draw_getpixelmode()
{
	return draw_pixelmode ;
}

void draw_putpixel( int x, int y, UINT32 color )
{
	dvr_draw_putpixel(screen_sockfd, x, y, color);
}

UINT32 draw_getpixel( int x, int y)
{
	return dvr_draw_getpixel(screen_sockfd, x, y);
}

// draw line, include point (x1,y1) and (x2, y2)
void draw_line(int x1, int y1, int x2, int y2 )
{
	dvr_draw_line(screen_sockfd, x1, y1, x2, y2 );
}

// draw circle centre on (cx, cy)
void draw_circle( int cx, int cy, int r )
{
	dvr_draw_circle(screen_sockfd, cx, cy, r) ;
}

void draw_fillcircle( int cx, int cy, int r )
{
	dvr_draw_fillcircle(screen_sockfd, cx, cy, r) ;
}

void draw_rect( int x, int y, int w, int h )
{
	dvr_draw_rect(screen_sockfd, x, y, w, h );
}

void draw_fillrect( int x, int y, int w, int h) 
{
	dvr_draw_fillrect(screen_sockfd, x, y, w, h );
}

int draw_openbmp( char * filename, struct BITMAP * bmp )
{
	struct BITMAPFILEHEADER bmpfile ;
	struct BITMAPINFOHEADER bmpinfo ;
	int   fd ;
	UINT8 * filebuf ;		// bmp file memory map
	UINT32  filelength ;	// bmp file length

	UINT16 bfType = 0;
	if( bmp == NULL ) {
		return 0 ;
	}

	fd = open(filename, O_RDONLY);
	if( fd<=0 ) {
		return 0 ;
	}
	filelength = lseek(fd, 0, SEEK_END);
	filebuf = (unsigned char *)malloc( filelength );
	if( filebuf == NULL ) {
		close(fd);
		return 0 ;
	}
	lseek(fd, 0, SEEK_SET);
	read( fd, filebuf, filelength );
	close( fd );
	bfType = *(UINT16 *)filebuf;

	if( bfType != 19778 ) {
		// try .lzma uncompress
		int lzmabufsize = lzmadecsize( filebuf ) ;
		if( lzmabufsize > 0 ) {									// uncompress size success.
			UINT8 * lzmabuf = (UINT8 *)malloc( lzmabufsize ) ;
			if( lzmabuf ) {
				filelength = lzmadec( filebuf, filelength, lzmabuf, lzmabufsize ) ;
				if( (int)filelength == lzmabufsize ) {
					free( filebuf ) ;							// free orignal buffer
					filebuf = lzmabuf ;
					bfType = *(UINT16 *)filebuf;
				}
				else {
					free( lzmabuf );
				}
			}
		}
	}

	if( bfType != 19778 ) {					// not a BMP file
		free( filebuf );
		return 0 ;
	}

	memcpy( &bmpfile, filebuf+2 , sizeof( bmpfile ) );
	memcpy( &bmpinfo, filebuf+14, sizeof( bmpinfo ) );

	if( filelength != bmpfile.bfSize ) {	// wrong file size?
		free( filebuf );
		return 0;
	}

    bmp->width = bmpinfo.biWidth ;
	bmp->height = bmpinfo.biHeight ;
	bmp->bits_per_pixel = bmpinfo.biBitCount ;
	bmp->bytes_per_line = ((bmp->bits_per_pixel * bmp->width + 31)/32)*4;
	bmp->bits = (unsigned char *)malloc( bmp->bytes_per_line * bmp->height ) ;
	if( bmp->bits ) {
		memcpy( bmp->bits, filebuf + bmpfile.bfOffBits, bmp->bytes_per_line * bmp->height );
		free( filebuf );
		return 1;
	}
	else {
		free( filebuf );
		return 0;
	}
}

void draw_closebmp( struct BITMAP * bmp )
{
	free( bmp->bits );
}

void draw_bitmap( struct BITMAP * bmp, int dx, int dy, int sx, int sy, int w, int h ) 
{
	dvr_draw_bitmap(screen_sockfd, bmp, dx, dy, sx, sy, w, h );
}

// stretch a bitmap
void draw_stretchbitmap( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
{
	dvr_draw_stretchbitmap(screen_sockfd, bmp, dx, dy, dw, dh, sx, sy, sw, sh ) ;
}

// read out bits from frame buffer, should call draw_closebmp () to close bmp after use
int draw_readbitmap(struct BITMAP * bmp, int x, int y, int w, int h )
{
	return dvr_draw_readbitmap(screen_sockfd, bmp, x, y, w, h );
}

int draw_fontwidth(struct BITMAP * font)
{
	return (font->width+9) / 16 ;
}

int draw_fontheight(struct BITMAP * font)
{
	return (font->height+4) / 6 ;
}

void draw_setfont(struct BITMAP * font)
{
	if( font!=draw_font_cache ) {
		draw_font_cache=font ;
		dvr_draw_setfont(screen_sockfd, draw_font_cache);
	}
}

void draw_text( int dx, int dy, char * text, struct BITMAP * font)
{
	draw_setfont(font);
	dvr_draw_text(screen_sockfd, dx, dy, text);
}

void draw_text_ex( int dx, int dy, char * text, struct BITMAP * font, int fontw, int fonth)
{
	draw_setfont(font);
	dvr_draw_textex(screen_sockfd, dx, dy, fontw, fonth, text);
}

