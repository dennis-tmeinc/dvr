
// a simple window system base on Eagle 32 frame buffer API

#ifndef __FBWINDOW_H__
#define __FBWINDOW_H__

#include "fbdraw.h"

class window {
protected:
	int m_x, m_y, m_w, m_h ;	// window's position relate to parent window
	int m_offset_x, m_offset_y ; // window's position offset. only vaild during draw period.
	window * m_parent ;			// parent window
	window * m_child ;			// lowest child
	window * m_brother ;		// brother window on top of this window

	int m_id ;				// my id

	int	m_idnext ;			// TAB order next
	int m_idprev ;			// TAB order prev

	// window draw support
	int m_redraw ;			// requested to redraw
	int m_show ;			// 0: hide, 1: show

public:

	static window * topwindow ;			// top level window ;
	static window * focuswindow ;	// current focussed window
	static int      gredraw ;		// globle window redraw flag

	window() {
		m_x = 0 ;
		m_y = 0 ;
		m_w = 1 ;
		m_h = 1 ;
		m_parent = NULL ;
		m_child = NULL ;
		m_brother = NULL ;
		m_id = 0 ;
		m_idnext = 0 ;			// TAB order next
		m_idprev = 0 ;			// TAB order prev
		m_redraw = 1 ;
		m_show = 1 ;			// show by default
	}

	window( window * parent, int id, int x, int y, int w, int h ) {
		m_id = id ;
		m_x = x ;
		m_y = y ;
		m_w = w ;
		m_h = h ;
		m_child = NULL ;
		m_brother = NULL ;
		m_idnext = 0 ;			// TAB order next
		m_idprev = 0 ;			// TAB order prev
		m_parent = parent ;
		if( parent ) {
			parent->addchild( this );
		}
		m_redraw = 1 ;
		m_show = 1 ;			// show by default
	}

	virtual ~window()
	{
		if( m_parent ) {
			m_parent->removechild( this ) ;
		}
		while( m_child ) {
			delete m_child ;
		}
	}

	void addchild(window * child)
	{
		child->m_brother = m_child ;
		m_child = child ;
	}

	void removechild(window * child)
	{
		window * children ;
		window * bro ;
		m_redraw = 1 ;						// need redraw
		if( m_child == child ) {
			m_child = m_child->m_brother;
			return ;
		}
		children = m_child ;
		while( children ) {
			bro = children->m_brother ;
			if( bro == child ) {
				children->m_brother = child->m_brother ;
				return ;
			}
			children=bro ;
		}
	}

	int	getx() {
		return m_x ;
	}

	int gety() {
		return m_y ;
	}
	
	int getw() {
		return m_w ;
	}
	
	int geth() {
		return m_h ;
	}

	void setpos( int x, int y, int w, int h ) {
		m_x = x ;
		m_y = y ;
		m_w = w ;
		m_h = h ;
		m_redraw = 1 ;
		if( m_parent ) {
			m_parent->redraw();
		}
	}

	// mouse event test
	int mousetest( int x, int y, int buttons ) {	
		window * child ;
		window * childonmouse ;
		int childx, childy, childx2, childy2 ;
		if( x>=0 && x<m_w && y>=0 && y<m_h ) {
			child = m_child ;
			childonmouse = NULL ;
			while( child ) {
				childx = child->getx();
				childy = child->gety();
				childx2 = child->getw()+childx;
				childy2 = child->geth()+childy;
				if( x>=childx && x<childx2 && y>=childy && y<childy2 ) {
					childonmouse = child ;
				}
				child = child->brother();
			}
			if( childonmouse != NULL ) {
				childx = childonmouse->getx();
				childy = childonmouse->gety();
				childonmouse->mousetest( x-childx, y-childy, buttons );
			}
			else {
				onmouse( x, y, buttons );
			}
			return 1 ;
		}
		return (0);
	}

	// set focus window
	int setfocus( int focus=1 ) {		
		if( focus ) {
			if( focuswindow == this ) {
				return 1 ;
			}
			else {
				if( focuswindow!=NULL ) {
					focuswindow->setfocus(0);
				}
				focuswindow=this ;
				onfocus(1);
				return 0 ;
			}
		}
		else {
			if( focuswindow == this ) {
				onfocus( 0 ) ;
				focuswindow = NULL ;
				return 1 ;
			}
			else {
				return 0;
			}
		}
	}

	int isfocus() {
		return focuswindow == this ;
	}

	window * brother() {
		return m_brother ;
	}

	window * child() {
		return m_child ;
	}
	
	window * parent() {
		return m_parent ;
	}

	void redraw() {
		m_redraw = 1 ;
		gredraw = 1 ;
	}

	int offset_x() {			// calculate window's x position on screen
		if( m_parent != NULL ) {
			return m_parent->offset_x() + m_x ;
		}
		else {
			return m_x ;
		}
	}

	int offset_y() {			// calculate window's y position on screen
		if( m_parent != NULL  ) {
			return m_parent->offset_y() + m_y ;
		}
		else {
			return m_y ;
		}
	}

	void draw() {
		window * child ;
		if( m_show ) {
			if( m_redraw ) {
				m_offset_x = offset_x() ;
				m_offset_y = offset_y() ;
				draw_setdrawarea( m_offset_x, m_offset_y, m_w, m_h ); 
				paint();
				m_redraw=0 ;
				draw_resetdrawarea();
			}
			child = m_child ;
			while( child ) {
				if( m_redraw ) {
					child->redraw();			// force child window to redraw
				}
				child->draw();
				child = child->brother();
			}
			m_redraw=0 ;
		}
	}

	int isshow() {
		return m_show ;
	}

	int getID() {
		return m_id;
	}

	void setID( int id ) {
		m_id = id ;
	}

	window * findwindow( int id ) {		// find window by given id
		window * found ;
		window * children ;
		if( m_id == id ) {
			return this ;
		}
		children = m_child ;
		while( children ) {
			found = children->findwindow( id ) ;
			if( found ) {					
				return found ;
			}
			children = children->brother();
		}
		return NULL ;
	}

	// overrideable

	virtual void show() {
		if( m_show == 0 ) {
			m_show = 1 ;
			redraw() ;
		}
	}

	virtual void hide() {
		if( m_show != 0 ) {
			m_show = 0 ;
			if( m_parent ) {
				m_parent->redraw();
			}
		}
	}

	// event handler

	virtual void paint() {
	}
	virtual void onmouse( int x, int y, int buttons ) {
	}
	virtual void onfocus( int focus ) {
	}

			
	// drawing interface base on window position

	void setcolor( UINT32 color ) {
		draw_setcolor( color ) ;
	}
	
	void setpixelmode( int pixelmode ) {
		draw_setpixelmode( pixelmode );
	}

	void putpixel( int x, int y, UINT32 color ) {
		draw_putpixel( m_offset_x+x, m_offset_y+y, color );
	}

	UINT32 getpixel( int x, int y ) {
		return draw_getpixel( m_offset_x+x, m_offset_y+y );
	}

	void blendpixel( int x, int y, UINT32 color ) {
		draw_blendpixel( m_offset_x+x, m_offset_y+y, color );
	}

	void drawline( int x1, int y1, int x2, int y2 ) {
		draw_line( m_offset_x+x1, m_offset_y+y1, m_offset_x+x2, m_offset_y+y2 );
	}

	void drawrect( int x, int y, int w, int h ) {
		draw_rect( m_offset_x+x, m_offset_y+y, w, h );
	}

	void fillrect( int x, int y, int w, int h) {
		draw_fillrect( m_offset_x+x, m_offset_y+y, w, h );
	}

	void drawcircle( int cx, int cy, int r ) {
		draw_circle( m_offset_x+cx, m_offset_y+cy, r );
	}

	void fillcircle( int cx, int cy, int r ) {
		draw_fillcircle(m_offset_x+cx, m_offset_y+cy, r );
	}

	struct BITMAP * openbitmap( char * filename ) {
		struct BITMAP * bmp ;
		bmp = new struct BITMAP ;
		if( draw_openbmp( filename, bmp ) ) {
			return bmp ;
		}
		else {
			delete bmp ;
			return NULL ;
		}
	}

	void closebitmap( struct BITMAP * bmp ) {
		if( bmp ) {
			draw_closebmp( bmp );
			delete bmp ;
		}
	}
	
	void drawbitmap(struct BITMAP * bmp, int dx, int dy, int sx, int sy, int w, int h ) {
		draw_bitmap( bmp, 
					m_offset_x+dx, m_offset_y+dy, 
					sx, sy, 
					w, h ) ;
	}

	void draw_stretchbitmap( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
	{
		draw_stretchbitmap( bmp, 
							m_offset_x+dx, m_offset_y+dy, 
							dw, dh, 
							sx, sy, 
							sw, sh ) ;
	}

	struct BITMAP * readbitmap( int x, int y, int w, int h ) {
		struct BITMAP * bmp ;
		bmp = new struct BITMAP ;
		if( draw_readbitmap( bmp, m_offset_x+x, m_offset_y+y, w, h ) ) {
			return bmp ;
		}
		else {
			delete bmp ;
			return NULL;
		}
	}

	int fontwidth( struct BITMAP * font ) {
		return draw_fontwidth(font);
	}
	
	int fontheight( struct BITMAP * font ) {
		return draw_fontheight( font );
	}

	void drawtext( int dx, int dy, char * text, struct BITMAP * font )
	{
		draw_text( m_offset_x+dx, m_offset_y+dy, text, font );
	}

	void drawtextex( int dx, int dy, char * text, struct BITMAP * font, int fontw, int fonth)
	{
		draw_text_ex( m_offset_x+dx, m_offset_y+dy, text, font, fontw, fonth);
	}

};

#define ID_CURSOR	(0x111)

class cursor : public window {
protected:
	struct BITMAP * cursorbits ;
	struct BITMAP * savedbits ;
	int    m_xhotspot, m_yhotspot ;
public:
	cursor(char * cursorfilename, int xhotspot, int yhotspot) :
		window( NULL, ID_CURSOR, 0, 0, 32, 32 )
	{
		cursorbits = openbitmap( cursorfilename ) ;
		if( cursorbits ) {
			m_w = cursorbits->width ;
			m_h = cursorbits->height;
		}
		savedbits = NULL ;
		m_xhotspot = xhotspot ;
		m_yhotspot = yhotspot ;
		m_show = 0 ;
	}

	~cursor() {
		if( m_show != 0 ) {
			hide();
		}
		if( cursorbits!= NULL ) {
			closebitmap( cursorbits );
		}
		if( savedbits!= NULL ) {
			closebitmap( savedbits ) ;
		}
	}

	virtual void move( int x, int y )
	{
		int nx, ny ;
		nx = x - m_xhotspot ;
		ny = y - m_yhotspot ;
		if( m_show ) {
			if( nx!=m_x || ny!=m_y ) {
				hide();
				m_x = nx ;
				m_y = ny ;
				show();
			}
		}
		else {
			m_x = nx ;
			m_y = ny ;
		}
	}

	virtual void show() {
		if( m_show == 0 ) {
			m_show = 1 ;
			m_offset_x = offset_x() ;
			m_offset_y = offset_y() ;
			savedbits = readbitmap( 0, 0, m_w, m_h );
			if( cursorbits!=NULL ) {
				setpixelmode (DRAW_PIXELMODE_BLEND);
				drawbitmap( cursorbits, 0, 0, 0, 0, m_w, m_h );
			}
		}
	}

	virtual void hide() {
		if( m_show != 0 ) {
			m_show = 0 ;
			if( savedbits != NULL ) {
				m_offset_x = offset_x() ;
				m_offset_y = offset_y() ;
				setpixelmode (DRAW_PIXELMODE_COPY);
				drawbitmap( savedbits, 0,0, 0, 0, 0, 0 );
				closebitmap( savedbits );
				savedbits = NULL ;
			}
		}
	}

};

class button : public window {
protected:
	UINT32 m_buttonfacecolor ;
	UINT32 m_bordorcolor ;
	int  m_state ;					// 0: released, 1: focused, 2: pushed
	char * m_buttontext ;
public:
	button( char * buttontext, window * parent, int id, int x, int y, int w, int h ) :
		window( parent, id, x, y, w, h ) 
	{
		int textlen ;
		m_bordorcolor = COLOR(0xe0, 0xe0, 0xe0, 0xff) ;
		m_buttonfacecolor = COLOR(0x80, 0x80, 0x20, 0xff);
		textlen = strlen( buttontext );
		m_buttontext = new char [textlen+2] ;
		strcpy( m_buttontext, buttontext );
		m_state=0 ;
	}
	~button()
	{
		delete m_buttontext ;
	}

	virtual void paint() {
		struct BITMAP * font ;
		setpixelmode (1);
		setcolor (m_bordorcolor) ;
		drawrect(0, 0, m_w, m_h);
		if( m_state == 0 ) {
			setcolor (m_buttonfacecolor) ;
		}
		else if( m_state==1 ) {
			setcolor( COLOR(128, 128, 200, 255 ) );
		}
		else {
			setcolor( COLOR(56, 200, 1, 255 ) );
		}
		fillrect (1, 1, m_w-2, m_h-2 );
		font = openbitmap( "mono18.font" );
		if( font ) {
			setcolor( COLOR(0, 0, 0, 0xff ));
			int textwidth = strlen(m_buttontext) ;
			textwidth *= fontwidth (font) ;
			int textheight = fontheight (font );
			int dx, dy ;
			dx = (m_w-textwidth)/2 ;
			dy = (m_h-textheight)/2 ;
			drawtext ( dx, dy, m_buttontext, font);
			closebitmap (font);
		}
	}

	virtual void onmouse( int x, int y, int buttons ) {
		setfocus(1);
		if( buttons & 0x1 ) {
			if( m_state!=2 ) {
				m_state=2 ;
				redraw();
			}
		}
		else {
			// click of a button
			if( m_state!=1 ) {
				m_state=1 ;
				redraw();
			}
		}
	}

	virtual void onfocus( int focus ) {
		int state ;
		if( focus ) {
			state = 1 ;
		}
		else {
			state = 0 ;
		}
		if( state != m_state ) {
			m_state=state;
			redraw();
		}
	}
			
};	

#endif 	
//  __FBWINDOW_H__
