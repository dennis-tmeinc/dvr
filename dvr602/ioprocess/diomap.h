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
    volatile int     usage ;         // how many processes use this structure
    volatile int     lock ;          // >0 if some process is writing to this structure
    volatile pid_t	iopid ;			// process id of io process
    volatile pid_t	dvrpid ;		// process id of dvr server, 0 when dvrsvr is down
    volatile pid_t   glogpid ;       // process id of glog (gpslog)
    
    volatile int		inputnum ;
    volatile unsigned int inputmap ;		// 32 input pin max
    volatile int		outputnum ;
    volatile unsigned int outputmap ;	// 32 output pin max
    
    volatile int		dvrcmd ;		// -1: status error, 0: status OK, 1: restart(resume), 2: suspend, 3: stop record, 4: start record
    volatile int     dvrstatus ;		// bit0: running, bit1: recording, bit2: video lost, bit3: no motion, bit4: network active, bit5: disk ready, bit6: no camera data, bit15: error condition
    
    volatile int     poweroff ;
    volatile int		lockpower ;		// 1: lock power (don't turn off power), 0: unlock power
    volatile int		dvrwatchdog ;	// dvr watchdog counter, dvr should clear this number
    
    volatile unsigned short rtc_year ;
    volatile unsigned short rtc_month ;
    volatile unsigned short rtc_day ;
    volatile unsigned short rtc_hour ;
    volatile unsigned short rtc_minute ;
    volatile unsigned short rtc_second ;
    volatile unsigned short rtc_millisecond ;
    volatile int	    rtc_cmd ;		// DVR rtc command, 1: read rtc, 2: set rtc, 3: sync system time to rtc, 0: cmd finish, -1: error
    
    // GPS communicate area
    volatile int		gps_valid ;			// GPS process set to 1 for valid data
    volatile double	gps_speed ;			// knots
    volatile double	gps_direction ;     // degree
    volatile double  gps_latitude ;		// degree, + for north, - for south
    volatile double  gps_longitud ;      // degree, + for east,  - for west
    volatile double  gps_gpstime ;		// seconds from 1970-1-1 12:00:00 (utc)
   
	// Extra IO
	// Panel LEDs
	volatile unsigned int	panel_led ;		// bit 0: USB flash LED, bit1: Error LED, bit2: Video Lost LED
									// set 0 turn off, set 1 to flash
	volatile unsigned int	devicepower ;	// power on/off 
                                    // bit 0: GPS, bit 1:  Slave_Eagle32, bit 2: Network Switch, bit 3: POE, bit 4: camera power
    volatile int    iotemperature ;          // io board temperature
    volatile int    hdtemperature ;          // hard drive temperature
    
    // G force sensor value
    volatile int     gforce_log0 ;         // indicator
    volatile float   gforce_right_0 ;
    volatile float   gforce_forward_0 ;
    volatile float   gforce_down_0 ;    
    volatile int     gforce_log1 ;         // indicator
    volatile float   gforce_right_1 ;
    volatile float   gforce_forward_1 ;
    volatile float   gforce_down_1 ;    
    volatile float   gforce_right_d ;
    volatile float   gforce_forward_d ;
    volatile float   gforce_down_d ;    
    
    // beeper
    volatile int     beeper ;

  // app_mode
  volatile int current_mode;

  // don't do disk_check (also used to stop copy process)
  volatile int nodiskcheck;

  volatile int mcu_cmd; // 1: HD off, 2: HD on
  volatile int reserved[10];
} ;

// app_mode
enum { APPMODE_QUIT,
       APPMODE_RUN,
       APPMODE_SHUTDOWNDELAY,
       APPMODE_NORECORD,
       APPMODE_RESTARTDVR,
       APPMODE_PRESTANDBY,
       APPMODE_STANDBY,
       APPMODE_SHUTDOWN,
       APPMODE_REBOOT,
       APPMODE_REINITAPP};

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
