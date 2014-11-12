// dio memory map

#ifndef __DIOMAP_H__
#define __DIOMAP_H__

#include <semaphore.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

struct dio_mmap {
    int     usage ;         // how many processes use this structure
    int     lock ;          // >0 if some process is writing to this structure
    pid_t   iopid ;         // process id of io process
    pid_t   dvrpid ;        // process id of dvr server, 0 when dvrsvr is down
    pid_t   glogpid ;       // process id of glog (gpslog)

	// 266
	pid_t   tab102pid;
        

    int		inputnum ;
    unsigned int inputmap ;     // 32 input pin max
    int		outputnum ;
    unsigned int outputmap ;	// 32 output pin max

    int     dvrcmd ;		// -1: status error, 0: status OK, 1: restart(resume), 2: suspend, 3: stop record, 4: start record
    int     dvrstatus ;		// (see DVR STATUS bit definition) bit0: running, bit1: recording, bit2: video lost, bit3: no motion, bit4: network active, bit5: disk ready, bit6: no camera data, bit15: error condition
    char    iomsg[128] ;        // IO message to display on screen

    int     poweroff ;
    int     lockpower ;		// 1: lock power (don't turn off power), 0: unlock power
    int     dvrwatchdog ;	// dvr watchdog counter, dvr should clear this number
    int     iomode ;            // io runing mode
    int     suspendtimer ;      // suspend timer for IO suspend mode (during file copying)

    unsigned short rtc_year ;
    unsigned short rtc_month ;
    unsigned short rtc_day ;
    unsigned short rtc_hour ;
    unsigned short rtc_minute ;
    unsigned short rtc_second ;
    unsigned short rtc_millisecond ;
    int	    rtc_cmd ;               // DVR rtc command, 1: read rtc, 2: set rtc, 3: sync system time to rtc, 0: cmd finish, -1: error

    // GPS communicate area
    int         gps_connection;		// 266

    int     gps_valid ;             // 1 for valid gps signal data
    double  gps_speed ;             // knots
    double  gps_direction ;         // degree
    double  gps_latitude ;          // degree, + for north, - for south
    double  gps_longitud ;          // degree, + for east,  - for west
    double  gps_gpstime ;           // seconds from 1970-1-1 12:00:00 (utc)

    // Extra IO
    // Panel LEDs
    unsigned int	panel_led ;	// bit 0: USB flash LED, bit1: Error LED, bit2: Video Lost LED
                                        // set 0 turn off, set 1 to flash
    unsigned int	devicepower ;	// power on/off
                                        // bit 0: GPS, bit 1:  Slave_Eagle32, bit 2: Network Switch, bit 3: POE, bit 4: camera power
                                        // (another set device power, 2011-05-05) bit 8: Wifi, bit 9: POW, bit 10: Radar
    int    iotemperature ;		// io board temperature
    int    hdtemperature ;              // hard drive temperature
    
    // 266
    int    hdtemperature1 ;          // hard drive temperature
    int    hdtemperature2 ;          // hard drive temperature

    // G force sensor value
    //int     gforce_serialno ;           // indicator
    //float   gforce_right ;
    //float   gforce_forward ;
    //float   gforce_down ;

    // G force sensor value (266)
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


	// 266 stuffs :
	
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
    

    // DVR camera status ;
    int     camera_status[8] ;          // bit 0 = signal, bit 1 = recording, bit 2 = motion, bit 3 = camera video data available

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
                                        //  BIT 13:  Speaker Mute Botton (virtual) 1: pressed, auto release
                                        //  BIT 14:  Sperker On Botton (virtual) 1: pressed, auto release

    unsigned int pwii_output ;          // LEDs and device power (outputs)
                                        // BIT 0: C1 LED
                                        // BIT 1: C2 LED
                                        // BIT 2: MIC LED
                                        // BIT 3: ERROR LED
                                        // BIT 4: POWER LED
                                        // BIT 5: BO_LED
                                        // BIT 6: Backlight LED
                                        // BIT 7: LP zoom in

                                        // BIT 8: GPS antenna power
                                        // BIT 9: GPS POWER
                                        // BIT 10: RF900 POWER
                                        // BIT 11: LCD power
                                        // BIT 12: standby mode, 1: standby, 0: running		// black out
                                        // BIT 13: WIFI power
    int     pwii_error_LED_flash_timer ; // LED flash timer (output),  0: stayon, others in 0.25 second step
    char    pwii_VRI[128] ;             // current VRI(video recording Id)

    // Wifi status
    int     smartserver ;               // smartserver detected
    char    smartserver_interface[32] ;

} ;

// bit7: archiving disk ready, bit8: sensor active, bit15: error condition

// dvr status bits
#define DVR_RUN         (0x1)			// dvr running
#define DVR_RECORD      (0x2)			// recording
#define DVR_VIDEOLOST   (0x4)			// Video signal lost (any channel)
#define DVR_NOMOTION    (0x8)			// No motion detected (any channel)
#define DVR_NETWORK     (0x10)			// Network active
#define DVR_DISKREADY   (0x20)			// disk ready ( recording disk )
#define DVR_NODATA      (0x40)			// no video data (any channel)
#define DVR_LOCK        (0x80)			// recording locked file (any channel) (PWII only)
#define DVR_ARCH        (0x100)			// archive thread running
#define DVR_ARCHDISK    (0x200)			// archive disk ready
#define DVR_SENSOR      (0x400)			// sensor active

#define DVR_FAILED      (0x4000)		// should ioprocess reboot system?
#define DVR_ERROR       (0x8000)

// app_mode ( bill )
#define APPMODE_QUIT            (0)
#define APPMODE_RUN             (1)
#define APPMODE_SHUTDOWNDELAY   (2)
#define APPMODE_NORECORD        (3)
#define APPMODE_PRESTANDBY      (4)
#define APPMODE_STANDBY         (5)
#define APPMODE_SHUTDOWN        (6)
#define APPMODE_REBOOT          (7)
#define APPMODE_REINITAPP       (8)


// io mode
#define IOMODE_QUIT            (0)
#define IOMODE_RUN             (1)
#define IOMODE_SHUTDOWNDELAY   (2)
#define IOMODE_ARCHIVE         (3)
#define IOMODE_DETECTWIRELESS  (4)
#define IOMODE_UPLOADING       (5)
#define IOMODE_STANDBY         (6)
#define IOMODE_SHUTDOWN        (7)
#define IOMODE_REBOOT          (8)
#define IOMODE_REINITAPP       (9)
#define IOMODE_SUSPEND         (10)
#define IOMODE_POWEROFF        (11)

// HARD DRIVE LED and STATUS
#define HDLED	(0x10)

// device power bit
#define DEVICE_POWER_GPS		(1)
#define DEVICE_POWER_SLAVE		(1<<1)
#define DEVICE_POWER_NETWORKSWITCH	(1<<2)
#define DEVICE_POWER_POE		(1<<3)
#define DEVICE_POWER_CAMERA		(1<<4)

// new adds on power bit
#define DEVICE_POWER_WIFI		(1<<8)		// wifi power on PW unit
#define DEVICE_POWER_POEPOWER           (1<<9)          // wow , another POE power?
#define DEVICE_POWER_RADAR		(1<<10)		// Radar power, what is this?
#define DEVICE_POWER_HD			(1<<11)		// Power for Hard drive, CAUTION: turn off this actually turn off system (why?)

// power bits on runing mode
#define DEVICE_POWER_RUN ( DEVICE_POWER_GPS | DEVICE_POWER_SLAVE | DEVICE_POWER_NETWORKSWITCH | DEVICE_POWER_POE | DEVICE_POWER_CAMERA | DEVICE_POWER_HD | DEVICE_POWER_POEPOWER)
// power bits on standby mode ( ********** keep HD power, or system will turn off *************** )
#define DEVICE_POWER_STANDBY ( DEVICE_POWER_HD )

// PWII CDC buttons
#define  PWII_BT_REW            (1)
#define  PWII_BT_PP             (1<<1)
#define  PWII_BT_FF             (1<<2)
#define  PWII_BT_ST             (1<<3)
#define  PWII_BT_POWER          (PWII_BT_ST)
#define  PWII_BT_PR             (1<<4)
#define  PWII_BT_NX             (1<<5)
#define  PWII_BT_C1             (1<<8)
#define  PWII_BT_C2             (1<<9)
#define  PWII_BT_TM             (1<<10)
#define  PWII_BT_LP             (1<<11)
#define  PWII_BT_BO             (1<<12)
#define  PWII_BT_SPKMUTE        (1<<13)
#define  PWII_BT_SPKON          (1<<14)

// PWII CDC led
#define PWII_LED_C1             (1)
#define PWII_LED_C2             (1<<1)
#define PWII_LED_MIC            (1<<2)
#define PWII_LED_ERROR          (1<<3)
#define PWII_LED_POWER          (1<<4)
#define PWII_LED_BO             (1<<5)
#define PWII_LED_BACKLIGHT      (1<<6)

#define PWII_LP_ZOOMIN			(1<<7)

#define PWII_POWER_ANTENNA		(1<<8)
#define PWII_POWER_GPS			(1<<9)
#define PWII_POWER_RF900		(1<<10)
#define PWII_POWER_LCD			(1<<11)
#define PWII_POWER_BLACKOUT		(1<<12)
#define PWII_POWER_STANDBY		(PWII_POWER_BLACKOUT)
#define PWII_POWER_WIFI			(1<<13)


extern struct dio_mmap * p_dio_mmap;
void dio_lock();
void dio_unlock();
struct dio_mmap * dio_mmap(char * mmapfile=NULL);
void dio_munmap();



// PWII key codes

enum e_keycode {
    VK_RETURN=0x0D, // ENTER key
    VK_ESCAPE=0x1B, // ESC key
    VK_SPACE=0x20,  // SPACEBAR
    VK_PRIOR,       // PAGE UP key
    VK_NEXT,        // PAGE DOWN key
    VK_END,         // END key
    VK_HOME,        // HOME key
    VK_LEFT,        // LEFT ARROW key
    VK_UP,          // UP ARROW key
    VK_RIGHT,       // RIGHT ARROW key
    VK_DOWN,        // DOWN ARROW key

    VK_MEDIA_NEXT_TRACK=0xB0, // Next Track key
    VK_MEDIA_PREV_TRACK,      // Previous Track key
    VK_MEDIA_STOP,            // Stop Media key
    VK_MEDIA_PLAY_PAUSE,      // Play/Pause Media key

    // pwii definition
    VK_EM   = 0xE0 ,    // EVEMT
    VK_LP,              // PWII, LP key (zoom in)
    VK_POWER,           // PWII, B/O
    VK_SILENCE,         // PWII, Mute
    VK_RECON,           // Turn on all record (force on)
    VK_RECOFF,          // Turn off all record
    VK_C1,              // PWII, Camera1
    VK_C2,              // PWII, Camera2
    VK_C3,              // PWII, Camera3
    VK_C4,              // PWII, Camera4
    VK_C5,              // PWII, Camera5
    VK_C6,              // PWII, Camera6
    VK_C7,              // PWII, Camera7
    VK_C8,              // PWII, Camera8
    VK_MUTE,            // PWII, Faked Mute button
    VK_SPKON            // PWII, Faked Speaker On button
} ;

#define VK_FRONT (VK_C1)
#define VK_REAR (VK_C2)
#define VK_TM   (VK_EM)


#endif
