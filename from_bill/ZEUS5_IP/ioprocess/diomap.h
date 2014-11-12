// dio memory map

/* --- Changes ---
 * 08/24/2009 by Harrison
 *   1. Added current_mode to save app_mode info
 *   2. moved APPMODE macros from ioprocess.cpp to here (also used by httpd)
 *
 * 09/11/2009 by Harrison
 *   1. Added nodiskcheck flag (ioprocess --> dvrsvr),
 *      set this flag during tab102 download
 *
*/


#ifndef __DIOMAP_H__
#define __DIOMAP_H__

struct dio_mmap {
    int     usage ;         // how many processes use this structure
    int     lock ;          // >0 if some process is writing to this structure
    pid_t   iopid ;			// process id of io process
    pid_t   dvrpid ;		// process id of dvr server, 0 when dvrsvr is down
    pid_t   glogpid ;       // process id of glog (gpslog)
    pid_t   tab102pid;
    
    int		inputnum ;
    unsigned int inputmap ;		// 32 input pin max
    int		 outputnum ;
    unsigned int outputmap ;	// 32 output pin max
    
    int	    dvrcmd ;		// -1: status error, 0: status OK, 1: restart(resume), 2: suspend, 3: stop record, 4: start record
    int     dvrstatus ;		// bit0: running, bit1: recording, bit2: video lost, bit3: no motion, bit4: network active, bit5: disk ready, bit6: no camera data, bit15: error condition
    
    int     poweroff ;
    int	    lockpower ;	// 1: lock power (don't turn off power), 0: unlock power
    int	    dvrwatchdog ;	// dvr watchdog counter, dvr should clear this number
    
    unsigned short rtc_year ;
    unsigned short rtc_month ;
    unsigned short rtc_day ;
    unsigned short rtc_hour ;
    unsigned short rtc_minute ;
    unsigned short rtc_second ;
    unsigned short rtc_millisecond ;
    int	    rtc_cmd ;		// DVR rtc command, 1: read rtc, 2: set rtc, 3: sync system time to rtc, 0: cmd finish, -1: error
    
    // GPS communicate area
    int         gps_connection;
    int		gps_valid ;			// GPS process set to 1 for valid data
    double	gps_speed ;			// knots
    double	gps_direction ;     // degree
    double  gps_latitude ;		// degree, + for north, - for south
    double  gps_longitud ;      // degree, + for east,  - for west
    double  gps_gpstime ;		// seconds from 1970-1-1 12:00:00 (utc)
   
	// Extra IO
	// Panel LEDs
    unsigned int	panel_led ;	// bit 0: USB flash LED, bit1: Error LED, bit2: Video Lost LED
	// set 0 turn off, set 1 to flash
    unsigned int	devicepower ;	// power on/off 
    // bit 0: GPS, bit 1:  Slave_Eagle32, bit 2: Network Switch, bit 3: POE, bit 4: camera power
    int    iotemperature ;          // io board temperature
    int    hdtemperature1 ;          // hard drive temperature
    int    hdtemperature2 ;          // hard drive temperature
    
    // G force sensor value
    int     gforce_log0 ;         // indicator
    float   gforce_right_0 ;
    float   gforce_forward_0 ;
    float   gforce_down_0 ;    
    int     gforce_log1 ;         // indicator
    float   gforce_right_1 ;
    float   gforce_forward_1 ;
    float   gforce_down_1 ;    
    
    float   gforce_forward_d;
    float   gforce_down_d ;     
    float   gforce_right_d ;  
    int     gforce_changed;
    int     synctimestart;
    
    // beeper
    int     beeper ;

  // app_mode
    int current_mode;

  // don't do disk_check (also used to stop copy process)
    int nodiskcheck;

    int mcu_cmd; // 1: HD off, 2: HD on
    int ishybrid_copy; //1 hybrid is copying ,0 hybrid is not
    int tab102_ready;
    int fileclosed;
    int tab102_isLive;
} ;

// app_mode
#define APPMODE_QUIT            (0)
#define APPMODE_RUN             (1)
#define APPMODE_SHUTDOWNDELAY   (2)
#define APPMODE_NORECORD        (3)
#define APPMODE_PRESTANDBY      (4)
#define APPMODE_STANDBY         (5)
#define APPMODE_SHUTDOWN        (6)
#define APPMODE_REBOOT          (7)
#define APPMODE_REINITAPP       (8)

// dvr status bits
#define DVR_RUN         (0x1)
#define DVR_RECORD      (0x2)
#define DVR_VIDEOLOST   (0x4)
#define DVR_NOMOTION    (0x8)
#define DVR_NETWORK     (0x10)
#define DVR_DISKREADY   (0x20)
#define DVR_NODATA      (0x40)
#define DVR_ERROR       (0x8000)

#endif
