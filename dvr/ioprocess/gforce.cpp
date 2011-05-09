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
#include <math.h>
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
#include "gforce.h"

// functions from ioprocess.cpp
int dvr_log(char *fmt, ...);

int gforce_log_enable;
int gforce_available;
int gforce_crashdata_enable;
int gforce_mountangle;

static char disk_curdiskfile[260] ;

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
/*
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

// direction table from harrison.
// 0:Front,1:Back, 2:Right, 3:Left, 4:Bottom, 5:Top 
#ifdef PWII_APP
static char direction_table[24][3] = 
{
  {0, 2, 0x61}, // Forward:front, Upward:right    Leftward:top
  {0, 3, 0x51}, // Forward:Front, Upward:left,    Leftward:bottom
  {0, 4, 0x11}, // Forward:Front, Upward:bottom,  Leftward:right 
  {0, 5, 0x21}, // Forward:Front, Upward:top,    Leftward:left 
  {1, 2, 0x62}, // Forward:back,  Upward:right,    Leftward:bottom 
  {1, 3, 0x52}, // Forward:back,  Upward:left,    Leftward:top
  {1, 4, 0x12}, // Forward:back,  Upward:bottom,    Leftward:left
  {1, 5, 0x22}, // Forward:back, Upward:top,    Leftward:right 
  {2, 0, 0x32}, // Forward:right, Upward:front,    Leftward:bottom
  {2, 1, 0x42}, // Forward:right, Upward:back,    Leftward:top
  {2, 4, 0x18}, // Forward:right, Upward:bottom,    Leftward:back
  {2, 5, 0x28}, // Forward:right, Upward:top,    Leftward:front
  {3, 0, 0x31}, // Forward:left, Upward:front,    Leftward:top
  {3, 1, 0x41}, // Forward:left, Upward:back,    Leftward:bottom
  {3, 4, 0x14}, // Forward:left, Upward:bottom,    Leftward:front
  {3, 5, 0x24}, // Forward:left, Upward:top,    Leftward:back
  {4, 0, 0x38}, // Forward:bottom, Upward:front,    Leftward:left
  {4, 1, 0x48}, // Forward:bottom, Upward:back,    Leftward:right
  {4, 2, 0x68}, // Forward:bottom, Upward:right,    Leftward:front
  {4, 3, 0x58}, // Forward:bottom, Upward:left,    Leftward:back
  {5, 0, 0x34}, // Forward:top, Upward:front,    Leftward:right
  {5, 1, 0x44}, // Forward:top, Upward:back,    Leftward:left
  {5, 2, 0x64}, // Forward:top, Upward:right,    Leftward:back
  {5, 3, 0x54}  // Forward:top, Upward:left,    Leftward:front
};
#else		// TVS APP
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
#endif

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

unsigned char * gforce_crashdata=NULL ;
int             gforce_crashdatalen=0 ;

void gforce_freecrashdata()
{
    if( gforce_crashdata ) {
        free(gforce_crashdata) ;
        gforce_crashdata=NULL ;
    }
    gforce_crashdatalen=0 ;
}

int gforce_savecrashdata()
{
    char hostname[128] ;
    char crashdatafilename[256] ;
    struct tm t ;
    time_t tt ;
    unsigned char dbuf[256] ;
    FILE * crashdatafile  ;
    FILE * diskfile ;

    if( gforce_crashdata == NULL || gforce_crashdatalen<=0 ) {
        gforce_freecrashdata();
        return 0;
    }
    
    diskfile = fopen( disk_curdiskfile, "r");

    if( diskfile ) {
        if( fscanf( diskfile, "%s", dbuf )>0 ) {

            tt=time(NULL);
            localtime_r(&tt, &t);
            gethostname( hostname, 127 );
            
            sprintf(crashdatafilename, "%s/smartlog/%s_%04d%02d%02d%02d%02d%02d_TAB102log_L.log",
                    dbuf,
                    hostname,
                    t.tm_year+1900,
                    t.tm_mon+1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec );
            crashdatafile = fopen( crashdatafilename, "w" );
            if( crashdatafile ) {
                dvr_log( "Write crash data, size: %d", gforce_crashdatalen);
                fwrite( gforce_crashdata, 1, gforce_crashdatalen, crashdatafile );
                fclose( crashdatafile );
                gforce_freecrashdata();
            }
        }
        fclose(diskfile);
    }
    return 0;
}

#define UPLOAD_ACK_SIZE (10)

int gforce_getcrashdata()
{
    unsigned int uploadSize ;
    unsigned int nbytes ;
    int success=0;
    int iomode ;
	char rsp[MCU_MAX_MSGSIZE] ;
	int  rsize ;
	
    if( gforce_log_enable==0  || gforce_crashdata_enable==0 ) {
        return 0 ;
    }
    
    iomode = p_dio_mmap->iomode ;
    p_dio_mmap->iomode=0;

    // free previous data
    gforce_freecrashdata();
    
    dvr_log( "Start gforce crash data reading." );
    dvrsvr_susp() ;
    glog_susp();
    if( usewatchdog ) {
        mcu_watchdogdisable();      // disable watchdog
//        mcu_watchdogenable(100) ;   // set watchdog to 100 seconds. (usally uploading last 65 seconds)
    }
    
    if( p_dio_mmap->poweroff ) {
        // extend power off delay
        mcu_poweroffdelay(120);
    }
    sleep(3);
    
    rsize = mcu_cmd(rsp, MCU_CMD_GSENSORUPLOAD, 1, 
        (int)(direction_table[gsensor_direction][2]) );      // direction
    if( rsize >= 10 ) {
        uploadSize = 
            (rsp[5] << 24) | 
            (rsp[6] << 16) | 
            (rsp[7] << 8) |
            rsp[8];
        if (uploadSize>0) {
            //1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
            // + 8(0g + checksum)
            int bufsize = uploadSize + 1024;
            gforce_crashdata =(unsigned char *)malloc(bufsize);

            netdbg_print("gforce upload, requst: %d bytes\n", uploadSize );
            nbytes = mcu_read((char *)gforce_crashdata, bufsize, 1000000, 1000000 );
            netdbg_print("gforce upload, read: %d bytes\n", nbytes );

//            if (nbytes >= uploadSize + UPLOAD_ACK_SIZE + 8) {
            if (nbytes >= uploadSize) {
                if (checksum(gforce_crashdata, nbytes)==0) {
                    success=1 ;
                    gforce_crashdatalen=nbytes ;
                    dvr_log( "Read gforce crash data success, data size %d", gforce_crashdatalen );
                    // ack
                } else {
                    dvr_log( "Gforce crash data checksum error" );
                    gforce_freecrashdata();
                }
            }
            else {
                dvr_log("Read gforce crash data error");
                gforce_freecrashdata();
            }
            mcu_cmd(NULL, MCU_CMD_GSENSORUPLOADACK, !success );
        }
        else {
            dvr_log( "No gforce crash data" );
        }
    }

    gforce_reinit();
    dvrsvr_resume() ;
    glog_resume();
    p_dio_mmap->iomode=iomode;
	
    return 0;
}

// recevied gforce sensor data
void gforce_log( int x, int y, int z )
{
    int gback, gright, gbuttom ;            // in unit direction
    int gbusforward, gbusright, gbusdown ;  // in vehicle direction
    float gf, gr, gd, mountangle ;          // in vehicle direction

    if( gforce_log_enable==0 )              // gforce not enabled
        return ;

    if( gforce_available == 0 ) {           // gforce_init may not return correct availability answer, so we do it here. 
        FILE * fgsensor ;
        dvr_log("G force sensor available!");
        fgsensor = fopen( VAR_DIR"/gsensor", "w" );
        if( fgsensor ) {
            fprintf(fgsensor, "1");
            fclose(fgsensor);
        }
        gforce_available = 1 ;
    }        

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

    netdbg_print("G vehicle    F=%d, R=%d, D=%d\n",
           gbusforward,
           gbusright,
           gbusdown );

    if( gforce_mountangle ) {
        mountangle = M_PI * gforce_mountangle / 180.0 ;
        gf = (gbusforward * cos( mountangle ) - gbusdown * sin( mountangle ))/14.0 ;
        gd = (gbusforward * sin( mountangle ) + gbusdown * cos( mountangle ))/14.0 ;
    }
    else {
        gf = gbusforward/14.0 ;
        gd = gbusdown/14.0 ;
    }
    gr = gbusright/14.0 ;

    if( netdbg_on ) {
        netdbg_print("G value after adjusted,  F=%5.2f, R=%5.2f, D=%5.2f, angle=%.0f\n",
                     gf,gr,gd, 180.0*atan( gf/gd )/M_PI );
    }

    // save to log
    dio_lock();
    p_dio_mmap->gforce_serialno++ ;
    p_dio_mmap->gforce_forward = gf ;
    p_dio_mmap->gforce_right = gr ;
    p_dio_mmap->gforce_down = gd ;
    dio_unlock();
}

// calibrate g-sensor.
//   return 0: no sensor, 1: ok, -1: command failed
//   comunications:  
//       input :  p_dio_mmap->rtc_cmd = 10 
//       output : 
//            success => p_dio_mmap->rtc_cmd = 0 
//                       p_dio_mmap->rtc_year = response code
//            failed  => p_dio_mmap->rtc_cmd = -1
int gforce_calibration()
{
	char rsp[MCU_MAX_MSGSIZE] ;
    // send init value to mcu
    if( mcu_cmd(rsp, MCU_CMD_GSENSORCALIBRATION, 0 )>=7 ) {    // command success
        dio_lock();
        p_dio_mmap->rtc_year = rsp[5] ;
        p_dio_mmap->rtc_cmd = 0 ;
        dio_unlock();
        return 0 ;
    }
    else {
        p_dio_mmap->rtc_cmd = -1 ;
        return -1 ;
    }
}


// Enter/Leave g-sensor mount angle calibration.
void gforce_calibrate_mountangle(int cal)
{
    if( cal ) {
        // send small trigger init value to mcu
        mcu_cmd(NULL, MCU_CMD_GSENSORINIT, 
                20,      // 20 parameters 
                1,       // enable GForce
                direction_table[unit_direction][2],   // unit direction code
                1, -1, 1, -1, 1, -1,                  // small trigger value to make gforce output every time.
                2, -2, 2, -2, 2, -2,
                100, -100, 100, -100, 100, -100 );    // big crash value to stop it playing
        // set mount angle to zero while calibrating
        gforce_mountangle = 0 ;                                             
    }
    else {
        dvr_log("Mounting angle calibration stopped!");
        gforce_reinit();
    }
}

// re-initialize gforce sensor
void gforce_reinit()
{
    config dvrconfig(CFG_FILE);
    dvr_log("Re-initialize gforce sensor");
    gforce_init( dvrconfig );
}

// initialize gforce sensor
void gforce_init( config & dvrconfig ) 
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
    gforce_mountangle = dvrconfig.getvalueint( "io", "gsensor_mountangle" );
    
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

    v = dvrconfig.getvalue( "io", "gsensor_direction" );
    if( v.length()>0 ) {
        gsensor_direction=dvrconfig.getvalueint( "io", "gsensor_direction" );
    }
    if( gsensor_direction<0 || gsensor_direction>=24 ) {
        gsensor_direction = DEFAULT_DIRECTION  ;
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
    rsize = mcu_cmd(rsp, MCU_CMD_GSENSORINIT, 
                       20,      // 20 parameters 
                       1,       // enable GForce
//                       0x12,    // 0x12=keep all value in it original direction (@&$^-$)
                       direction_table[unit_direction][2],                      // unit direction code
                       base_x_pos, base_x_neg, base_y_pos, base_y_neg, base_z_pos, base_z_neg,
                       trigger_x_pos, trigger_x_neg, trigger_y_pos, trigger_y_neg, trigger_z_pos, trigger_z_neg,
                       crash_x_pos, crash_x_neg, crash_y_pos, crash_y_neg, crash_z_pos, crash_z_neg ) ;

    if( rsize>0 && rsp[5] ) { // g_sensor available
        FILE * fgsensor ;
        dvr_log("G force sensor detected!");
        fgsensor = fopen( VAR_DIR"/gsensor", "w" );
        if( fgsensor ) {
            fprintf(fgsensor, "1");
            fclose(fgsensor);
        }
        gforce_available = 1 ;
        if( gforce_log_enable==0 ) {
            // disable GFORCE
/*
             // this command (disable GSENSOR ) could make MCU crazy, disabled 2011-01-13
             mcu_cmd( MCU_CMD_GSENSORINIT, 
                    20,      // 20 parameters 
                    0,       // disable GForce
                    direction_table[unit_direction][2],                      // unit direction code
                    base_x_pos, base_x_neg, base_y_pos, base_y_neg, base_z_pos, base_z_neg,
                    trigger_x_pos, trigger_x_neg, trigger_y_pos, trigger_y_neg, trigger_z_pos, trigger_z_neg,
                    crash_x_pos, crash_x_neg, crash_y_pos, crash_y_neg, crash_z_pos, crash_z_neg ) ;
*/
        }
    }
    else {
        dvr_log("G force sensor init failed!");
    }

	strncpy( disk_curdiskfile, dvrconfig.getvalue("system", "currentdisk"), sizeof(disk_curdiskfile));
    if( strlen( disk_curdiskfile )<2) {
		strcpy( disk_curdiskfile, VAR_DIR"/dvrcurdisk" ) ;
    }
	
}

void gforce_finish()
{
}
