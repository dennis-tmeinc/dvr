/*
 file gforce.cpp,
    read g force sensor data from mcu 
 */

#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>

#include "../cfg.h"

#include "../dvrsvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "netdbg.h"
#include "mcu.h"
#include "diomap.h"

// functions from ioprocess.cpp
int dvr_log(char *fmt, ...);

int gforce_log_enable=0 ;

// g value converting vectors , (Forward, Right, Down) = [vecttabe] * (Back, Right, Buttom)
static signed char g_convert[24][3][3] = 
{
//  Front -> Forward    
    {   // Front -> Forward, Right -> Upward
        {-1, 0, 0},
        { 0, 0,-1},
        { 0,-1, 0}
    },
    {   // Front -> Forward, Left  -> Upward
        {-1, 0, 0},
        { 0, 0, 1},
        { 0, 1, 0}
    },
    {   // Front -> Forward, Bottom  -> Upward
        {-1, 0, 0},
        { 0, 1, 0},
        { 0, 0,-1}
    },
    {   // Front -> Forward, Top -> Upward
        {-1, 0, 0},
        { 0,-1, 0},
        { 0, 0, 1}
    },
//---- Back->Forward
    {   // Back -> Forward, Right -> Upward
        { 1, 0, 0},
        { 0, 0, 1},
        { 0,-1, 0}
    },
    {   // Back -> Forward, Left -> Upward
        { 1, 0, 0},
        { 0, 0,-1},
        { 0, 1, 0}
    },
    {   // Back -> Forward, Button -> Upward
        { 1, 0, 0},
        { 0,-1, 0},
        { 0, 0,-1}
    },
    {   // Back -> Forward, Top -> Upward
        { 1, 0, 0},
        { 0, 1, 0},
        { 0, 0, 1}
    },
//---- Right -> Forward
    {   // Right -> Forward, Front -> Upward
        { 0, 1, 0},
        { 0, 0, 1},
        { 1, 0, 0}
    },
    {   // Right -> Forward, Back -> Upward
        { 0, 1, 0},
        { 0, 0,-1},
        {-1, 0, 0}
    },
    {   // Right -> Forward, Bottom -> Upward
        { 0, 1, 0},
        { 1, 0, 0},
        { 0, 0,-1}
    },
    {   // Right -> Forward, Top -> Upward
        { 0, 1, 0},
        {-1, 0, 0},
        { 0, 0, 1}
    },
//---- Left -> Forward
    {   // Left -> Forward, Front -> Upward
        { 0,-1, 0},
        { 0, 0,-1},
        { 1, 0, 0}
    },
    {   // Left -> Forward, Back -> Upward
        { 0,-1, 0},
        { 0, 0, 1},
        {-1, 0, 0}
    },
    {   // Left -> Forward, Bottom -> Upward
        { 0,-1, 0},
        {-1, 0, 0},
        { 0, 0,-1}
    },
    {   // Left -> Forward, Top -> Upward
        { 0,-1, 0},
        { 1, 0, 0},
        { 0, 0, 1}
    },
//---- Bottom -> Forward
    {   // Bottom -> Forward, Front -> Upward
        { 0, 0, 1},
        { 0,-1, 0},
        { 1, 0, 0}
    },
    {   // Bottom -> Forward, Back -> Upward
        { 0, 0, 1},
        { 0, 1, 0},
        {-1, 0, 0}
    },
    {   // Bottom -> Forward, Right -> Upward
        { 0, 0, 1},
        {-1, 0, 0},
        { 0,-1, 0}
    },
    {   // Bottom -> Forward, Left -> Upward
        { 0, 0, 1},
        { 1, 0, 0},
        { 0, 1, 0}
    },
//---- Top -> Forward
    {   // Top -> Forward, Front -> Upward
        { 0, 0,-1},
        { 0, 1, 0},
        { 1, 0, 0}
    },
    {   // Top -> Forward, Back -> Upward
        { 0, 0,-1},
        { 0,-1, 0},
        {-1, 0, 0}
    },
    {   // Top -> Forward, Right -> Upward
        { 0, 0,-1},
        { 1, 0, 0},
        { 0,-1, 0}
    },
    {   // Top -> Forward, Left -> Upward
        { 0, 0,-1},
        {-1, 0, 0},
        { 0, 1, 0}
    }

} ;

/* original direction table 
static char direction_table[24][2] = 
{
    {0, 2}, {0, 3}, {0, 4}, {0,5},      // Front -> Forward
    {1, 2}, {1, 3}, {1, 4}, {1,5},      // Back  -> Forward
    {2, 0}, {2, 1}, {2, 4}, {2,5},      // Right -> Forward
    {3, 0}, {3, 1}, {3, 4}, {3,5},      // Left  -> Forward
    {4, 0}, {4, 1}, {4, 2}, {4,3},      // Buttom -> Forward
    {5, 0}, {5, 1}, {5, 2}, {5,3},      // Top   -> Forward
};
*/

// direction table from harrison.  ??? what is the third number ???
// 0:Front,1:Back, 2:Right, 3:Left, 4:Bottom, 5:Top 
static char direction_table[24][3] = 
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

#define DEFAULT_DIRECTION   (7)
static int gsensor_direction = DEFAULT_DIRECTION ;
float g_sensor_trigger_forward ;
float g_sensor_trigger_backward ;
float g_sensor_trigger_right ;
float g_sensor_trigger_left ;
float g_sensor_trigger_down ;
float g_sensor_trigger_up ;
// new parameters for g sensor. (2010-04-08)
float g_sensor_base_forward ;
float g_sensor_base_backward ;
float g_sensor_base_right ;
float g_sensor_base_left ;
float g_sensor_base_down ;
float g_sensor_base_up ;
float g_sensor_crash_forward ;
float g_sensor_crash_backward ;
float g_sensor_crash_right ;
float g_sensor_crash_left ;
float g_sensor_crash_down ;
float g_sensor_crash_up ;


// return gsensor available
int mcu_gsensorinit_old()
{
    char * responds ;
    float trigger_back, trigger_front ;
    float trigger_right, trigger_left ;
    float trigger_bottom, trigger_top ;
    char tr_rt, tr_lf, tr_bk, tr_fr, tr_bt, tr_tp ;
    
    if( !gforce_log_enable ) {
        return 0 ;
    }

    trigger_back =
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_trigger_forward +
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_trigger_right +
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_trigger_down ;
    trigger_right = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_trigger_forward + 
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_trigger_right + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_trigger_down ;
    trigger_bottom = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_trigger_forward + 
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_trigger_right + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_trigger_down ;
    trigger_front = 
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_trigger_backward + 
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_trigger_left + 
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_trigger_up ;
    trigger_left = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_trigger_backward + 
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_trigger_left + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_trigger_up ;
    trigger_top = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_trigger_backward + 
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_trigger_left + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_trigger_up ;

    if( trigger_right >= trigger_left ) {
        tr_rt  = (signed char)(trigger_right*0xe) ;    // Right Trigger
        tr_lf  = (signed char)(trigger_left*0xe) ;     // Left Trigger
    }
    else {
        tr_rt  = (signed char)(trigger_left*0xe) ;     // Right Trigger
        tr_lf  = (signed char)(trigger_right*0xe) ;    // Left Trigger
    }

    if( trigger_back >= trigger_front ) {
        tr_bk  = (signed char)(trigger_back*0xe) ;    // Back Trigger
        tr_fr  = (signed char)(trigger_front*0xe) ;    // Front Trigger
    }
    else {
        tr_bk  = (signed char)(trigger_front*0xe) ;    // Back Trigger
        tr_fr  = (signed char)(trigger_back*0xe) ;    // Front Trigger
    }

    if( trigger_bottom >= trigger_top ) {
        tr_bt = (signed char)(trigger_bottom*0xe) ;    // Bottom Trigger
        tr_tp = (signed char)(trigger_top*0xe) ;    // Top Trigger
    }
    else {
        tr_bt = (signed char)(trigger_top*0xe) ;    // Bottom Trigger
        tr_tp = (signed char)(trigger_bottom*0xe) ;    // Top Trigger
    }
    
    responds = mcu_cmd( MCU_CMD_GSENSORINIT, 6, tr_rt, tr_lf, tr_bk, tr_fr, tr_bt, tr_tp );
    if( responds ) {
        return responds[5] ;        // g_sensor_available
    }
    return 0 ;
}

// init gsensor
// return 1 if gsensor available
int mcu_gsensorinit()
{
    char * responds ;

    float trigger_back, trigger_front ;
    float trigger_right, trigger_left ;
    float trigger_bottom, trigger_top ;
    int XP, XN, YP, YN, ZP, ZN;

    float base_back, base_front ;
    float base_right, base_left ;
    float base_bottom, base_top ;
    int BXP, BXN, BYP, BYN, BZP, BZN;

    float crash_back, crash_front ;
    float crash_right, crash_left ;
    float crash_bottom, crash_top ;
    int CXP, CXN, CYP, CYN, CZP, CZN;

    if( !gforce_log_enable ) {
        return 0 ;
    }

    trigger_back = 
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_trigger_forward + 
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_trigger_right + 
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_trigger_down ;
    trigger_right = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_trigger_forward + 
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_trigger_right + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_trigger_down ;
    trigger_bottom = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_trigger_forward + 
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_trigger_right + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_trigger_down ;

    trigger_front = 
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_trigger_backward +
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_trigger_left + 
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_trigger_up ;
    trigger_left = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_trigger_backward +
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_trigger_left + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_trigger_up ;
    trigger_top = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_trigger_backward +
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_trigger_left + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_trigger_up ;

    if( trigger_right >= trigger_left ) {
        XP  = (int)(trigger_right*14) ;    // Right Trigger
        XN  = (int)(trigger_left*14) ;     // Left Trigger
    }
    else {
        XP  = (int)(trigger_left*14) ;     // Right Trigger
        XN  = (int)(trigger_right*14) ;    // Left Trigger
    }

    if( trigger_back >= trigger_front ) {
        YP  = (int)(trigger_back*14) ;    // Back Trigger
        YN  = (int)(trigger_front*14) ;    // Front Trigger
    }
    else {
        YP  = (int)(trigger_front*14) ;    // Back Trigger
        YN  = (int)(trigger_back*14) ;    // Front Trigger
    }

    if( trigger_bottom >= trigger_top ) {
        ZP  = (int)(trigger_bottom*14) ;    // Bottom Trigger
        ZN = (int)(trigger_top*14) ;    // Top Trigger
    }
    else {
        ZP  = (int)(trigger_top*14) ;    // Bottom Trigger
        ZN = (int)(trigger_bottom*14) ;    // Top Trigger
    }

    base_back = 
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_base_forward + 
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_base_right + 
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_base_down ;
    base_right = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_base_forward + 
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_base_right + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_base_down ;
    base_bottom = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_base_forward + 
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_base_right + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_base_down ;

    base_front = 
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_base_backward +
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_base_left + 
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_base_up ;
    base_left = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_base_backward +
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_base_left + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_base_up ;
    base_top = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_base_backward +
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_base_left + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_base_up ;

    if( base_right >= base_left ) {
        BXP  = (int)(base_right*14) ;    // Right Trigger
        BXN  = (int)(base_left*14) ;     // Left Trigger
    }
    else {
        BXP  = (int)(base_left*14) ;     // Right Trigger
        BXN  = (int)(base_right*14) ;    // Left Trigger
    }

    if( base_back >= base_front ) {
        BYP  = (int)(base_back*14) ;    // Back Trigger
        BYN  = (int)(base_front*14) ;    // Front Trigger
    }
    else {
        BYP  = (int)(base_front*14) ;    // Back Trigger
        BYN  = (int)(base_back*14) ;    // Front Trigger
    }

    if( base_bottom >= base_top ) {
        BZP  = (int)(base_bottom*14) ;    // Bottom Trigger
        BZN = (int)(base_top*14) ;    // Top Trigger
    }
    else {
        BZP  = (int)(base_top*14) ;    // Bottom Trigger
        BZN = (int)(base_bottom*14) ;    // Top Trigger
    }

    crash_back = 
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_crash_forward + 
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_crash_right + 
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_crash_down ;
    crash_right = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_crash_forward + 
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_crash_right + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_crash_down ;
    crash_bottom = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_crash_forward + 
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_crash_right + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_crash_down ;

    crash_front = 
        ((float)(g_convert[gsensor_direction][0][0])) * g_sensor_crash_backward +
        ((float)(g_convert[gsensor_direction][1][0])) * g_sensor_crash_left + 
        ((float)(g_convert[gsensor_direction][2][0])) * g_sensor_crash_up ;
    crash_left = 
        ((float)(g_convert[gsensor_direction][0][1])) * g_sensor_crash_backward +
        ((float)(g_convert[gsensor_direction][1][1])) * g_sensor_crash_left + 
        ((float)(g_convert[gsensor_direction][2][1])) * g_sensor_crash_up ;
    crash_top = 
        ((float)(g_convert[gsensor_direction][0][2])) * g_sensor_crash_backward +
        ((float)(g_convert[gsensor_direction][1][2])) * g_sensor_crash_left + 
        ((float)(g_convert[gsensor_direction][2][2])) * g_sensor_crash_up ;

    if( crash_right >= crash_left ) {
        CXP  = (int)(crash_right*14) ;    // Right Trigger
        CXN  = (int)(crash_left*14) ;     // Left Trigger
    }
    else {
        CXP  = (int)(crash_left*14) ;     // Right Trigger
        CXN  = (int)(crash_right*14) ;    // Left Trigger
    }

    if( crash_back >= crash_front ) {
        CYP  = (int)(crash_back*14) ;    // Back Trigger
        CYN  = (int)(crash_front*14) ;    // Front Trigger
    }
    else {
        CYP  = (int)(crash_front*14) ;    // Back Trigger
        CYN  = (int)(crash_back*14) ;    // Front Trigger
    }

    if( crash_bottom >= crash_top ) {
        CZP  = (int)(crash_bottom*14) ;    // Bottom Trigger
        CZN = (int)(crash_top*14) ;    // Top Trigger
    }
    else {
        CZP  = (int)(crash_top*14) ;    // Bottom Trigger
        CZN = (int)(crash_bottom*14) ;    // Top Trigger
    }

    responds = mcu_cmd( MCU_CMD_GSENSORINIT, 20, 
        1,                                          // enable GForce
        (int)(direction_table[gsensor_direction][2]),      // direction
        BXP, BXN, BYP, BYN, BZP, BZN,               // base parameter
        XP, XN, YP, YN, ZP, ZN,                     // trigger parameter
        CXP, CXN, CYP, CYN, CZP, CZN );             // crash parameter
    if( responds ) {
        return responds[5] ;        // g_sensor_available
    }
    return 0 ;
}


void gforce_log( float gright, float gback, float gbuttom ) 
{
    //        float gback, gright, gbuttom ;
    float gbusforward, gbusright, gbusdown ;

    netdbg_print("Accelerometer, --------- right=%.2f , back   =%.2f , buttom=%.2f .\n",     
           gright,
           gback,
           gbuttom );

    // converting
    gbusforward = 
        ((float)(signed char)(g_convert[gsensor_direction][0][0])) * gback + 
        ((float)(signed char)(g_convert[gsensor_direction][0][1])) * gright + 
        ((float)(signed char)(g_convert[gsensor_direction][0][2])) * gbuttom ;
    gbusright = 
        ((float)(signed char)(g_convert[gsensor_direction][1][0])) * gback + 
        ((float)(signed char)(g_convert[gsensor_direction][1][1])) * gright + 
        ((float)(signed char)(g_convert[gsensor_direction][1][2])) * gbuttom ;
    gbusdown = 
        ((float)(signed char)(g_convert[gsensor_direction][2][0])) * gback + 
        ((float)(signed char)(g_convert[gsensor_direction][2][1])) * gright + 
        ((float)(signed char)(g_convert[gsensor_direction][2][2])) * gbuttom ;

    netdbg_print("Accelerometer, converted right=%.2f , forward=%.2f , down  =%.2f .\n",     
           gbusright,
           gbusforward,
           gbusdown );
       
    // save to log
    dio_lock();
    p_dio_mmap->gforce_serialno++ ;
    p_dio_mmap->gforce_right = gbusright ;
    p_dio_mmap->gforce_forward = gbusforward ;
    p_dio_mmap->gforce_down = gbusdown ;
    dio_unlock();
    
}


// initialize serial port
void gforce_init( config & dvrconfig ) 
{
    string v ;
    int i ;

    gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");

    // get gsensor direction setup
    
    int dforward, dupward ;
    dforward = dvrconfig.getvalueint( "io", "gsensor_forward");	
    dupward  = dvrconfig.getvalueint( "io", "gsensor_upward");	
    gsensor_direction = DEFAULT_DIRECTION ;
    for(i=0; i<24; i++) {
        if( dforward == direction_table[i][0] && dupward == direction_table[i][1] ) {
            gsensor_direction = i ;
            break;
        }
    }
    p_dio_mmap->gforce_serialno=0;
    
    g_sensor_trigger_forward = 0.5 ;
    v = dvrconfig.getvalue( "io", "gsensor_forward_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_trigger_forward);
    }
    g_sensor_trigger_backward = -0.5 ;
    v = dvrconfig.getvalue( "io", "gsensor_backward_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_trigger_backward);
    }
    g_sensor_trigger_right = 0.5 ;
    v = dvrconfig.getvalue( "io", "gsensor_right_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_trigger_right);
    }
    g_sensor_trigger_left = -0.5 ;
    v = dvrconfig.getvalue( "io", "gsensor_left_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_trigger_left);
    }
    g_sensor_trigger_down = 1.0+2.5 ;
    v = dvrconfig.getvalue( "io", "gsensor_down_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_trigger_down);
    }
    g_sensor_trigger_up = 1.0-2.5 ;
    v = dvrconfig.getvalue( "io", "gsensor_up_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_trigger_up);
    }

    // new parameters for gsensor 
    g_sensor_base_forward = 0.2 ;
    v = dvrconfig.getvalue( "io", "gsensor_forward_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_base_forward);
    }
    g_sensor_base_backward = -0.2 ;
    v = dvrconfig.getvalue( "io", "gsensor_backward_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_base_backward);
    }
    g_sensor_base_right = 0.2 ;
    v = dvrconfig.getvalue( "io", "gsensor_right_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_base_right);
    }
    g_sensor_base_left = -0.2 ;
    v = dvrconfig.getvalue( "io", "gsensor_left_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_base_left);
    }
    g_sensor_base_down = 1.0+2.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_down_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_base_down);
    }
    g_sensor_base_up = 1.0-2.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_up_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_base_up);
    }
    
    g_sensor_crash_forward = 3.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_forward_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_crash_forward);
    }
    g_sensor_crash_backward = -3.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_backward_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_crash_backward);
    }
    g_sensor_crash_right = 3.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_right_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_crash_right);
    }
    g_sensor_crash_left = -3.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_left_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_crash_left);
    }
    g_sensor_crash_down = 1.0+5.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_down_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_crash_down);
    }
    g_sensor_crash_up = 1.0-5.0 ;
    v = dvrconfig.getvalue( "io", "gsensor_up_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &g_sensor_crash_up);
    }
    
     // gsensor trigger setup
    if( mcu_gsensorinit() ) {       // g sensor available
        FILE * fgsensor ;
        dvr_log("G force sensor detected!");
        fgsensor = fopen( "/var/dvr/gsensor", "w" );
        if( fgsensor ) {
            fprintf(fgsensor, "1");
            fclose(fgsensor);
        }
    }
    else {
        dvr_log("No G force sensor.");
    }
}

void gforce_finish()
{

}
