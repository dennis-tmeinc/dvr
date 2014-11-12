
#ifndef __MCU_H__
#define __MCU_H__

// MCU POWER DOWN CODE
//01: software watchdog reset the power
//02: CPU request system reset
//03: key off before system boot up ready
//04: key off power
//05: bootup time out
//06: power input is too low/high when power on
//07: power input is too low/high after system already on
//08: power input is not stable
//09: HD heater on
//0a: temperature is too high when power on
//0b: temperature is too high after power on
//0c: hardware reset
//0d: CPU request power off
//0e: Normal power off

#define ID_HOST     (0)
#define ID_MCU      (1)

#define MCU_CMD_RESET	        (0)
#define MCU_CMD_REBOOT	        (0x01) 			// this command would send back response compare to MCU_CMD_RESET (USED on ZEUS project)
#define MCU_CMD_READRTC         (0x6)
#define MCU_CMD_WRITERTC        (0x7)
#define MCU_CMD_POWEROFFDELAY	(0x8)
#define MCU_CMD_IOTEMPERATURE   (0x0b)
#define MCU_CMD_HDTEMPERATURE   (0x0c)
#define MCU_CMD_BOOTUPREADY     (0x11)
#define MCU_CMD_SETSENSOR2INV   (0x13)
#define MCU_CMD_GETSENSOR2INV   (0x14)
#define MCU_CMD_KICKWATCHDOG    (0x18)
#define MCU_CMD_SETWATCHDOGTIMEOUT (0x19)
#define MCU_CMD_ENABLEWATCHDOG  (0x1a)
#define MCU_CMD_DISABLEWATCHDOG (0x1b)
#define MCU_CMD_DIGITALINPUT	(0x1d)
#define MCU_CMD_CAMERA_ZOOMIN   (0x1f)
#define MCU_CMD_CAMERA_ZOOMOUT  (0x20)
#define MCU_CMD_MICON	        (0x21)
#define MCU_CMD_MICOFF	        (0x22)
#define MCU_CMD_INCARMICON	    (0x23)
#define MCU_CMD_INCARMICOFF	    (0x24)
#define MCU_CMD_HDPOWERON       (0x28)
#define MCU_CMD_HDPOWEROFF      (0x29)
#define MCU_CMD_GETVERSION	    (0x2d)
#define MCU_CMD_DEVICEPOWER     (0x2e)
#define MCU_CMD_PANELLED        (0x2f)
#define MCU_CMD_DIGITALOUTPUT	(0x31)

// gforce sensor cmd
#define MCU_CMD_GSENSORINIT	    (0x34)
#define MCU_CMD_GSENSORUPLOAD   (0x35)
#define MCU_CMD_GSENSORUPLOADACK (0x36)
#define MCU_CMD_GSENSORUPLOADACK (0x36)
#define MCU_CMD_GSENSORCALIBRATION (0x3F)
#define MCU_CMD_GSENSORPEAK     (0x40)

#define MCU_CMD_READCODE	    (0x41)

#define MCU_CMD_WIFIPOWER	    (0x38)
#define MCU_CMD_POEPOWER	    (0x3a)
#define MCU_CMD_RADARPOWER	    (0x3b)

// setup sensor 2/3 power on
#define MCU_CMD_SENSOR23		(0x37)
#define MCU_CMD_SENSORINVMAP	(MCU_CMD_SENSOR23)

#ifdef SUPPORT_YAGF
#define MCU_CMD_BLOCKMCU        (0x42)
#endif

// mcu input
#define MCU_INPUT_DIO           (0x1C)
#define MCU_INPUT_IGNITIONOFF   (0x08)
#define MCU_INPUT_IGNITIONON    (0x09)
#define MCU_INPUT_GSENSOR       (0x40)

#define MCU_CMD_DELAY       (500000)
#define MIN_SERIAL_DELAY	(100000)
#define DEFSERIALBAUD	    (115200)



// open serial port
int serial_open(char * device, int buadrate) ;
int serial_dataready(int handle, int usdelay=MIN_SERIAL_DELAY, int * usremain=NULL);


extern int mcu_oninput( char * inputmsg );

// Generic mcu communications

// Check if data ready to read on serial port
//     return 0: no data
//            1: yes
int mcu_dataready(int usdelay, int * usremain=NULL);
int mcu_read(char * sbuf, size_t bufsize, int wait=100000, int interval=10000);
int mcu_write(void * buf, int bufsize);

// clear receiving buffer
void mcu_clear(int delay=0);

// check data check sum
// return 0: checksum correct
char mcu_checksum( char * data );
// calculate check sum of data
void mcu_calchecksum( char * data );

// send constructed mcu message
int mcu_send( char * msg );
// receive a mcu message
int mcu_recv( char * rmsg, int rsize, int usdelay=1000000, int * usremain=NULL);

// send command to mcu without waiting for responds
int mcu_sendcmd(int cmd, int datalen=0, ...);
int mcu_sendcmd_target(int target, int cmd, int datalen=0, ...);
// send command and wait for responds
int mcu_cmd(char * rsp, int cmd, int datalen=0, ...);
int mcu_cmd_target(char * rsp, int target, int cmd, int datalen=0, ...);
// check mcu input
// parameter
//      wait - micro-seconds waiting
// return
//		number of input message received.
//      0: timeout (no message)
//     -1: error
int mcu_input(int usdelay);

// response to mcu
//    msg : original received input from MCU
void mcu_response(char * msg, int datalen=0, ... );
// restart mcu port, some times serial port die
void  mcu_restart();
// initialize serial port
void mcu_init( config & dvrconfig );
void mcu_finish();

#define MCU_MAX_MSGSIZE (100)

// mcu commands

int mcu_bootupready(int* D1);
int mcu_iosensorrequest();
int mcu_iosensor_send();
int mcu_readcode();
// digital output
int mcu_dio_output( unsigned int outputmap );
// digital input
int mcu_dio_input();
int mcu_reboot();
// get MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_version(char * version);
// zoom in camera
int mcu_camera_zoomin(int zoomin);
int mcu_battery_check( int * voltage = NULL );
int mcu_set_pancam( int cameramode=0 );
int mcu_poe_power( int onoff=1 );
int mcu_campower();

/*
int mcu_reset();
int mcu_reboot();
int mcu_bootupready();
time_t mcu_r_rtc( struct tm * ptm = NULL );
int mcu_w_rtc(time_t tt);
void mcu_rtctosys();
void mcu_readrtc();
void mcu_led(int led, int flash);
void mcu_devicepower(int device, int poweron );
int mcu_version(char * version);
void mcu_poweroffdelay(int delay=50);
void mcu_watchdogenable(int timeout=10);
void mcu_watchdogdisable();
int mcu_watchdogkick();
int mcu_iotemperature();
int mcu_hdtemperature();
void mcu_hdpoweron();
void mcu_hdpoweroff();
int mcu_doutput();
void mcu_dinput_map(char * ibuf);
int mcu_dinput();
int mcu_update_firmware( char * firmwarefile) ;
void mcu_camera_zoom( int zoomin );
void mcu_sensor23poweron(int sensor2inv, int sensor3inv);
void mcu_restart();
void mcu_init(config & dvrconfig) ;
void mcu_finish();
*/

#endif      // __MCU_H__
