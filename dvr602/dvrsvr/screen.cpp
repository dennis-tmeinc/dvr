
// screen process

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include "eagle32/davinci_sdk.h"
#include "dvr.h"

#include "fbwindow.h"

extern int eagle32_channels ;

#define MAIN_OUTPUT  (0)

#define SCREEN_ID   (500)

static char mousedevname[] = "/dev/input/mice" ;
static int  mousedev = 0 ;
static int mousemaxx, mousemaxy ;
static int mousex, mousey ;
static int mousebuttons ;

static int ScreenNum = 2 ;
static window * topwindow ;

window * window::focuswindow ;
window * window::mouseonwindow ;
int window::gredraw ;
static cursor * arrow ;


static void screen_setmode()
{
    // setup preview display screen	
    //	SetOutputFormat(STANDARD_NTSC);
    int res = 0 ;
    int i;
    unsigned char displayorder[16] ;
         
    for(i=0; i<16; i++) {
        displayorder[i]=i ;
    }
    
    // clear display
    ClrDisplayParams(MAIN_OUTPUT,0);
         
    res=SetDisplayOrder(MAIN_OUTPUT, displayorder );
    res=SetDisplayParams(MAIN_OUTPUT, ScreenNum);

}

// record light on video screen
class record_light : public window {
    struct BITMAP * light ;
    public:
    record_light( window * parent, int id, int x, int y, int w, int h ) :
        window( parent, id, x, y, w, h ) 
     {
     }
        
        // event handler
    protected:
        virtual void paint() {
        }
};

class video_screen : public window {
    UINT32 m_focuscolor ;
    UINT32 m_color ;
    int    m_screen ;       // 1: first screen, 2: second screen
    int    m_select ;       // 1: currently selected, 0: unselected. (only one selected video screen)
    int    m_playmode ;		// 0: preview, 1: playback (include preview ipcamera)
    
    public:
    video_screen( window * parent, int id, int x, int y, int w, int h ) :
        window( parent, id, x, y, w, h ) 
     {
         m_focuscolor = COLOR( 0xe0, 0x10, 0x10, 0xff);
         m_color = COLOR( 0x30, 0x20, 0x20, 0xff);
         m_screen = id - SCREEN_ID ;
         m_select = 0 ;
         // create recording light
         
         if( eagle32_channels>0 ) {
             // set up preview screen
             SetPreviewScreen(MAIN_OUTPUT,m_screen,1);
             // res=SetPreviewAudio(MAIN_OUTPUT,m_screen,1);
         }
     }
     ~video_screen() {
         if( eagle32_channels>0 ) {
             
             // clean preview screen
             SetPreviewScreen(MAIN_OUTPUT,m_screen,0);
             SetPreviewAudio(MAIN_OUTPUT,m_screen,0);
         }
     }
        
     int select(int s) {
         if( m_select != s && eagle32_channels>0 ) {
             m_select = s ;
             if( m_select ) {
                 // set audio
                 SetPreviewAudio(MAIN_OUTPUT,m_screen,1);
             }
             else {
                 // clear audio
                 SetPreviewAudio(MAIN_OUTPUT,m_screen,0);
             }
         }
         return s ;
     }
        
        // event handler
    protected:
        virtual void paint() {
            setcolor (COLOR(0,0,0,0)) ;	// full transparent
            setpixelmode (DRAW_PIXELMODE_COPY);
            fillrect ( 0, 0, m_w, m_h );	
            if( m_select ) {
                setcolor (m_focuscolor) ;	// focused border
            }
            else {
                setcolor (m_color) ;	    // border color
            }
            drawrect(0, 0, m_w, m_h);		// draw border
            drawrect(1, 1, m_w-2, m_h-2 );
        }
        
        virtual void onmouse( int x, int y, int buttons ) {
            if( (buttons & 0x11)!=0x11 ){
                return ;
            }
            setfocus (1);
        }			
        
        virtual void onfocus( int focus ) {
            int i;
            video_screen * vs ;
            for( i=0; i<ScreenNum; i++ ) {
                vs = (video_screen *) m_parent->findwindow( SCREEN_ID + i + 1 ) ;
                if( vs==NULL ) break;
                if( vs==this ) {
                    select(1);
                }
                else {
                    vs->select(0);
                }
            }
            redraw();
        }
        
};


class controlpanel : public window {
    protected:
        UINT32 m_backgroundcolor;
        window * firstchild ;
        cursor * mycursor ;
    public:
        controlpanel( window * parent,int x, int y, int w, int h )
            : window( parent, 0, x, y, w, h )
     {
         // create all controls
     }
        
     ~controlpanel() {
     }
};

class mainwin : public window {
    protected:
        UINT32 m_backgroundcolor;
        window * firstchild ;
        cursor * mycursor ;
    public:
    mainwin() :
        window( NULL, 0, 0, 0, 720, 480 )
     {
         config dvrconfig(dvrconfigfile);
         video_screen * vs ;
         m_x=0 ;
         m_y=0 ;
         m_w = draw_screenwidth ();
         m_h = draw_screenheight ();
         
         if( ScreenNum==1 ) {
             vs = new video_screen( this, SCREEN_ID+1, 10,  0, 700, 480 );
             vs->select(1);
         }
         else if( ScreenNum==2 ) {
             vs = new video_screen( this, SCREEN_ID+1, 10,  120, 350, 240 );
             vs->select(1);
             vs = new video_screen( this, SCREEN_ID+2, 360, 120, 350, 240 );
         }
         else {
             vs = new video_screen( this, SCREEN_ID+1, 10,  0, 350, 240 );
             vs->select(1);
             vs = new video_screen( this, SCREEN_ID+2, 360, 0, 350, 240 );
         }
         
         string logo;
         logo = dvrconfig.getvalue("VideoOut", "logo");
         
         string control ;
         control = dvrconfig.getvalue("VideoOut", "control" );
         int ctrl_x, ctrl_y, ctrl_w, ctrl_h ;
         ctrl_x=0 ;
         ctrl_y=0 ;
         ctrl_w=0 ;
         ctrl_h=0 ;
         sscanf(control.getstring(), "%d,%d,%d,%d", &ctrl_x, &ctrl_y, &ctrl_w, &ctrl_h );
         if( ctrl_x>0 && ctrl_y>0 && ctrl_w>0 && ctrl_h>0 ) {
             new controlpanel( this, ctrl_x, ctrl_y, ctrl_w, ctrl_h );
         }
     }
        
     ~mainwin(){

     }
        
        // event handler
    protected:
        virtual void paint() {
            setcolor (COLOR( 0, 0, 0x10, 0xff) );
            setpixelmode (DRAW_PIXELMODE_COPY);
            fillrect ( 0, 0, m_w, m_h );
        }
        
        virtual void onkey( int keycode, int keydown ) {
            if( window::focuswindow && window::focuswindow!=this ) {
                window::focuswindow->key( keycode, keydown );
            }
        }
};

int screen_mouseevent(int x, int y, int buttons)
{
    if( arrow ) {
        arrow->move( x, y ) ;
    }
    if( topwindow!=NULL ) {
        topwindow->mouseevent( x, y, buttons );
    }
    return 1;
}

int screen_key( int keycode, int keydown ) 
{
    if( topwindow ) {
        topwindow->key( keycode, keydown );
    }
    return 0;
}

int screen_draw()
{
    if( window::gredraw ) {
        window::gredraw=0 ;
        if( arrow ) {
            arrow->hide();			// hide mouse cursor
        }
        topwindow->draw();			// do all the painting
        draw_resetdrawarea ();		// reset draw area to default
        if( arrow ) {
            arrow->show();			// show mouse cursor
        }
    }
    return 0;
}

struct mouse_event {
    int x, y ;
    int buttons ;
} ;

static int readok(int fd, int usdelay)
{
    struct timeval timeout = { 0, usdelay };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(fd, &fds);
    } else {
        return 0;
    }
}

static int getmouseevent( struct mouse_event * mev, int usdelay )
{
    int offsetx=0, offsety=0;
    int x=mousex, y=mousey ;
    int buttons=mousebuttons ;
    UINT8 mousebuf[8] ;
    if( mousedev>0 ) {
        if( readok(mousedev, usdelay) ) {
            if( read(mousedev, mousebuf, 8) >=3 && 
               ( (mousebuf[0] & 0xc8 )==0x8 ) ) {
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
                   x+=offsetx ;
                   if( x<0 )x=0 ;
                   if( x>mousemaxx ) x=mousemaxx ;
                   y-=offsety ;
                   if( y<0 )y=0;
                   if( y>mousemaxy ) y=mousemaxy ;
               }
        }
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
    else {
        usleep(usdelay);
    }
    return 0 ;
}

struct key_event {
    int keycode ;
    int keydown ;
} ;

static int getkeyevent( struct key_event * kev )
{
    // no implemented
    return 0 ;
}

// called periodically by main process
int screen_io(int usdelay)
{
    struct mouse_event mev ;
    struct key_event kev ;
    
    if( getmouseevent( &mev, usdelay ) ) {
        screen_mouseevent(mev.x, mev.y, mev.buttons );
    }
    else if( getkeyevent( &kev ) ) {
        screen_key ( kev.keycode, kev.keydown );
    }
    else {
        screen_draw();
    }
    return 0;
}

int screen_onframe( cap_frame * capframe )
{
    //	if( g_AvReady[capframe->channel] ) {
    //		InputAvData( g_Playhandle[capframe->channel], capframe->framedata, capframe->framesize );		
    //	}
    return 0;
}


// initialize screen
void screen_init()
{
    config dvrconfig(dvrconfigfile);
    ScreenNum = dvrconfig.getvalueint("VideoOut", "screennum" );
    if( ScreenNum!=4 || ScreenNum!=2 ) {		// support 4, 2, 1 screen mode
        ScreenNum=1 ;			// set to two screen mode by default
    }	
    
    // select Video output format to NTSC
    draw_init(STANDARD_NTSC);
    
    if( eagle32_channels>0 ) {
        SetOutputFormat( STANDARD_NTSC ) ;
        screen_setmode();    
    }
    
    topwindow = new mainwin();
    topwindow->redraw();
    
    // initialize mouse and pointer
    mousedev = open( mousedevname, O_RDONLY );
    if( mousedev>0 ) {
        mousemaxx = draw_screenwidth ()-1 ;
        mousemaxy = draw_screenheight ()-1 ;
        mousex = mousemaxx/2 ;
        mousey = mousemaxy/2 ;
        mousebuttons = 0 ;
        arrow = new cursor( "arrow.cursor", 0, 0 );
        arrow->move( -100, -100 );		// make cursor disppear at first
    }
    
}

// screen finish, clean up
void screen_uninit()
{
    // close mouse device
    if( mousedev>0 ) {
        // remove cursor
        delete arrow ;
        arrow = NULL ;
        
        close( mousedev );
        mousedev = -1 ;
    }
    mousebuttons = 0 ;
    
    // delete all windows
    if( topwindow ) {
        delete topwindow ;
        topwindow = NULL ;
        window::focuswindow = NULL ;
    }
    
    draw_finish();
}
