// dio memory map

#ifndef __DIOMAP_H__
#define __DIOMAP_H__

#include <semaphore.h>

struct dio_mmap {
    int     usage ;         // how many processes use this structure
    int     lock ;          // >0 if some process is writing to this structure
    pid_t	iopid ;			// process id of io process
    pid_t	dvrpid ;		// process id of dvr server, 0 when dvrsvr is down
    pid_t   glogpid ;       // process id of glog (gpslog)
    
    int		inputnum ;
    unsigned int inputmap ;		// 32 input pin max
    int		outputnum ;
    unsigned int outputmap ;	// 32 output pin max
    
    int		dvrcmd ;		// -1: status error, 0: status OK, 1: restart(resume), 2: suspend, 3: stop record, 4: start record
    int     dvrstatus ;		// bit0: running, bit1: recording, bit2: video lost, bit3: no motion, bit4: network active, bit5: disk ready, bit6: no camera data, bit15: error condition
    char    iomsg[128] ;    // IO message to display on screen
    
    int     poweroff ;
    int		lockpower ;		// 1: lock power (don't turn off power), 0: unlock power
    int		dvrwatchdog ;	// dvr watchdog counter, dvr should clear this number
    int     iomode ;        // io runing mode

    unsigned short rtc_year ;
    unsigned short rtc_month ;
    unsigned short rtc_day ;
    unsigned short rtc_hour ;
    unsigned short rtc_minute ;
    unsigned short rtc_second ;
    unsigned short rtc_millisecond ;
    int	    rtc_cmd ;		// DVR rtc command, 1: read rtc, 2: set rtc, 3: sync system time to rtc, 0: cmd finish, -1: error
    
    // GPS communicate area
    int		gps_valid ;			// 1 for valid gps signal data
    double	gps_speed ;			// knots
    double	gps_direction ;     // degree
    double  gps_latitude ;		// degree, + for north, - for south
    double  gps_longitud ;      // degree, + for east,  - for west
    double  gps_gpstime ;		// seconds from 1970-1-1 12:00:00 (utc)
   
	// Extra IO
	// Panel LEDs
	unsigned int	panel_led ;		// bit 0: USB flash LED, bit1: Error LED, bit2: Video Lost LED
									// set 0 turn off, set 1 to flash
	unsigned int	devicepower ;	// power on/off 
                                    // bit 0: GPS, bit 1:  Slave_Eagle32, bit 2: Network Switch, bit 3: POE, bit 4: camera power
    int    iotemperature ;          // io board temperature
    int    hdtemperature ;          // hard drive temperature
    
    // G force sensor value
    int     gforce_serialno ;    // indicator
    float   gforce_right ;
    float   gforce_forward ;
    float   gforce_down ;    

    // DVR camera status ;
    int     camera_status[8] ;  // bit 0 = signal, bit 1 = recording, bit 2 = motion, bit 3 = camera video data available
    
    // PWII mcu support
    unsigned int pwii_buttons ;         // pwii button,  1: pressed, 0: released
                                         //      DD BIT0: Function REW
                                         //      DD BIT1: Function P/P
                                         //      DD BIT2: Function FF
                                         //      DD BIT3: Function ST/PWR
                                         //      DD BIT4: Function PR
                                         //      DD BIT5: Function NX
                                        //  BIT 8: front camera button, 1: pressed, auto release
                                        //  BIT 9: back camera button,  1: pressed, auto release
                                        //  BIT 10:  tm, 1: pressed, 0: released
                                        //  BIT 11:  lp, 1: pressed, 0: released
                                        //  BIT 12:  blackout, 1: pressed, 0: released
    
    unsigned int pwii_output ;          // LEDs and device power (outputs)
                                        // BIT 0: C1 LED
                                        // BIT 1: C2 LED
                                        // BIT 2: MIC LED
                                        // BIT 3: ERROR LED
                                        // BIT 4: POWER LED
                                        // BIT 5: BO_LED
                                        // BIT 6: Backlight LED
    
                                        // BIT 8: GPS antenna power
                                        // BIT 9: GPS POWER
                                        // BIT 10: RF900 POWER
                                        // BIT 11: LCD power
                                        // BIT 12: standby mode, 1: standby, 0: running
                                        // BIT 13: WIFI power
    int     pwii_error_LED_flash_timer ; // LED flash timer (output),  0: stayon, others in 0.25 second step
    char    pwii_VRI[128] ;             // current VRI(video recording Id)

    // Wifi status
    int     smartserver ;        // smartserver detected
    int     wifi_req ;

} ;

// dvr status bits
#define DVR_RUN         (0x1)
#define DVR_RECORD      (0x2)
#define DVR_VIDEOLOST   (0x4)
#define DVR_NOMOTION    (0x8)
#define DVR_NETWORK     (0x10)
#define DVR_DISKREADY   (0x20)
#define DVR_NODATA      (0x40)
#define DVR_LOCK        (0x80)
#define DVR_FAILED      (0x4000)        // should ioprocess reboot system?
#define DVR_ERROR       (0x8000)

// io mode
#define IOMODE_QUIT            (0)
#define IOMODE_RUN             (1)
#define IOMODE_SHUTDOWNDELAY   (2)
#define IOMODE_DETECTWIRELESS  (3)
#define IOMODE_UPLOADING       (4)
#define IOMODE_STANDBY         (5)
#define IOMODE_SHUTDOWN        (6)
#define IOMODE_REBOOT          (7)
#define IOMODE_REINITAPP       (8)

#endif
