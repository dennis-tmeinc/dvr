/*
 file mcu.cpp,
    communications to mcu 
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

#define MCU_BUFFER_SIZE (200)
static char mcu_buffer[MCU_BUFFER_SIZE] ;
static int  mcu_buffer_len ;
static int  mcu_buffer_pointer ;

int mcu_handle = 0 ;
int mcu_baud = 115200 ;
char mcu_dev[100] = "/dev/ttyS1" ;
int mcupowerdelaytime = 0 ;
unsigned int mcu_doutputmap ;
int mcu_inputmissed ;
int output_inverted=0 ;
int mcu_program_delay ;

// translate output bits
#define MAXOUTPUT    (8)
unsigned int output_map_table [MAXOUTPUT] = {
    1,              // bit 0
    (1<<1),         // bit 1
    (1<<8),         // bit 2   (buzzer)
    (1<<3),         // bit 3
    (1<<4),         // bit 4
    (1<<2),         // bit 5
    (1<<6),         // bit 6
    (1<<7)          // bit 7
} ;

static struct baud_table_t {
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

// open serial port
int serial_open(char * device, int buadrate) 
{
    int hserial ;
    int i;
   
    // check if serial device match stdin ?
    struct stat stdinstat ;
    struct stat devstat ;
    int r1, r2 ;
    r1 = fstat( 0, &stdinstat ) ;           // get stdin stat
    r2 = stat( device, &devstat ) ;
    
    if( r1==0 && r2==0 && stdinstat.st_dev == devstat.st_dev && stdinstat.st_ino == devstat.st_ino ) { // matched stdin
        netdbg_print("Stdin match serail port!\n");
        hserial = dup(0);                   // duplicate stdin
 		fcntl(hserial, F_SETFL, O_RDWR | O_NOCTTY );
    }
    else {
       hserial = open( device, O_RDWR | O_NOCTTY );
    }
   
    if( hserial > 0 ) {
#ifdef EAGLE32        
        if( strcmp( device, "/dev/ttyS1")==0 ) {    // this is hikvision specific serial port
            // Use Hikvision API to setup serial port (RS485)
            InitRS485(hserial, buadrate, DATAB8, STOPB1, NOPARITY, NOCTRL);
        }
        else 
#endif            
        {
            struct termios tios ;
            speed_t baud_t ;
            tcgetattr(hserial, &tios);
            // set serial port attributes
            tios.c_cflag = CS8|CLOCAL|CREAD;
            tios.c_iflag = IGNPAR;
            tios.c_oflag = 0;
            tios.c_lflag = 0;       // ICANON;
            tios.c_cc[VMIN]=10;		// minimum char 
            tios.c_cc[VTIME]=1;		// 0.1 sec time out
            baud_t = B115200 ;
            for( i=0; i<7; i++ ) {
                if( buadrate == baud_table[i].baudrate ) {
                    baud_t = baud_table[i].baudv ;
                    break;
                }
            }
            
            cfsetispeed(&tios, baud_t);
            cfsetospeed(&tios, baud_t);
            
            tcflush(hserial, TCIFLUSH);
            tcsetattr(hserial, TCSANOW, &tios);
        }
        return hserial ;
    }
    return 0 ;
}

//  
int serial_dataready(int handle, int usdelay, int * usremain)
{
    struct timeval timeout ;
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(handle, &fds);
    timeout.tv_sec = usdelay/1000000 ;
    timeout.tv_usec = usdelay%1000000 ;

    if (select(handle + 1, &fds, NULL, NULL, &timeout) > 0) {
        if( FD_ISSET(handle, &fds) ) {
            if( usremain ) {
                *usremain = (int)(timeout.tv_sec*1000000 + timeout.tv_usec) ;
            }
            return 1 ;
        }
    }

    if( usremain ) {
        *usremain = 0 ;
    }
    return 0;
}

#define MAXDEVICES  (8)
int shandles[MAXDEVICES] ;

// check serial port ready on multi ports  
int serial_mready(int usdelay)
{
    int i;
    int maxh=0 ;
    struct timeval timeout ;
    fd_set fds;

    timeout.tv_sec = usdelay/1000000 ;
    timeout.tv_usec = usdelay%1000000 ;

    FD_ZERO(&fds);
    for( i=0; i<MAXDEVICES; i++ ) {
        if( shandles[i]>0 ) {
            FD_SET(shandles[i], &fds);
            if( shandles[i]>maxh ) {
                maxh=shandles[i] ;
            }
        }
    }
    
    if (select(maxh + 1, &fds, NULL, NULL, &timeout) > 0) {
        return 1 ;
    }
    return 0;
}

// Check if data ready to read on serial port
//     return 0: no data
//            1: yes
int mcu_dataready(int usdelay, int * usremain)
{
    if( mcu_handle<=0 ) {
        return 0;
    }
    if( mcu_buffer_len > mcu_buffer_pointer ) {       // buffer available?
        if( usremain ) {
            *usremain = usdelay ;
        }
        return 1 ;
    }
    if( serial_dataready( mcu_handle, usdelay, usremain ) ) {
        mcu_buffer_pointer=0 ;
        mcu_buffer_len = read(mcu_handle, mcu_buffer, MCU_BUFFER_SIZE);
        if( mcu_buffer_len > 0 ) {       // buffer available?
            return 1 ;
        }
    }
    if( usremain ) {
        *usremain = 0 ;
    }
    mcu_buffer_len = 0 ;
    return 0;
}

int mcu_read(char * sbuf, size_t bufsize, int wait, int interval)
{
    size_t nread=0 ;
    if( mcu_dataready(wait) ) {
        while( nread<bufsize && mcu_dataready(interval) ) {
#ifdef  MCU_DEBUG
            printf( "%02x ", (int) mcu_buffer[mcu_buffer_pointer] );
#endif
            *sbuf++ = mcu_buffer[mcu_buffer_pointer++] ;
            nread++;
        }
    }
    return nread ;
}

int mcu_write(void * buf, int bufsize)
{
    if( mcu_handle>0 ) {
#ifdef MCU_DEBUG
        int i;
        for(i=0;i<bufsize;i++) {
            printf( "%02x ", (int)(((char *)buf)[i]) );
        }
#endif
        return write( mcu_handle, buf, bufsize);
    }
    return 0;
}

// clear receiving buffer
void mcu_clear(int delay)
{
    int i;
#ifdef MCU_DEBUG
    printf("clear: ");
#endif  
    char buf[100] ;
    for(i=0;i<100;i++) {
        if( mcu_read( buf, sizeof(buf), 0, 0 )==0 ) {
            break ;
        }
    }
#ifdef MCU_DEBUG
    printf("\n");
#endif                
}

unsigned char checksum( unsigned char * data, int datalen ) 
{
    unsigned char ck=0;
    while( datalen-->0 ) {
        ck+=*data++;
    }
    return ck ;
}

// check data check sum
// return 0: checksum correct
char mcu_checksum( char * data ) 
{
    if( *data<6 || *data>60 ) {
        return (char)-1;
    }
    return (char)checksum((unsigned char *)data, *data);
}

// calculate check sum of data
void mcu_calchecksum( char * data )
{
    char ck = 0 ;
    char i = *data ;
    
    while( --i>0 ) {
        ck-=*data++ ;
    }
    *data = ck ;
}

// send command to mcu
// return 0 : failed
//        >0 : success
int mcu_send( char * cmd ) 
{
    if( *cmd<5 || *cmd>40 ) { 	// wrong data
        return 0 ;
    }
    mcu_calchecksum(cmd );

#ifdef  MCU_DEBUG
// for debugging    
    struct timeval tv ;
    gettimeofday(&tv, NULL);
    int r ;
    struct tm * ptm ;
    ptm = localtime(&tv.tv_sec);
    double sec ;
    sec = ptm->tm_sec + tv.tv_usec/1000000.0 ;
    
    printf("%02d:%02d:%05.3f Send: ", ptm->tm_hour, ptm->tm_min, sec);
    r=mcu_write(cmd, (int)(*cmd));
    printf("\n");
    return r ;
#else    
    return mcu_write(cmd, (int)(*cmd));
#endif
}

// receive one data package from mcu
// return : received msg
//          NULL : failed
char * mcu_recvmsg()
{
    int n;
    static char msgbuf[100] ;

#ifdef MCU_DEBUG
    // for debugging 
    struct timeval tv ;
    gettimeofday(&tv, NULL);
    struct tm * ptm ;
    ptm = localtime(&tv.tv_sec);
    double sec ;
    sec = ptm->tm_sec + tv.tv_usec/1000000.0 ;
    
    printf("%02d:%02d:%05.3f Recv: ", ptm->tm_hour, ptm->tm_min, sec);
#endif    
    
    if( mcu_read(msgbuf, 1) ) {
        n=(int) msgbuf[0] ;
        if( n>=5 && n<(int)sizeof(msgbuf) ) {
            n=mcu_read(&msgbuf[1], n-1)+1 ; 
            if( n==(int)msgbuf[0] &&                   // correct size
                msgbuf[1]==0 &&                 // correct target
                mcu_checksum( msgbuf ) == 0 )   // correct checksum
            {
#ifdef MCU_DEBUG
                printf("\n");
#endif
                return msgbuf ;
            }
            if( strncmp(msgbuf, "Enter", 5 )==0 ) {
                // MCU reboots, why?
                dvr_log("MCU power down!!!");
                sync();sync();sync();
            }
        }
    }

#ifdef MCU_DEBUG
    // for debugging 
    printf("** error! \n");
#endif
    mcu_inputmissed = 1 ;
    mcu_clear(100000);
    return NULL ;
}

// to receive a respondes message from mcu
char * mcu_recv( int usdelay = MIN_SERIAL_DELAY, int * usremain=NULL )
{
    char * mcu_msg ;
    while( mcu_dataready(usdelay, &usdelay) ) {
        mcu_msg = mcu_recvmsg();
        if( mcu_msg ) {
            if( mcu_msg[4]=='\x02' ) {             // is it an input message ?
                mcu_checkinputbuf(mcu_msg) ;
            }
            else {
#ifdef MCU_DEBUG
                if( mcu_msg[3]=='\x1f' ) {         // error report, not action for this now
                    printf("A possible error report detected!\n" );
                }
#endif
                if( usremain ) {
                    * usremain=usdelay ;
                }
                return mcu_msg ;                  // it could be a responding message
            }
        }
        else {
            break;
        }
    }
    if( usremain ) {
        * usremain=usdelay ;
    }
    return NULL ;
}


// send command to mcu without waiting for responds
int mcu_sendcmd(int cmd, int datalen, ...)
{
    int i ;
    va_list v ;
    char mcu_buf[datalen+10] ;

    mcu_buf[0] = (char)(datalen+6) ;
    mcu_buf[1] = (char)1 ;
    mcu_buf[2] = (char)0 ;
    mcu_buf[3] = (char)cmd ;
    mcu_buf[4] = (char)2 ;

    va_start( v, datalen ) ;
    for( i=0 ; i<datalen ; i++ ) {
        mcu_buf[5+i] = (char) va_arg( v, int );
    }
    va_end(v);
    return mcu_send(mcu_buf) ;
}


// wait for response from mcu
// return valid rsp buffer
//        NULL if failed or timeout
char * mcu_waitrsp(int target, int cmd, int delay=MCU_CMD_DELAY)
{
    char * mcu_rsp ;
    while( delay>0 ) {                          // wait until delay time out
        mcu_rsp = mcu_recv( delay, &delay ) ;   
        if( mcu_rsp ) {     // received a responds
            if( mcu_rsp[2]==(char)target &&         // resp from correct target (MCU)
                mcu_rsp[3]==(char)cmd &&            // correct resp for CMD
                mcu_rsp[4]==3 )                     // right responds
            {
                return mcu_rsp ;           
            }                                       // ignor wrong resps and continue waiting
        }
        else {
            break;
        }
    }
    return NULL ;
}


// check mcu input
// parameter
//      wait - micro-seconds waiting
// return 
//		number of input message received.
//      0: timeout (no message)
//     -1: error
int mcu_input(int usdelay)
{
    int res = 0 ;
    char * mcu_msg ;
    while( mcu_dataready(usdelay, &usdelay) ) {
        mcu_msg = mcu_recvmsg();
        if( mcu_msg ) {
            if( mcu_msg[4]=='\x02' ) {             // is it an input message ?
                mcu_checkinputbuf(mcu_msg) ;
                res++;
            }
        }
        else {
            return -1 ;
        }
    }
    return res ;
}

// Send command to mcu and check responds
// return 
//       valid response string
//       NULL if failed
char * mcu_cmd(int cmd, int datalen, ...)
{
    int i ;
    va_list v ;
    char * mcu_rsp ;
    char mcu_buf[datalen+10] ;
    static int mcu_error ;

    mcu_buf[0] = (char)(datalen+6) ;
    mcu_buf[1] = (char)1 ;                      // target MCU
    mcu_buf[2] = (char)0 ;
    mcu_buf[3] = (char)cmd ;
    mcu_buf[4] = (char)2 ;

    va_start( v, datalen ) ;
    for( i=0 ; i<datalen ; i++ ) {
        mcu_buf[5+i] = (char) va_arg( v, int );
    }
    va_end(v);
    i=3 ;
    while( i-->0 ) {
        if(mcu_send(mcu_buf)) {
            mcu_rsp = mcu_waitrsp(1, cmd) ;         // wait rsp from MCU
            if( mcu_rsp ) {
                mcu_error = 0 ;
                return mcu_rsp ;
            }
        }

#ifdef MCU_DEBUG
        printf("Retry!!!\n" );
#endif
        if( ++mcu_error>10 ) {
            sleep(1);
            mcu_restart();      // restart mcu port
            mcu_error=0 ;
        }
    }

    return NULL ;
}

// response to mcu 
void mcu_response(char * msg, int datalen, ... )
{
    int i ;
    va_list v ;
    char mcu_buf[datalen+10] ;
    mcu_buf[0] = datalen+6 ;
    mcu_buf[1] = msg[2] ;
    mcu_buf[2] = 0 ;                // cpu
    mcu_buf[3] = msg[3] ;           // response to same command
    mcu_buf[4] = 3 ;                // response
    va_start( v, datalen ) ;
    for( i=0 ; i<datalen ; i++ ) {
        mcu_buf[5+i] = (char) va_arg( v, int );
    }
    va_end(v);
    mcu_send( mcu_buf );
}

// misc mcu commands

// reboot mcu, (with force-power-on)
int mcu_reset()
{
    int r ;
    char enteracommand[200] ;
    mcu_sendcmd( MCU_CMD_RESET ) ;
    r = mcu_read(enteracommand, sizeof(enteracommand)-1, 30000000, 1000000);
    if( r>10 ) {
        enteracommand[r]=0 ;
        if( strcasestr( enteracommand, "command" ) ) {
            dvr_log("Resetting MCU!");
            return 1 ;
        }
    }
    return 0 ;
}

// return bcd value
static int getbcd(unsigned char c)
{
    return (int)(c/16)*10 + c%16 ;
}

int mcu_readcode()
{
    char * v = mcu_cmd(MCU_CMD_READCODE);
    if( v ) {
        if( v[5]!=(unsigned char)0xff ) {
            struct tm ttm ;
            time_t tt ;
            memset( &ttm, 0, sizeof(ttm) );
            ttm.tm_year = getbcd(v[12]) + 100 ;
            ttm.tm_mon  = getbcd(v[11]) - 1;
            ttm.tm_mday = getbcd(v[10]) ;
            ttm.tm_hour = getbcd(v[8]) ;
            ttm.tm_min  = getbcd(v[7]) ;
            ttm.tm_sec  = getbcd(v[6]) ;
            ttm.tm_isdst= -1 ;
            tt=timegm(&ttm);
            if( (int)tt>0 ) {
                localtime_r(&tt, &ttm);
                dvr_log( "MCU shutdown code: %02x at %04d-%02d-%02d %02d:%02d:%02d",
                        (int)(v[5]),
                        ttm.tm_year+1900,
                        ttm.tm_mon+1,
                        ttm.tm_mday,
                        ttm.tm_hour,
                        ttm.tm_min,
                        ttm.tm_sec );
            }
        }
        return 1 ;
    }
    dvr_log( "Read MCU shutdown code failed.");
    return 0 ;
}

// return time_t: success
//             -1: failed
time_t mcu_r_rtc( struct tm * ptm )
{
    struct tm ttm ;
    time_t tt ;
    char * responds ;

    responds = mcu_cmd( MCU_CMD_READRTC ) ;
    if( responds ) {
        memset( &ttm, 0, sizeof(ttm) );
        ttm.tm_year = getbcd(responds[11]) + 100 ;
        ttm.tm_mon  = getbcd(responds[10]) - 1;
        ttm.tm_mday = getbcd(responds[9]) ;
        ttm.tm_hour = getbcd(responds[7]) ;
        ttm.tm_min  = getbcd(responds[6]) ;
        ttm.tm_sec  = getbcd(responds[5]) ;
        ttm.tm_isdst= -1 ;

        if( ttm.tm_year>105 && ttm.tm_year<135 &&
            ttm.tm_mon>=0 && ttm.tm_mon<=11 &&
            ttm.tm_mday>=1 && ttm.tm_mday<=31 &&
            ttm.tm_hour>=0 && ttm.tm_hour<=24 &&
            ttm.tm_min>=0 && ttm.tm_min<=60 &&
            ttm.tm_sec>=0 && ttm.tm_sec<=60 ) 
        {
            tt = timegm( &ttm );
            if( (int)tt > 0 ) {
                if(ptm) {
                    *ptm = ttm ;
                }
                return tt ;
            }
        }
    }
    dvr_log("Read clock from MCU failed!");
    return (time_t)-1;
}


void mcu_readrtc()
{
    time_t t ;
    struct tm ttm ;
    t = mcu_r_rtc(&ttm) ;
    if( (int)t > 0 ) {
        dio_lock();
        p_dio_mmap->rtc_year  = ttm.tm_year+1900 ;
        p_dio_mmap->rtc_month = ttm.tm_mon+1 ;
        p_dio_mmap->rtc_day   = ttm.tm_mday ;
        p_dio_mmap->rtc_hour  = ttm.tm_hour ;
        p_dio_mmap->rtc_minute= ttm.tm_min ;
        p_dio_mmap->rtc_second= ttm.tm_sec ;
        p_dio_mmap->rtc_millisecond = 0 ;
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
        dio_unlock();
        return ;
    }
    p_dio_mmap->rtc_cmd   = -1 ;	// cmd error.
}


// 3 LEDs On Panel
//   parameters:
//      led:  0= USB flash LED, 1= Error LED, 2 = Video Lost LED
//      flash: 0=off, 1=flash
void mcu_led(int led, int flash)
{
    mcu_cmd( MCU_CMD_PANELLED, 2, led, (flash!=0) );
}

// Device Power
//   parameters:
//      device:  0= GPS, 1= Slave Eagle boards, 2=Network switch
//      poweron: 0=poweroff, 1=poweron
void mcu_devicepower(int device, int poweron )
{
    mcu_cmd( MCU_CMD_DEVICEPOWER, 2, device, poweron );
}


int mcu_bootupready()
{
    static int mcuready = 0 ;
    int i ;
    char * rsp ;
    if( mcuready ) {
        return 1 ;
    }
    if( (rsp=mcu_cmd(MCU_CMD_BOOTUPREADY, 1, 0 ))!=NULL ) {
        int rlen = *rsp - 6 ;
        char status[200] ;
        char tst[10] ;
        status[0]=0 ;
        for( i=0; i<rlen; i++ ) {
            sprintf(tst, " %02x", (unsigned int)(unsigned char)rsp[5+i] );
            strcat( status, tst );
        }
        dvr_log( "MCU ready with status :%s", status );
        mcuready = 1 ;
        mcu_readcode();
        return 1 ;
    }
    sync();
    sync();
    sync();
    return 0 ;
}

// get MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_version(char * version)
{
    char * v ;
    v = mcu_cmd( MCU_CMD_GETVERSION ) ;
    if( v ) {
        memcpy( version, &v[5], *v-6 );
        version[*v-6]=0 ;
        return *v-6 ;
    }
    return 0 ;
}


int mcu_doutput()
{
    unsigned int outputmap = p_dio_mmap->outputmap ;
    unsigned int outputmapx ;
    int i ;
    unsigned int dout ;
    
    if( outputmap == mcu_doutputmap ) return 1 ;

    outputmapx = (outputmap^output_inverted) ;

    // bit 2 is beep. (from July 6, 2009)
    // translate output bits ;
    dout = 0 ;
    for(i=0; i<MAXOUTPUT; i++) {
        if( outputmapx & output_map_table[i] ) {
            dout|=(1<<i) ;
        }
    }
    if( mcu_cmd(MCU_CMD_DIGITALOUTPUT, 1, dout) ) {
        mcu_doutputmap=outputmap ;
    }
    return 1;
}

// get mcu digital input
int mcu_dinput()
{
    char * v ;
    v = mcu_cmd( MCU_CMD_DIGITALINPUT ) ;
    if( v ) {
        mcu_dinput_help(v);
        return 1 ;
    }
    return 0 ;
}

void mcu_initsensor2(int invert)
{
    char * rsp ;
    rsp = mcu_cmd(MCU_CMD_GETSENSOR2INV);
    if( rsp ) {
        if( invert != ((int)rsp[5]) ) {         // settings are from whats in MCU
            mcu_cmd(MCU_CMD_SETSENSOR2INV,1,invert);
        }
    }
}

// return 1: success
//        0: failed
int mcu_w_rtc(time_t tt)
{
    struct tm t ;
    gmtime_r( &tt, &t);
    if( mcu_cmd( MCU_CMD_WRITERTC, 7, 
                bcd( t.tm_sec ),
                bcd( t.tm_min ),
                bcd( t.tm_hour ),
                bcd( t.tm_wday ),
                bcd( t.tm_mday ),
                bcd( t.tm_mon + 1 ),
                bcd( t.tm_year ) ) ) 
    {
        return 1 ;
    }
    return 0 ;
}

// set more mcu power off delay (keep power alive), (called every few seconds)
void mcu_poweroffdelay(int delay)
{
    int inc ;
    char * responds ;
    if( mcupowerdelaytime < delay ) {
        inc = delay-mcupowerdelaytime ;
        responds = mcu_cmd( MCU_CMD_POWEROFFDELAY, 2, inc/256, inc%256 );
    }
    else {
        responds = mcu_cmd( MCU_CMD_POWEROFFDELAY, 2, 0, 0 );
    }
    if( responds ) {
        mcupowerdelaytime = ((unsigned)(responds[5]))*256+((unsigned)responds[6]) ;
        netdbg_print("extend mcu power for %ds remain %ds\n", delay, mcupowerdelaytime );
    }
}

void mcu_watchdogenable(int timeout)
{
    // set watchdog timeout
    mcu_cmd( MCU_CMD_SETWATCHDOGTIMEOUT, 2, timeout/256, timeout%256 ) ;
    // enable watchdog
    if( mcu_cmd( MCU_CMD_ENABLEWATCHDOG  ) ) {
        watchdogenabled = 1 ;
        netdbg_print("mcu watchdog enable, timeout %d s.\n", timeout );
    }
}

void mcu_watchdogdisable()
{
    if( watchdogenabled ) {
        if(mcu_cmd( MCU_CMD_DISABLEWATCHDOG  )) {
            watchdogenabled=0 ;
            netdbg_print("mcu watchdog disabled.\n" );
        }
    }
}

// return 0: error, >0 success
int mcu_watchdogkick()
{
    if( usewatchdog ) {
        if( watchdogenabled==0 ) {
            mcu_watchdogenable(watchdogtimeout);
        }

        if( mcu_cmd(MCU_CMD_KICKWATCHDOG) ) {
            netdbg_print("mcu watchdog kick.\n" );
        }
    }
    return 0;
}

// get io board temperature
int mcu_iotemperature()
{
    char * v = mcu_cmd(MCU_CMD_IOTEMPERATURE) ;
    if( v ) {
        return (int)(signed char)v[5] ;
    }
    return -128;
}

// get hd temperature
int mcu_hdtemperature()
{
#ifdef  MDVR_APP
    char * v = mcu_cmd(MCU_CMD_HDTEMPERATURE) ;
    if( v ) {
        return (int)(signed char)v[5] ;
    }
#endif    
    return -128 ;
}

void mcu_hdpoweron()
{
    mcu_cmd(MCU_CMD_HDPOWERON);
}

void mcu_hdpoweroff()
{
    mcu_cmd(MCU_CMD_HDPOWEROFF);
}


// update mcu firmware
// return 1: success, 0: failed
int mcu_update_firmware( char * firmwarefile) 
{
    int res = 0 ;
    FILE * fwfile ;
    FILE * fwmsgfile=NULL ;
    FILE * fwprogfile=NULL ;
    char * fwmsg ;
    int c ;
    int rd ;
    char responds[200] ;
    static char cmd_updatefirmware[8] = "\x05\x01\x00\x01\x02\xcc" ;

    netdbg_print("Start update mcu firmware.\n");

    fwmsg=getenv("MCUMSG");
    if( fwmsg ) {
        fwmsgfile = fopen( fwmsg, "w" );
    }

    fwmsg=getenv("MCUPROG");
    if( fwmsg ) {
        fwprogfile = fopen( fwmsg, "w" );
        fprintf(fwprogfile, "0");
        rewind(fwprogfile);
        fflush(fwprogfile);
    }
    
    fwfile=fopen(firmwarefile, "rb");

    if( fwmsgfile ) {
        fprintf(fwmsgfile, "Verify mcu firmware file ......" );
        fflush( fwmsgfile );
    }
    
	// check firmware file is valid
	if( fwfile ) {
		// check valid file contents. 
		if( fgets( responds, 200, fwfile ))
		{
            // example of HEX file line ":02EEFE00382AB0"
			if( responds[0]==':' ) {
                unsigned int l=0 ;
                unsigned int sum=0;
                unsigned int s ;
                sscanf( &responds[1], "%2x", &l );
                res=1 ;
                l+=5 ;
                for( c=0; c<(int)l ; c++ ) {
                    if( sscanf( &responds[1+c*2], "%2x", &s )==1 ) {
                        sum+=s ;
                    }
                    else {
                        res=0;
                        break;
                    }
                }
                if( (sum&0xff)!=0 ) {
                    res=0 ;
                }
			}
		}
	}

	if( res==0 ) {
		// invalid file
		if( fwfile) fclose( fwfile );
		netdbg_print("Invalid mcu firmware file.\n");
        if( fwmsgfile ) {
            fprintf(fwmsgfile, "Failed!\r\n");
            fflush( fwmsgfile );
            fclose( fwmsgfile );
        }
        return 0 ;
	}
	res=0;

    if( fwmsgfile ) {
        fprintf(fwmsgfile, "OK.\r\n");
        fflush( fwmsgfile );
    }
    
	// rewind
	fseek( fwfile, 0, SEEK_SET );
	
    // 
    netdbg_print("Start update MCU firmware: %s\n", firmwarefile);
    
    // reset mcu
    netdbg_print("Reset MCU.\n");
    if( fwmsgfile ) {
        fprintf(fwmsgfile, "Resetting MCU......");
        fflush( fwmsgfile );
    }
    
    if( mcu_reset()==0 ) {              // reset failed.
        netdbg_print("Failed\n");
        if( fwmsgfile ) {
            fprintf(fwmsgfile, "Failed!\r\n");
            fflush( fwmsgfile );
            fclose( fwmsgfile );
        }        
        return 0;
    }
    netdbg_print("Done.\n");
    if( fwmsgfile ) {
        fprintf(fwmsgfile, "OK.\r\n");
        fflush( fwmsgfile );
    }
    
    // clean out serial buffer
    memset( responds, 0, sizeof(responds)) ;
    mcu_write( responds, sizeof(responds));
    mcu_clear(1000000);
    
    netdbg_print("Erasing.\n");
    if( fwmsgfile ) {
        fprintf(fwmsgfile, "Erasing MCU......");
        fflush( fwmsgfile );
    }
    
    rd=0 ;
    if( mcu_write( cmd_updatefirmware, 5 ) ) {
        rd=mcu_read( responds, sizeof(responds), 60000000, 500000 ) ;
    }
    
    if( rd>=5 && 
       responds[rd-5]==0x05 && 
       responds[rd-4]==0x0 && 
       responds[rd-3]==0x01 && 
       responds[rd-2]==0x01 && 
       responds[rd-1]==0x03 ) 
    {
        // ok ;
        netdbg_print("Done.\n");
        res=0 ;
    }
    else {
        netdbg_print("Failed.\n");
        if( fwmsgfile ) {
            fprintf(fwmsgfile, "Failed!\r\n");
            fflush( fwmsgfile );
            fclose( fwmsgfile );
        }        
        return 0;                    // can't start fw update
    }

    if( fwmsgfile ) {
        fprintf(fwmsgfile, "OK.\r\n");
        fflush( fwmsgfile );
    }

    netdbg_print("Programming.\n");
    if( fwmsgfile ) {
        fprintf(fwmsgfile, "Programming MCU......");
        fflush( fwmsgfile );
    }

    // send firmware 
    fseek( fwfile, 0, SEEK_END );
    int mcufilelen = ftell( fwfile );
    if( mcufilelen < 1000 ) {
        if( fwmsgfile ) {
            fprintf(fwmsgfile, "Error in file!\r\n");
            fflush( fwmsgfile );
            fclose( fwmsgfile );
        }        
        return 0;                    // can't start fw update
    }
    int proglen = 0 ;
    fseek( fwfile, 0, SEEK_SET );
    
    res = 0 ;
    while( (c=fgetc( fwfile ))!=EOF ) {
        if( mcu_write( &c, 1)!=1 ) 
            break;
        if( c=='\n' ) {
            usleep( mcu_program_delay ) ;
        }
        if( fwprogfile ) {
            proglen++ ;
            if( proglen%(mcufilelen/100)==9 ) {
                fprintf( fwprogfile, "%d", (proglen*100)/mcufilelen  );
                rewind( fwprogfile );
                fflush( fwprogfile );
            }
        }
        if( c=='\n' && mcu_dataready (0) ) {
            rd = mcu_read(responds, sizeof(responds), 200000, 200000 );
            if( rd>=5 &&
               responds[0]==0x05 && 
               responds[1]==0x0 && 
               responds[2]==0x01 && 
               responds[3]==0x03 && 
               responds[4]==0x02 ) {
                   res=1 ;
                    break;                  // program done.
            }
            else if( rd>=5 &&
               responds[rd-5]==0x05 && 
               responds[rd-4]==0x0 && 
               responds[rd-3]==0x01 && 
               responds[rd-2]==0x03 && 
               responds[rd-1]==0x02 ) {
                   res=1 ;
                    break;                  // program done.
            }
            else if( rd>=6 &&
               responds[0]==0x06 && 
               responds[1]==0x0 && 
               responds[2]==0x01 && 
               responds[3]==0x01 && 
               responds[4]==0x03 ) {
                    break;                  // receive error report
            }
            else if( rd>=6 &&
               responds[rd-6]==0x06 && 
               responds[rd-5]==0x0 && 
               responds[rd-4]==0x01 && 
               responds[rd-3]==0x01 && 
               responds[rd-2]==0x03 ) {
                    break;                  // receive error report
            }
        }
    }

    fclose( fwfile );

    // wait a bit (10 seconds), for firmware update done signal
    if( res==0 ) {
        rd = mcu_read(responds, sizeof(responds), 10000000, 200000 );
        if( rd>=5 &&
            responds[0]==0x05 && 
            responds[1]==0x0 && 
            responds[2]==0x01 && 
            responds[3]==0x03 && 
            responds[4]==0x02 ) 
        {
            res=1 ;
        }
        else if( rd>=5 &&
            responds[rd-5]==0x05 && 
            responds[rd-4]==0x0 && 
            responds[rd-3]==0x01 && 
            responds[rd-2]==0x03 && 
            responds[rd-1]==0x02 ) 
        {
            res=1 ;
        }
    }
    if( res ) {
        netdbg_print("Done.\n");
        if( fwprogfile ) {
            fprintf( fwprogfile, "100"  );
            fflush( fwprogfile );
        }        
        if( fwmsgfile ) {
            fprintf(fwmsgfile, "Done.\n");
            fprintf(fwmsgfile, "Please reset the unit.\n");
            fflush( fwmsgfile );
        }
    }
    else {
        netdbg_print("Failed.\n");
        if( fwmsgfile ) {
            fprintf(fwmsgfile, "Failed.\n");
            fflush( fwmsgfile );
        }
    }
    if( fwmsgfile ) {
        fclose( fwmsgfile );
    }
    if( fwprogfile ) {
        fclose( fwprogfile );
    }
    return res ;
}


#ifdef PWII_APP

// Send cmd to pwii and check responds
char * mcu_pwii_cmd(int cmd, int datalen, ...)
{
    int i ;
    va_list v ;
    char * mcu_rsp ;
    char mcu_buf[datalen+10] ;

    mcu_buf[0] = (char)(datalen+6) ;
    mcu_buf[1] = (char)5 ;                      // target PWII
    mcu_buf[2] = (char)0 ;
    mcu_buf[3] = (char)cmd ;
    mcu_buf[4] = (char)2 ;

    va_start( v, datalen ) ;
    for( i=0 ; i<datalen ; i++ ) {
        mcu_buf[5+i] = (char) va_arg( v, int );
    }
    va_end(v);
    i=3 ;
    while( i-->0 ) {
        if(mcu_send(mcu_buf)) {
            mcu_rsp = mcu_waitrsp(5, cmd) ;         // wait rsp from PWII
            if( mcu_rsp ) {
                return mcu_rsp ;
            }
        }

#ifdef MCU_DEBUG
        printf("Retry!!!\n" );
#endif

    }

    return NULL ;
}

int mcu_pwii_bootupready()
{
    if( mcu_pwii_cmd(PWII_CMD_BOOTUPREADY)!=NULL ) {
        return 1 ;
    }
    return 0 ;
}

// get pwii MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_pwii_version(char * version)
{
    char * v ;
    v = mcu_pwii_cmd(PWII_CMD_VERSION) ;
    if( v ) {
        memcpy( version, &v[5], *v-6 );
        version[*v-6]=0 ;
        return *v-6 ;
    }
    return 0 ;
}

unsigned int mcu_pwii_ouputstatus()
{
    unsigned int outputmap = 0 ;
    char * ibuf = mcu_pwii_cmd( PWII_CMD_OUTPUTSTATUS, 2, 0, 0 ) ;
    if( ibuf ) {
        if( ibuf[6] & 1 ) outputmap|=1 ;        // C1 LED
        if( ibuf[6] & 2 ) outputmap|=2 ;        // C2 LED
        if( ibuf[6] & 4 ) outputmap|=4 ;        // MIC
        if( ibuf[6] & 0x18 ) outputmap|=8 ;     // Error
        if( ibuf[6] & 0x20 ) outputmap|=0x10 ;  // POWER LED
        if( ibuf[6] & 0x40 ) outputmap|=0x20 ;  // BO_LED
        if( ibuf[6] & 0x80 ) outputmap|=0x40 ;  // Backlight LED
        
        if( ibuf[5] & 1 ) outputmap|=0x800 ;    // LCD POWER
        if( ibuf[5] & 2 ) outputmap|=0x200 ;    // GPS POWER
        if( ibuf[5] & 4 ) outputmap|=0x400 ;    // RF900 POWER
    }
    return outputmap ;
}

#endif          // PWII_APP


// restart mcu port, some times serial port die
void  mcu_restart()
{
   if( mcu_handle > 0 ) {
        close( mcu_handle );
        mcu_handle = 0 ;
    }
    mcu_handle = serial_open( mcu_dev, mcu_baud );
    mcu_buffer_len = 0 ;
    if( mcu_handle<=0 ) {
        // even no serail port, we still let process run
        netdbg_print("Serial port failed!\n");
    }
}

// initialize serial port
void mcu_init( config & dvrconfig ) 
{
    string v ;
    double dv ;
    int i ;

    // initilize serial port
    v = dvrconfig.getvalue( "io", "serialport");
    if( v.length()>0 ) {
        strcpy( mcu_dev, v.getstring() );
    }
    mcu_baud = dvrconfig.getvalueint("io", "serialbaudrate");
    if( mcu_baud<2400 || mcu_baud>115200 ) {
            mcu_baud=DEFSERIALBAUD ;
    }
    
    if( mcu_handle > 0 ) {
        close( mcu_handle );
        mcu_handle = 0 ;
    }
    mcu_handle = serial_open( mcu_dev, mcu_baud );
    mcu_buffer_len = 0 ;
    if( mcu_handle<=0 ) {
        // even no serail port, we still let process run
        netdbg_print("Can not open serial port %s.\n", mcu_dev);
    }
    
    output_inverted = 0 ;
    
    for( i=0; i<32 ; i++ ) {
        char outinvkey[50] ;
        sprintf(outinvkey, "output%d_inverted", i+1);
        if( dvrconfig.getvalueint( "io", outinvkey ) ) {
            output_inverted |= (1<<i) ;
        }
    }

    v = dvrconfig.getvalue( "io", "mcu_program_delay" ) ;
    if( v.length()>0 ) {
        sscanf( v.getstring(), "%lf", &dv );
        mcu_program_delay = (int)(dv*1000000.0) ;
        if( mcu_program_delay>1000000 ) {
            mcu_program_delay = 999999 ;
        }
        else if( mcu_program_delay<1000 ) {
            mcu_program_delay = 0 ;
        }
    }
    else {
        mcu_program_delay = 20000 ;
    }
    
    // boot mcu (power processor)
    if( mcu_bootupready () ) {
        // get MCU version
        char mcu_firmware_version[80] ;
        if( mcu_version( mcu_firmware_version ) ) {
            dvr_log("MCU version: %s", mcu_firmware_version );
            FILE * mcuversionfile=fopen("/var/dvr/mcuversion", "w");
            if( mcuversionfile ) {
                fprintf( mcuversionfile, "%s", mcu_firmware_version );
                fclose( mcuversionfile );
            }
        }        
    }
    else {
        dvr_log("MCU failed.");
    }

#ifdef TVS_APP
    // setup GP5 polarity for TVS 
    mcu_initsensor2(dvrconfig.getvalueint( "sensor2", "inverted" ));
#endif
        

}

void mcu_finish()
{
    // close serial port
    if( mcu_handle>0 ) {
        close( mcu_handle );
        mcu_handle=0;
    }
}
