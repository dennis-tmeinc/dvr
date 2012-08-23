/*
 file yagf.cpp

    Yet Another G-Force

    ( alias of new tab102b, refer original tab102b codes to glog )

*/

#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "netdbg.h"
#include "mcu.h"
#include "diomap.h"
#include "yagf.h"

// functions from ioprocess.cpp
int dvr_log(char *fmt, ...);

extern int gforce_log_enable;
extern int gforce_available;
extern int gforce_crashdata_enable;

// direction table
// 0:Front,1:Back, 2:Right, 3:Left, 4:Bottom, 5:Top

static char yagf_direction_table[24][3] =
{
    {0, 2, 0x62}, // Forward:front, Upward:right    Leftward:top
    {0, 3, 0x52}, // Forward:Front, Upward:left,    Leftward:bottom
    {0, 4, 0x22}, // Forward:Front, Upward:bottom,  Leftward:right
    {0, 5, 0x12}, // Forward:Front, Upward:top,    Leftward:left
    {1, 2, 0x61}, // Forward:back,  Upward:right,    Leftward:bottom
    {1, 3, 0x51}, // Forward:back,  Upward:left,    Leftward:top
    {1, 4, 0x21}, // Forward:back,  Upward:bottom,    Leftward:left
    {1, 5, 0x11}, // Forward:back, Upward:top,    Leftward:right
    {2, 0, 0x42}, // Forward:right, Upward:front,    Leftward:bottom
    {2, 1, 0x32}, // Forward:right, Upward:back,    Leftward:top
    {2, 4, 0x28}, // Forward:right, Upward:bottom,    Leftward:back
    {2, 5, 0x18}, // Forward:right, Upward:top,    Leftward:front
    {3, 0, 0x41}, // Forward:left, Upward:front,    Leftward:top
    {3, 1, 0x31}, // Forward:left, Upward:back,    Leftward:bottom
    {3, 4, 0x24}, // Forward:left, Upward:bottom,    Leftward:front
    {3, 5, 0x14}, // Forward:left, Upward:top,    Leftward:back
    {4, 0, 0x48}, // Forward:bottom, Upward:front,    Leftward:left
    {4, 1, 0x38}, // Forward:bottom, Upward:back,    Leftward:right
    {4, 2, 0x68}, // Forward:bottom, Upward:right,    Leftward:front
    {4, 3, 0x58}, // Forward:bottom, Upward:left,    Leftward:back
    {5, 0, 0x44}, // Forward:top, Upward:front,    Leftward:right
    {5, 1, 0x34}, // Forward:top, Upward:back,    Leftward:left
    {5, 2, 0x64}, // Forward:top, Upward:right,    Leftward:back
    {5, 3, 0x54}  // Forward:top, Upward:left,    Leftward:front
};


int yagf_cmd(char * rsp, int cmd, int datalen=0, ...);

// send YAGF commnd and wait for responds
int yagf_cmd(char * rsp, int cmd, int datalen, ...)
{
    int res ;
    va_list v ;
    va_start( v, datalen );
    res = mcu_cmd_va( ID_YAGF, rsp, cmd, datalen, v );
    return res ;
}

void yagf_setrtc()
{
    struct timeval tv ;
    time_t tt ;
    struct tm t ;
    gettimeofday(&tv, NULL);
    tt = (time_t) tv.tv_sec ;
    gmtime_r( &tt, &t );
    yagf_cmd(NULL, YAGF_CMD_SETRTC, 7,
                    bcd( t.tm_sec ),
                    bcd( t.tm_min ),
                    bcd( t.tm_hour ),
                    bcd( t.tm_wday + 1 ),
                    bcd( t.tm_mday ),
                    bcd( t.tm_mon + 1 ),
                    bcd( t.tm_year-100 ) ) ;
}

void yagf_bootready()
{
    yagf_cmd(NULL, YAGF_CMD_BOOTUPREADY );
}

void yagf_enablepeak()
{
    yagf_cmd(NULL, YAGF_CMD_ENABLEPEAK );
}


// recevied gforce sensor data
void yagf_log( int x, int y, int z )
{
    netdbg_print("yagf value X=%d, Y=%d, Z=%d\n",
                 x,
                 y,
                 z );

    float gf, gr, gd ;
    gf = (x-512.0)/ 37.0 ;
    gr = (y-512.0)/ 37.0 ;
    gd = (z-512.0)/ 37.0 ;

    // save to log
    dio_lock();
    p_dio_mmap->gforce_serialno++ ;
    p_dio_mmap->gforce_forward = gf ;
    p_dio_mmap->gforce_right = gr ;
    p_dio_mmap->gforce_down = gd ;
    dio_unlock();
}


void yagf_input( char * ibuf )
{
    mcu_response( ibuf );                 		// possitive response, do it anyway
    if( ibuf[3]==YAGF_REPORT_PEAK ) {
        int x, y, z ;
        x = ((unsigned int)(unsigned char)ibuf[5]) ;
        x <<= 8 ;
        x = ((unsigned int)(unsigned char)ibuf[6]) ;
        y = ((unsigned int)(unsigned char)ibuf[7]) ;
        y <<= 8 ;
        y = ((unsigned int)(unsigned char)ibuf[8]) ;
        z = ((unsigned int)(unsigned char)ibuf[9]) ;
        z <<= 8 ;
        z = ((unsigned int)(unsigned char)ibuf[10]) ;

        yagf_log( x, y, z ) ;
    }
    // ignor every thing else
}


// initialize gforce sensor
void yagf_init( config & dvrconfig )
{
    int i ;
    string v ;
    char rsp[MCU_MAX_MSGSIZE];
    int  rsize ;
    float fv ;
    int x, y, z ;
    // value in sensor's direction (X/Y/Z)
    int trigger_x_pos, trigger_x_neg, trigger_y_pos, trigger_y_neg, trigger_z_pos, trigger_z_neg ;
    int base_x_pos, base_x_neg, base_y_pos, base_y_neg, base_z_pos, base_z_neg ;
    int crash_x_pos, crash_x_neg, crash_y_pos, crash_y_neg, crash_z_pos, crash_z_neg ;

    // reset gforce value
    dio_lock();
    p_dio_mmap->gforce_serialno=0 ;
    p_dio_mmap->gforce_forward = 0.0 ;
    p_dio_mmap->gforce_right = 0.0 ;
    p_dio_mmap->gforce_down = 1.0 ;
    dio_unlock();

    // gforce log enabled ?
    gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");
    gforce_available = 0 ;
    gforce_crashdata_enable = dvrconfig.getvalueint( "io", "gsensor_crashdata" );

    // unit direction
    int dforward, dupward ;
    dforward = dvrconfig.getvalueint( "io", "gsensor_forward");
    dupward  = dvrconfig.getvalueint( "io", "gsensor_upward");
    int unit_direction = 0 ;
    for(i=0; i<24; i++) {
        if( dforward == yagf_direction_table[i][0] && dupward == yagf_direction_table[i][1] ) {
            unit_direction = i ;
            netdbg_print("unit direction number: %d\n", i );
            break;
        }
    }

    // trigger value in vehicle direction

    // forword trigger value
    fv = 0.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_forward_trigger");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    x = (int)(fv*37.0+512.0);
    if( x<0 ) x=-x ;

    // right trigger value
    fv = 0.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_right_trigger");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    y = (int)(fv*37.0+512.0) ;
    if( y<0 ) y=-y ;

    // down trigger value
    fv = 1.0+2.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_down_trigger");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    z = (int)(fv*37.0+512.0) ;
    if( z<0 ) z=-z ;

    trigger_x_pos=x ;
    trigger_y_pos=y ;
    trigger_z_pos=z ;

    // backward trigger value
    fv = -0.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_backward_trigger");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    x = (int)(fv*37.0+512.0) ;
    if( x>0 ) x=-x ;

    // left trigger value
    fv = -0.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_left_trigger");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    y = (int)(fv*37.0+512.0) ;
    if( y>0 ) y=-y ;

    // up trigger value
    fv = 1.0-2.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_up_trigger");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    z = (int)(fv*37.0+512.0) ;
    if( z>0 ) z=-z ;

    trigger_x_neg=x ;
    trigger_y_neg=y ;
    trigger_z_neg=z ;

    // base value in vehicle direction

    // forword base value
    fv = 0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_forward_base");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    x = (int)(fv*37.0+512.0) ;
    if( x<0 ) x=-x ;

    // right base value
    fv = 0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_right_base");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    y = (int)(fv*37.0+512.0) ;
    if( y<0 ) y=-y ;

    // down base value
    fv = 1.0+2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_down_base");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    z = (int)(fv*37.0+512.0) ;
    if( z<0 ) z=-z ;

    base_x_pos=x ;
    base_y_pos=y ;
    base_z_pos=z ;

    // backward base value
    fv = -0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_backward_base");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    x = (int)(fv*37.0+512.0) ;
    if( x>0 ) x=-x ;

    // left base value
    fv = -0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_left_base");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    y = (int)(fv*37.0+512.0) ;
    if( y>0 ) y=-y ;

    // up base value
    fv = 1.0-2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_up_base");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    z = (int)(fv*37.0+512.0) ;
    if( z>0 ) z=-z ;

    base_x_neg=x ;
    base_y_neg=y ;
    base_z_neg=z ;

    // crash value in vehicle direction

    // forword crash value
    fv = 3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_forward_crash");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    x = (int)(fv*37.0+512.0) ;
    if( x<0 ) x=-x ;

    // right crash value
    fv = 3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_right_crash");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    y = (int)(fv*37.0+512.0) ;
    if( y<0 ) y=-y ;

    // down crash value
    fv = 1.0+5.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_down_crash");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    z = (int)(fv*37.0+512.0) ;
    if( z<0 ) z=-z ;

    crash_x_pos=x ;
    crash_y_pos=y ;
    crash_z_pos=z ;

    // backward crash value
    fv = -3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_backward_crash");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    x = (int)(fv*37.0+512.0) ;
    if( x>0 ) x=-x ;

    // left crash value
    fv = -3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_left_crash");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    y = (int)(fv*37.0+512.0) ;
    if( y>0 ) y=-y ;

    // up crash value
    fv = 1.0-3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_up_crash");
    if( v.length()>0 ) {
        sscanf(v,"%f", &fv);
    }
    z = (int)(fv*37.0+512.0) ;
    if( z>0 ) z=-z ;

    crash_x_neg=x ;
    crash_y_neg=y ;
    crash_z_neg=z ;

    // adjust negtive value
    if( base_x_pos<base_x_neg ) {
        x=base_x_pos ;
        base_x_pos=base_x_neg ;
        base_x_neg=x ;
    }
    if( base_y_pos<base_y_neg ) {
        y=base_y_pos ;
        base_y_pos=base_y_neg ;
        base_y_neg=y ;
    }
    if( base_z_pos<base_z_neg ) {
        z=base_z_pos ;
        base_z_pos=base_z_neg ;
        base_z_neg=z ;
    }
    if( trigger_x_pos<trigger_x_neg ) {
        x=trigger_x_pos ;
        trigger_x_pos=trigger_x_neg ;
        trigger_x_neg=x ;
    }
    if( trigger_y_pos<trigger_y_neg ) {
        y=trigger_y_pos ;
        trigger_y_pos=trigger_y_neg ;
        trigger_y_neg=y ;
    }
    if( trigger_z_pos<trigger_z_neg ) {
        z=trigger_z_pos ;
        trigger_z_pos=trigger_z_neg ;
        trigger_z_neg=z ;
    }
    if( crash_x_pos<crash_x_neg ) {
        x=crash_x_pos ;
        crash_x_pos=crash_x_neg ;
        crash_x_neg=x ;
    }
    if( crash_y_pos<crash_y_neg ) {
        y=crash_y_pos ;
        crash_y_pos=crash_y_neg ;
        crash_y_neg=y ;
    }
    if( crash_z_pos<crash_z_neg ) {
        z=crash_z_pos ;
        crash_z_pos=crash_z_neg ;
        crash_z_neg=z ;
    }

    // do this before set trigger
    yagf_setrtc();

    // set trigger value
    rsize = yagf_cmd(rsp, YAGF_CMD_SETTRIGGER,
                     37,

                     base_x_pos>>8, base_x_pos&0xff,
                     base_x_neg>>8, base_x_neg&0xff,
                     base_y_pos>>8, base_y_pos&0xff,
                     base_y_neg>>8, base_y_neg&0xff,
                     base_z_pos>>8, base_z_pos&0xff,
                     base_z_neg>>8, base_z_neg&0xff,

                     trigger_x_pos>>8, trigger_x_pos&0xff,
                     trigger_x_neg>>8, trigger_x_neg&0xff,
                     trigger_y_pos>>8, trigger_y_pos&0xff,
                     trigger_y_neg>>8, trigger_y_neg&0xff,
                     trigger_z_pos>>8, trigger_z_pos&0xff,
                     trigger_z_neg>>8, trigger_z_neg&0xff,

                     crash_x_pos>>8, crash_x_pos&0xff,
                     crash_x_neg>>8, crash_x_neg&0xff,
                     crash_y_pos>>8, crash_y_pos&0xff,
                     crash_y_neg>>8, crash_y_neg&0xff,
                     crash_z_pos>>8, crash_z_pos&0xff,
                     crash_z_neg>>8, crash_z_neg&0xff,

                     yagf_direction_table[unit_direction][2] );

    if( rsize>0 && rsp[5] ) { // g_sensor available
        dvr_log("tab102b gforce sensor detected!");
    }
    else {
        dvr_log("tab102b gforce sensor init failed!");
    }

    // let it start
    yagf_bootready();
    yagf_enablepeak();

}

void yagf_finish()
{
}
