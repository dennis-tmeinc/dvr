#include <unistd.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>


#include "fbdraw.h"
#include "fbwindow.h"

int app_quit=0 ;

void sig_handler(int signum)
{
   app_quit=1 ;
}

char mousedevname[] = "/dev/input/mice" ;
static int  mousedev ;
static int mousemaxx, mousemaxy ;
static int mousex, mousey ;
static int mousebuttons ;
static int mousedelaytime ;


// return 
//        0 : failed
//        1 : success
int appinit()
{
	draw_init(0);
	mousedev = open( mousedevname, O_RDONLY );
	if( mousedev>0 ) {
		mousemaxx = draw_screenwidth ()-1 ;
		mousemaxy = draw_screenheight ()-1 ;
		mousex = mousemaxx/2 ;
		mousey = mousemaxy/2 ;
		mousebuttons = 0 ;
		mousedelaytime = 12500 ;
	}
	return 1;
}
// app finish, clean up
void appfinit()
{
	if( mousedev>0 ) {
		close( mousedev );
	}
	draw_finish();
}

struct mouse_event {
	int x, y ;
	int buttons ;
} ;


int readok(int fd)
{
	struct timeval timeout = { 0, mousedelaytime };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
		return FD_ISSET(fd, &fds);
	} else {
		return 0;
	}
}

int getmouseevent( struct mouse_event * mev )
{
	int res = 0 ;
    int rbytes ;
	int offsetx, offsety, x, y ;
	int buttons ;
	UINT8 mousebuf[10] ;
	if( mousedev>0 ) {
		if( readok(mousedev) ) {
			rbytes = read(mousedev, mousebuf, 10) ;
			if( rbytes>=3 ) {
				if( (mousebuf[0] & 0xc8 )==0x8 ) {
					if( mousebuf[0] & 0x10 ) {
						offsetx = ((-1)&(~0x0ff)) | mousebuf[1] ;
					}
					else {
						offsetx = mousebuf[1] ;
					}
					if( mousebuf[0] & 0x20 ) {
						offsety = ((-1)&(~0x0ff)) | mousebuf[2] ;
					}
					else {
						offsety = mousebuf[2] ;
					}
					buttons = mousebuf[0] & 0x7 ;
				}
				x=mousex+offsetx ;
				if( x<0 )x=0 ;
				if( x>mousemaxx ) x=mousemaxx ;
				y=mousey-offsety ;
				if( y<0 )y=0;
				if( y>mousemaxy ) y=mousemaxy ;
				if( x!=mousex || y!=mousey || buttons!=mousebuttons ) {
					mev->x = x ;
					mev->y = y ;
					mev->buttons = buttons | ( (buttons<<4)^(mousebuttons<<4) ) ;
					if( mousex!=x || mousey!=y ) {
						mev->buttons |= 0x100 ;
					}
					mousex=x ;
					mousey=y ;
					mousebuttons=buttons ;
					return 1 ;
				}
			}
		}
	}
	return res ;
}

void testline( )
{
    int i ;

	draw_setcolor( COLOR(0, 0, 0x80, 0xff)  );
	draw_fillscreen();

	draw_setcolor( COLOR(0xff, 0xff, 0, 0xff ));
	draw_fillcircle( 400, 400, 100 );

	draw_setcolor( COLOR(0xff, 0xff, 0x80, 0xff)  );
	draw_fillrect( 21, 50, 600, 50 );
	draw_setcolor( COLOR( 0x00, 0, 0, 0 ) );
    draw_fillrect( 22, 51, 598, 48 );
	draw_setcolor( COLOR( 0x80, 0xff, 0, 0xff ) );

//	DrawLine( 601, 100, 601, 500, color );

	int t = 571 ;
	draw_fillrect( t, 100, 1, 400);
	draw_fillrect( t+6, 100, 2, 400 );
	draw_fillrect( t+12, 100, 3, 400 );
	draw_fillrect( t+18, 100, 4, 400 );

	draw_setcolor( COLOR(0xff, 0xff, 0xff, 0xff)  );
    for(i=100; i<200; i++ ) { 
		draw_line( i+100, 200, i+200, 300 );
	}

	draw_rect( 150, 150,  100, 100 );

	draw_setcolor(COLOR( 0, 0, 0, 0xff));

	struct BITMAP bmp ;
	if( draw_openbmp ("mono32b.font", &bmp) ) {
		draw_setpixelmode (0);
		draw_setcolor(COLOR( 0xff, 0xff, 0, 0x40));
		draw_stretchbitmap( &bmp, 400, 283, 800, 850, 0, 0, 0, 0);
		draw_closebmp( &bmp );
	}

	draw_setcolor(COLOR(0xf0, 0x0, 0, 0xf0));

	if( draw_openbmp( "mono32b.font", &bmp) ) {
		draw_text( 60, 150, " !\"#$%&'()*+,", &bmp );
		draw_text( 60, 150+1*40, "0123456789:;<=>?", &bmp );
		draw_text( 60, 150+2*40, "@ABCDEFGHIJKLMNO", &bmp );
		draw_text( 60, 150+3*40, "PQRSTUVWXYZ[\\]^_", &bmp );
		draw_text( 60, 150+4*40, "`abcdefghijklmno", &bmp );
		draw_text( 60, 150+5*40, "pqrstasdfasdfuvwxyz{|}~ ", &bmp );
		draw_text( 60, 450, "60x450", &bmp);
		draw_text_ex( 60, 500, "60x450", &bmp, 20, 20 );
		draw_line( 60, 450, 200, 450 );
		draw_text( 60, 550, "60x550", &bmp);
		draw_line( 60, 550, 200, 550 );
		draw_closebmp (&bmp);
	}

	if( draw_readbitmap(&bmp, 100, 200, 100, 100 ) ) {
		draw_bitmap (&bmp, 500, 100, 0, 0, 0, 0 );
		draw_closebmp (&bmp) ;
	}

}

class childwin : public window {
protected:
	UINT32 m_color ;	
public:
	childwin( window * parent, int id, int x, int y, int w, int h, UINT32 color = COLOR( 0xff, 0xff, 0, 0xd0 ) ) :
		window( parent, id, x, y, w, h ) 
	{
		m_color = color;
	}

	virtual void paint() {
		setcolor (m_color) ;
		setpixelmode (DRAW_PIXELMODE_COPY);
		fillcircle ( m_w/2, m_h/2, m_h/2-2 );
		struct BITMAP * bmp ;
		bmp = readbitmap( m_w/2, m_h/2, 1, 1 );
		if( bmp ) {
			closebitmap(bmp);
		}
	}
			
};

class mainwin : public window {
protected:
	UINT32 m_backgroundcolor;
	window * firstchild ;
public:
	mainwin() :
		window() 
	{
		m_x=0 ;
		m_y=0 ;
		m_w = draw_screenwidth ();
		m_h = draw_screenheight ();
		m_id = 1 ;
		m_backgroundcolor = COLOR( 0, 0, 0x80, 0x80);

		firstchild = new childwin( this, 2, 10, 10, 50, 50 );
		new childwin( this, 3, 50, 200, 101, 50, COLOR(0x0, 0xff, 0, 0x55) );

		new button( "Push 1",  this, 4, 200, 300, 120, 30 );
		new button( "Push 2",  this, 5, 200, 340, 120, 30 );
		new button( "Push 31 abcdefgh ijk l", this, 6, 200, 380, 120, 30 );
		new button( "OK",      this, 7, 200, 420, 120, 30 );
		new button( "Cancel",  this, 8, 200, 460, 120, 30 );
	}

	virtual void paint() {
		setcolor (m_backgroundcolor) ;
		setpixelmode (0);
		fillrect ( 0, 0, m_w, m_h );
	}

	virtual void onmouse( int x, int y, int buttons ) {
		setfocus(1);
	}

	virtual void onfocus( int focus ) {
	}
};		

window * window::topwindow ;
window * window::focuswindow ;
int window::gredraw ;

void testwin()
{
	cursor * arrow ;
	window::topwindow = new mainwin();
	window::topwindow->draw();

	arrow = new cursor("arrow.cursor", 0, 0);
	arrow->show();

	struct mouse_event mev ;
	while(1) {
		if( getmouseevent( &mev ) ) {
			arrow->move( mev.x, mev.y ) ;
			window::topwindow->mousetest( mev.x, mev.y, mev.buttons );
		}
		else {
			if( window::gredraw ) {
				arrow->hide();
				window::topwindow->draw();
				arrow->show();
				window::gredraw=0 ;
			}
		}
	}

	delete arrow ;
	delete window::topwindow ;
}

int main()
{
	if( appinit()==0 ) {
		return 1;
	}

	testline();	

	testwin();


	appfinit();

	return 0;
}

