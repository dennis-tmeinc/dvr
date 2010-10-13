/*
 file gforce.cpp,
    read g force sensor data from mcu 
*/

/*
 
For vehicle, we define six directions, they are natural direction of a vehicle.
 
    Forward   -       The direction where vehicle run forward
    Backward  -       Opposite of Forward
 
    Right     -       The right hand side where a person inside the vehicle and face forward
    Left      -       Opposite of Right
 
    Down      -       The direction toward the earth
    Up        -       Opposite of Down
 
For dvr device, directions are base on six faces of the device. A device can be a DVR box or a G-sensor device.
 
    Front     -       Manufacturer defined. Usually it is the face of front panel where has model number, company logo and some indicators
    Back      -       Opposite of front face. Usually it is the side where most cable connected to.
 
    Right     -       The right hand side of the device when a person faces the device's front side.
    Left      -       Opposite of Right face.
 
    Bottom    -       Manufacturer defined. Usually it is the direction of the mounting surface
    Top       -       Opposite of Bottom face.
 
G sensing report to device of acceleration value (G value) base on device direction. 
    Right +  / Left -       ------     Right is positive, left is negative 
    Back +   / Front -      ------     Back is positive, front is negative
    Bottom + / Top -        ------     Bottom is positive, top is negative 
 
User (Installer) specify device mounting directions on DVR's setup screen, two mounting direction need to be setup.
    Forward direction    -    6 options:  Front, Back, Right, Left, Bottom, Top       (default is Back)
    Upward direction     -    6 options:  Front, Back, Right, Left, Bottom, Top       (default is Top)
    ( default mounting directions is : back of unit face forward, top of unit face upward, right side face to run )

Base on mounting direction setting, DVR application convert G value from device direction to vehicle direction, and record to log file.
Vihecle direction :
    Right + / Left -         ------    Right is positive, Left is negative  
    Forward + / Backward -   ------    Forward is positive, Backward is negative
    Down + / Up -            ------    Down is positive, Up is negative 
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

// g value converting vectors ,
// for sensor to unit convertion :
//      (Back, Right, Buttom)  = [vecttabe] * (X, Y, -Z)        // X,Y,Z are based on value from sensor
// for unit to vehicle convertion:
//      (Forward, Right, Down) = [vecttabe] * (Back, Right, Buttom)
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

/* 
 direction table: 0=front, 1=back, 2=right, 3=left, 4=buttom, 5=top
*/
static char direction_table[24][2] = 
{
    {0, 2}, {0, 3}, {0, 4}, {0,5},      // Front -> Forward
    {1, 2}, {1, 3}, {1, 4}, {1,5},      // Back  -> Forward
    {2, 0}, {2, 1}, {2, 4}, {2,5},      // Right -> Forward
    {3, 0}, {3, 1}, {3, 4}, {3,5},      // Left  -> Forward
    {4, 0}, {4, 1}, {4, 2}, {4,3},      // Buttom -> Forward
    {5, 0}, {5, 1}, {5, 2}, {5,3},      // Top   -> Forward
};

// direction table from harrison.  ??? what is this third number for ???
// 0:Front,1:Back, 2:Right, 3:Left, 4:Bottom, 5:Top 
/*
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
*/

#define DEFAULT_DIRECTION  (7)   
static int gsensor_direction=DEFAULT_DIRECTION ;    // sensor direction relate to dvr unit. (HARD CODED to 7) 
static int unit_direction ;                         // unit direction relate to vehicle.

static void direction_convertor( int &x, int &y, int &z, int direction, int revert=0 )
{
    int cx, cy, cz ;
    if( revert==0 ) {
        cx = 
            g_convert[direction][0][0] * x + 
            g_convert[direction][0][1] * y + 
            g_convert[direction][0][2] * z ;
        cy = 
            g_convert[direction][1][0] * x + 
            g_convert[direction][1][1] * y + 
            g_convert[direction][1][2] * z ;
        cz = 
            g_convert[direction][2][0] * x + 
            g_convert[direction][2][1] * y + 
            g_convert[direction][2][2] * z ;
    }
    else {
        cx = 
            g_convert[direction][0][0] * x + 
            g_convert[direction][1][0] * y + 
            g_convert[direction][2][0] * z ;
        cy = 
            g_convert[direction][0][1] * x + 
            g_convert[direction][1][1] * y + 
            g_convert[direction][2][1] * z ;
        cz = 
            g_convert[direction][0][2] * x + 
            g_convert[direction][1][2] * y + 
            g_convert[direction][2][2] * z ;
    }
    x=cx ;
    y=cy ;
    z=cz ;
}

void gforce_writeTab102Data(unsigned char *buf, int len)
{
    char filename[256];
    struct tm tm;
    char hostname[128] ;

    gethostname(hostname, 128) ;

    time_t t = time(NULL);
    localtime_r(&t, &tm);
    snprintf(filename, sizeof(filename),
             "/var/dvr/%s_%04d%02d%02d%02d%02d%02d_TAB102log_L.log",
             hostname,
             tm.tm_year + 1900,
             tm.tm_mon + 1,
             tm.tm_mday,
             tm.tm_hour,
             tm.tm_min,
             tm.tm_sec );
    FILE *fp;   
    fp = fopen(filename, "w");
    if (fp) {

        /*
         todo:
         convert origin X/Y/Z -> Unit Direction -> Vehicle Direction
         before save to disk.
         */

        fwrite(buf, 1, len, fp);
        fclose(fp);
    }
}

int gforce_checksum(unsigned char * buf, int bsize)
{
    unsigned char cs = 0;
    int i;

    for (i = 0; i < bsize; i++) {
        cs += buf[i];
    }

    return cs==0 ;
}

#define UPLOAD_ACK_SIZE (10)

int gforce_upload()
{
    unsigned char * responds ;
    unsigned int uploadSize ;
    unsigned int nbytes ;
    responds = (unsigned char *)mcu_cmd( MCU_CMD_GSENSORUPLOAD, 1, 
        (int)(direction_table[gsensor_direction][2]) );      // direction
    if( responds!=NULL && *responds>=10 ) {
        uploadSize = 
            (responds[5] << 24) | 
            (responds[6] << 16) | 
            (responds[7] << 8) |
            responds[8];
        netdbg_print("gforce: upload size:%d\n", uploadSize);
        if (uploadSize) {
            //1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
            // + 8(0g + checksum)
            int bufsize = uploadSize + 1024;
            unsigned char *tbuf = (unsigned char *)malloc(bufsize);
            if (!tbuf) {
                netdbg_print("gforce: no enough memory!");
                return 0 ;
            }
            nbytes = mcu_read((char *)tbuf, bufsize, 1000000, 1000000 );
            netdbg_print("gforce: uploaded %d bytes", nbytes );

            int success=0 ;
            if (nbytes >= uploadSize + UPLOAD_ACK_SIZE + 8) {
                if (gforce_checksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
                    success = 1;
                    gforce_writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8);
                } else {
                    netdbg_print("gforce: upload checksum error.");
                }
            }
            free(tbuf);
            responds = (unsigned char *)mcu_cmd( MCU_CMD_GSENSORUPLOADACK, success );
            if( responds ) {
                return 1 ;
            }
        }
    }
    return 0;
}

// recevied gforce sensor data
void gforce_log( int x, int y, int z )
{
    int gback, gright, gbuttom ;            // in unit direction
    int gbusforward, gbusright, gbusdown ;  // in vehicle direction

    netdbg_print("Gsensor value X=%d, Y=%d, Z=%d\n",     
           x,
           y,
           z );

    // convert sensor value (X/Y/X) to unit direction (back/right/buttom). 
    //  gsensor_direction has been Hard coded to 7 (DEFAULT) for PWII, back=x bottom=-z
    gback = x ;
    gright = y ;
    gbuttom = -z ;
    direction_convertor( gback, gright, gbuttom, gsensor_direction ) ;
    
    netdbg_print("G unit value  B=%d, R=%d, T=%d\n",     
           gback,
           gright,
           gbuttom );
    
    // convert from unit direction(back/right/buttom) to vehicle direction(forward/right/down). (base on user defined unit direction)
    gbusforward=gback ;
    gbusright=gright ;
    gbusdown=gbuttom ;
    direction_convertor( gbusforward, gbusright, gbusdown, unit_direction ) ;

    netdbg_print("G vehicle v   F=%d, R=%d, D=%d\n",
           gbusforward,
           gbusright,
           gbusdown );
       
    // save to log
    dio_lock();
    p_dio_mmap->gforce_serialno++ ;
    p_dio_mmap->gforce_forward = gbusforward/14.0 ;
    p_dio_mmap->gforce_right = gbusright/14.0 ;
    p_dio_mmap->gforce_down = gbusdown/14.0 ;
    dio_unlock();
    
}

// initialize serial port
void gforce_init( config & dvrconfig ) 
{
    int i ;
    string v ;
    float fv ;
    int x, y, z ;
    // value in sensor's direction (X/Y/Z)
    int trigger_x_pos, trigger_x_neg, trigger_y_pos, trigger_y_neg, trigger_z_pos, trigger_z_neg ;
    int base_x_pos, base_x_neg, base_y_pos, base_y_neg, base_z_pos, base_z_neg ;
    int crash_x_pos, crash_x_neg, crash_y_pos, crash_y_neg, crash_z_pos, crash_z_neg ;

    p_dio_mmap->gforce_serialno=0 ;
    // gforce log enabled ?
    gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");

    // unit direction
    int dforward, dupward ;
    dforward = dvrconfig.getvalueint( "io", "gsensor_forward");	
    dupward  = dvrconfig.getvalueint( "io", "gsensor_upward");	
    unit_direction = DEFAULT_DIRECTION ;
    for(i=0; i<24; i++) {
        if( dforward == direction_table[i][0] && dupward == direction_table[i][1] ) {
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
        sscanf(v.getstring(),"%f", &fv);
    }
    x = (int)(fv*14) ;
    if( x<0 ) x=-x ;

    // right trigger value
    fv = 0.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_right_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    y = (int)(fv*14) ;
    if( y<0 ) y=-y ;
   
    // down trigger value
    fv = 1.0+2.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_down_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    z = (int)(fv*14) ;
    if( z<0 ) z=-z ;
       
    // convert trigger value from vehicle direction to unit direction
    direction_convertor( x, y, z, unit_direction, 1 );
    // convert trigger value from unit direction to sensor direction (X/Y/Z)
    direction_convertor( x, y, z, gsensor_direction, 1 );
    trigger_x_pos=x ;
    trigger_y_pos=y ;
    trigger_z_pos=z ;

    // backward trigger value
    fv = -0.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_backward_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    x = (int)(fv*14) ;
    if( x>0 ) x=-x ;

    // left trigger value
    fv = -0.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_left_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    y = (int)(fv*14) ;
    if( y>0 ) y=-y ;
   
    // up trigger value
    fv = 1.0-2.5 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_up_trigger");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    z = (int)(fv*14) ;
    if( z>0 ) z=-z ;
       
    // convert trigger value from vehicle direction to unit direction
    direction_convertor( x, y, z, unit_direction, 1 );
    // convert trigger value from unit direction to sensor direction (X/Y/Z)
    direction_convertor( x, y, z, gsensor_direction, 1 );
    trigger_x_neg=x ;
    trigger_y_neg=y ;
    trigger_z_neg=z ;

    // base value in vehicle direction

    // forword base value
    fv = 0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_forward_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    x = (int)(fv*14) ;
    if( x<0 ) x=-x ;

    // right base value
    fv = 0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_right_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    y = (int)(fv*14) ;
    if( y<0 ) y=-y ;
   
    // down base value
    fv = 1.0+2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_down_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    z = (int)(fv*14) ;
    if( z<0 ) z=-z ;
       
    // convert base value from vehicle direction to unit direction
    direction_convertor( x, y, z, unit_direction, 1 );
    // convert base value from unit direction to sensor direction (X/Y/Z)
    direction_convertor( x, y, z, gsensor_direction, 1 );
    base_x_pos=x ;
    base_y_pos=y ;
    base_z_pos=z ;

    // backward base value
    fv = -0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_backward_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    x = (int)(fv*14) ;
    if( x>0 ) x=-x ;

    // left base value
    fv = -0.2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_left_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    y = (int)(fv*14) ;
    if( y>0 ) y=-y ;
   
    // up base value
    fv = 1.0-2 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_up_base");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    z = (int)(fv*14) ;
    if( z>0 ) z=-z ;
       
    // convert base value from vehicle direction to unit direction
    direction_convertor( x, y, z, unit_direction, 1 );
    // convert base value from unit direction to sensor direction (X/Y/Z)
    direction_convertor( x, y, z, gsensor_direction, 1 );
    base_x_neg=x ;
    base_y_neg=y ;
    base_z_neg=z ;

    // crash value in vehicle direction

    // forword crash value
    fv = 3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_forward_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    x = (int)(fv*14) ;
    if( x<0 ) x=-x ;

    // right crash value
    fv = 3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_right_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    y = (int)(fv*14) ;
    if( y<0 ) y=-y ;
   
    // down crash value
    fv = 1.0+5.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_down_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    z = (int)(fv*14) ;
    if( z<0 ) z=-z ;
       
    // convert crash value from vehicle direction to unit direction
    direction_convertor( x, y, z, unit_direction, 1 );
    // convert crash value from unit direction to sensor direction (X/Y/Z)
    direction_convertor( x, y, z, gsensor_direction, 1 );
    crash_x_pos=x ;
    crash_y_pos=y ;
    crash_z_pos=z ;

    // backward crash value
    fv = -3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_backward_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    x = (int)(fv*14) ;
    if( x>0 ) x=-x ;

    // left crash value
    fv = -3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_left_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    y = (int)(fv*14) ;
    if( y>0 ) y=-y ;
   
    // up crash value
    fv = 1.0-3.0 ;               // default value
    v = dvrconfig.getvalue( "io", "gsensor_up_crash");
    if( v.length()>0 ) {
        sscanf(v.getstring(),"%f", &fv);
    }
    z = (int)(fv*14) ;
    if( z>0 ) z=-z ;
       
    // convert crash value from vehicle direction to unit direction
    direction_convertor( x, y, z, unit_direction, 1 );
    // convert crash value from unit direction to sensor direction (X/Y/Z)
    direction_convertor( x, y, z, gsensor_direction, 1 );
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
     // send init value to mcu
    char * responds = mcu_cmd( MCU_CMD_GSENSORINIT, 
                       20,      // 20 parameters 
                       1,       // enable GForce
                       0x12,    // 0x12=keep all value in it original direction (@&$^-$)
                       base_x_pos, base_x_neg, base_y_pos, base_y_neg, base_z_pos, base_z_neg,
                       trigger_x_pos, trigger_x_neg, trigger_y_pos, trigger_y_neg, trigger_z_pos, trigger_z_neg,
                       crash_x_pos, crash_x_neg, crash_y_pos, crash_y_neg, crash_z_pos, crash_z_neg ) ;
    if( responds && responds[5] ) { // g_sensor available
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
        gforce_log_enable=0;                // dont' log and show gforce
    }
}

void gforce_finish()
{

}
