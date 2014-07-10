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

#include "fbdraw.h"

/* frame buffer variables */
static char colordev[] = "/dev/fb/0" ;
static struct fb_var_screeninfo colorbufferinfo;
static UINT16 * colorbuffer ;
static int  colorbuffersize ;
static char alphadev[] = "/dev/fb/2" ;
static struct fb_var_screeninfo alphabufferinfo;
static UINT8  * alphabuffer ;
static int  alphabuffersize ;

// extra line below screen
#define EXTRALINE	(576-480+64)		

static UINT32 draw_drawcolor ;
static UINT32 draw_pixelmode ;		// 0: copy, 1: blend
static int draw_minx, draw_maxx, draw_miny, draw_maxy ;	// drawing area

// Color convertion
#define COLOR32_RGB( r, g, b ) 	 ( (((UINT32)(r))<<16) | (((UINT32)(g))<<8) | ((UINT32)(b)) )

#define COLOR16_RGB( r, g, b ) ( ((((UINT16)(r))&0xf8)<<8) | ((((UINT16)(g))&0xfc)<<3) | ((((UINT16)(b)))>>3) )
#define COLOR16_R( color ) ( ( (color)>>8 ) & 0xf8 )
#define COLOR16_G( color ) ( ( (color)>>3 ) & 0xfc )
#define COLOR16_B( color ) ( ( (color)<<3 ) & 0xf8 )

#define COLOR_32TO16( color ) 	( (((color)>>3)&0x01f) | (((color)>>5)&0x07e0) | (((color)>>8)&0x0f800) )
#define COLOR_16TO32( color )	( (((color)<<3)&0x0f8) | (((color)<<5)&0x0fc00) | ((((UINT32)(color))<<8)&0x0f80000) )

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

int lzmadecsize( unsigned char * lzmabuf );
int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int lzmaoutsize );

// input
//       videoformat: 0=default, 1:NTSC, 2:PAL
// return 
//        0 : failed
//        1 : success
int draw_init(int videoformat)
{

	int    fb_colorfd ;
	int	   fb_alphafd ;
	char * p ;

	fb_colorfd = open(colordev, O_RDWR ) ;
	if( fb_colorfd<=0 ) {
		printf("Can't open color device file!\n");
		return 0 ;
	}

	if (ioctl(fb_colorfd, FBIOGET_VSCREENINFO, &colorbufferinfo) == -1) {
		printf("Error reading variable info for color device file\n" );
		return 0;
	}

	if( videoformat>0 ) {
		if( videoformat==1 ) {
			colorbufferinfo.yres = 480 ;		// NTSC
		}
		else {
			colorbufferinfo.yres = 576 ;		// PAL
		}
		ioctl(fb_colorfd, FBIOPUT_VSCREENINFO, &colorbufferinfo);
	}

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

	if( videoformat>0 ) {
		if( videoformat==1 ) {
			alphabufferinfo.yres = 480 ;		// NTSC
		}
		else {
			alphabufferinfo.yres = 576 ;		// PAL
		}
		ioctl(fb_alphafd, FBIOPUT_VSCREENINFO, &alphabufferinfo);
	}

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

	draw_setcolor(COLOR( 0xff, 0xff, 0xff, 0xff ));
    draw_setpixelmode( DRAW_PIXELMODE_COPY );

	draw_resetdrawarea();

	return 1;
}

// draw finish, clean up
void draw_finish()
{
	// clean up shared memory
	munmap( alphabuffer, alphabuffersize );
	munmap( colorbuffer, colorbuffersize );
}

int draw_screenwidth()
{
	return colorbufferinfo.xres ;
}

int draw_screenheight()
{
	return colorbufferinfo.yres ;
}

int draw_resetdrawarea()
{
	draw_minx = draw_miny = 0 ;
	draw_maxx = colorbufferinfo.xres_virtual ;
	draw_maxy = colorbufferinfo.yres+EXTRALINE ;
	return 0;
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
	if( draw_miny<0 ) {
		draw_miny=0;
	}
	if( draw_maxx>colorbufferinfo.xres_virtual ) {
		draw_maxx=colorbufferinfo.xres_virtual ;
	}
	if( draw_maxy>colorbufferinfo.yres ) {
		draw_maxy=colorbufferinfo.yres ;
	}
	if( draw_minx > draw_maxx || draw_miny > draw_maxy ) {
		draw_minx = 0 ;
		draw_miny = 0 ;
		draw_maxx = 0 ;
		draw_maxy = 0 ;		
	}
	return 0;
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

// unchecked blend pixel
static void draw_blendpixel_32( int x, int y, UINT32 color )
{
    UINT8  * palpha ;
	UINT16 * pcolor ;
	UINT16  ocolor ;
	UINT32  oldr, oldg, oldb ;
	UINT32  nr, ng, nb ;
	UINT8   oldalpha ;
	UINT8   nalpha, alpha ;
	palpha = alphabuffer + (y*alphabufferinfo.xres_virtual+x)/2 ;
	nalpha = color>>29 ;
	if( nalpha>=7 ) {
		colorbuffer[ y*colorbufferinfo.xres_virtual + x ] = (UINT16) COLOR_32TO16(color) ; 
		*palpha |= (x&1)?7:0x70 ;
	}
	else if (nalpha>0 ) {
		oldalpha = (x&1) ? ((*palpha & 0x70) >> 4) : (*palpha & 0x7) ;
		alpha  = nalpha+oldalpha-nalpha*oldalpha/7 ;		// blended alpha
		nr = COLOR_R( color ) ;
		ng = COLOR_G( color ) ;
		nb = COLOR_B( color ) ;
		pcolor = colorbuffer+ y*colorbufferinfo.xres_virtual + x  ;
		ocolor = *pcolor ;
		oldr = COLOR16_R( ocolor ) ;
		oldg = COLOR16_G( ocolor ) ;
		oldb = COLOR16_B( ocolor ) ;
		nr  = ( nr * nalpha + oldr * oldalpha  - oldr * oldalpha * nalpha /7 ) / alpha ;
		ng  = ( ng * nalpha + oldg * oldalpha  - oldg * oldalpha * nalpha /7 ) / alpha ;
		nb  = ( nb * nalpha + oldb * oldalpha  - oldb * oldalpha * nalpha /7 ) / alpha ;
		*pcolor = COLOR16_RGB( nr, ng, nb );
		*palpha = (x&1) ? ( (*palpha & 0x70 )|alpha ) : ((*palpha & 0x07 )|(alpha<<4) );
	}
}

void draw_putpixel( int x, int y, UINT32 color )
{
    UINT8 * palpha ;
	if( x<draw_minx || y<draw_miny || x>=draw_maxx || y>=draw_maxy )
		return ;
	colorbuffer[ y*colorbufferinfo.xres_virtual + x ] = (UINT16) COLOR_32TO16(color) ; 
	palpha = alphabuffer + (y*alphabufferinfo.xres_virtual+x)/2 ;
	*palpha = (x&1) ? ( (*palpha & 0xf0 ) | (color>>29) ) : ( (*palpha & 0x0f ) | ((color>>25)&0x70) ) ;
}

void draw_blendpixel( int x, int y, UINT32 color )
{
	if( x<draw_minx || y<draw_miny || x>=draw_maxx || y>=draw_maxy )
		return ;
	draw_blendpixel_32( x, y, color );
}

UINT32 draw_getpixel( int x, int y)
{
	UINT32 color ;
	if( x<0 || y<0 || x>=colorbufferinfo.xres_virtual || y>=colorbufferinfo.yres )
		return (UINT32)0;
	color = (UINT32)colorbuffer[ y*colorbufferinfo.xres_virtual + x ] ;
	if( x&1 ) {
		return  COLOR_16TO32( color ) | 
				( (UINT32)alphabuffer[ (y*alphabufferinfo.xres_virtual+x)/2 ]<<29 ) ;
	}
	else {
		return  COLOR_16TO32( color ) | 
				( (UINT32)(alphabuffer[ (y*alphabufferinfo.xres_virtual+x)/2 ]&0x70)<<25 ) ;
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
	void (*pixelfunc)( int x, int y, UINT32 color ) ;
	UINT32 color = draw_drawcolor ;
	if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
		pixelfunc = draw_blendpixel ;
	}
	else if( draw_pixelmode == DRAW_PIXELMODE_COPY ){
		pixelfunc = draw_putpixel ;
	}
	else {
		return ;		// unknown draw mode
	}

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

	// scale deltas and store in dx2 and dy2
	dx2 = dx * 2;
	dy2 = dy * 2;

	if (dx > dy)	// dx is the major axis
	{
		// initialize the error term
		err = dy2 - dx;

		for (i = 0; i <= dx; i++)
		{
			pixelfunc( x1, y1, color );
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
			pixelfunc( x1, y1, color );
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
	UINT32 color = draw_drawcolor ;
	void (*pixelfunc)( int x, int y, UINT32 color) ;
	if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
		pixelfunc = draw_blendpixel ;
	}
	else if( draw_pixelmode == DRAW_PIXELMODE_COPY ){
		pixelfunc = draw_putpixel ;
	}
	else {
		return ;		// unknown draw mode
	}
	
	while( x<=y ) {
		if( x==0 ) {
			pixelfunc ( cx, cy+r, color );
			pixelfunc ( cx, cy-r, color );
			pixelfunc ( cx+r, cy, color );
			pixelfunc ( cx-r, cy, color );
		}
		else if( x== y ) {
			pixelfunc ( cx+x, cy+y, color );
			pixelfunc ( cx+x, cy-y, color );
			pixelfunc ( cx-x, cy+y, color );
			pixelfunc ( cx-x, cy-y, color );
			break;
		}
		else {
			pixelfunc ( cx+x, cy+y, color );
			pixelfunc ( cx+x, cy-y, color );
			pixelfunc ( cx-x, cy+y, color );
			pixelfunc ( cx-x, cy-y, color );
			pixelfunc ( cx+y, cy+x, color );
			pixelfunc ( cx+y, cy-x, color );
			pixelfunc ( cx-y, cy+x, color );
			pixelfunc ( cx-y, cy-x, color );
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

	draw_fillrect( cx-y, cy,  y*2+1 ,1 );
	while( x<=y ) {
		draw_fillrect( cx-y, cy+x,  y*2+1 ,1 );
		draw_fillrect( cx-y, cy-x,  y*2+1 ,1 );
		if( x== y )
			break ;
		if( xx+yy > rr ) {
			draw_fillrect( cx-x, cy+y,  x*2+1 ,1 );
			draw_fillrect( cx-x, cy-y,  x*2+1 ,1 );
			yy = yy - y * 2 + 1 ;
			y-- ;
		}
		xx = xx + x * 2 + 1 ;
		x++ ;
	}
}

void draw_rect( int x, int y, int w, int h )
{
	int x2, y2 ;
	x2 = x+w-1 ;
	y2 = y+h-1 ;
	draw_line( x,  y, x2, y );
	draw_line( x2, y, x2, y2);
	draw_line( x2, y2, x, y2);
	draw_line( x, y2, x, y );
}

// unchecked fill
static void draw_fillrect_copy( int x, int y, int w, int h) 
{
    int ix ;
	int dy ;
	int dx ;
	UINT16 * colorline ;
    UINT8  * alphaline ;
	UINT32   color32 ;
    UINT8    alpha ;
	
	dx = x+w-2 ;
	dy = y+h ;
	color32 = draw_drawcolor ;
    alpha   = (color32>>29) | ((color32>>25)&0x70) ;	// double pixel alpha
	color32 = COLOR_32TO16( color32 ) ;
	color32 |= color32<<16 ;				// make a double pixel color
    for( ; y<dy ; y++ ) {
		ix = x ;
	    colorline = & colorbuffer[ y * colorbufferinfo.xres_virtual + ix ] ;
		alphaline = & alphabuffer[ (y * alphabufferinfo.xres_virtual + ix)/2 ];
		if( ix&1 ) {
			*colorline++ = (UINT16)color32 ;
			*alphaline = (*alphaline & 0xf0 )|(alpha & 0x0f) ;
			alphaline++ ;
			ix++ ;
		}
        for( ; ix<=dx ; ix+=2 ) {			// two pixel per cycle
			*(UINT32 *)colorline = color32 ;
			colorline+=2 ;
			* alphaline++ = alpha ;
		}
		if( dx&1 ) {
			*colorline = (UINT16)color32 ;
			*alphaline = (*alphaline & 0x0f )|(alpha & 0xf0) ;
		}
	}
}

void draw_fillrect( int x, int y, int w, int h) 
{
	if( w<0 ) {
		x+=w+1 ;
		w=-w ;
	}
	if( h<0 ) {
		y+=w+1 ;
		h=-h ;
	}
	// check drawing limit
	if( x<draw_minx ) {
		w-=(draw_minx-x) ;
		x=draw_minx ;
	}
	if( y<draw_miny ) {
		h-=(draw_miny-y) ;
		y=draw_miny ;
	}
	if( x+w > draw_maxx ) {
		w = draw_maxx-x ;
	}
	if( y+h > draw_maxy ) {
		h = draw_maxy-y ;
	}

	if( w<=0 || h<=0 ) {
		return ;
	}

	if( draw_pixelmode == DRAW_PIXELMODE_COPY ) {
		draw_fillrect_copy( x, y, w, h);
	}
	else if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
		int ix, iy ;
		int dx, dy ;
		UINT32 color = draw_drawcolor ;
		dx=x+w ;
		dy=y+h ;
		for( iy=y; iy<dy; iy++ ) {
			for(ix=x; ix<dx; ix++) {
				draw_blendpixel_32( ix, iy, color );
			}
		}
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
	filebuf = malloc( filelength );
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
				if( filelength == lzmabufsize ) {
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
	bmp->bits = malloc( bmp->bytes_per_line * bmp->height ) ;
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
	void (*pixelfunc)( int x, int y, UINT32 color ) ;
	if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
		pixelfunc = draw_blendpixel ;
	}
	else if( draw_pixelmode == DRAW_PIXELMODE_COPY ){
		pixelfunc = draw_putpixel ;
	}
	else {
		return ;		// unknown draw mode
	}

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

	if( bmp->bits_per_pixel == 32 ) {
		UINT32 * bits32line ;
		for( iy=0, y=dy ; iy<h ; iy++, y++ ) {
//			sline = bmp->bits + (bmp->height - (sy+iy) -1)* bmp->bytes_per_line + sx*4 ;
			bits32line = (UINT32 *)(bmp->bits + (bmp->height - (sy+iy) -1)* bmp->bytes_per_line + sx*4) ;
				for( x=dx, ix=0; ix<w; ix++, x++) {
//				b = * sline++ ;
//				g = * sline++ ;
//				r = * sline++ ;
//				a = * sline++ ;
//				pixelfunc( x, y, COLOR( r, g, b, a ) );
				pixelfunc( x, y, *bits32line++ );
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
				pixelfunc( x, y, COLOR( r, g, b, 0xff ) );
			}
		}
	}
	else if( bmp->bits_per_pixel == 1 ) {
		for( iy=0, y=dy ; iy<h ; iy++, y++ ) {
			sline = bmp->bits + (bmp->height - (sy+iy) -1)* bmp->bytes_per_line ;
			for( x=dx, ix=0; ix<w; ix++, x++) {
				if( sline[(ix+sx)/8] & (0x80>>((ix+sx)%8)) ) {
					pixelfunc( x, y, draw_drawcolor );
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
	void (*pixelfunc)( int x, int y, UINT32 color ) ;
	if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
		pixelfunc = draw_blendpixel ;
	}
	else if( draw_pixelmode == DRAW_PIXELMODE_COPY ){
		pixelfunc = draw_putpixel ;
	}
	else {
		return ;		// unknown draw mode
	}
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
				pixelfunc( dx+idx, dy+idy, 0 ) ;				// full transparent
			}
			else {
				color_r /= color_alpha ;
				color_g /= color_alpha ;
				color_b /= color_alpha ;
				color_alpha = (color_alpha+compresspixels-1)/compresspixels ;
				pixelfunc( dx+idx, dy+idy, COLOR( color_r, color_g, color_b, color_alpha ) );
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
	void (*pixelfunc)( int x, int y, UINT32 color ) ;
	if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
		pixelfunc = draw_blendpixel ;
	}
	else if( draw_pixelmode == DRAW_PIXELMODE_COPY ){
		pixelfunc = draw_putpixel ;
	}
	else {
		return ;		// unknown draw mode
	}
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
				pixelfunc( dx+idx, dy+idy, COLOR( color_r, color_g, color_b, 0xff ) );
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
	void (*pixelfunc)( int x, int y, UINT32 color ) ;
	if( draw_pixelmode == DRAW_PIXELMODE_BLEND ) {
		pixelfunc = draw_blendpixel ;
	}
	else if( draw_pixelmode == DRAW_PIXELMODE_COPY ){
		pixelfunc = draw_putpixel ;
	}
	else {
		return ;		// unknown draw mode
	}
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
				pixelfunc( dx+idx, dy+idy, draw_drawcolor );
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
