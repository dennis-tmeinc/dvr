#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>

#ifndef INT8
#define INT8    signed char
#endif
#ifndef INT16
#define INT16   signed short int
#endif
#ifndef INT32
#define INT32   signed long int
#endif
#ifndef UINT8
#define UINT8   unsigned char
#endif
#ifndef UINT16
#define UINT16  unsigned short int
#endif
#ifndef UINT32
#define UINT32  unsigned long int
#endif

struct BITMAP {
    int width ;
    int height ;
    int bytes_per_line ;
    int bits_per_pixel ;
    UINT8 * filemap ;
    int filemaptype ;		// 0: mmap, 1: malloc
    UINT32  filelength ;
    UINT8 * bits ;
} ;

// r, g, b, a all range from 0-255 
#define COLOR(r,g,b,a) ( ((a)<<24) | (((r)&0xf8)<<8) | (((g)&0xfc)<<3) | (((b)&0xff)>>3) )

#define DRAW_PIXELMODE_COPY	(0)
#define DRAW_PIXELMODE_BLEND (1)

#ifdef __cplusplus
extern "C" {
#endif 
    
    // return 
    //        0 : failed
    //        1 : success
    int draw_init() ;
    // draw finish, clean up
    void draw_finish();
    int draw_screenwidth();
    int draw_screenheight();
    void draw_setcolor( UINT32 color ) ;
    UINT32 draw_getcolor();
    void draw_setmaskcolor( UINT32 color );
    UINT32 draw_getmaskcolor();
    void draw_setpixelmode( int pixelmode );
    int draw_getpixelmode();
    void draw_pixel( int x, int y, UINT32 color );
    void draw_blendpixel( int x, int y, UINT32 color );
    void draw_line(int x1, int y1, int x2, int y2 );
    void draw_circle( int cx, int cy, int r );
    void draw_rect( int x, int y, int w, int h );
    void draw_fillrect( int x, int y, int w, int h) ;
    int draw_openbmp( char * filename, struct BITMAP * bmp );
    void draw_closebmp( struct BITMAP * bmp );
    void draw_bitmap( struct BITMAP * bmp, int dx, int dy, int sx, int sy, int w, int h ) ;
    void draw_stretchbitmap( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) ;
    int draw_readbitmap(struct BITMAP * bmp, int x, int y, int w, int h );
    int draw_textwidth(struct BITMAP * font);
    int draw_textheight(struct BITMAP * font);
    void draw_text( int dx, int dy, char * text, struct BITMAP * font);
    void draw_text_ex( int dx, int dy, char * text, struct BITMAP * font, int fontw, int fonth);
    
#ifdef __cplusplus
}
#endif 


#define draw_fillscreen() draw_fillrect( 0, 0, draw_screenwidth(), draw_screenheight() ) 
