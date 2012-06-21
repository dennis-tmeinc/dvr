
// a simple window system base on Eagle 32 frame buffer API

#ifndef __FBWINDOW_H__
#define __FBWINDOW_H__

#include "draw.h"

struct rect {
	int x, y, w, h ;
} ;

// recource is a BITMAP, it can be used as a cursor, a font, or a picture
class resource {
    struct BITMAP m_bmp ;
    
public:
    static char resource_dir[128] ;

    resource() {
        m_bmp.bits = NULL ;
    }
    
    void close() {
        if( m_bmp.bits ) {
			draw_closebmp( &m_bmp );
            m_bmp.bits = NULL ;
        }
    }
    
    int open( char * filename ) {
        char filepath[256] ;
        close();
        if( filename ) {
            sprintf(filepath, "%s/%s", resource_dir, filename );
            return draw_openbmp( filepath, &m_bmp );
        }
        return 0 ;
    }
    
    resource( char * filename ) {
        m_bmp.bits = NULL ;
        open( filename );
    }
    
    ~resource() {
        close();
    }

    int valid() {
        return ( m_bmp.bits!=NULL ) ;
    }

    int width() {
        return m_bmp.width ;
    }

    int height () {
        return m_bmp.height ;
    }

    // return font width, when resource is a font file
    int fontwidth() {
		return draw_fontwidth(&m_bmp);
	}
    
    // return font height, when resource is a font file
    int fontheight() {
		return draw_fontheight(&m_bmp);
	}

    struct BITMAP * bitmap(){
        return &m_bmp ;
    }

    resource & operator = (BITMAP & bitmap) {
        close();
        m_bmp.width = bitmap.width ;
        m_bmp.height = bitmap.height ;
        m_bmp.bits_per_pixel = bitmap.bits_per_pixel ;
        m_bmp.bytes_per_line = bitmap.bytes_per_line ;
        if( bitmap.bits ) {
            int bitsize = m_bmp.height * m_bmp.bytes_per_line ;
            m_bmp.bits = (unsigned char *)malloc( bitsize );
            if( m_bmp.bits ) {
                memcpy( m_bmp.bits, bitmap.bits, bitsize );
            }
        }
        return *this ;
    }

    resource & operator = (resource & res) {
        return (*this = *(res.bitmap()));
    }
    
} ;

class window {
protected:
    int m_id ;					// window ID
    int m_alive ;		    	// window alive or dead
    
    struct rect m_pos ;         // windows position relate to parent window
    int m_screen_x, m_screen_y ; // window's position relate to screen. only vaild during draw period.
    window * m_parent ;			// parent window
    window * m_child ;			// lowest child
    window * m_brother ;		// brother window on top of this window

    // window properties
    int m_show ;				// 0: hide, 1: show

    // window draw support
    int m_redraw ;				// requested to redraw
    struct rect m_drawrect ;

    // window timer
    struct w_timer {
        int id ;
        int interval ;          // in milli-second
        int starttime ;         // in milli-second
    } m_timer ;

public:

    static window * focuswindow ;	    // current focussed window
    static window * mouseonwindow ;	    // current mouse pointed window
    static int      gredraw ;		    // globle window redraw flag
    static char     resourcepath[128] ;  // resource file location

    window() {
        m_alive = 1 ;
        m_id = 0 ;
        m_pos.x = m_pos.y = 0 ;
        m_pos.w = m_pos.h = 0 ;
        m_parent = NULL ;
        m_child = NULL ;
        m_brother = NULL ;
        m_redraw = 0 ;
        m_show = 0 ;			// no show by default
        m_timer.id=0;
    }

    window( window * parent, int id, int x, int y, int w, int h ) {
        m_alive = 1 ;
        m_id = id ;
        m_pos.x = x ;
        m_pos.y = y ;
        m_pos.w = w ;
        m_pos.h = h ;
        m_child = NULL ;
        m_brother = NULL ;
        m_parent = parent ;
        if( parent ) {
            parent->addchild( this );
        }
        m_show = 1 ;			// show by default
        m_timer.id=0;
        redraw();
    }

    virtual ~window()
    {
        hide();
        while( m_child ) {
            delete m_child ;
        }
        if( m_parent ) {
            m_parent->removechild( this ) ;
        }
        if( focuswindow==this ) {
            focuswindow = m_parent ;	// give focus to parent
        }
        if( mouseonwindow == this ) {
            mouseonwindow = NULL ;
        }
    }

    void destroy() {
        m_alive=0 ;
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
        return m_pos.x ;
    }

    int gety() {
        return m_pos.y ;
    }

    int getw() {
        return m_pos.w ;
    }

    int geth() {
        return m_pos.h ;
    }

    void getpos( struct rect * pos ) {
        *pos = m_pos ;
    }

    void setpos( struct rect * pos ) {
        m_pos = *pos ;
        if( m_parent ) {
            m_parent->redraw();
        }
        else {
            redraw();
        }
    }

    // mouse event, x, y is mouse position on parent's windows
    //              parameter :buttons = low 4 bits=button status, high 4 bits=press/release
    int mouseevent( int x, int y, int buttons ) {
        int processed = 0 ;
        if( m_brother ) {
            // brother windows are on top of me, so brother process first.
            processed = m_brother->mouseevent( x, y, buttons );
        }
        if( processed == 0 ) {
            // mouse point inside this window
            if( m_show && x>=m_pos.x && y>=m_pos.y && x<(m_pos.x+m_pos.w) && y<(m_pos.y+m_pos.h) ) {
                if( m_child ) {
                    processed = m_child->mouseevent( x-m_pos.x, y-m_pos.y, buttons );
                }
                if( processed==0) {
                    if( mouseonwindow != this ) {
                        if( mouseonwindow )
                            mouseonwindow->onmouseleave();
                        mouseonwindow = this ;
                        onmouseenter();
                    }
                    onmouse( x-m_pos.x, y-m_pos.y, buttons );
                    processed = 1 ;
                }
            }
        }
        return processed ;
    }

    int isfocus() {
        return focuswindow == this ;
    }

    // set focus window
    int setfocus( int focus=1 ) {
        if( focus ) {
            if( isfocus() ) {
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
            if( isfocus() ) {
                onfocus( 0 ) ;								// issue lost focus event
                focuswindow = NULL ;
                return 1 ;
            }
            else {
                return 0;
            }
        }
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

    int screen_x( int x=0 ) {			// calculate window's x position on screen
        if( m_parent != NULL ) {
            return m_parent->screen_x( m_pos.x + x ) ;
        }
        else {
            return m_pos.x + x ;
        }
    }

    int screen_y( int y=0 ) {			// calculate window's y position on screen
        if( m_parent != NULL  ) {
            return m_parent->screen_y( m_pos.y + y ) ;
        }
        else {
            return m_pos.y + y ;
        }
    }

    // set drawarea in screen coordinate
    void preparepaint( struct rect * drawarea=NULL ){
        m_screen_x = screen_x() ;
        m_screen_y = screen_y() ;
        if( drawarea == NULL ) {
            draw_setdrawarea( m_screen_x, m_screen_y, m_pos.w, m_pos.h );
        }
        else {
            draw_setdrawarea( drawarea->x, drawarea->y, drawarea->w, drawarea->h );
        }
    }

    void redraw( ) {
        if( m_show ) {
            gredraw = 1 ;
            m_redraw = 1 ;
            m_drawrect.x = 0 ;
            m_drawrect.y = 0 ;
            m_drawrect.w = m_pos.w ;
            m_drawrect.h = m_pos.h ;
        }
    }

    // draw window
    //     parentdrawarea is allowed drawing area of parent window (in screen coordinate), NULL for full window
    void draw( struct rect * parentdrawarea=NULL ) {
        static struct rect drawrect ;
        if( m_show ) {
            struct rect mydrawarea ;
            if( parentdrawarea==NULL ) {
                mydrawarea.x = screen_x() ;
                mydrawarea.y = screen_y() ;
                mydrawarea.h = m_pos.h ;
                mydrawarea.w = m_pos.w ;
                drawrect.w = 0 ;                // clean draw rect at first
            }
            else {
                // limit draw area to my window border
                mydrawarea.x = screen_x() ;
                mydrawarea.y = screen_y() ;
                mydrawarea.w = mydrawarea.x+m_pos.w ;
                mydrawarea.h = mydrawarea.y+m_pos.h ;
                if( mydrawarea.x < parentdrawarea->x )
                    mydrawarea.x = parentdrawarea->x ;
                if( mydrawarea.y < parentdrawarea->y )
                    mydrawarea.y = parentdrawarea->y ;
                if( mydrawarea.w > parentdrawarea->x+parentdrawarea->w )
                    mydrawarea.w = parentdrawarea->x+parentdrawarea->w ;
                if( mydrawarea.h > parentdrawarea->x+parentdrawarea->h )
                    mydrawarea.h = parentdrawarea->y+parentdrawarea->h ;
                mydrawarea.w -= mydrawarea.x ;
                mydrawarea.h -= mydrawarea.y ;
            }
            if( mydrawarea.w > 0 && mydrawarea.h > 0 ) {
                // check if draw area over lap with draw rect
                if( (!m_redraw) && drawrect.w>0 && drawrect.h>0 &&
                    mydrawarea.x < (drawrect.x+drawrect.w) &&
                    mydrawarea.y < (drawrect.y+drawrect.h) &&
                    drawrect.x < (mydrawarea.x+mydrawarea.w) &&
                    drawrect.y < (mydrawarea.y+mydrawarea.h) )
                {
                    m_redraw = 1 ;
                }
                
                if( m_redraw ) {
                    preparepaint(&mydrawarea);
                    paint();								        // paint window
                    draw_setdrawarea( 0, 0, 0, 0 );


                    // include mydrawarea into drawrect
                    if( drawrect.w > 0 && drawrect.h > 0 ) {
                        if( (drawrect.x+drawrect.w) > (mydrawarea.x+mydrawarea.w) ) {
                            drawrect.w = drawrect.x+drawrect.w;
                        }
                        else {
                            drawrect.w = mydrawarea.x+mydrawarea.w;
                        }
                        if( (drawrect.y+drawrect.h) > (mydrawarea.y+mydrawarea.h) ) {
                            drawrect.h = drawrect.y+drawrect.h;
                        }
                        else {
                            drawrect.h = mydrawarea.y+mydrawarea.h;
                        }
                        if( drawrect.x > mydrawarea.x ){
                            drawrect.x = mydrawarea.x ;
                        }
                        if( drawrect.y > mydrawarea.y ){
                            drawrect.y = mydrawarea.y ;
                        }
                        drawrect.w -= drawrect.x ;
                        drawrect.h -= drawrect.y ;
                    }
                    else {
                        drawrect = mydrawarea ;
                    }
                }
                if( m_child ) {
                    m_child->draw(&mydrawarea);    // redraw children
                }
            }
            m_redraw=0 ;
        }
        if( m_brother ) {
            m_brother->draw(parentdrawarea) ;
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

    // interval on miliseconds
    void settimer( int interval, int id=1 ) {
        m_timer.id = id ;
        if( id ) {
            if( interval>0 ) {
                m_timer.interval = interval ;
            }
            m_timer.starttime = g_timetick ;
        }
    }

    // interval on miliseconds
    void killtimer( int ) {
        m_timer.starttime = g_timetick ;
        m_timer.id = 0 ;
    }

    void checktimer() {

        if( m_child ) m_child->checktimer();
        if( m_brother ) m_brother->checktimer();

        // add alive check here
        if( m_alive==0 ) {
            delete this ;
        }
        else if( m_timer.id && g_timetick >= (m_timer.starttime+m_timer.interval) ) {
            int tid = m_timer.id ;
            m_timer.id = 0 ;
            ontimer(tid);
        }
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

    void drawline( int x1, int y1, int x2, int y2 ) {
        draw_line( m_screen_x+x1, m_screen_y+y1, m_screen_x+x2, m_screen_y+y2 );
    }

    void drawrect( int x, int y, int w, int h ) {
        draw_rect( m_screen_x+x, m_screen_y+y, w, h );
    }

    void drawrect( rect * drawrect ) {
        draw_rect( m_screen_x+drawrect->x, m_screen_y+drawrect->y, drawrect->w, drawrect->h );
    }

    void fillrect( int x, int y, int w, int h) {
        draw_fillrect( m_screen_x+x, m_screen_y+y, w, h );
    }

    void fillrect( rect * drawrect ) {
        draw_fillrect( m_screen_x+drawrect->x, m_screen_y+drawrect->y, drawrect->w, drawrect->h );
    }

    void drawcircle( int cx, int cy, int r ) {
        draw_circle( m_screen_x+cx, m_screen_y+cy, r );
    }

    void fillcircle( int cx, int cy, int r ) {
        draw_fillcircle(m_screen_x+cx, m_screen_y+cy, r );
    }

    /*
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

    void stretchbitmap( struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh ) 
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

    */

    void drawbitmap(resource & res, int dx, int dy, int sx=0, int sy=0, int w=-1, int h=-1 ) {
        if( res.valid() ) {
            if( w<0 ) {
                w=res.width();
            }
            if( h<0 ) {
                h=res.height();
            }
		    draw_bitmap( res.bitmap(), 
					m_screen_x+dx, m_screen_y+dy, 
					sx, sy, 
					w, h ) ;
        }
	}

	void stretchbitmap( resource & res, int dx, int dy, int dw, int dh, int sx=0, int sy=0, int sw=-1, int sh=-1 ) 
	{
        if( res.valid() ) {
            if( sw<0 ) {
                sw=res.width();
            }
            if( sh<0 ) {
                sh=res.height();
            }
            draw_stretchbitmap( res.bitmap(), 
							m_screen_x+dx, m_screen_y+dy, 
							dw, dh, 
							sx, sy, 
							sw, sh ) ;
        }
	}

    int readbitmap( resource & res, int x, int y, int w, int h ) {
        res.close();
        return draw_readbitmap( res.bitmap(), m_screen_x+x, m_screen_y+y, w, h );
    }

    void drawtext( int dx, int dy, char * text, resource & font )
	{
		draw_text( m_screen_x+dx, m_screen_y+dy, text, font.bitmap() );
	}

	void drawtextex( int dx, int dy, char * text, resource & font, int fontw, int fonth)
	{
		draw_text_ex( m_screen_x+dx, m_screen_y+dy, text, font.bitmap(), fontw, fonth);
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
		if( m_show ) {
			if( m_parent ) {
//				m_parent->invalidate( &m_pos ) ;
				m_parent->redraw();
			}
			m_show = 0 ;
			return 1 ;
		}
		else {
			return 0 ;
		}
	}

	// process key event, return 1 if processed or return 0;
	virtual int key(int keycode, int keydown) {
        int processed = onkey( keycode, keydown );			// issue onkey event
        if( processed == 0 && m_child ) {
            processed = m_child->key( keycode, keydown ) ;
        }
        if( processed == 0 && m_brother ) {
            processed = m_brother->key( keycode, keydown ) ;
        }
		return processed ;
	}

	// processing command
	void command( int id, void * param=NULL ) {
		oncommand( id, param );
	}

	// event handler
protected:
	virtual void paint() {						// paint window
	}

	virtual void oncommand( int id, void * param ) {	// children send command to parent
	}

    // buttons : low 4 bits=button status, high 4 bits=press/release
    virtual void onmouse( int x, int y, int buttons ) {		// mouse event
	}

	virtual void onmouseenter() {							// mouse pointer enter window
	}

	virtual void onmouseleave() {							// mouse pointer leave window
	}

	// keyboard or button input, return 1: processed (key event don't go to children), 0: no processed
	virtual int onkey( int keycode, int keydown ) {		    // keyboard/keypad event
		return 0 ;
	}

	virtual int onfocus( int focus ) {						// change fouse
        return 0 ;
	}

    // timer call back functions
    virtual void ontimer( int id ) {
    }

};

class cursor : public window {
protected:
    resource cursorbits ;
    resource savedbits ;
	int    m_xhotspot, m_yhotspot ;
public:
	cursor(char * cursorfilename, int xhotspot=0, int yhotspot=0) :
		window( NULL, 0, 0, 0, 8, 8 )
	{
		m_show = 0 ;
        setcursor( cursorfilename, xhotspot, yhotspot);
    }

	~cursor() {
		if( m_show != 0 ) {
			hide();
		}
	}

    void setcursor( char * cursorfile, int xhotspot=0, int yhotspot=0) {
        int oshow = m_show ;
        if( oshow ) {
            hide();         // hide old cursor
        }
        cursorbits.close();
        cursorbits.open(cursorfile);
		if( cursorbits.valid() ) {
			m_pos.w = cursorbits.width(); 
			m_pos.h = cursorbits.height() ;
		}
		m_xhotspot = xhotspot ;
		m_yhotspot = yhotspot ;
        if( oshow ) {
            show();         // show new cursor
        }
    }
        
	void move( int x, int y )
	{
		int nx, ny ;
		nx = x - m_xhotspot ;
		ny = y - m_yhotspot ;
		if( m_show ) {
			if( nx!=m_pos.x || ny!=m_pos.y ) {
				hide();
				m_pos.x = nx ;
				m_pos.y = ny ;
				show();
			}
		}
		else {
			m_pos.x = nx ;
			m_pos.y = ny ;
		}
	}

	// overrideable
public:
	virtual int show() {
		if( m_show == 0 ) {
			m_show = 1 ;
			preparepaint();
			readbitmap( savedbits, 0, 0, m_pos.w, m_pos.h );
            if( cursorbits.valid()) {
				setpixelmode (DRAW_PIXELMODE_BLEND);
				drawbitmap( cursorbits, 0, 0, 0, 0, m_pos.w, m_pos.h );
			}
			return 0;
		}
		else {
			return 1 ;
		}
	}

	virtual int hide() {
		if( m_show ) {
			m_show = 0 ;
			if( savedbits.valid() ) {
				preparepaint();
				setpixelmode (DRAW_PIXELMODE_COPY);
				drawbitmap( savedbits, 0,0, 0, 0, 0, 0 );
                savedbits.close();
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
	int    m_state ;					// 0: released, 1: selected (mouse in), 2: pushed
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
		setpixelmode (DRAW_PIXELMODE_COPY);
		setcolor (COLOR(0xe0, 0xe0, 0xe0, 0xc0)) ;
		drawrect(0, 0, m_pos.w, m_pos.h);
		if( m_state == 0 ) {
			setcolor (COLOR(0xe0, 0xe0, 0xe0, 0xc0)) ;
		}
		else if( m_state==1 ) {
			setcolor( COLOR(128, 128, 200, 255 ) );
		}
		else {		// m_state==2
			setcolor( COLOR(56, 200, 1, 255 ) );
		}
		fillrect (1, 1, m_pos.w-2, m_pos.h-2 );
        resource font ( "mono18.font" );
		if( font.valid() ) {
			setpixelmode (DRAW_PIXELMODE_BLEND);
			setcolor( COLOR(0, 0, 0, 0xff ));
			int textwidth = strlen(m_buttontext) ;
			textwidth *= font.fontwidth();
			int textheight = font.fontheight();
			int dx, dy ;
			dx = (m_pos.w-textwidth)/2 ;				// calculate center position
			dy = (m_pos.h-textheight)/2 ;
			drawtext ( dx, dy, m_buttontext, font);
		}
	}

	virtual void onclick() {
		if( m_parent ) {
			m_parent->command(m_id);
		}
	}

    virtual void onmouseenter() {
        m_state=1;
        redraw();
    }
    
	virtual void onmouse( int x, int y, int buttons ) {
        if( buttons & 0x10 ) {          // button press/release
            if( buttons & 1 ) {         // press
                m_state=2 ;
            }
            else {                      // release
                m_state=1 ;
                onclick();
            }
            redraw();
        }
	}

	virtual void onmouseleave() {
		m_state=0;
		redraw();
	}
};	

#endif 	
//  __FBWINDOW_H__
