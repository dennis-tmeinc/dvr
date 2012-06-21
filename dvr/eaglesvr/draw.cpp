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
#ifdef EAGLE32
#include "eagle32/davinci_sdk.h"
#endif
#ifdef EAGLE34
#include "eagle34/davinci_sdk.h"
#endif

#include "draw.h"

// extra line below screen
#define EXTRALINE	(576-480+64)		

static UINT32 draw_drawcolor ;
static UINT32 draw_pixelmode ;		// 0: copy, 1: blend
static int draw_minx, draw_maxx, draw_miny, draw_maxy ;	// drawing area

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

#ifdef EAGLE32

#define COLOR16_RGB( r, g, b )  ((UINT16)( ((((UINT16)(r))<<8)&0xf800) | ((((UINT16)(g))<<3)&0x07e0) | ((((UINT16)(b))>>3)&0x01f) ))
#define COLOR16_R( color )      (((color)&0x0f800)>>8)
#define COLOR16_G( color )      (((color)&0x07e0)>>3)
#define COLOR16_B( color )      (((color)&0x1f)<<3)

/* frame buffer variables */
static char colordev[] = "/dev/fb/0" ;
static struct fb_var_screeninfo colorbufferinfo;
static UINT16 * colorbuffer=NULL ;
static int  colorbuffersize ;
static char alphadev[] = "/dev/fb/2" ;
static struct fb_var_screeninfo alphabufferinfo;
static UINT8  * alphabuffer=NULL ;
static int  alphabuffersize ;

#endif      // EAGLE32

#ifdef EAGLE34

// Color space converting

//Y = 0.299R+0.587G+0.114B
//U = 0.564(B - Y) = -0.169R-0.331G+0.500B
//V = 0.713(R - Y) = 0.500R-0.419G-0.081B
#define Y(R,G,B) ( (            77 * (R) + 150 * (G) +  29 * (B) ) >> 8) 
#define U(R,G,B) ( ( 128*256 -  43 * (R) -  85 * (G) + 128 * (B) ) >> 8) 
#define V(R,G,B) ( ( 128*256 + 128 * (R) - 107 * (G) -  21 * (B) ) >> 8)

static UINT32 draw_buf[DRAW_H][DRAW_W] ;

#endif

static int draw_w ;
static int draw_h ;

// input
//       videoformat: 0=default, 1:NTSC, 2:PAL
// return 
//        0 : failed
//        1 : success
int draw_init(int videoformat)
{

#ifdef EAGLE32
    int    fb_colorfd ;
    int	   fb_alphafd ;
    char * p ;

    // clean up
    draw_finish();

    fb_colorfd = open(colordev, O_RDWR ) ;
    if( fb_colorfd<=0 ) {
        printf("Can't open color device file!\n");
        return 0 ;
    }

    if (ioctl(fb_colorfd, FBIOGET_VSCREENINFO, &colorbufferinfo) == -1) {
        printf("Error reading variable info for color device file\n" );
        return 0;
    }

    if( videoformat==STANDARD_NTSC ) {
        colorbufferinfo.yres = 480 ;		// NTSC
    }
    else {
        colorbufferinfo.yres = 576 ;		// PAL
    }
    ioctl(fb_colorfd, FBIOPUT_VSCREENINFO, &colorbufferinfo);

    if (ioctl(fb_colorfd, FBIOGET_VSCREENINFO, &colorbufferinfo) == -1) {
        printf("Error reading variable info for color device file\n" );
        return 0;
    }

    if( colorbufferinfo.bits_per_pixel != sizeof(* colorbuffer) *8 ) {
        printf("Wrong bit format for color buffer!\n");
        return 0;
    }

    colorbuffersize = colorbufferinfo.xres_virtual * (colorbufferinfo.yres+EXTRALINE) * colorbufferinfo.bits_per_pixel / 8  ;

    p=(char *)mmap( NULL, colorbuffersize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_colorfd, 0 );
    if( p==(char *)-1 || p==NULL ) {
        printf( "Color IO memory map failed!\n");
        return 0;
    }
    colorbuffer = (UINT16 *) p ;
    close( fb_colorfd );					// file handle can be closed now

    fb_alphafd = open(alphadev, O_RDWR ) ;
    if( fb_alphafd<=0 ) {
        printf("Can't open alpha device file!\n");
        return 0 ;
    }

    if (ioctl(fb_alphafd, FBIOGET_VSCREENINFO, &alphabufferinfo) == -1) {
        printf("Error reading variable info for alpha device file\n" );
        return 0;
    }

    if( videoformat==STANDARD_NTSC ) {
        alphabufferinfo.yres = 480 ;		// NTSC
    }
    else {
        alphabufferinfo.yres = 576 ;		// PAL
    }
    ioctl(fb_alphafd, FBIOPUT_VSCREENINFO, &alphabufferinfo);

    if (ioctl(fb_alphafd, FBIOGET_VSCREENINFO, &alphabufferinfo) == -1) {
        printf("Error reading variable info for alpha device file\n" );
        return 0;
    }

    if( alphabufferinfo.bits_per_pixel != 4 ) {
        printf("Wrong bit format for alpha buffer!\n");
        return 0;
    }

    alphabuffersize = alphabufferinfo.xres_virtual * (alphabufferinfo.yres+EXTRALINE)  / 2  ;

    p=(char *)mmap( NULL, alphabuffersize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_alphafd, 0 );
    if( p==(char *)-1 || p==NULL ) {
        printf( "Alpha IO memory map failed!\n");
        return 0;
    }

    alphabuffer = (UINT8 *) p ;
    close( fb_alphafd );					// file handle can be closed now

    draw_w = colorbufferinfo.xres ;
    draw_h = colorbufferinfo.yres ;
    
#endif

#ifdef EAGLE34    
    //    memset( draw_buf, 0, sizeof( draw_buf ) );

    draw_w = DRAW_W ;
    if( videoformat == STANDARD_NTSC ) {
        draw_h = 480 ;
    }
    else if( videoformat == STANDARD_PAL ) {
        draw_h = 576 ;
    }
    else {
        draw_h = DRAW_H ;
    }
#endif    

    draw_setcolor(0);
    draw_setpixelmode( DRAW_PIXELMODE_COPY );
    draw_setdrawarea( 0, 0, 0, 0 );

    return 1;
}

#ifdef EAGLE34

struct draw_rect {
    int x1, y1, x2, y2, enable;
} ;
static struct draw_rect draw_area[SDK_MAX_MENU_DISP_NUM] ; 

// copy data from draw_buf to MenuBuf of Eagle34
static void draw_menu_copybuffer(int x, int y, int w, int h)
{
    int ix, iy ;
    struct SDK_MENU_BUF_PARAM MenuBufParam;

    memset( &MenuBufParam, 0, sizeof(MenuBufParam) );
    GetMenuBufParam(&MenuBufParam);

    w += x ;
    h += y ;

    for( iy=y; iy<h; iy++) {
        for( ix=x; ix<w; ix++ ) {
            UINT32 pixel, r, g, b ;
            pixel = draw_getpixel( ix, iy ) ;
            if( COLOR_A( pixel ) < 32 ) {
                MenuBufParam.pMenuY[(iy)*MenuBufParam.MenuBufW+ix]=0 ;      // Y=0, U=V=128 is mask color (make pixel transparent but no color)
                if( (iy&1)==0 ) {
                    MenuBufParam.pMenuU[(iy/2)*MenuBufParam.MenuBufW+ix]=128 ;
                }
            }
            else {
                r = COLOR_R( pixel ) ;
                g = COLOR_G( pixel ) ;
                b = COLOR_B( pixel ) ;
                MenuBufParam.pMenuY[iy*MenuBufParam.MenuBufW+ix]=Y(r, g, b) ;
                if( (iy&1)==0 ) {
                    if( (ix&1)==0 ) {
                        MenuBufParam.pMenuU[(iy/2)*MenuBufParam.MenuBufW+ix]=U(r, g, b) ;
                    }
                    else {
                        MenuBufParam.pMenuU[(iy/2)*MenuBufParam.MenuBufW+ix]=V(r, g, b) ;
                    }
                }
            }
        }
    }

    return ;
}

static void draw_menu_update()
{
    int i;
    struct SDK_MENU_REFRESH_PARAM MenuRefresh;
    struct SDK_MENU_DISP_PARAM  MenuParam;

    memset( &MenuParam, 0, sizeof(MenuParam));
    MenuRefresh.index=0;
    MenuRefresh.bFullScreenRefresh=0;

    for ( i=0; i<SDK_MAX_MENU_DISP_NUM; i++ ) {
        if( draw_area[i].enable ) {
            MenuRefresh.x = draw_area[i].x1 & (~7) ;
            MenuRefresh.w = ( (draw_area[i].x2+7)&(~7) )-MenuRefresh.x ;
            MenuRefresh.y = draw_area[i].y1 & (~3) ;
            MenuRefresh.h = ( (draw_area[i].y2+3)&(~3) )-MenuRefresh.y ;
            draw_menu_copybuffer(MenuRefresh.x, MenuRefresh.y, MenuRefresh.w, MenuRefresh.h);

            // also refresh video out
            RefreshMenuDisp(&MenuRefresh);

            MenuParam.DispParam[i].bEnable = 1;
            MenuParam.DispParam[i].menuAlpha = SDK_ALPHA_MENU_BACK31;
            MenuParam.DispParam[i].x = MenuRefresh.x;
            MenuParam.DispParam[i].y = MenuRefresh.y;
            MenuParam.DispParam[i].w = MenuRefresh.w;
            MenuParam.DispParam[i].h = MenuRefresh.h;

        }
        else {
            MenuParam.DispParam[i].bEnable=0 ;
        }
    }

    MenuParam.bFilter = 1;
    SetMenuDispParam(&MenuParam);

}

static void draw_area_addrect( int x, int y, int w, int h )
{
    int x2, y2, i ;
    int freearea ;

    x2 = x+w ;
    y2 = y+h ;

    // clip to draw area
    if( x<draw_minx ) {
        x=draw_minx ;
    }
    if( x2>draw_maxx ) {
        x2=draw_maxx ;
    }
    if( x2<=x ) {
        return ;
    }
    if( y<draw_miny ) {
        y=draw_miny ;
    }
    if( y2>draw_maxy ) {
        y2=draw_maxy ;
    }
    if( y2<=y ) {
        return ;
    }

    freearea=-1 ;
    for ( i=0; i<SDK_MAX_MENU_DISP_NUM; i++ ) {
        if( draw_area[i].enable ) {
            // check if new area overlap existing area
            if( x<=draw_area[i].x2 && x2>=draw_area[i].x1 && y<=draw_area[i].y2 && y2>=draw_area[i].y1 ) {
                // expand current area
                if( draw_area[i].x1>x ) draw_area[i].x1=x ;
                if( draw_area[i].y1>y ) draw_area[i].y1=y ;
                if( draw_area[i].x2<x2 ) draw_area[i].x2=x2 ;
                if( draw_area[i].y2<y2 ) draw_area[i].y2=y2 ;
                return ;
            }
        }
        else {
            freearea=i ;
        }
    }

    // create a new area
    if( freearea>=0 ) {
        draw_area[freearea].x1=x ;
        draw_area[freearea].y1=y ;
        draw_area[freearea].x2=x2 ;
        draw_area[freearea].y2=y2 ;
        draw_area[freearea].enable=1 ;
    }
    else {
        // no free area, find the closest area
        int distance = 10000000 ; 	// a number big enough
        freearea = 0 ;
        for ( i=0; i<SDK_MAX_MENU_DISP_NUM; i++ ) {
            int dx, dy, dis ;
            dx = x-draw_area[i].x1 ;
            dy = y-draw_area[i].y1 ;
            dis = dx*dx+dy*dy ;
            if( dis < distance ) {
                distance=dis ;
                freearea=i ;
            }
        }
        // MERGE into this area
        if( draw_area[freearea].x1>x ) draw_area[freearea].x1=x ;
        if( draw_area[freearea].y1>y ) draw_area[freearea].y1=y ;
        if( draw_area[freearea].x2<x2 ) draw_area[freearea].x2=x2 ;
        if( draw_area[freearea].y2<y2 ) draw_area[freearea].y2=y2 ;
    }
}

static void draw_area_clear( int x, int y, int w, int h )
{
    int x2, y2, i ;

    x2 = x+w ;
    y2 = y+h ;

    // clip to draw area
    if( x<draw_minx ) {
        x=draw_minx ;
    }
    if( x2>draw_maxx ) {
        x2=draw_maxx ;
    }
    if( x2<=x ) {
        return ;
    }
    if( y<draw_miny ) {
        y=draw_miny ;
    }
    if( y2>draw_maxy ) {
        y2=draw_maxy ;
    }
    if( y2<=y ) {
        return ;
    }
    for ( i=0; i<SDK_MAX_MENU_DISP_NUM; i++ ) {
        if( draw_area[i].enable &&
            x<=draw_area[i].x1 &&
            x2>=draw_area[i].x2 &&
            y<=draw_area[i].y1 &&
            y2>=draw_area[i].y2 )
        {
            draw_area[i].enable=0;
        }
    }
}

#endif    

// draw finish, clean up
void draw_finish()
{
#ifdef EAGLE32
    // clean up shared memory
    if( alphabuffer != NULL ) {
        munmap( alphabuffer, alphabuffersize );
        alphabuffer = NULL ;
    }
    if( colorbuffer != NULL ) {
        munmap( colorbuffer, colorbuffersize );
        colorbuffer = NULL ;
    }
#endif    
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
    draw_minx = x ;
    draw_miny = y ;
    draw_maxx = x+w ;
    draw_maxy = y+h ;
    if( draw_minx<0 ) {
        draw_minx=0;
    }
    if( draw_maxx>draw_w  ) {
        draw_maxx=draw_w  ;
    }
    if( draw_miny<0 ) {
        draw_miny=0;
    }
    if( draw_maxy>draw_h ) {
        draw_maxy=draw_h ;
    }
    if( draw_minx >= draw_maxx || draw_miny >= draw_maxy ) {
        draw_minx = 0 ;
        draw_miny = 0 ;
        draw_maxx = 0 ;
        draw_maxy = 0 ;
    }
    return 0;
}

int draw_refresh()
{
#ifdef EAGLE34        
    draw_menu_update();
#endif  	
    return 1 ;
}

void draw_setcolor( UINT32 color ) 
{
    draw_drawcolor = color ;
}

UINT32 draw_getcolor()
{
    return draw_drawcolor ;
}

void draw_setpixelmode( int pixelmode )
{
    draw_pixelmode = pixelmode ;
}

int draw_getpixelmode()
{
    return draw_pixelmode ;
}

UINT32 draw_getpixel( int x, int y)
{
    if( x<0 || y<0 || x>=draw_w || y>=draw_h )
        return (UINT32)0;
#ifdef EAGLE32
    UINT32 color16 = (UINT32)colorbuffer[ y*colorbufferinfo.xres_virtual + x ] ;
    UINT32 alpha ;
    if( x&1 ) {
        alpha = (alphabuffer[ (y*alphabufferinfo.xres_virtual+x)/2 ]&0x7) << 5 ;
    }
    else {
        alpha = (alphabuffer[ (y*alphabufferinfo.xres_virtual+x)/2 ]&0x70) << 1 ;
    }
    if( alpha>=0xe0 ) alpha = 0xff ;
    return COLOR( COLOR16_R(color16), COLOR16_G(color16), COLOR16_B(color16), alpha ) ;
#endif
#ifdef EAGLE34    
    return draw_buf[y][x] ;
#endif    
}

// copy pixel without boundary check
static void draw_putpixel_copy( int x, int y, UINT32 color )
{
#ifdef EAGLE32
    UINT8 * palpha ;
    palpha = alphabuffer + (y*alphabufferinfo.xres_virtual+x)/2 ;
    *palpha = (x&1) ?
                  ( (*palpha & 0xf0 ) | (COLOR_A( color )>>5) ) :
                  ( (*palpha & 0x0f ) | ((COLOR_A( color )>>1)&0x70) ) ;
    colorbuffer[ y*colorbufferinfo.xres_virtual + x ] = COLOR16_RGB( COLOR_R(color), COLOR_G(color), COLOR_B(color) ) ;
#endif
#ifdef EAGLE34
    draw_buf[y][x]=color ;
#endif    		
}

// blend pixel without boundary check
static void draw_putpixel_blend( int x, int y, UINT32 color )
{
    UINT32  alpha ;
    UINT32  ocolor ;
    UINT32  o_r, o_g, o_b, o_a ;
    UINT32  n_r, n_g, n_b, n_a ;
#ifdef EAGLE32        
    ocolor = (UINT32)colorbuffer[ y*colorbufferinfo.xres_virtual + x ] ;
    o_r = COLOR16_R( ocolor ) ;
    o_g = COLOR16_G( ocolor ) ;
    o_b = COLOR16_B( ocolor ) ;
    if( x&1 ) {
        o_a = (alphabuffer[ (y*alphabufferinfo.xres_virtual+x)/2 ]&0x7) << 5 ;
    }
    else {
        o_a = (alphabuffer[ (y*alphabufferinfo.xres_virtual+x)/2 ]&0x70) << 1 ;
    }
    if( o_a>=0xe0 ) o_a = 0xff ;
#endif        
#ifdef EAGLE34
    ocolor = draw_buf[y][x] ;
    o_r = COLOR_R( ocolor ) ;
    o_g = COLOR_G( ocolor ) ;
    o_b = COLOR_B( ocolor ) ;
    o_a = COLOR_A( ocolor ) ;
#endif        
    n_r = COLOR_R( color ) ;
    n_g = COLOR_G( color ) ;
    n_b = COLOR_B( color ) ;
    n_a = COLOR_A( color ) ;
    alpha  = n_a+o_a-(n_a*o_a)/255 ;		// blended alpha
    n_r  = ( n_r * n_a + o_r * o_a  - (( o_r * o_a * n_a )/255) ) / alpha ;
    n_g  = ( n_g * n_a + o_g * o_a  - (( o_g * o_a * n_a )/255) ) / alpha ;
    n_b  = ( n_b * n_a + o_b * o_a  - (( o_b * o_a * n_a )/255) ) / alpha ;
    color = COLOR( n_r, n_g, n_b, alpha );

    draw_putpixel_copy( x, y, color );
}

void draw_putpixel( int x, int y, UINT32 color )
{
    if( x<draw_minx || y<draw_miny || x>=draw_maxx || y>=draw_maxy )
        return ;
    if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
        draw_putpixel_blend( x, y, color );
    }
    else {
        draw_putpixel_copy( x, y, color );
    }
}

#ifdef EAGLE34
void draw_putpixel_eagle34( int x, int y, UINT32 color )
{
    if( COLOR_A(color)>=32 ) {
        draw_area_addrect(x, y, 1, 1 );
    }
    draw_putpixel( x, y, color );
}
#endif

static void draw_fillline( int x, int y, int w )
{
    // check drawing limit
    if( y<draw_miny || y>=draw_maxy ) {
        return ;
    }

    if( x<draw_minx ) {
        w-=(draw_minx-x) ;
        x=draw_minx ;
    }
    
    if( x+w > draw_maxx ) {
        w = draw_maxx-x ;
    }
    
    if( w<=0 ) {
        return ;
    }
    
    if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
        w+=x ;
        for( ; x<w ; x++ ) {
            draw_putpixel_blend( x, y, draw_drawcolor );
        }
    }
    else {
#ifdef EAGLE32
        UINT32   color16 ;
        UINT16 * colorline ;
        UINT8  * alphaline ;
        UINT8    alpha ;

        color16 = COLOR16_RGB( COLOR_R(draw_drawcolor), COLOR_G(draw_drawcolor), COLOR_B(draw_drawcolor) );
        color16 |= color16<<16 ;				    // make a double pixel color
        colorline = & colorbuffer[ y * colorbufferinfo.xres_virtual + x ] ;
        alpha = COLOR_A(draw_drawcolor) ;
        alpha = (alpha>>5) | ((alpha>>1)&0x70) ;    // make a double pixel alpha
        alphaline = & alphabuffer[ (y * alphabufferinfo.xres_virtual + x)/2 ];

        w += x-1 ;      // w address to last pixel to draw
        if( x&1 ) {
            *colorline++ = (UINT16)color16 ;
            *alphaline = (*alphaline & 0xf0 )|(alpha & 0x0f) ;
            alphaline++ ;
            x++ ;
        }
        for( ; x<w ; x+=2 ) {			// two pixel per cycle
            *(UINT32 *)colorline = color16 ;
            colorline+=2 ;
            * alphaline++ = alpha ;
        }
        if( x==w ) {
            *colorline = (UINT16)color16 ;
            *alphaline = (*alphaline & 0x0f )|(alpha & 0xf0) ;
        }
#endif
#ifdef EAGLE34
        w+=x ;
        for( ; x<w ; x++ ) {
            draw_buf[y][x]=draw_drawcolor ;
        }
#endif        
    }
}

// draw line, include point (x1,y1) and (x2, y2)
void draw_line(int x1, int y1, int x2, int y2 )
{
    int		dx,		//deltas
            dy,
            dx2,	//scaled deltas
            dy2,
            ix,		//increase rate on the x axis
            iy,		//increase rate on the y axis
            err,	//the error term
            i;		//looping variable

    // difference between starting and ending points
    dx = x2 - x1;
    dy = y2 - y1;

    // calculate direction of the vector and store in ix and iy
    if (dx >= 0) {
        ix = 1;
    }
    else {
        ix = -1 ;
        dx = -dx ;
    }

    if (dy >= 0) {
        iy = 1;
    }
    else {
        iy = -1;
        dy = -dy ;
    }

#ifdef EAGLE34
    // add menu area for eagle34
    if( COLOR_A(draw_drawcolor)>=32 ) {
        draw_area_addrect( ix>0?x1:x2, iy>0?y1:y2, dx, dy ) ;
    }
#endif	

    // scale deltas and store in dx2 and dy2
    dx2 = dx * 2;
    dy2 = dy * 2;

    if (dx > dy)	// dx is the major axis
    {
        // initialize the error term
        err = dy2 - dx;

        for (i = 0; i <= dx; i++)
        {
            draw_putpixel( x1, y1, draw_drawcolor );
            if (err >= 0)
            {
                err -= dx2;
                y1 += iy;
            }
            err += dy2;
            x1 += ix ;
        }
    }
    else 		// dy is the major axis
    {
        // initialize the error term
        err = dx2 - dy;

        for (i = 0; i <= dy; i++)
        {
            draw_putpixel( x1, y1, draw_drawcolor  );
            if (err >= 0)
            {
                err -= dy2;
                x1+=ix ;
            }
            err += dx2;
            y1 += iy;
        }
    }
}

// draw circle centre on (cx, cy)
void draw_circle( int cx, int cy, int r )
{
    int x = 0 ;
    int y = r ;
    int yy = r*r ;
    int rr = yy+r ;
    int xx = 0 ;

#ifdef EAGLE34
    // add menu area for eagle34
    if( COLOR_A(draw_drawcolor)>=32 ) {
        draw_area_addrect( cx-r, cy-r, r+r+1, r+r+1 ) ;
    }
#endif	

    while( x<=y ) {
        if( x==0 ) {
            draw_putpixel ( cx, cy+r, draw_drawcolor );
            draw_putpixel ( cx, cy-r, draw_drawcolor );
            draw_putpixel ( cx+r, cy, draw_drawcolor );
            draw_putpixel ( cx-r, cy, draw_drawcolor );
        }
        else if( x== y ) {
            draw_putpixel ( cx+x, cy+y, draw_drawcolor );
            draw_putpixel ( cx+x, cy-y, draw_drawcolor );
            draw_putpixel ( cx-x, cy+y, draw_drawcolor );
            draw_putpixel ( cx-x, cy-y, draw_drawcolor );
            break;
        }
        else {
            draw_putpixel ( cx+x, cy+y, draw_drawcolor );
            draw_putpixel ( cx+x, cy-y, draw_drawcolor );
            draw_putpixel ( cx-x, cy+y, draw_drawcolor );
            draw_putpixel ( cx-x, cy-y, draw_drawcolor );
            draw_putpixel ( cx+y, cy+x, draw_drawcolor );
            draw_putpixel ( cx+y, cy-x, draw_drawcolor );
            draw_putpixel ( cx-y, cy+x, draw_drawcolor );
            draw_putpixel ( cx-y, cy-x, draw_drawcolor );
        }
        if( xx+yy > rr ) {
            yy = yy - y * 2 + 1 ;
            y-- ;
        }
        xx = xx + x * 2 + 1 ;
        x++ ;
    }
}

void draw_fillcircle( int cx, int cy, int r )
{
    int x = 1 ;
    int y = r ;
    int yy = r*r ;
    int rr = yy+r ;
    int xx = 1 ;

#ifdef EAGLE34
    // add menu area for eagle34
    if( COLOR_A(draw_drawcolor)>=32 ) {
        draw_area_addrect( cx-r, cy-r, r+r+1, r+r+1 ) ;
    }
#endif	

    draw_fillline( cx-y, cy,  y*2+1 );
    while( x<=y ) {
        draw_fillline( cx-y, cy+x,  y*2+1 );
        draw_fillline( cx-y, cy-x,  y*2+1 );
        if( x== y )
            break ;
        if( xx+yy > rr ) {
            draw_fillline( cx-x, cy+y,  x*2+1 );
            draw_fillline( cx-x, cy-y,  x*2+1 );
            yy = yy - y * 2 + 1 ;
            y-- ;
        }
        xx = xx + x * 2 + 1 ;
        x++ ;
    }
}

void draw_rect( int x, int y, int w, int h )
{
    int ix, iy, x2, y2 ;
    if( w>0 && h>0 ) {
#ifdef EAGLE34
	// add menu area for eagle34
	if( COLOR_A(draw_drawcolor)>=32 ) {
	    draw_area_addrect( x, y, w, h ) ;
	}
#endif			
	x2 = x+w-1 ;
	y2 = y+h-1 ;
	for( ix=x; ix<=x2; ix++ ) {
	    draw_putpixel( ix, y, draw_drawcolor );
	    draw_putpixel( ix, y2, draw_drawcolor );
	}
	for( iy=y; iy<=y2; iy++ ) {
	    draw_putpixel( x, iy, draw_drawcolor );
	    draw_putpixel( x2, iy, draw_drawcolor );
	}
    }
}

void draw_fillrect( int x, int y, int w, int h) 
{

#ifdef EAGLE34
    // add menu area for eagle34
    if( COLOR_A(draw_drawcolor)>=32 ) {
        draw_area_addrect( x, y, w, h ) ;
    }
    else {
        // here is the only place to clear draw area
        draw_area_clear( x, y, w, h ) ;
    }
#endif	

    h+=y ;
    for( ; y<h; y++ ) {
        draw_fillline( x, y, w ) ;
    }
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
    int ix, iy, x, y ;
    UINT8 * sline ;
    UINT8 r, g, b ;

    if( w<=0 || h<=0 ) {
        w = bmp->width - sx ;
        h = bmp->height - sy ;
    }

    if( sx<0 ) {
        w+=sx ;
        sx=0 ;
        if( w<=0 ) {
            return ;
        }
    }
    if( sy<0 ) {
        h+=sy ;
        sy=0 ;
        if( h<=0 ) {
            return ;
        }
    }
    if( sx+w > bmp->width ) {
        w = bmp->width-sx ;
        if( w<=0 ) {
            return ;
        }
    }
    if( sy+h > bmp->height ) {
        h = bmp->height-sy ;
        if( h<=0 ) {
            return ;
        }
    }

#ifdef EAGLE34
    // add menu area for eagle34
    draw_area_addrect( dx, dy, w, h ) ;
#endif	

    if( bmp->bits_per_pixel == 32 ) {
        for( iy=0, y=dy ; iy<h ; iy++, y++ ) {
            //			sline = bmp->bits + (bmp->height - (sy+iy) -1)* bmp->bytes_per_line + sx*4 ;
            UINT32 * bits32line = (UINT32 *)(bmp->bits + (bmp->height - (sy+iy) -1)* bmp->bytes_per_line + sx*4) ;
            for( x=dx, ix=0; ix<w; ix++, x++) {
                //				b = * sline++ ;
                //				g = * sline++ ;
                //				r = * sline++ ;
                //				a = * sline++ ;
                //				draw_putpixel( x, y, COLOR( r, g, b, a ) );
                draw_putpixel( x, y, *bits32line++ );
            }
        }
    }
    else if( bmp->bits_per_pixel == 24 ) {
        for( iy=0, y=dy ; iy<h ; iy++, y++ ) {
            sline = bmp->bits + (bmp->height - (sy+iy) -1)* bmp->bytes_per_line + sx*3 ;
            for( x=dx, ix=0; ix<w; ix++, x++) {
                b = * sline++ ;
                g = * sline++ ;
                r = * sline++ ;
                draw_putpixel( x, y, COLOR( r, g, b, 0xff ) );
            }
        }
    }
    else if( bmp->bits_per_pixel == 1 ) {
        for( iy=0, y=dy ; iy<h ; iy++, y++ ) {
            sline = bmp->bits + (bmp->height - (sy+iy) -1)* bmp->bytes_per_line ;
            for( x=dx, ix=0; ix<w; ix++, x++) {
                if( sline[(ix+sx)/8] & (0x80>>((ix+sx)%8)) ) {
                    draw_putpixel( x, y, draw_drawcolor );
                }
            }
        }
    }
}


// stretch a 32bit bitmap with alpha
void draw_stretchbitmap_32b( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
{
    int idx, idy, isx, isy ;
    int sx_step, sy_step ;
    UINT8 * sline ;
    int sx_compress, sy_compress ;
    sx_compress = (sw+dw-1)/dw ;
    sy_compress = (sh+dh-1)/dh ;

    for( idy=0, isy=0 ; idy<dh ; idy++ ) {

        if( isy*dh > idy*sh ) {
            sy_step = sy_compress-1 ;
        }
        else {
            sy_step = sy_compress ;
        }

        for( idx=0, isx=0; idx<dw; idx++ ) {
            if( isx*dw > idx*sw ) {
                sx_step = sx_compress-1 ;
            }
            else {
                sx_step = sx_compress ;
            }

            int cx, cy ;
            int iix, iiy ;
            int compresspixels=0 ;
            cx=sx_step ;
            if( cx<=0 ) cx=1 ;
            cy=sy_step ;
            if( cy<=0 ) cy=1 ;
            UINT32 color_r = 0;
            UINT32 color_g = 0;
            UINT32 color_b = 0;
            UINT32 color_alpha   = 0;

            for( iiy=0 ; iiy<cy ;iiy++) {
                int yy=sy+isy+iiy ;
                if( yy>=sy+sh ) break ;
                // source bitmap bits line
                int xx = sx+isx ;
                sline = bmp->bits + (bmp->height - yy -1) * bmp->bytes_per_line + xx*4;
                for( iix=0; iix<cx; iix++, xx++ ) {
                    if( xx>=sx+sw ) break ;
                    compresspixels++ ;
                    UINT32 alpha = sline[3] ;
                    color_b += alpha * (*sline++);
                    color_g += alpha * (*sline++);
                    color_r += alpha * (*sline++);
                    color_alpha   += alpha ;
                    sline++;
                }
            }


            if( color_alpha == 0 || compresspixels == 0 ) {
                draw_putpixel( dx+idx, dy+idy, 0 ) ;				// full transparent
            }
            else {
                color_r /= color_alpha ;
                color_g /= color_alpha ;
                color_b /= color_alpha ;
                color_alpha = (color_alpha+compresspixels-1)/compresspixels ;
                draw_putpixel( dx+idx, dy+idy, COLOR( color_r, color_g, color_b, color_alpha ) );
            }

            // step on source x;
            isx+=sx_step ;
        }

        // step on source line
        isy+=sy_step ;
    }
}

// stretch a 24bit bitmap
void draw_stretchbitmap_24b( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
{
    int idx, idy, isx, isy ;
    int sx_step, sy_step ;
    UINT8 * sline ;
    int sx_compress, sy_compress ;

    sx_compress = (sw+dw-1)/dw ;
    sy_compress = (sh+dh-1)/dh ;

    for( idy=0, isy=0 ; idy<dh ; idy++ ) {

        if( isy*dh > idy*sh ) {
            sy_step = sy_compress-1 ;
        }
        else {
            sy_step = sy_compress ;
        }

        for( idx=0, isx=0; idx<dw; idx++ ) {
            if( isx*dw > idx*sw ) {
                sx_step = sx_compress-1 ;
            }
            else {
                sx_step = sx_compress ;
            }
            int cx, cy ;
            int iix, iiy ;
            int compresspixels=0 ;
            cx=sx_step ;
            if( cx<=0 ) cx=1 ;
            cy=sy_step ;
            if( cy<=0 ) cy=1 ;
            UINT32 color_r = 0;
            UINT32 color_g = 0;
            UINT32 color_b = 0;

            for( iiy=0 ; iiy<cy ;iiy++) {
                int yy=sy+isy+iiy ;
                if( yy>=sy+sh ) break ;
                // source bitmap bits line
                int xx = sx+isx ;
                sline = bmp->bits + (bmp->height - yy -1) * bmp->bytes_per_line + xx*3;
                for( iix=0; iix<cx; iix++, xx++ ) {
                    if( xx>=sx+sw ) break ;
                    compresspixels++ ;
                    color_b += *sline++  ;
                    color_g += *sline++  ;
                    color_r += *sline++  ;
                }
            }

            if( compresspixels > 0 ) {
                color_r /= compresspixels ;
                color_g /= compresspixels ;
                color_b /= compresspixels ;
                draw_putpixel( dx+idx, dy+idy, COLOR( color_r, color_g, color_b, 0xff ) );
            }

            // step on source x;
            isx+=sx_step ;
        }

        // step on source line
        isy+=sy_step ;
    }
}

// stretch a 1bit bitmap
void draw_stretchbitmap_1b( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
{
    int idx, idy, isx, isy ;
    int sx_step, sy_step ;
    UINT8 * sline ;
    int sx_compress, sy_compress ;
    sx_compress = (sw+dw-1)/dw ;
    sy_compress = (sh+dh-1)/dh ;

    for( idy=0, isy=0 ; idy<dh ; idy++ ) {

        if( isy*dh > idy*sh ) {
            sy_step = sy_compress-1 ;
        }
        else {
            sy_step = sy_compress ;
        }

        for( idx=0, isx=0; idx<dw; idx++ ) {
            if( isx*dw > idx*sw ) {
                sx_step = sx_compress-1 ;
            }
            else {
                sx_step = sx_compress ;
            }

            int cx, cy ;
            int iix, iiy ;
            int compresspixels=0 ;
            cx=sx_step ;
            if( cx<=0 ) cx=1 ;
            cy=sy_step ;
            if( cy<=0 ) cy=1 ;

            for( iiy=0 ; iiy<cy ;iiy++) {
                int yy=sy+isy+iiy ;
                if( yy>=sy+sh ) break ;
                // source bitmap bits line
                sline = bmp->bits + (bmp->height - yy -1)* bmp->bytes_per_line ;

                for( iix=0; iix<cx; iix++ ) {
                    int xx=sx+isx+iix ;
                    if( xx>=sx+sw ) break ;
                    if( sline[xx/8]  & (0x80>>(xx%8)) ) {
                        compresspixels++ ;
                    }
                }
            }

            if( compresspixels > 0 ) {
                draw_putpixel( dx+idx, dy+idy, draw_drawcolor );
            }
            // step on source x;
            isx+=sx_step ;
        }

        // step on source line
        isy+=sy_step ;
    }
}

// stretch a bitmap
void draw_stretchbitmap( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
{

    if( dw<=0 || dh<=0 ) {
        return ;
    }

    if( sw<=0 || sh<=0 ) {
        sw = bmp->width - sx ;
        sh = bmp->height - sy ;
    }

    if( sx<0 ) {
        sw+=sx ;
        sx=0 ;
        if( sw<=0 ) {
            return ;
        }
    }
    if( sy<0 ) {
        sh+=sy ;
        sy=0 ;
        if( sh<=0 ) {
            return ;
        }
    }
    if( sx+sw > bmp->width ) {
        sw = bmp->width-sx ;
        if( sw<=0 ) {
            return ;
        }
    }
    if( sy+sh > bmp->height ) {
        sh = bmp->height-sy ;
        if( sh<=0 ) {
            return ;
        }
    }

#ifdef EAGLE34
    // add menu area for eagle34
    draw_area_addrect( dx, dy, dw, dh ) ;
#endif	

    if( bmp->bits_per_pixel == 32 ) {
        draw_stretchbitmap_32b(bmp, dx, dy, dw, dh, sx, sy, sw, sh ) ;
    }
    else if( bmp->bits_per_pixel == 24 ) {
        draw_stretchbitmap_24b(bmp, dx, dy, dw, dh, sx, sy, sw, sh ) ;
    }
    else if( bmp->bits_per_pixel == 1 ) {
        draw_stretchbitmap_1b(bmp, dx, dy, dw, dh, sx, sy, sw, sh ) ;
    }
}

// read out bits from frame buffer, should call draw_closebmp () to close bmp after use
int draw_readbitmap(struct BITMAP * bmp, int x, int y, int w, int h )
{
    //	UINT32 color ;
    UINT32 * dline ;
    int dx, sx, sy ;

    // create new bitmap
    if( w<=0 || h<=0 ) {
        return 0 ;
    }
    bmp->width = w ;
    bmp->height = h ;
    bmp->bits_per_pixel=32 ;
    bmp->bytes_per_line=w*4 ;
    bmp->bits = (UINT8 *)malloc(bmp->bytes_per_line * bmp->height) ;  // bits should be align to UINT32
    if( bmp->bits == NULL ) {
        return 0 ;
    }

    dx = x+w ;
    dline = (UINT32 *)bmp->bits ;
    for( sy=y+h-1; sy>=y ; sy-- ){
        for( sx=x; sx<dx; sx++ ) {
            //			color = draw_getpixel( sx, sy ) ;
            //			*dline++ = COLOR_B( color );		// blue
            //			*dline++ = COLOR_G( color );		// green
            //			*dline++ = COLOR_R( color );		// red
            //			*dline++ = COLOR_A( color );		// alpha
            *dline++ = draw_getpixel( sx, sy ) ;
        }
    }
    return 1 ;		// success
}

int draw_fontwidth(struct BITMAP * font)
{
    return (font->width+9) / 16 ;
}

int draw_fontheight(struct BITMAP * font)
{
    return (font->height+4) / 6 ;
}

void draw_text( int dx, int dy, char * text, struct BITMAP * font)
{
    int xp, yp, w, h, col, row ;
    w = draw_fontwidth(font);
    h = draw_fontheight(font);
    while( *text ) {
        if( *text>32 && *text<=127 ) {
            row = (*text - 32 )/16 ;
            col = (*text - 32 )%16 ;
            xp  = font->width * col / 16 ;
            yp  = font->height * row / 6 ;
            draw_bitmap(font, dx, dy, xp, yp, w, h );
        }
        dx+=w ;
        text++;
    }
}

void draw_text_ex( int dx, int dy, char * text, struct BITMAP * font, int fontw, int fonth)
{
    int xp, yp, w, h, col, row ;
    w = draw_fontwidth(font);
    h = draw_fontheight(font);
    while( *text ) {
        if( *text>32 && *text<=127 ) {
            row = (*text - 32 )/16 ;
            col = (*text - 32 )%16 ;
            xp  = font->width * col / 16 ;
            yp  = font->height * row / 6 ;
            draw_stretchbitmap(font, dx, dy, fontw, fonth, xp, yp, w, h );
        }
        dx+=fontw ;
        text++;
    }
}
