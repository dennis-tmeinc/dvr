#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "../dvrsvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

// HARD DRIVE LED and STATUS
#define HDLED	(0x10)
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)

struct baud_table_t {
    speed_t baudv ;
    int baudrate ;
} baud_table[7] = {
    { B2400, 	2400},
    { B4800,	4800},
    { B9600,	9600},
    { B19200,	19200},
    { B38400,	38400},
    { B57600,	57600},
    { B115200,	115200} 
} ;

struct dio_mmap * p_dio_mmap ;

#define SERIAL_DELAY	(500000)
#define DEFSERIALBAUD	(115200)

char dvriomap[256] = "/var/dvr/dvriomap" ;
char serial_dev[256] = "/dev/ttyS1" ;
char * pidfile = "/var/dvr/ioprocess.pid" ;
int serial_handle ;
unsigned int mcu_poweroffdelaytimer ;

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;
int ledflashing=0;
int watchdogenabled=0;
int gpsvalid = 0 ;
int app_mode = 0 ;          // 0: quit!, 1: run, 2: shutdown delay 3: standby, 4: reboot (30s before hard reboot)
unsigned int panelled=0 ;
unsigned int devicepower=0xffff;


void sig_handler(int signum)
{
    if( signum == SIGUSR1 ) {
        ledflashing=1 ;
    }
    else if( signum == SIGUSR2 ) {
        if( p_dio_mmap ) {
            p_dio_mmap->outputmap=0;
        }
        ledflashing=0 ;
    }
    else {
        app_mode=0;
    }
}

void dio_lock()
{
    int i;
    for( i=0; i<1000; i++ ) {
        if( p_dio_mmap->lock>0 ) {
            usleep(1);
        }
        else {
            break ;
        }
    }
    p_dio_mmap->lock=1;
}

void dio_unlock()
{
    p_dio_mmap->lock=0;
}

// Check if data ready to read on serial port
//     return 0: no data
//            1: yes
int serial_dataready(int microsecondsdelay)
{
    struct timeval timeout ;
    fd_set fds;
    if( serial_handle<=0 ) {
        return 0;
    }
    timeout.tv_sec = microsecondsdelay/1000000 ;
    timeout.tv_usec = microsecondsdelay%1000000 ;
    FD_ZERO(&fds);
    FD_SET(serial_handle, &fds);
    if (select(serial_handle + 1, &fds, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(serial_handle, &fds);
    } else {
        return 0;
    }
}

int serial_read(void * buf, size_t bufsize)
{
    if( serial_handle>0 ) {
        return read( serial_handle, buf, bufsize);
    }
    return 0;
}

int serial_write(void * buf, size_t bufsize)
{
    if( serial_handle>0 ) {
        return write( serial_handle, buf, bufsize);
    }
    return 0;
}

#define MCU_INPUTNUM (6)
#define MCU_OUTPUTNUM (4)

unsigned int mcu_doutputmap ;

// check data check sum
// return 0: checksum correct
int mcu_checksun( char * data ) 
{
    char ck;
    int i ;
    if( data[0]<5 || data[0]>40 ) {
        return -1 ;
    }
    ck=0;
    for( i=0; i<(int)data[0]; i++ ) {
        ck+=data[i] ;
    }
    return (int)ck;
}

// calculate check sum of data
void mcu_calchecksun( char * data )
{
    char ck ;
    int i;
    if( data[0]<5 || data[0]>40 ) {	// wrong data
        return ;
    }
    ck=0 ;
    for( i=0; i<(data[0]-1); i++ ) {
        ck-=data[i] ;
    }
    data[i] = ck ;
}

// send command to mcu
// return 0 : failed
//        >0 : success
int mcu_send( char * cmd ) 
{
    if( *cmd<5 || *cmd>40 ) { 	// wrong data
        return 0 ;
    }
    mcu_calchecksun( cmd );
    return serial_write(cmd, (int)(*cmd));
}

// receive one data package from mcu
// return 0: failed
//       >0: bytes of data received
int mcu_recv( char * recv, int size )
{
    int n;
    if( size<5 ) {				// minimum packge size?
        return 0 ;
    }
    while( serial_dataready(SERIAL_DELAY) ) {
        if( serial_read(recv, 1) ) {
            n=(int) *recv ;
            if( n>=5 && n<=size ) {
                n=serial_read(recv+1, n-1) ;
                n++ ;
                if(n==recv[0] && 
                   recv[1]==0 && 
                   recv[2]==1 &&
                   mcu_checksun( recv ) == 0 ) {
                       return n;
                   }
            }
        }
    }
    return 0 ;
}

// receive dont cared data from mcu
void mcu_recv_dontcare()
{
    char rbuf[20] ;
    while( serial_dataready(SERIAL_DELAY) ) {
        serial_read(rbuf, 10);
    }
}

int mcu_bootupready()
{
    static char cmd_bootupready[8]="\x07\x01\x00\x11\x02\x00\xcc" ;
    char responds[20] ;
    
    while( serial_dataready(100000) ) {
        serial_read(responds, 20);
    }
    
    cmd_bootupready[5]=(char)watchdogenabled;
    if(mcu_send(cmd_bootupready)) {
        if( mcu_recv( responds, 20 ) ) {
            if( responds[3]=='\x11' &&
               responds[4]=='\x03' ) {
                   
                   p_dio_mmap->inputmap = responds[5] ;	// get default input map
                   
                   panelled=0 ;
                   devicepower=0xffff;
                   
                   return 1 ;
               }
        }
    }
    return 0 ;
}

int mcu_doutput()
{
    static char cmd_doutput_on[8] ="\x07\x01\x00\x25\x02\x00\xcc" ;
    static char cmd_doutput_off[8]="\x07\x01\x00\x26\x02\x00\xcc" ;
    char responds[20] ;
    int resok;
    
    int x = mcu_doutputmap^(p_dio_mmap->outputmap) ;
    
    if( x==0 ) {
        return 0 ;
    }
    mcu_doutputmap=p_dio_mmap->outputmap ;
    
    resok=0;
    // set output channel
    cmd_doutput_off[5] = (char)( (~mcu_doutputmap)&x ) ;
    
    if( cmd_doutput_off[5] ) {
        // send output off command
        if(mcu_send(cmd_doutput_off)) {
            if( mcu_recv( responds, 20 ) ) {
                if( responds[3]=='\x26' &&
                   responds[4]=='\x3' ) {
                       resok = 1 ;
                   }
            }
        }
    }
    
    // set ouput on channel
    cmd_doutput_on[5] = (char)( mcu_doutputmap&x ) ;
    
    if( cmd_doutput_on[5] ) {
        // send output on command
        if(mcu_send(cmd_doutput_on)) {
            if( mcu_recv( responds, 20 ) ) {
                if( responds[3]=='\x25' &&
                   responds[4]=='\x3' ) {
                       resok=1 ;
                   }
            }
        }
    }
    
    return resok;
}

// check mcu input
// parameter
//      usdelay - micro-seconds waiting
// return 
//		1: got something, digital input or power off input?
//		0: nothing
int mcu_input(int usdelay)
{
    int res = 0 ;
    int inum ;
    char ibuf[20] ;
    
    for(inum=0; inum<10; inum++ ) {
        if( serial_dataready(usdelay) ) {
            if( mcu_recv( ibuf, 20 )>0 ) {
                if( ibuf[3]=='\x1c' && ibuf[4]=='\x02' ) {	// dinput event
                    p_dio_mmap->inputmap = (int) (unsigned char) ibuf[5] ;
                    static char rsp_dinput[7] = "\x06\x01\x00\x1c\x03\xcc" ;
                    mcu_send(rsp_dinput);
                    res = 1;
                }
                else if( ibuf[3]=='\x08' && ibuf[4]=='\x02' ) {		// ignition off event
                    static char rsp_poweroff[10] = "\x08\x01\x00\x08\x03\x00\x10\xcc" ;
//                    rsp_poweroff[5] = ((p_dio_mmap->lockpower)>>8)&0xff ;
//                    rsp_poweroff[6] = (p_dio_mmap->lockpower)&0xff ;
                    mcu_send(rsp_poweroff);
                    mcu_poweroffdelaytimer = ((unsigned)(ibuf[6]))*256+((unsigned)ibuf[7]) ;
                    p_dio_mmap->poweroff = 1 ;						// send power off message to DVR
                    
                    printf("Ignition off\n");
                    res = 1;
                }
                else if( ibuf[3]=='\x09' && ibuf[4]=='\x02' ) {		// ignition on event
                    static char rsp_poweron[10] = "\x07\x01\x00\x09\x03\x00\xcc" ;
                    rsp_poweron[5]=(char)watchdogenabled;
                    mcu_send( rsp_poweron );
                    p_dio_mmap->poweroff = 0 ;						// send power on message to DVR
                    
                    printf("ignition on\n");
                    res = 1;
                }
            }
        }
        else {
            break;
        }
    }
    return res;
}

// get mcu digital input
int mcu_dinput()
{
    static char cmd_dinput[8]="\x06\x01\x00\x1d\x02\xcc" ;
    char responds[20] ;
    
    mcu_input(10000);
    
    if(mcu_send(cmd_dinput)) {
        if( mcu_recv( responds, 20 ) ) {
            if( responds[3]=='\x1d' &&
               responds[4]=='\x03' ) {
                   
                   p_dio_mmap->inputmap =(int) (unsigned char) responds[5] ;	// get digital input map
                   
                   return 1 ;
               }
        }
    }
    return 0 ;
}

// set more mcu power off delay
//    return remain delay time
int mcu_poweroffdelay(int delay)
{
    int delaytime = 0 ;
    static char cmd_poweroffdelaytime[10]="\x08\x01\x00\x08\x02\x00\x01\xcc" ;
    char responds[20] ;
    if( delay ) {
        cmd_poweroffdelaytime[6]=30 ;
    }
    else {
        cmd_poweroffdelaytime[6]=2 ;
    }
    if( mcu_send(cmd_poweroffdelaytime) ) {
        if( mcu_recv( responds, 20 ) ) {
            if( responds[3]=='\x08' &&
                responds[4]=='\x03' ) {
                   delaytime = ((unsigned)(responds[5]))*256+((unsigned)responds[6]) ;
               }
        }
    }
    return delaytime ;
}

void mcu_watchdogenable()
{
    char responds[20] ;
    static char cmd_watchdogenable[10]="\x06\x01\x00\x1a\x02\xcc" ;
    if( mcu_send(cmd_watchdogenable) ) {
        // ignor the responds, if any.
        mcu_recv( responds, 20 );
    }
    watchdogenabled = 1 ;
}

void mcu_watchdogdisable()
{
    char responds[20] ;
    static char cmd_watchdogdisable[10]="\x06\x01\x00\x1b\x02\xcc" ;
    if( mcu_send(cmd_watchdogdisable) ) {
        // ignor the responds, if any.
        mcu_recv( responds, 20 );
    }
    watchdogenabled = 0 ;
}

void mcu_watchdogkick()
{
    char responds[20] ;
    static char cmd_kickwatchdog[10]="\x06\x01\x00\x18\x02\xcc" ;
    if( mcu_send(cmd_kickwatchdog) ) {
        // ignor the responds, if any.
        mcu_recv( responds, 20 );
    }
}

void mcu_hdpoweron()
{
    char responds[20] ;
    static char cmd_hdpoweron[10]="\x06\x01\x00\x28\x02\xcc" ;
    if( mcu_send(cmd_hdpoweron) ) {
        // ignor the responds, if any.
        mcu_recv( responds, 20 );
    }
}

void mcu_hdpoweroff()
{
    char responds[20] ;
    static char cmd_hdpoweroff[10]="\x06\x01\x00\x29\x02\xcc" ;
    if( mcu_send(cmd_hdpoweroff) ) {
        // ignor the responds, if any.
        mcu_recv( responds, 20 );
    }
}

// return time_t: success
//             0: failed
time_t mcu_r_rtc()
{
    struct tm t ;
    static char cmd_readrtc[8] = "\x06\x01\x00\x06\x02\xf1" ;
    char responds[20] ;
    if( mcu_send( cmd_readrtc ) ) {
        if( mcu_recv( responds, 20 ) ) {
            if( responds[3]==6 &&
               responds[4]==3 ) {
                   t.tm_year = responds[11]/16*10 + responds[11]%16 + 100;
                   t.tm_mon  = responds[10]/16*10 + responds[10]%16 - 1;
                   t.tm_mday = responds[ 9]/16*10 + responds[ 9]%16 ;
                   t.tm_hour = responds[ 7]/16*10 + responds[ 7]%16 ;
                   t.tm_min  = responds[ 6]/16*10 + responds[ 6]%16 ;
                   t.tm_sec  = responds[ 5]/16*10 + responds[ 5]%16 ;
                   t.tm_isdst = -1 ;
                   return timegm( & t ) ;
               }
        }
    }
    return (time_t)0;
}

void mcu_readrtc()
{
    static char cmd_readrtc[8] = "\x06\x01\x00\x06\x02\xf1" ;
    char responds[20] ;
    if( mcu_send( cmd_readrtc ) ) {
        if( mcu_recv( responds, 20 ) ) {
            if( responds[3]==6 &&
               responds[4]==3 ) {
                   p_dio_mmap->rtc_year  = 2000+responds[11]/16*10 + responds[11]%16 ;
                   p_dio_mmap->rtc_month =      responds[10]/16*10 + responds[10]%16 ;
                   p_dio_mmap->rtc_day   =      responds[ 9]/16*10 + responds[ 9]%16 ;
                   p_dio_mmap->rtc_hour  =      responds[ 7]/16*10 + responds[ 7]%16 ;
                   p_dio_mmap->rtc_minute=      responds[ 6]/16*10 + responds[ 6]%16 ;
                   p_dio_mmap->rtc_second=      responds[ 5]/16*10 + responds[ 5]%16 ;
                   p_dio_mmap->rtc_millisecond = 0 ;
                   p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
                   return ;
               }
        }
    }
    p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.
}

// 3 LEDs On Panel
//   parameters:
//      led:  0= USB flash LED, 1= Error LED, 2 = Video Lost LED
//      flash: 0=off, 1=flash
void mcu_led(int led, int flash)
{
    static char cmd_ledctrl[10] = "\x08\x01\x00\x2f\x02\x00\x00\xcc" ;
    cmd_ledctrl[5]=(char)led ;
    cmd_ledctrl[6]=(char)(flash!=0) ;
    if( mcu_send( cmd_ledctrl ) ) {
        mcu_recv_dontcare();			// don't care
    }
}

// Device Power
//   parameters:
//      device:  0= GPS, 1= Slave Eagle boards, 2=Network switch
//      poweron: 0=poweroff, 1=poweron
void mcu_devicepower(int device, int poweron )
{
    static char cmd_devicepower[10] = "\x08\x01\x00\x2e\x02\x00\x00\xcc" ;
    cmd_devicepower[5]=(char)device ;
    cmd_devicepower[6]=(char)(poweron!=0) ;
    if( mcu_send( cmd_devicepower ) ) {
        mcu_recv_dontcare();			// don't care
    }
}

void mcu_reset()
{
    static char cmd_reset[8]="\x06\x01\x00\x00\x02\xcc" ;
    if( mcu_send( cmd_reset ) ) {
        mcu_recv_dontcare();			// don't care
    }
}	

// update mcu firmware
// return 1: success, 0: failed
int mcu_update_firmware( char * firmwarebuf, int size ) 
{
    int res = 0 ;
    int ts, s ;
    char responds[20] ;
    static char cmd_updatefirmware[8] = "\x06\x01\x00\x01\x02\xcc" ;
    
    // reset mcu
    mcu_reset();
    
    // send update command	
    if( mcu_send( cmd_updatefirmware ) ) {
        mcu_recv_dontcare();			// don't care
    }
    
    // send firmware 
    ts=0 ;
    while( ts<size ) {
        s = serial_write ( &firmwarebuf[ts], size-ts ) ;
        if( s<0 ) {
            break;		// error
        }
        ts+=s ;
    }
    
    // wait a bit
    if( serial_dataready(5000000) ) {
        if( mcu_recv( responds, 20 ) ) {
            if( responds[3]==0x03 && responds[4]==0x02 ) {
                res=1 ;
            }
        }
    }
    return res ;
}

static char bcd(int v)
{
    return (char)(((v/10)%10)*0x10 + v%10) ;
}

// return 1: success
//        0: failed
int mcu_w_rtc(time_t tt)
{
    char cmd_setrtc[16] ;
    char responds[20] ;
    struct tm t ;
    gmtime_r( &tt, &t);
    cmd_setrtc[0] = 0x0d ;
    cmd_setrtc[1] = 1 ;
    cmd_setrtc[2] = 0 ;
    cmd_setrtc[3] = 7 ;
    cmd_setrtc[4] = 2 ;
    cmd_setrtc[5] = bcd( t.tm_sec ) ;
    cmd_setrtc[6] = bcd( t.tm_min ) ;
    cmd_setrtc[7] = bcd( t.tm_hour );
    cmd_setrtc[8] = 0 ;						// day of week ?
    cmd_setrtc[9] = bcd(  t.tm_mday );
    cmd_setrtc[10] = bcd( t.tm_mon + 1 );
    cmd_setrtc[11] = bcd( t.tm_year + 1900 - 2000 ) ;
    
    if( mcu_send(cmd_setrtc) ) {
        if( mcu_recv(responds, 20 ) ) {
            if( responds[3]==7 &&
               responds[4]==3 ) {
                   return 1;
               }
        }
    }
    return 0 ;
}

void mcu_setrtc()
{
    char cmd_setrtc[16] ;
    char responds[20] ;
    cmd_setrtc[0] = 0x0d ;
    cmd_setrtc[1] = 1 ;
    cmd_setrtc[2] = 0 ;
    cmd_setrtc[3] = 7 ;
    cmd_setrtc[4] = 2 ;
    cmd_setrtc[5] = bcd( p_dio_mmap->rtc_second ) ;
    cmd_setrtc[6] = bcd( p_dio_mmap->rtc_minute );
    cmd_setrtc[7] = bcd( p_dio_mmap->rtc_hour );
    cmd_setrtc[8] = 0 ;						// day of week ?
    cmd_setrtc[9] = bcd( p_dio_mmap->rtc_day );
    cmd_setrtc[10] = bcd( p_dio_mmap->rtc_month) ;
    cmd_setrtc[11] = bcd( p_dio_mmap->rtc_year) ;
    
    if( mcu_send(cmd_setrtc) ) {
        if( mcu_recv(responds, 20 ) ) {
            if( responds[3]==7 &&
               responds[4]==3 ) {
                   p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
                   return ;
               }
        }
    }
    p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.
}

// sync system time to rtc
void mcu_syncrtc()
{
    time_t t ;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    t = (time_t) current_time.tv_sec ;
    if( mcu_w_rtc(t) ) {
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
    }
    else {
        p_dio_mmap->rtc_cmd   = -1 ;	// cmd error
    }
}

void mcu_rtctosys()
{
    struct timeval tv ;
    time_t rtc ;
    rtc=mcu_r_rtc();
    if( rtc!=(time_t)0 ) {
        tv.tv_sec=rtc ;
        tv.tv_usec=0;
        settimeofday(&tv, NULL);
    }
}

void mcu_adjtime()
{
    struct timeval tv ;
    time_t rtc ;
    rtc=mcu_r_rtc();
    if( rtc!=(time_t)0 ) {
        gettimeofday(&tv, NULL);
        if( rtc!=tv.tv_sec ) {
            tv.tv_sec =(time_t)((int)rtc - (int)tv.tv_sec ) ;
            tv.tv_usec = 0 ;
            adjtime( &tv, NULL );
        }
    }
}

void time_syncgps ()
{
    struct timeval tv ;
    int diff ;
    time_t gpstime ;
    dio_lock();
    if( p_dio_mmap->gps_valid ) {
        gettimeofday(&tv, NULL);
        gpstime = (time_t) p_dio_mmap->gps_gpstime ;
        if( gpstime!=tv.tv_sec ) {
            diff = (int)gpstime - (int)tv.tv_sec ;
            if( diff>2 || diff<-2 ) {
                tv.tv_sec=gpstime ;
                tv.tv_usec= (int)((p_dio_mmap->gps_gpstime - gpstime)*1000.0);
                settimeofday( &tv, NULL );
                mcu_w_rtc(gpstime) ;
            }
            else {
                tv.tv_sec =(time_t)diff ;
                tv.tv_usec = 0 ;
                adjtime( &tv, NULL );
            }
        }
    }
    dio_unlock();
}

void time_syncmcu()
{
    int diff ;
    struct timeval tv ;
    time_t rtc ;
    rtc=mcu_r_rtc();
    if( rtc!=(time_t)0 ) {
        gettimeofday(&tv, NULL);
        if( rtc!=tv.tv_sec ) {
            diff = (int)rtc - (int)tv.tv_sec ;
            if( diff>2 || diff<-2 ) {
                tv.tv_sec=rtc ;
                tv.tv_usec=0;
                settimeofday( &tv, NULL );
            }
            else {
                tv.tv_sec =(time_t)diff ;
                tv.tv_usec = 0 ;
                adjtime( &tv, NULL );
            }
        }
    }
}

void dvrsvr_down()
{
    pid_t dvrpid = p_dio_mmap->dvrpid ;
    if( dvrpid > 0 ) {
        kill( dvrpid, SIGUSR1 ) ;				// suspend DVRSVR
        while( p_dio_mmap->dvrpid ) {
            p_dio_mmap->outputmap ^= HDLED ;
            mcu_input(200000);
            mcu_doutput();
        }
        sync(); 
    }
}

void dvrsvr_up()
{
}

int g_syncrtc=0 ;

// return 
//        0 : failed
//        1 : success
int appinit()
{
    FILE * pidf ;
    int fd ;
    int i;
    int port=0;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    v = dvrconfig.getvalue( "system", "iomapfile");
    char * iomapfile = v.getstring();
    if( iomapfile && strlen(iomapfile)>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    fd = open(dvriomap, O_RDWR | O_CREAT, S_IRWXU);
    if( fd<=0 ) {
        printf("Can't create io map file!\n");
        return 0 ;
    }
    ftruncate(fd, sizeof(struct dio_mmap));
    p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );								// fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    
    if( p_dio_mmap->usage <=0 ) {
        memset( p_dio_mmap, 0, sizeof( struct dio_mmap ) );
    }
    p_dio_mmap->usage++;                           // add one usage
    
    // initialize shared memory
    p_dio_mmap->inputnum = MCU_INPUTNUM ;	
    p_dio_mmap->outputnum = MCU_OUTPUTNUM ;	
    
    p_dio_mmap->panel_led = 0;
    p_dio_mmap->devicepower = 0xffff;				// assume all device is power on
    
    p_dio_mmap->iopid = getpid () ;					// io process pid
    
    // check if sync rtc wanted
    
    g_syncrtc = dvrconfig.getvalueint("io", "syncrtc");
    
    // initilize serial port
    v = dvrconfig.getvalue( "io", "serialport");
    if( v.length()>0 ) {
        p = v.getstring() ;
    }
    else {
        p = serial_dev ;
    }
    if( strcmp( p, "/dev/ttyS1")==0 ) {
        port=1 ;
    }
    serial_handle = open( p, O_RDWR | O_NOCTTY | O_NDELAY );
    if( serial_handle > 0 ) {
        int baud ;
        baud = dvrconfig.getvalueint("io", "serialbaudrate");
        if( baud<2400 || baud>115200 ) {
            baud=DEFSERIALBAUD ;
        }
        fcntl(serial_handle, F_SETFL, 0);
        if( port==1 ) {		// port 1
            // Use Hikvision API to setup serial port (RS485)
            InitRS485(serial_handle, baud, DATAB8, STOPB1, NOPARITY, NOCTRL);
        }
        else {
            struct termios tios ;
            speed_t baud_t ;
            tcgetattr(serial_handle, &tios);
            // set serial port attributes
            tios.c_cflag = CS8|CLOCAL|CREAD;
            tios.c_iflag = IGNPAR;
            tios.c_oflag = 0;
            tios.c_lflag = 0;       // ICANON;
            tios.c_cc[VMIN]=0;		// minimum char 0
            tios.c_cc[VTIME]=2;		// 0.2 sec time out
            baud_t = B115200 ;
            for( i=0; i<7; i++ ) {
                if( baud == baud_table[i].baudrate ) {
                    baud_t = baud_table[i].baudv ;
                    break;
                }
            }
            
            cfsetispeed(&tios, baud_t);
            cfsetospeed(&tios, baud_t);
            
            tcflush(serial_handle, TCIFLUSH);
            tcsetattr(serial_handle, TCSANOW, &tios);
        }
    }
    else {
        // even no serail port, we still let process run
        printf("Serial port failed!\n");
    }
    
    pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }
    
    return 1;
}

// app finish, clean up
void appfinish()
{
    int lastuser ;
    
    // close serial port
    if( serial_handle>0 ) {
        close( serial_handle );
        serial_handle=0;
    }
    
    p_dio_mmap->iopid = 0 ;
    p_dio_mmap->usage-- ;
    
    if( p_dio_mmap->usage <= 0 ) {
        lastuser=1 ;
    }
    else {
        lastuser=0;
    }
    
    // clean up shared memory
    munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    
    if( lastuser ) {
        unlink( dvriomap );         // remove map file.
    }
    
    // delete pid file
    unlink( pidfile );
}

int main()
{
    int i;
    unsigned int serialno=0;
    int oldhdlock = 0 ;							// HD lock status, default 0 : open
    int hdlock ;								// HD lock status
    int noinputwatchdog = 0 ;                   // noinput watchdog
    
    if( appinit()==0 ) {
        return 1;
    }
    
    watchdogenabled = 0 ;
    //	watchdogenabled = 1 ;
    
    // initialize mcu (power processor)
    if( mcu_bootupready () ) {
        printf("MCU UP!\n");
    }
    else {
        printf("MCU failed!\n");
    }
    
    // initialize output
    mcu_doutputmap=~(p_dio_mmap->outputmap) ;
    
    //
    // initialize power
    
    // setup signal handler	
    signal(SIGQUIT, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);
    
    printf( "DVR IO process started!\n");
    
    if( g_syncrtc ) {
        mcu_rtctosys();
    }
    
    // get default digital input
    mcu_dinput();
    
    
    if( watchdogenabled ) {
        mcu_watchdogenable();
    }
    
    app_mode = 1 ;
    while( app_mode ) {
        // do input pin polling
        //
        if( mcu_input(50000) == 0 ) {				// with 0.05 second delay.
            // no inputs
            noinputwatchdog++ ;
            if( noinputwatchdog>100000 ) {           // ~~ 1.3 hour
                mcu_bootupready () ;               // restart mcu ???
                noinputwatchdog=0 ;
            }
        }
        else {
            noinputwatchdog=0 ;
        }
        
        // do ignition pin polling
        //  ... did in mcu_input()
        
         /* Sorry , I have no control over here &-<
        if( p_dio_mmap->poweroff && p_dio_mmap->lockpower ) {
            
             
             // toggling lock power pin. 
             if( serialno%149==0 ) {			// every 3 seconds
                 printf("Powerdelay timer = %d ,", mcu_poweroffdelaytimer );
                 if( mcu_poweroffdelaytimer<25 ) {
                     mcu_poweroffdelay(15);
                     printf("call powerdelay(15), timer = %d\n", mcu_poweroffdelaytimer );
        }
        else {
            mcu_poweroffdelay(1);
            printf("call powerdelay(1), timer = %d\n", mcu_poweroffdelaytimer );
        }
        
        }
            
        }
        */
        
        // rtc command check
        if( p_dio_mmap->rtc_cmd != 0 ) {
            if( p_dio_mmap->rtc_cmd == 1 ) {
                mcu_readrtc(); 
            }
            else if( p_dio_mmap->rtc_cmd == 2 ) {
                mcu_setrtc();
            }
            else if( p_dio_mmap->rtc_cmd == 3 ) {
                mcu_syncrtc();
            }
            else {
                p_dio_mmap->rtc_cmd=-1;		// command error, (unknown cmd)
            }
        }
        
        // do digital output
        if( p_dio_mmap->outputmap!=mcu_doutputmap ) {	// output map changed.
            // do real output pin here
            mcu_doutput();
        }
        
        if( serialno%21==0 ) {		// every one seconds
            
            // do power management mode switching
            if( app_mode==1 ) {                         // running mode
                p_dio_mmap->devicepower = 0xffff ;
                if( p_dio_mmap->poweroff && p_dio_mmap->lockpower==0 ) {            // ignition off detected
                    shutdowndelay_start ;
                    app_mode=2 ;
                }
                else {
                }
            }
            else if( app_mode==2 ) {                    // shutdown delay
                if( p_dio_mmap->poweroff==0 ) {
                    app_mode=1 ;                        // back to normal
                }
                else {
                    mcu_poweroffdelay ();
                    if( delay time out ? ) {
                        // suspend dvrsvr_down 
                        p_dio_mmap->devicepower=0 ;     // turn off all devices poweroff
                        app_mode=3 ;        
                    }
                }
            }
            else if( app_mode==3 ) {                    // standby
                // resume dvrsvr 
                ...
                    if( p_dio_mmap->poweroff==0 ) {
                        p_dio_mmap->devicepower=0xffff ;    // turn all devices power
                        app_mode=1 ;                        // back to normal
                    }
                else {
                    mcu_poweroffdelay ();
                    if( standby time out ? ) {
                        app_mode=4 ;        
                    }
                }
            }
            else if( app_mode==4 ) {                    // reboot
                
            }
            else {              // other? must be error
                app_mode=1 ;
            }
            
            
            if( ledflashing ) {
                p_dio_mmap->outputmap ^= 0xff ;
            }
            
            // kick watchdog
            if( p_dio_mmap->dvrpid == 0 ) {
                mcu_watchdogkick();
            }
            else {				// DVRSVR running?
                if( (++(p_dio_mmap->dvrwatchdog)) < 30 ) {	// 10 seconds dvr watchdog timer
                    mcu_watchdogkick();
                }
                else {			// halt system ? or reboot ?
                    sync();
                    //					reboot(RB_AUTOBOOT);
                    printf("IOPROCESS: DVR server halt!\n");
                    p_dio_mmap->dvrwatchdog=0 ;
                    //					break;
                }
            }
            
            // check HD plug-in state
            hdlock = (p_dio_mmap->inputmap & (HDINSERTED|HDKEYLOCK) )==0 ;	// HD plugged in and locked
            p_dio_mmap->hdkey=hdlock ;
            if( hdlock != oldhdlock ) {
                oldhdlock = hdlock ;
                if( hdlock ) {
                    // turn on HD power
                    mcu_hdpoweron() ;
                    // turn on HD led
                    p_dio_mmap->outputmap |= HDLED ;
                }
                else {
                    // suspend DVRSVR process
                    pid_t dvrpid = p_dio_mmap->dvrpid ;
                    if( dvrpid > 0 ) {
                        kill(dvrpid, SIGUSR1);         // request dvrsvr to turn down
                        for( i=0; i<80; i++ ) {
                            if( p_dio_mmap->dvrpid <= 0 ) 
                                break;
                            mcu_input(200000);
                            p_dio_mmap->outputmap ^= HDLED ;
                            mcu_doutput();
                        }
                        if( i>78 ) {
                            printf("dvrsvr may dead!\n");
                        }
                    }
                    // umount disks
                    system("/davinci/dvr/umountdisks") ;
                    // flash HD led for few seconds
                    for( i=0; i<6; i++ ) {
                        mcu_input(200000);
                        p_dio_mmap->outputmap ^= HDLED ;
                        mcu_doutput();
                    }
                    // turn off HD power
                    mcu_input(20000);
                    mcu_hdpoweroff();
                    // flash HD led for few seconds
                    for( i=0; i<6; i++ ) {
                        p_dio_mmap->outputmap ^= HDLED ;
                        mcu_input(200000);
                        mcu_doutput();
                    }
                    if( dvrpid > 0 ) {
                        kill(dvrpid, SIGUSR2);              // resume dvrsvr
                        // flash HD led for few more seconds, wait dvrsvr up
                        for( i=0; i<30; i++ ) {
                            p_dio_mmap->outputmap ^= HDLED ;
                            mcu_input(200000);
                            mcu_doutput();
                            if( p_dio_mmap->dvrpid > 0 ) {
                                break;
                            }
                        }
                    }
                    // turn off HD LED
                    p_dio_mmap->outputmap &= ~ HDLED ;
                    
                    mcu_input(20000);
                }
            }
        }
        
        // update panel LEDs
        if( panelled != p_dio_mmap->panel_led ) {
            for( i=0; i<16; i++ ) {
                if( (panelled ^ p_dio_mmap->panel_led) & (1<<i) ) {
                    mcu_led(i, p_dio_mmap->panel_led & (1<<i) );
                }
            }
            panelled = p_dio_mmap->panel_led ;
        }
        
        // update device power
        if( devicepower != p_dio_mmap->devicepower ) {
            for( i=0; i<16; i++ ) {
                if( (devicepower ^ p_dio_mmap->devicepower) & (1<<i) ) {
                    mcu_devicepower(i, p_dio_mmap->devicepower & (1<<i) );
                }
            }
            devicepower = p_dio_mmap->devicepower ;
        }
        
        if( gpsvalid != p_dio_mmap->gps_valid ) {
            gpsvalid = p_dio_mmap->gps_valid ;
            if( gpsvalid ) {
                time_syncgps () ;
            }
        }
        
        // adjust system time with RTC
        if( g_syncrtc && serialno%100000==0 ) {							// call adjust time about every one hour
            if( gpsvalid ) {
                time_syncgps();
            }
            else {
                time_syncmcu();
            }
        }
        
        serialno++;
    }
    
    if( watchdogenabled ) {
        mcu_watchdogdisable();
    }
    
    appfinish();
    
    printf( "DVR IO process ended!\n");
    return 0;
}

