
// a simple window system base on Eagle 32 frame buffer API

#ifndef __FBWINDOW_H__
#define __FBWINDOW_H__

#include "fbdraw.h"

struct rect {
	int x, y, w, h ;
} ;

class window {
protected:
	int m_x, m_y ;				// window's position relate to parent window
	int m_w, m_h ;				// window's size
	int m_screen_x, m_screen_y ; // window's position relate to screen. only vaild during draw period.
	window * m_parent ;			// parent window
	window * m_child ;			// lowest child
	window * m_brother ;		// brother window on top of this window

	int m_id ;					// window ID

	// window draw support
	int m_redraw ;				// requested to redraw
	// window properties
	int m_show ;				// 0: hide, 1: show

public:

	static window * focuswindow ;	    // current focussed window
	static window * mouseonwindow ;	    // current mouse pointed window
	static int      gredraw ;		    // globle window redraw flag

	window() {
		m_x = 0 ;
		m_y = 0 ;
		m_w = 1 ;
		m_h = 1 ;
		m_parent = NULL ;
		m_child = NULL ;
		m_brother = NULL ;
		m_id = 0 ;
		m_redraw = 1 ;
		m_show = 0 ;			// no show by default
	}

	window( window * parent, int id, int x, int y, int w, int h ) {
		m_id = id ;
		m_x = x ;
		m_y = y ;
		m_w = w ;
		m_h = h ;
		m_child = NULL ;
		m_brother = NULL ;
		m_parent = NULL ;
		if( parent ) {
			parent->addchild( this );
		}
		m_redraw = 1 ;
		m_show = 1 ;			// show by default
	}

	virtual ~window()
	{
		if( focuswindow==this ) {
			focuswindow = NULL ;
		}
		if( mouseonwindow == this ) {
			mouseonwindow = NULL ;
		}
		if( m_parent ) {
			m_parent->removechild( this ) ;
		}
		while( m_child ) {
			delete m_child ;
		}
	}

	void addchild(window * child)
	{
		window * topchild ;
		if( m_child == NULL ) {
			m_child = child ;
		}
		else {
			topchild = m_child ;
			while( topchild->m_brother ) {
				topchild=topchild->m_brother ;
			}
			topchild->m_brother = child ;
		}
		child->m_parent = this ;
		child->m_brother = NULL ;
	}

	void removechild(window * child)
	{
		window * children ;
		window * bro ;
		if( m_child != child ) {
			children = m_child ;
			while( children ) {
				bro = children->m_brother ;
				if( bro == child ) {
					children->m_brother = bro->m_brother ;
					break ;
				}
				children=bro ;
			}
		}
		else {
			m_child = child->m_brother;
		}

		child->m_brother=NULL ;
		child->m_parent=NULL ;
		if( child->isshow() ) {
			redraw();					// need redraw
		}
	}

	void movetotop(window * child) {
		removechild( child );
		addchild( child );
	}

	void movetobottom(window * child) {
		removechild( child );
		child->m_brother = m_child ;
		child->m_parent=this ;
		m_child=child ;
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

	void getpos( struct rect * pos ) {
		pos->x = m_x ;
		pos->y = m_y ;
		pos->w = m_w ;
		pos->h = m_h ;
	}

	void setpos( struct rect * pos ) {
		m_x = pos->x ;
		m_y = pos->y ;
		m_w = pos->w ;
		m_h = pos->h ;
		if( m_parent ) {
			m_parent->redraw();
		}
		else {
			redraw();
		}
	}

	void setpos( int x, int y, int w, int h ) {
		m_x = x ;
		m_y = y ;
		m_w = w ;
		m_h = h ;
		if( m_parent ) {
			m_parent->redraw();
		}
		else {
			redraw();
		}
	}

	// mouse event, if pointer on a child window, the event send to child window
	void mouseevent( int x, int y, int buttons ) {	
		window * child ;
		window * childonmouse ;
		int childx, childy, childx2, childy2 ;
		childonmouse = NULL ;
		child = m_child ;
		while( child ) {
			if(child->isshow()) {
				childx = child->getx();
				childy = child->gety();
				childx2 = child->getw()+childx;
				childy2 = child->geth()+childy;
				if( x>=childx && x<childx2 && y>=childy && y<childy2 ) {
					childonmouse = child ;
				}
			}
			child = child->brother();
		}
		if( childonmouse != NULL ) {
			childx = childonmouse->getx();
			childy = childonmouse->gety();
			childonmouse->mouseevent( x-childx, y-childy, buttons );
		}
		else {
			if( mouseonwindow != this ) {
				if( mouseonwindow ) {
					mouseonwindow->onmouseleave() ;			// issue onmouseleave event
				}
				mouseonwindow = this ;
			}
			onmouse( x, y, buttons );						// issue onmouse event
		}
	}

	// set focus window
	int setfocus( int focus=1 ) {		
		if( focus ) {
			if( focuswindow == this ) {
				return 1 ;
			}
			else {
				if( focuswindow!=NULL ) {
					focuswindow->onfocus(0);				// issue onfocus event
				}
				focuswindow=this ;
				onfocus(1);									// issue onfocus event
				return 0 ;
			}
		}
		else {
			if( focuswindow == this ) {
				onfocus( 0 ) ;								// issue lost focus event
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

	void redraw( int drawoverlappedbrother=1 ) {
		m_redraw = 1 ;
		gredraw = 1 ;
		if( m_show ) {
			window * child ;
			child = m_child ;
			while( child ) {
				child->redraw(0);			// force all child window to redraw
				child = child->brother();
			}
			
			if( drawoverlappedbrother ) {	// redraw brother windows on top
				window * topbrother ;
				topbrother=m_brother ;
				while( topbrother ) {
					// check if brother overlaps
					int x2, y2 ;
					int bx1, bx2, by1, by2 ;
					x2 = m_x+m_w ;
					y2 = m_y+m_h ;
					bx1 = topbrother->getx() ;
					bx2 = bx1+topbrother->getw();
					by1 = topbrother->gety();
					by2 = by1+topbrother->geth();
					if( ! (bx1 >= x2 ||
						   m_x >= bx2 ||
						   by1 >= y2	||
						   m_y >= by2 ) ) {
						topbrother->redraw(1) ;
					}
					topbrother=topbrother->brother();
				}
			}
		}
	}

	int screen_x() {			// calculate window's x position on screen
		if( m_parent != NULL ) {
			return m_parent->screen_x() + m_x ;
		}
		else {
			return m_x ;
		}
	}

	int screen_y() {			// calculate window's y position on screen
		if( m_parent != NULL  ) {
			return m_parent->screen_y() + m_y ;
		}
		else {
			return m_y ;
		}
	}

	void preparepaint( struct rect * drawarea=NULL ){
		m_screen_x = screen_x() ;
		m_screen_y = screen_y() ;
		if( drawarea == NULL ) {
			draw_setdrawarea( m_screen_x, m_screen_y, m_w, m_h ); 
		}
		else {
			draw_setdrawarea( drawarea->x+m_screen_x, drawarea->y+m_screen_y, drawarea->w, drawarea->h ); 
		}
	}

	// draw window and all its children. 
	//     parentdrawarea is allowed drawing area of parent window (int parent coordinate), NULL for full window
	void draw( struct rect * parentdrawarea=NULL ) {
		window * child ;
		struct rect mydrawarea ;
		if( parentdrawarea==NULL ) {
			mydrawarea.x=mydrawarea.y=0;
			mydrawarea.w = m_w ;
			mydrawarea.h = m_h ;
		}
		else {
			// limit draw area to window border
			mydrawarea = *parentdrawarea ;
			mydrawarea.x -= m_x ;
			mydrawarea.y -= m_y ;
			int x2, y2 ;
			x2 = mydrawarea.x+mydrawarea.w ;
			y2 = mydrawarea.y+mydrawarea.h ;
			if( mydrawarea.x<0 ) {
				mydrawarea.x=0 ;
			}
			if( mydrawarea.y<0 ) {
				mydrawarea.y=0 ;
			}
			if( x2 > m_w ) {
				x2 = m_w ;
			}
			if( y2 > m_h ) {
				y2 = m_h ;
			}
			if( mydrawarea.x >= x2 || mydrawarea.y >= y2 ) {
				m_redraw=0 ;
				return ;			// out of draw area ;
			}
			mydrawarea.w = x2-mydrawarea.x ;
			mydrawarea.h = y2-mydrawarea.y ;
		} 
		
		if( m_show ) {
			if( m_redraw ) {
				preparepaint(&mydrawarea);
				paint();								// paint window
			}
			child = m_child ;
			while( child ) {
				child->draw(&mydrawarea);
				child = child->brother();
			}
			m_redraw=0 ;
		}
	}

	int isshow() {
		return m_show ;
	}

	int getid() {
		return m_id;
	}

	void setid( int id ) {
		m_id = id ;
	}

	window * findwindow( int id ) {		// find child window by given id
		window * children ;
		children = m_child ;
		while( children ) {
			if( children->getid() == id ) {
				return children ;
			}
			children = children->brother();
		}
		return NULL ;
	}
		
// drawing API base on window position
public:
	void setcolor( UINT32 color ) {
		draw_setcolor( color ) ;
	}
	
	void setpixelmode( int pixelmode ) {
		draw_setpixelmode( pixelmode );
	}

	void putpixel( int x, int y, UINT32 color ) {
		draw_putpixel( m_screen_x+x, m_screen_y+y, color );
	}

	UINT32 getpixel( int x, int y ) {
		return draw_getpixel( m_screen_x+x, m_screen_y+y );
	}

	void blendpixel( int x, int y, UINT32 color ) {
		draw_blendpixel( m_screen_x+x, m_screen_y+y, color );
	}

	void drawline( int x1, int y1, int x2, int y2 ) {
		draw_line( m_screen_x+x1, m_screen_y+y1, m_screen_x+x2, m_screen_y+y2 );
	}

	void drawrect( int x, int y, int w, int h ) {
		draw_rect( m_screen_x+x, m_screen_y+y, w, h );
	}

	void fillrect( int x, int y, int w, int h) {
		draw_fillrect( m_screen_x+x, m_screen_y+y, w, h );
	}

	void drawcircle( int cx, int cy, int r ) {
		draw_circle( m_screen_x+cx, m_screen_y+cy, r );
	}

	void fillcircle( int cx, int cy, int r ) {
		draw_fillcircle(m_screen_x+cx, m_screen_y+cy, r );
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
					m_screen_x+dx, m_screen_y+dy, 
					sx, sy, 
					w, h ) ;
	}

	void draw_stretchbitmap( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
	{
		draw_stretchbitmap( bmp, 
							m_screen_x+dx, m_screen_y+dy, 
							dw, dh, 
							sx, sy, 
							sw, sh ) ;
	}

	struct BITMAP * readbitmap( int x, int y, int w, int h ) {
		struct BITMAP * bmp ;
		bmp = new struct BITMAP ;
		if( draw_readbitmap( bmp, m_screen_x+x, m_screen_y+y, w, h ) ) {
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
		draw_text( m_screen_x+dx, m_screen_y+dy, text, font );
	}

	void drawtextex( int dx, int dy, char * text, struct BITMAP * font, int fontw, int fonth)
	{
		draw_text_ex( m_screen_x+dx, m_screen_y+dy, text, font, fontw, fonth);
	}

	// overrideable
public:
	virtual int show() {			// return old show status
		if( m_show == 0 ) {
			m_show = 1 ;
			redraw() ;
			return 0 ;
		}
		else {
			return 1 ;
		}
	}

	virtual int hide() {			// return old show status
		if( m_show != 0 ) {
			m_show = 0 ;
			if( m_parent ) {
				m_parent->redraw();
			}
			return 1 ;
		}
		else {
			return 0 ;
		}
	}

	// process key event, return 1 if processed or return 0;
	virtual int key(int keycode, int keydown) {
		if( isfocus() ) {
			onkey( keycode, keydown );			// issue onkey event
			return 1 ;
		}
		else {
			return 0 ;
		}
	}

	// process children command
	void command( int id, void * param=NULL ) {
		oncommand( id, param );
	}

	// event handler
protected:
	virtual void paint() {						// paint window
	}

	virtual void oncommand( int id, void * param ) {	// children send command to parent
	}

	virtual void onmouse( int x, int y, int buttons ) {		// mouse event
	}

	virtual void onmouseleave() {							// mouse pointer leave window
	}

	// keyboard or button input
	virtual void onkey( int keycode, int keydown ) {		// keyboard/keypad event
	}

	virtual void onfocus( int focus ) {						// change fouse
	}

};

class cursor : public window {
protected:
	struct BITMAP * cursorbits ;
	struct BITMAP * savedbits ;
	int    m_xhotspot, m_yhotspot ;
public:
	cursor(char * cursorfilename, int xhotspot, int yhotspot) :
		window( NULL, 0, 0, 0, 8, 8 )
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

	void move( int x, int y )
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

	// overrideable
public:
	virtual int show() {
		if( m_show == 0 ) {
   			if( savedbits != NULL ) {
				closebitmap( savedbits );
			}
			m_show = 1 ;
			preparepaint();
			savedbits = readbitmap( 0, 0, m_w, m_h );
			if( cursorbits!=NULL ) {
				setpixelmode (DRAW_PIXELMODE_BLEND);
				drawbitmap( cursorbits, 0, 0, 0, 0, m_w, m_h );
			}
			return 0;
		}
		else {
			return 1 ;
		}
	}

	virtual int hide() {
		if( m_show != 0 ) {
			m_show = 0 ;
			if( savedbits != NULL ) {
				preparepaint();
				setpixelmode (DRAW_PIXELMODE_COPY);
				drawbitmap( savedbits, 0,0, 0, 0, 0, 0 );
				closebitmap( savedbits );
				savedbits = NULL ;
			}
			return 1 ;
		}
		else {
			return 0;
		}
	}

};

class button : public window {
protected:
	char * m_buttontext ;
	int    m_state ;					// 0: released, 1: focused, 2: pushed
public:
	button( char * buttontext, window * parent, int id, int x, int y, int w, int h ) :
		window( parent, id, x, y, w, h ) 
	{
		int textlen ;
		textlen = strlen( buttontext );
		m_buttontext = new char [textlen+1] ;
		strcpy( m_buttontext, buttontext );
		m_state=0 ;
	}
	~button()
	{
		delete m_buttontext ;
	}

	// event handler
protected:
	virtual void paint() {
		struct BITMAP * font ;
		setpixelmode (DRAW_PIXELMODE_COPY);
		setcolor (COLOR(0xe0, 0xe0, 0xe0, 0xc0)) ;
		drawrect(0, 0, m_w, m_h);
		if( m_state == 0 ) {
			setcolor (COLOR(0xe0, 0xe0, 0xe0, 0xc0)) ;
		}
		else if( m_state==1 ) {
			setcolor( COLOR(128, 128, 200, 255 ) );
		}
		else {		// m_state==2
			setcolor( COLOR(56, 200, 1, 255 ) );
		}
		fillrect (1, 1, m_w-2, m_h-2 );
		font = openbitmap( "mono18.font" );
		if( font ) {
			setpixelmode (DRAW_PIXELMODE_BLEND);
			setcolor( COLOR(0, 0, 0, 0xff ));
			int textwidth = strlen(m_buttontext) ;
			textwidth *= fontwidth (font) ;
			int textheight = fontheight (font );
			int dx, dy ;
			dx = (m_w-textwidth)/2 ;				// calculate center position
			dy = (m_h-textheight)/2 ;
			drawtext ( dx, dy, m_buttontext, font);
			closebitmap (font);
		}
	}

	virtual void onclick() {
		if( m_parent ) {
			m_parent->command(m_id);
		}
	}

	virtual void onmouse( int x, int y, int buttons ) {
		if( (buttons & 0x11)==0x11 ) {		// button click
			setfocus(1);
			if( m_state!=2 ) {
				m_state=2 ;
				redraw();
			}
		}
		else {
			if( m_state==2 && (buttons & 0x11)==0x10 ) {	// button release
				onclick();									// ... do command
				m_state=1;
				redraw();
			}
			if( m_state==0 ) {
				m_state=1 ;
				redraw();
			}
		}
	}

	virtual void onmouseleave() {
		if( m_state!=0 ) {
			m_state=0;
			redraw();
		}
	}
};	

#endif 	
//  __FBWINDOW_H__
