#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>

#include "../cfg.h"

#include "../dvrsvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

// options
int standbyhdoff = 0 ;
int usewatchdog = 0 ;
int hd_timeout = 60 ;       // hard drive ready timeout

// HARD DRIVE LED and STATUS
#define HDLED	(0x10)

#define RECVBUFSIZE (50)

// input pin maping
#ifdef MDVR_APP
#define MAXINPUT    (12)
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)
unsigned int input_map_table [MAXINPUT] = {
    1,          // bit 0
    (1<<1),     // bit 1
    (1<<2),     // bit 2
    (1<<3),     // bit 3
    (1<<4),     // bit 4
    (1<<5),     // bit 5
    (1<<10),    // bit 6
    (1<<11),    // bit 7
    (1<<12),    // bit 8
    (1<<13),    // bit 9
    (1<<14),    // bit 10
    (1<<15)     // bit 11
} ;

#endif  // MDVR_APP

#ifdef TVS_APP
#define MAXINPUT    (12)
#define HDINSERTED	(0x40)
#define HDKEYLOCK	(0x80)
unsigned int input_map_table [MAXINPUT] = {
    1,          // bit 0
    (1<<1),     // bit 1
    (1<<2),     // bit 2
    (1<<3),     // bit 3
    (1<<4),     // bit 4
    (1<<5),     // bit 5
    (1<<10),    // bit 6
    (1<<11),    // bit 7
    (1<<12),    // bit 8
    (1<<13),    // bit 9
    (1<<14),    // bit 10
    (1<<15)     // bit 11
} ;

#endif  // TVS_APP

#ifdef PWII_APP
#define MAXINPUT    (12)
#define HDINSERTED	(1<<6)
#define HDKEYLOCK	(1<<7)
#define PWII_MIC    (1<<11)
unsigned int input_map_table [MAXINPUT] = {
    1,          // bit 0
    (1<<1),     // bit 1
    (1<<2),     // bit 2
    (1<<3),     // bit 3
    (1<<4),     // bit 4
    (1<<5),     // bit 5
    (1<<10),    // bit 6
    (1<<8),     // bit 7
    (1<<12),    // bit 8
    (1<<13),    // bit 9
    (1<<14),    // bit 10
    (1<<15),    // bit 11
} ;

#endif  // PWII_APP

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

int hdlock=0 ;								// HD lock status
int hdinserted=0 ;

#define PANELLEDNUM (3)
#define DEVICEPOWERNUM (5)

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

struct dio_mmap * p_dio_mmap=NULL ;

#define SERIAL_DELAY	(100000)
#define DEFSERIALBAUD	(115200)
#define MCU_CMD_DELAY   (1000000)

char dvriomap[256] = "/var/dvr/dvriomap" ;
char serial_dev[256] = "/dev/ttyS1" ;
char * pidfile = "/var/dvr/ioprocess.pid" ;
int serial_handle = 0 ;
int serial_baud = 115200 ;
int mcupowerdelaytime = 0 ;
char mcu_firmware_version[80] ;

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;
char logfile[128] = "dvrlog.txt" ;
char temp_logfile[128] ;
int watchdogenabled=0 ;
int watchdogtimeout=30 ;
int gpsvalid = 0 ;
int gforce_log_enable=0 ;
int output_inverted=0 ;

#ifdef PWII_APP
int pwii_tracemark_inverted=0 ;
int pwii_rear_ch ;
int pwii_front_ch ;
#endif

#define APPMODE_QUIT            (0)
#define APPMODE_RUN             (1)
#define APPMODE_SHUTDOWNDELAY   (2)
#define APPMODE_NORECORD        (3)
#define APPMODE_STANDBY         (5)
#define APPMODE_SHUTDOWN        (6)
#define APPMODE_REBOOT          (7)
#define APPMODE_REINITAPP       (8)

int app_mode = APPMODE_QUIT ;

unsigned int panelled=0 ;
unsigned int devicepower=0xffff;

int wifi_on = 0 ;

pid_t   pid_smartftp = 0 ;

void sig_handler(int signum)
{
    if( signum==SIGUSR2 ) {
        app_mode=APPMODE_REINITAPP ;
    }
    else {
        app_mode=APPMODE_QUIT;
    }
}

// return bcd value
int getbcd(unsigned char c)
{
    return (int)(c/16)*10 + c%16 ;
}

// write log to file.
// return 
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(char *fmt, ...)
{
    int res = 0 ;
    static char logfilename[512]="" ;
    char lbuf[512];
    char hostname[128] ;
    FILE *flog;
    time_t ti;
    int l;
    va_list ap ;

    flog = fopen("/var/dvr/dvrcurdisk", "r");
    if( flog ) {
        if( fscanf(flog, "%s", lbuf )==1 ) {
            gethostname(hostname, 128);
            sprintf(logfilename, "%s/_%s_/%s", lbuf, hostname, logfile);
        }
        fclose(flog);
    }

    ti = time(NULL);
    ctime_r(&ti, lbuf);
    l = strlen(lbuf);
    if (lbuf[l - 1] == '\n')
        lbuf[l - 1] = '\0';

    flog = fopen(logfilename, "a");
    if (flog) {
        fprintf(flog, "%s:IO:", lbuf);
        va_start( ap, fmt );
        vfprintf(flog, fmt, ap );
        va_end( ap );
        fprintf(flog, "\n");		
        if( fclose(flog)==0 ) {
            res=1 ;
        }
    }

    if( res==0 ) {
        flog = fopen(temp_logfile, "a");
        if (flog) {
            fprintf(flog, "%s:IO:", lbuf);
            va_start( ap, fmt );
            vfprintf(flog, fmt, ap );
            va_end( ap );
            fprintf(flog, " *\n");		
            if( fclose(flog)==0 ) {
                res=1 ;
            }
        }
    }
    
#ifdef MCU_DEBUG
    // also output to stdout (if in debugging)
    printf("%s:IO:", lbuf);
    va_start( ap, fmt );
    vprintf(fmt, ap );
    va_end( ap );
    printf("\n");		
#endif
        
    return res ;
}

int getshutdowndelaytime()
{
    config dvrconfig(dvrconfigfile);
    int v ;
    v = dvrconfig.getvalueint( "system", "shutdowndelay");
    if( v<10 ) {
        v=10 ;
    }
    if( v>7200 ) {
        v=7200 ;
    }
    return v ;
}

int getstandbytime()
{
    config dvrconfig(dvrconfigfile);
    int v ;
    v = dvrconfig.getvalueint( "system", "standbytime");
    if( v<0 ) {
        v=0 ;
    }
    if( v>43200 ) {
        v=43200 ;
    }
    return v ;
}

static unsigned int starttime=0 ;
void initruntime()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    starttime=tv.tv_sec ;
}

// return runtime in mili-seconds
unsigned int getruntime()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    return (unsigned int)(tv.tv_sec-starttime)*1000 + tv.tv_usec/1000 ;
}

// atomically swap value
int atomic_swap( int *m, int v)
{
    register int result ;
/*
    result=*m ;
    *m = v ;
*/
    asm volatile (
        "swp  %0, %2, [%1]\n\t"
        : "=r" (result)
        : "r" (m), "r" (v)
    ) ;
    return result ;
}

/*
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
*/

void dio_lock()
{
    while( atomic_swap( &(p_dio_mmap->lock), 1 ) ) {
        sleep(0);      // or sched_yield()
    }
}

void dio_unlock()
{
    p_dio_mmap->lock=0;
}

// set onboard rtc 
void rtc_set(time_t utctime)
{
    struct tm ut ;
    int hrtc = open("/dev/rtc", O_WRONLY );
    if( hrtc>0 ) {
        gmtime_r(&utctime, &ut);
        ioctl( hrtc, RTC_SET_TIME, &ut ) ;
        close( hrtc );
    }
}

#define SERIAL_BUFFER_TSIZE (2000)
static char serial_buffer[SERIAL_BUFFER_TSIZE] ;
static int  serial_buffer_len ;
static int  serial_buffer_pointer ;

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
    if( serial_buffer_len > 0 ) {       // buffer available?
        return 1 ;
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

int serial_read_hlp(void * buf, int bufsize)
{
    if( serial_buffer_len<=0 ) {   // serial buffer empty
        serial_buffer_pointer=0 ;
        serial_buffer_len = read(serial_handle, serial_buffer, SERIAL_BUFFER_TSIZE);
    }
    if( serial_buffer_len>0 ) { // buffer available
        int cpbytes = (serial_buffer_len > bufsize)?bufsize:serial_buffer_len ;
        memcpy( buf, &serial_buffer[serial_buffer_pointer], cpbytes );
        serial_buffer_pointer += cpbytes ;
        serial_buffer_len -= cpbytes ;
        return cpbytes ;
    }
    return 0 ;                  // err, no data.
}

int serial_read(void * buf, size_t bufsize)
{
    if( serial_handle>0 ) {
#ifdef MCU_DEBUG
        int i, n ;
        char * cbuf ;
        cbuf = (char *)buf ;
        n=serial_read_hlp(buf, bufsize);
        for( i=0; i<n ; i++ ) {
            printf("%02x ", (int)cbuf[i] );
        }
        return n ;
#else        
        return serial_read_hlp(buf, bufsize);
#endif
    }
    return 0;
}

int serial_read_withtimeout(void * buf, size_t bufsize, int microsecondstimeout)
{
    int n_toread = (int)bufsize ;
    int n_read ;
    char * readpointer = (char *)buf ;
    while( n_toread > 0 ) {
        if( serial_dataready(microsecondstimeout) ) {
            n_read = serial_read( readpointer, n_toread );
            if( n_read > 0 ) {
                n_toread-=n_read ;
                readpointer+=n_read ;
            }
            else {
                return 0 ;  // err?
            }
        }
        else {      // time out
            return (int)bufsize - n_toread ;
        }
    }
    return (int)bufsize ;
}

int serial_write(void * buf, size_t bufsize)
{
    if( serial_handle>0 ) {
#ifdef MCU_DEBUG
        size_t i ;
        char * cbuf ;
        cbuf = (char *)buf ;
        for( i=0; i<bufsize ; i++ ) {
            printf("%02x ", (int)cbuf[i] );
        }
#endif
        return write( serial_handle, buf, bufsize);
    }
    return 0;
}

void serial_clear(int usdelay=100 );

// clear receiving buffer
void serial_clear(int usdelay)
{
    int i;
    char rbuf[100] ;
#ifdef MCU_DEBUG
    printf("clear: ");
#endif     
    for(i=0;i<5000;i++) {
        if( serial_dataready(usdelay) ) {
            serial_read( rbuf, sizeof(rbuf));
        }
        else {
            break;
        }
    }
#ifdef MCU_DEBUG
    printf("\n");
#endif                
    serial_buffer_len = 0 ;
}

// initialize serial port
void serial_init() 
{
   int i;
    int port=0 ;
   
    if( serial_handle > 0 ) {
        close( serial_handle );
        serial_handle = 0 ;
    }
    if( strcmp( serial_dev, "/dev/ttyS1")==0 ) {
        port=1 ;
    }
//    serial_handle = open( serial_dev, O_RDWR | O_NOCTTY | O_NDELAY );
    serial_handle = open( serial_dev, O_RDWR | O_NOCTTY );
    if( serial_handle > 0 ) {
//        fcntl(serial_handle, F_SETFL, 0);
        if( port==1 ) {		// port 1
            // Use Hikvision API to setup serial port (RS485)
            InitRS485(serial_handle, serial_baud, DATAB8, STOPB1, NOPARITY, NOCTRL);
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
            tios.c_cc[VMIN]=10;		// minimum char 
            tios.c_cc[VTIME]=1;		// 0.1 sec time out
            baud_t = B115200 ;
            for( i=0; i<7; i++ ) {
                if( serial_baud == baud_table[i].baudrate ) {
                    baud_t = baud_table[i].baudv ;
                    break;
                }
            }
            
            cfsetispeed(&tios, baud_t);
            cfsetospeed(&tios, baud_t);
            
            tcflush(serial_handle, TCIFLUSH);
            tcsetattr(serial_handle, TCSANOW, &tios);
        }
        serial_buffer_len = 0 ;
    }
    else {
        // even no serail port, we still let process run
        printf("Serial port failed!\n");
    }
}

#define MCU_INPUTNUM (6)
#define MCU_OUTPUTNUM (4)

unsigned int mcu_doutputmap ;

// check data check sum
// return 0: checksum correct
int mcu_checksum( char * data ) 
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
void mcu_calchecksum( char * data )
{
    char ck = 0 ;
    unsigned int si = (unsigned int) *data ;
    while( --si>0 ) {
        ck-=*data++ ;
    }
    *data = ck ;
}

int mcu_input(int usdelay);

// send command to mcu
// return 0 : failed
//        >0 : success
int mcu_send( char * cmd ) 
{
    if( *cmd<5 || *cmd>40 ) { 	// wrong data
        return 0 ;
    }
    mcu_calchecksum( cmd );

#ifdef  MCU_DEBUG
// for debugging    
    struct timeval tv ;
    gettimeofday(&tv, NULL);
    int r ;
    struct tm * ptm ;
    ptm = localtime(&tv.tv_sec);
    double sec ;
    sec = ptm->tm_sec + tv.tv_usec/1000000.0 ;
    
    printf("%02d:%02d:%02.3f Send: ", ptm->tm_hour, ptm->tm_min, sec);
    r=serial_write(cmd, (int)(*cmd));
    printf("\n");
    return r ;
#else    
    return serial_write(cmd, (int)(*cmd));
#endif
}

int mcu_recv( char * recv, int size, int usdelay=SERIAL_DELAY );
int mcu_checkinputbuf(char * ibuf);

static int mcu_recverror = 0 ;

// receive one data package from mcu
// return 0: failed
//       >0: bytes of data received
int mcu_recvmsg( char * recv, int size )
{
    int n;

#ifdef MCU_DEBUG
    // for debugging 
    struct timeval tv ;
    gettimeofday(&tv, NULL);
    struct tm * ptm ;
    ptm = localtime(&tv.tv_sec);
    double sec ;
    sec = ptm->tm_sec + tv.tv_usec/1000000.0 ;
    
    printf("%02d:%02d:%02.3f Recv: ", ptm->tm_hour, ptm->tm_min, sec);
#endif    
    
    if( serial_read(recv, 1) ) {
        n=(int) *recv ;
        if( n>=5 && n<=size ) {
            n=serial_read_withtimeout(recv+1, n-1, 100000)+1 ;
            if( n==recv[0] && 
                recv[1]==0 && 
//                recv[2]==1 &&
                mcu_checksum( recv ) == 0 ) 
            {
#ifdef MCU_DEBUG
                printf("\n");
#endif
                return n ;
            }
        }
    }

#ifdef MCU_DEBUG
    // for debugging 
    printf(" - ERROR!\n");
#endif

    serial_clear();
    return 0 ;
}

// receiving a respondes message from mcu
int mcu_recv( char * recv, int size, int usdelay )
{
    int n;
    if( size<5 ) {				// minimum packge size?
        return 0 ;
    }
    
    while( serial_dataready(usdelay) ) {
        n = mcu_recvmsg (recv, size) ;
        if( n>4 ) {
            mcu_recverror=0 ;
            if( recv[4]=='\x02' ) {             // is it a input message ?
                mcu_checkinputbuf(recv) ;
                continue ;
            }
            else {
                if( recv[3]=='\x1f' ) {         // it is a error report, ignor this for now
#ifdef MCU_DEBUG
                    printf(" A error report detected!\n" );
#endif
                    continue ;
                }
                return n ;                      // it could be a respondes message
            }
        }
        else {
            break;
        }
    }

    if( ++mcu_recverror>5 ) {
        sleep(1);
        serial_init();
        mcu_recverror=0 ;
    }

    return 0 ;
}

// receive "Enter a command" , success: return 1, failed: return 0
int mcu_recv_enteracommand()
{
    int res = 0 ;
    char enteracommand[200] ;
    while( serial_dataready(100000) ) {
        memset( enteracommand, 0, sizeof(enteracommand));
        serial_read_withtimeout(enteracommand, sizeof(enteracommand), 100000);
        if( strcasestr( enteracommand, "Enter a command" )) {
            res=1 ;
        }
    }
    return res ;
}

// help function for sending command to mcu
static char * mcu_cmd(int cmd, int datalen=0, ...)
{
    static char responds[RECVBUFSIZE] ;
    int i ;
    char cmd_mcu[30] ;
    if( datalen >= 0  && datalen <20 ) {
        va_list v ;

        cmd_mcu[0] = (char)(datalen+6) ;
        cmd_mcu[1] = (char)1 ;         // target : MCU
        cmd_mcu[2] = (char)0 ;         // source : CPU
        cmd_mcu[3] = (char)cmd ;
        cmd_mcu[4] = (char)2 ;

        va_start( v, datalen ) ;
        for( i=0 ; i<datalen ; i++ ) {
            cmd_mcu[5+i] = (char) va_arg( v, int );
        }
        va_end(v);
        if(mcu_send(cmd_mcu)) {
            while( mcu_recv( responds, RECVBUFSIZE, MCU_CMD_DELAY )>5 ) {     // received a responds
                if( responds[1]==0 && 
                   responds[2]==1 &&
                   responds[3]==(char)cmd &&
                   responds[4]==3 )                             // right responds
                {
                    return responds ;           
                }
            }
        }
    }

    return NULL ;
}

// response to mcu 
static void mcu_response(char * cmd, int datalen=0, ... )
{
    int i ;
    char mcu_res[20] ;
    if( datalen >= 0  && datalen <10 ) {
        va_list v ;
        mcu_res[0] = datalen+6 ;
        mcu_res[1] = cmd[2] ;
        mcu_res[2] = 0 ;                // cpu
        mcu_res[3] = cmd[3] ;           // response to same command
        mcu_res[4] = 3 ;                // response
        va_start( v, datalen ) ;
        for( i=0 ; i<datalen ; i++ ) {
            mcu_res[5+i] = (char) va_arg( v, int );
        }
        va_end(v);
        mcu_send( mcu_res );
    }
}

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
        
static char direction_table[24][2] = 
{
    {0, 2}, {0, 3}, {0, 4}, {0,5},      // Front -> Forward
    {1, 2}, {1, 3}, {1, 4}, {1,5},      // Back  -> Forward
    {2, 0}, {2, 1}, {2, 4}, {2,5},      // Right -> Forward
    {3, 0}, {3, 1}, {3, 4}, {3,5},      // Left  -> Forward
    {4, 0}, {4, 1}, {4, 2}, {4,3},      // Buttom -> Forward
    {5, 0}, {5, 1}, {5, 2}, {5,3},      // Top   -> Forward
};

#define DEFAULT_DIRECTION   (7)
static int gsensor_direction = DEFAULT_DIRECTION ;
int  g_sensor_available ;
float g_sensor_trigger_forward ;
float g_sensor_trigger_backward ;
float g_sensor_trigger_right ;
float g_sensor_trigger_left ;
float g_sensor_trigger_down ;
float g_sensor_trigger_up ;


#ifdef PWII_APP

// help function for sending command to pwii
static char * mcu_pwii_cmd(int cmd, int datalen=0, ...)
{
    static char responds[RECVBUFSIZE] ;
    int i ;
    int retry=5 ;
    char cmd_pwii[30] ;
    if( datalen >= 0  && datalen <20 ) {
        va_list v ;

        cmd_pwii[0] = (char)(datalen+6) ;
        cmd_pwii[1] = (char)5 ;
        cmd_pwii[2] = (char)0 ;
        cmd_pwii[3] = (char)cmd ;
        cmd_pwii[4] = (char)2 ;

        va_start( v, datalen ) ;
        for( i=0 ; i<datalen ; i++ ) {
            cmd_pwii[5+i] = (char) va_arg( v, int );
        }
        va_end(v);
        while( retry-- ) {
            if(mcu_send(cmd_pwii)) {
                while( mcu_recv( responds, RECVBUFSIZE, MCU_CMD_DELAY )>5 ) {     // received a responds
                    if( responds[1]==0 && 
                       responds[2]==5 &&
                       responds[3]==(char)cmd &&
                       responds[4]==3 )                             // right responds
                    {
                        return responds ;           
                    }
                }
            }
        }
    }

    return NULL ;
}

int mcu_pwii_bootupready()
{
    int retry=10;
    while( --retry>0 ) {
        serial_clear() ;
        if( mcu_pwii_cmd(4)!=NULL ) {
            return 1 ;
        }
        sleep(1);
    }
    return 0 ;
}

// get pwii MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_pwii_version(char * version)
{
    int retry ;
    char * v ;
    
    for( retry=0; retry<10; retry++ ) {
        v = mcu_pwii_cmd(0x11) ;
        if( v ) {
              memcpy( version, &v[5], *v-6 );
              version[*v-6]=0 ;
              return *v-6 ;
        }
    }
    return 0 ;
}

// Set C1/C2 LED and set external mic
void mcu_pwii_setc1c2() 
{
   // C1 LED (front)
    if( p_dio_mmap->camera_status[pwii_front_ch] & 2 ) {         // front ca
        if( (p_dio_mmap->pwii_output & 1) == 0 ) {
            p_dio_mmap->pwii_output |= 1 ;
            // turn on mic
            mcu_cmd(0x21) ;
        }
    }
    else {
        if( (p_dio_mmap->pwii_output & 1) != 0 ) {
            p_dio_mmap->pwii_output &= (~1) ;
            // turn off mic
            mcu_cmd(0x22);
        }
    }
    
   // C2 LED
   if( p_dio_mmap->camera_status[pwii_rear_ch] & 2 ) {         // rear camera recording?
       p_dio_mmap->pwii_output |= 2 ;
   }
    else {
       p_dio_mmap->pwii_output &= (~2) ;
    }
}

static unsigned int pwii_outs = 0 ;

// update PWII outputs, include leds and powers
void mcu_pwii_output()
{

    unsigned int xouts ;

    mcu_pwii_setc1c2()  ;       // check c1 c2 led.
    
    xouts = pwii_outs ^ p_dio_mmap->pwii_output ;

    if( xouts ) {
        pwii_outs ^= xouts ;

        if( xouts & 0x1000 ) {         // BIT 12: standby mode, 1: standby, 0: running
            if( (pwii_outs&0x1000)!=0 ) {
                // clear LEDs
                // C1 LED 
                mcu_pwii_cmd( 0x12, 1, 0 );
                // C2 LED
                mcu_pwii_cmd( 0x13, 1, 0 );
                // bit 2: MIC LED
                mcu_pwii_cmd( 0xf, 1, 0 );
                // BIT 11: LCD power
                mcu_pwii_cmd( 0x16, 1, 0 );
                // standby
                mcu_pwii_cmd( 0x15, 1, 1 );
                // turn off POWER LED
                mcu_pwii_cmd( 0x14, 1, 0 );
            }
            else {
                p_dio_mmap->pwii_output |= 0x800 ;  // LCD turned on when get out of standby
                pwii_outs |= 0x800 ;
                mcu_pwii_cmd( 0x15, 1, 0 );
            }
            xouts |= 0x17 ;                   // refresh LEDs
        }

        if( (pwii_outs&0x1000)==0 ) {       // not in standby mode
            // BIT 4: POWER LED
            mcu_pwii_cmd( 0x14, 1, ((pwii_outs&0x10)!=0) );
            // C1 LED 
            mcu_pwii_cmd( 0x12, 1, ((pwii_outs&1)!=0) );
            // C2 LED
            mcu_pwii_cmd( 0x13, 1, ((pwii_outs&2)!=0) );
            // bit 2: MIC LED
            mcu_pwii_cmd( 0xf, 1, ((pwii_outs&4)!=0) );
            // BIT 11: LCD power
            mcu_pwii_cmd( 0x16, 1, ((pwii_outs&0x800)!=0) );
        }

        if( xouts & 8 ) {           // bit 3: ERROR LED
            if((pwii_outs&8)!=0) {
                if( p_dio_mmap->pwii_error_LED_flash_timer>0 ) {
                    mcu_pwii_cmd( 0x10, 2, 2, p_dio_mmap->pwii_error_LED_flash_timer );
                }
                else {
                    mcu_pwii_cmd( 0x10, 2, 1, 0 );
                }
            }
            else {
                mcu_pwii_cmd( 0x10, 2, 0, 0 );
            }
        }

        if( xouts & 0x100 ) {          // BIT 8: GPS antenna power
            mcu_pwii_cmd( 0x0e, 1, ((pwii_outs&0x100)!=0) );
        }
        
        if( xouts & 0x200 ) {         // BIT 9: GPS POWER
            mcu_pwii_cmd( 0x0d, 1, ((pwii_outs&0x200)!=0) );
        }

        if( xouts & 0x400 ) {          // BIT 10: RF900 POWER
            mcu_pwii_cmd( 0x0c, 1, ((pwii_outs&0x400)!=0) );
        }

        if( xouts & 0x2000 ) {          // BIT 13: WIFI power
            mcu_pwii_cmd( 0x19, 1, ((pwii_outs&0x2000)!=0) );
        }

    }
}

unsigned int mcu_pwii_ouputstatus()
{
    unsigned int outputmap = 0 ;
    char * ibuf = mcu_pwii_cmd( 0x17, 2, 0, 0 ) ;
    if( ibuf ) {
        if( ibuf[6] & 1 ) outputmap|=1 ;        // C1 LED
        if( ibuf[6] & 2 ) outputmap|=2 ;        // C2 LED
        if( ibuf[6] & 4 ) outputmap|=4 ;        // MIC
        if( ibuf[6] & 0x18 ) outputmap|=8 ;     // Error
        if( ibuf[6] & 0x20 ) outputmap|=0x10 ;  // POWER LED
        
        if( ibuf[5] & 1 ) outputmap|=0x800 ;    // LCD POWER
        if( ibuf[5] & 2 ) outputmap|=0x200 ;    // GPS POWER
        if( ibuf[5] & 4 ) outputmap|=0x400 ;    // RF900 POWER
    }
    return outputmap ;
}

void mcu_pwii_init()
{
    p_dio_mmap->pwii_output = 0xf10 ;
    pwii_outs = mcu_pwii_ouputstatus() ;
//    pwii_outs = 0xf10 ;
}

// zoom in/out front camera
void mcu_camera_zoom( int zoomin )
{
    if( zoomin ) {
        mcu_cmd(0x1f) ;
    }
    else {
        mcu_cmd(0x20) ;
    }
}

#endif  // PWII_APP

static void mcu_dinput_help(char * ibuf)
{
    unsigned int imap1, imap2 ;
    int i;
    // get digital input map
    imap1 = (unsigned char)ibuf[5]+256*(unsigned int)(unsigned char)ibuf[6] ; ;
    imap2 = 0 ;
    for( i=0; i<MAXINPUT; i++ ) {
        if( imap1 & input_map_table[i] )
            imap2 |= (1<<i) ;
    }
    p_dio_mmap->inputmap = imap2;
    // hdlock = (imap1 & (HDINSERTED|HDKEYLOCK) )==0 ;	// HD plugged in and locked
    hdlock = (imap1 & (HDKEYLOCK) )==0 ;	                // HD plugged in and locked
    hdinserted = (imap1 & HDINSERTED)==0 ;  // HD inserted

#ifdef      PWII_APP
    if( imap1 & PWII_MIC ) {
        p_dio_mmap->pwii_output &= ~4 ;      // turn off bit2 , MIC LED
    }
    else {
        p_dio_mmap->pwii_output |= 4 ;      // turn on bit2 , MIC LED
    }
#endif      // PWII_APP
}

static void mcu_gforce_log( float gright, float gback, float gbuttom ) 
{
    //        float gback, gright, gbuttom ;
    float gbusforward, gbusright, gbusdown ;

#ifdef MCU_DEBUG
    printf("Accelerometer, --------- right=%.2f , back   =%.2f , buttom=%.2f .\n",     
           gright,
           gback,
           gbuttom );
#endif            

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

#ifdef MCU_DEBUG
    printf("Accelerometer, converted right=%.2f , forward=%.2f , down  =%.2f .\n",     
           gbusright,
           gbusforward,
           gbusdown );
#endif            
    // save to log
    if( p_dio_mmap->gforce_log0==0 ) {
        p_dio_mmap->gforce_right_0 = gbusright ;
        p_dio_mmap->gforce_forward_0 = gbusforward ;
        p_dio_mmap->gforce_down_0 = gbusdown ;
        if( gforce_log_enable ) {
            p_dio_mmap->gforce_log0 = 1 ;
        }
    }
    else {
        p_dio_mmap->gforce_right_1 = gbusright ;
        p_dio_mmap->gforce_forward_1 = gbusforward ;
        p_dio_mmap->gforce_down_1 = gbusdown ;
        if( gforce_log_enable ) {
            p_dio_mmap->gforce_log1 = 1 ;
        }
    }                

}

int mcu_checkinputbuf(char * ibuf)
{
    if( ibuf[4]=='\x02' && ibuf[2]=='\x01' ) {     // mcu initiated message ?
        switch( ibuf[3] ) {
        case '\x1c' :                   // digital input event
            mcu_response( ibuf );
            mcu_dinput_help(ibuf);
            break;

        case '\x08' :               // ignition off event
            mcu_response( ibuf, 2, 0, 0 );    // response with two 0 data
            mcupowerdelaytime = 0 ;
            p_dio_mmap->poweroff = 1 ;						// send power off message to DVR
#ifdef MCU_DEBUG
            printf("Ignition off, mcu delay %d s\n", mcupowerdelaytime );
#endif            
            break;
            
        case '\x09' :	// ignition on event
            mcu_response( ibuf, 1, watchdogenabled  );      // response with watchdog enable flag
            p_dio_mmap->poweroff = 0 ;						// send power on message to DVR
#ifdef MCU_DEBUG
            printf("ignition on\n");
#endif            
            break ;

        case '\x40' :               // Accelerometer data
            mcu_response( ibuf );

            mcu_gforce_log( ((float)(signed char)ibuf[5])/14 ,
                           ((float)(signed char)ibuf[6])/14 ,
                           ((float)(signed char)ibuf[7])/14 ) ;
            break;
            
        default :
#ifdef MCU_DEBUG
            printf("Unknown message from MCU.\n");
#endif
            break;
        }
    }

#ifdef PWII_APP

    else if( ibuf[4]=='\x02' && ibuf[2]=='\x05' )          // input from MCU5 (MR. Henry?)
    {
#ifdef MCU_DEBUG
            printf("Messages from Mr. Henry!\n" );
#endif            
       switch( ibuf[3] ) {
       case '\x05' :                   // Front Camera (REC) button
           p_dio_mmap->pwii_buttons |= 0x100 ;      // bit 8: front camera
//           mcu_response( ibuf, 1, ((p_dio_mmap->pwii_output&1)!=0) );  // bit 0: c1 led
           mcu_response( ibuf, 1, 0 );                                  // bit 0: c1 led
           break;

       case '\x06' :                   // Back Seat Camera (C2) Starts/Stops Recording
           p_dio_mmap->pwii_buttons |= 0x200 ;      // bit 9: back camera
//           mcu_response( ibuf, 1, ((p_dio_mmap->pwii_output&2)!=0) );  // bit 1: c2 led
           mcu_response( ibuf, 1, 0 );                                  // bit 1: c2 led
           break;

       case '\x07' :                   // TM Button
           mcu_response( ibuf );
           if( ibuf[5] ) {
               p_dio_mmap->pwii_buttons |= 0x400 ;      // bit 10: tm button
           }
           else {
               p_dio_mmap->pwii_buttons &= (~0x400) ;   // bit 10: tm button
           }
           break;

       case '\x08' :                   // LP button
           mcu_response( ibuf );
           if( ibuf[5] ) {
                p_dio_mmap->pwii_buttons |= 0x800 ;      // bit 11: LP button
                mcu_camera_zoom( 1 );
           }
           else {
                p_dio_mmap->pwii_buttons &= (~0x800) ;   // bit 11: LP button
                mcu_camera_zoom( 0 );
           }
           break;

       case '\x0b' :                   // BIT 12:  blackout, 1: pressed, 0: released, auto release
           if( ibuf[5] ) {
                p_dio_mmap->pwii_buttons |= 0x1000 ;      // bit 12: blackout
           }
           else {
                p_dio_mmap->pwii_buttons &= (~0x1000) ;      // bit 12: blackout
           }
           mcu_response( ibuf );
           break;
           
       case '\x09' :                        // Video play Control
           p_dio_mmap->pwii_buttons &= (~0x3f) ;
           p_dio_mmap->pwii_buttons |= ((unsigned char)ibuf[5])^0x3f ;
           mcu_response( ibuf );
           break;

       case '\x18' :                        // CDC ask for boot up ready
           mcu_response( ibuf, 1, 1  );     // possitive response
           pwii_outs = ~p_dio_mmap->pwii_output ;   // force re-fresh LED outputs
           break;

       default :
#ifdef MCU_DEBUG
            printf("Unknown message from PWII MCU.\n");
#endif
            break;

       }
    }

#endif    // PWII_APP

    return 0 ;
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
    int n ;
    int repeat ;
    char ibuf[RECVBUFSIZE] ;
    int udelay = usdelay ;
    
    for(repeat=0; repeat<10; repeat++ ) {
        if( serial_dataready(udelay) ) {
            n = mcu_recvmsg( ibuf, sizeof(ibuf) ) ;            // mcu_recv() will process mcu input messages
            if( n>4 ) {
                udelay=10 ;
                if( ibuf[3] != '\x1f' ) {                   // ignor error reports for now
                    res=mcu_checkinputbuf(ibuf);
                }
#ifdef MCU_DEBUG
                else {
                    printf( "Error Report Detected!\n");
                }
#endif                
            }
        }
        else {
            break;
        }
    }
    return res;
}

int mcu_bootupready()
{
    int retry=10;
    while( --retry>0 ) {
        serial_clear() ;
        if( mcu_cmd(0x11, 1, 0 )!=NULL ) {
            return 1 ;
        }
        sleep(1);
    }
    return 0 ;
}

int mcu_readcode()
{
    char * v = mcu_cmd(0x41);
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

int mcu_reboot()
{
    static char cmd_reboot[8]="\x06\x01\x00\x12\x02\xcc" ;
    
    mcu_send(cmd_reboot) ;
    // no responds
    return 0 ;
}

// return gsensor available
int mcu_gsensorinit()
{
    static char cmd_gsensorinit[16]="\x0c\x01\x00\x34\x02\x01\x02\x03\x04\x05\x06\xcc" ;
    char responds[RECVBUFSIZE] ;
    int retry=10;
    float trigger_back, trigger_front ;
    float trigger_right, trigger_left ;
    float trigger_bottom, trigger_top ;
    
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
        cmd_gsensorinit[5]  = (signed char)(trigger_right*0xe) ;    // Right Trigger
        cmd_gsensorinit[6]  = (signed char)(trigger_left*0xe) ;     // Left Trigger
    }
    else {
        cmd_gsensorinit[5]  = (signed char)(trigger_left*0xe) ;     // Right Trigger
        cmd_gsensorinit[6]  = (signed char)(trigger_right*0xe) ;    // Left Trigger
    }
    
    if( trigger_back >= trigger_front ) {
        cmd_gsensorinit[7]  = (signed char)(trigger_back*0xe) ;    // Back Trigger
        cmd_gsensorinit[8]  = (signed char)(trigger_front*0xe) ;    // Front Trigger
    }
    else {
        cmd_gsensorinit[7]  = (signed char)(trigger_front*0xe) ;    // Back Trigger
        cmd_gsensorinit[8]  = (signed char)(trigger_back*0xe) ;    // Front Trigger
    }
    
    if( trigger_bottom >= trigger_top ) {
        cmd_gsensorinit[9]  = (signed char)(trigger_bottom*0xe) ;    // Bottom Trigger
        cmd_gsensorinit[10] = (signed char)(trigger_top*0xe) ;    // Top Trigger
    }
    else {
        cmd_gsensorinit[9]  = (signed char)(trigger_top*0xe) ;    // Bottom Trigger
        cmd_gsensorinit[10] = (signed char)(trigger_bottom*0xe) ;    // Top Trigger
    }
    
    g_sensor_available = 0 ;
    while( --retry>0 ) {
        serial_clear() ;
        if(mcu_send(cmd_gsensorinit)) {
            if( mcu_recv( responds, RECVBUFSIZE ) ) {
                if( responds[3]=='\x34' &&
                   responds[4]=='\x03'&&
                   responds[0]>6 ) 
                {
                    g_sensor_available = responds[5] ;
                    return g_sensor_available ;
                }
            }
        }
        sleep(1);
    }
    return 0 ;
}

// get MCU firmware version.
// return 0: failed
//       >0: size of version string (not include null char)
int mcu_version(char * version)
{
    int retry ;
    char * v ;
    for( retry=0; retry<10; retry++ ) {
        v = mcu_cmd( 0x2d ) ;
        if( v ) {
            memcpy( version, &v[5], *v-6 );
                    version[*v-6]=0 ;
                    return *v-6 ;
        }
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
    if( mcu_cmd(0x31, 1, dout) ) {
        mcu_doutputmap=outputmap ;
    }
    return 1;
}

// get mcu digital input
int mcu_dinput()
{
    char * v ;
    v = mcu_cmd( 0x1d ) ;
    if( v ) {
        mcu_dinput_help(v);
        return 1 ;
    }
    return 0 ;
}

static int delay_inc = 10 ;

// set more mcu power off delay (keep power alive), (called every few seconds)
void mcu_poweroffdelay()
{
    char * responds ;
    if( mcupowerdelaytime < 50 ) {
        responds = mcu_cmd( 0x08, 2, delay_inc/256, delay_inc%256 );
    }
    else {
        responds = mcu_cmd( 0x08, 2, 0, 0 );
    }
    if( responds ) {
        mcupowerdelaytime = ((unsigned)(responds[5]))*256+((unsigned)responds[6]) ;
#ifdef MCU_DEBUG        
        printf("mcu delay %d s\n", mcupowerdelaytime );
#endif                    
    }
}

void mcu_watchdogenable()
{
    int retry ;

    for( retry=0; retry<5; retry++) {
        // set watchdog timeout
        if( mcu_cmd( 0x19, 2, watchdogtimeout/256, watchdogtimeout%256 ) ) {
            break;
        }
    }
   
    for( retry=0; retry<5; retry++) {
        // enable watchdog
        if( mcu_cmd( 0x1a ) ) {
            break;
        }
    }
    
    watchdogenabled = 1 ;
}

void mcu_watchdogdisable()
{
    int retry ;
    for( retry=0; retry<5; retry++) {
        if( mcu_cmd( 0x1b ) ) {
            break ;
        }
    }
    watchdogenabled = 0 ;
}

// return 0: error, >0 success
int mcu_watchdogkick()
{
    mcu_cmd(0x18);
    return 0;
}

// get io board temperature
int mcu_iotemperature()
{
    char * v = mcu_cmd(0x0b) ;
    if( v ) {
        return (int)(signed char)v[5] ;
    }
    return -128;
}

// get hd temperature
int mcu_hdtemperature()
{
#ifdef  MDVR_APP
    char * v = mcu_cmd(0x0c) ;
    if( v ) {
        return (int)(signed char)v[5] ;
    }
#endif    
    return -128 ;
}

void mcu_hdpoweron()
{
    mcu_cmd(0x28);
}

void mcu_hdpoweroff()
{
    mcu_cmd(0x29);
}

// return time_t: success
//             -1: failed
time_t mcu_r_rtc( struct tm * ptm = NULL )
{
    int retry ;
    struct tm ttm ;
    time_t tt ;
    char * responds ;
    for( retry=0; retry<5; retry++ ) {
        responds = mcu_cmd( 0x06 ) ;
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
        p_dio_mmap->rtc_year  = ttm.tm_year+1900 ;
        p_dio_mmap->rtc_month = ttm.tm_mon+1 ;
        p_dio_mmap->rtc_day   = ttm.tm_mday ;
        p_dio_mmap->rtc_hour  = ttm.tm_hour ;
        p_dio_mmap->rtc_minute= ttm.tm_min ;
        p_dio_mmap->rtc_second= ttm.tm_sec ;
        p_dio_mmap->rtc_millisecond = 0 ;
        
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
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
    mcu_cmd( 0x2f, 2, led, (flash!=0) );
}

// Device Power
//   parameters:
//      device:  0= GPS, 1= Slave Eagle boards, 2=Network switch
//      poweron: 0=poweroff, 1=poweron
void mcu_devicepower(int device, int poweron )
{
    mcu_cmd( 0x2e, 2, device, poweron );
}

// ?
int mcu_reset()
{
    static char cmd_reset[8]="\x06\x01\x00\x00\x02\xcc" ;
    if( mcu_send( cmd_reset ) ) {
        if( serial_dataready (30000000) ) {
            if( mcu_recv_enteracommand() ) {
                return 1;
            }
        }
    }
    return 0 ;
}

// update mcu firmware
// return 1: success, 0: failed
int mcu_update_firmware( char * firmwarefile) 
{
    int res = 0 ;
    FILE * fwfile ;
    int c ;
    int rd ;
    char responds[200] ;
    static char cmd_updatefirmware[8] = "\x05\x01\x00\x01\x02\xcc" ;
    
    fwfile=fopen(firmwarefile, "rb");
    
    if( fwfile==NULL ) {
        return 0 ;                  // failed, can't open firmware file
    }
    
    // 
    printf("Start update MCU firmware: %s\n", firmwarefile);
    
    // reset mcu
    printf("Reset MCU.\n");
    if( mcu_reset()==0 ) {              // reset failed.
        printf("Failed\n");
        return 0;
    }
    printf("Done.\n");
    
    // clean out serial buffer
    memset( responds, 0, sizeof(responds)) ;
    serial_write( responds, sizeof(responds));
    serial_clear(1000000);
    
    
    printf("Erasing.\n");
    rd=0 ;
    if( serial_write( cmd_updatefirmware, 5 ) ) {
        if( serial_dataready (10000000 ) ) {
            rd=serial_read_withtimeout ( responds, sizeof(responds), 500000 ) ;
        }
    }
    
    if( rd>=5 && 
       responds[rd-5]==0x05 && 
       responds[rd-4]==0x0 && 
       responds[rd-3]==0x01 && 
       responds[rd-2]==0x01 && 
       responds[rd-1]==0x03 ) {
       // ok ;
       printf("Done.\n");
       res=0 ;
   }
   else {
       printf("Failed.\n");
       return 0;                    // can't start fw update
   }
    
    printf("Programming.\n");
    // send firmware 
    res = 0 ;
    while( (c=fgetc( fwfile ))!=EOF ) {
        if( serial_write( &c, 1)!=1 ) 
            break;
        if( c=='\n' && serial_dataready (0) ) {
            rd = serial_read_withtimeout (responds, sizeof(responds), 200000 );
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
    
    // wait a bit (5 seconds), for firmware update done signal
    if( res==0 && serial_dataready(5000000) ) {
            rd = serial_read_withtimeout (responds, sizeof(responds), 500000 );
            if( rd>=5 &&
               responds[0]==0x05 && 
               responds[1]==0x0 && 
               responds[2]==0x01 && 
               responds[3]==0x03 && 
               responds[4]==0x02 ) {
               res=1 ;
            }
            else if( rd>=5 &&
               responds[rd-5]==0x05 && 
               responds[rd-4]==0x0 && 
               responds[rd-3]==0x01 && 
               responds[rd-2]==0x03 && 
               responds[rd-1]==0x02 ) {
               res=1 ;
            }
    }
    if( res ) {
        printf("Done.\n");
    }
    else {
        printf("Failed.\n");
    }
    return res ;
}

static int bcd(int v)
{
    return (((v/10)%10)*0x10 + v%10) ;
}

// return 1: success
//        0: failed
int mcu_w_rtc(time_t tt)
{
    struct tm t ;
    gmtime_r( &tt, &t);
    if( mcu_cmd( 0x07, 7, 
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

void mcu_setrtc()
{

    if( mcu_cmd( 0x07, 7, 
                bcd( p_dio_mmap->rtc_second ),
                bcd( p_dio_mmap->rtc_minute ),
                bcd( p_dio_mmap->rtc_hour ),
                bcd( 0 ),
                bcd( p_dio_mmap->rtc_day ),
                bcd( p_dio_mmap->rtc_month ),
                bcd( p_dio_mmap->rtc_year ) ) ) 
    {
        p_dio_mmap->rtc_cmd   = 0 ;	// cmd finish
        return ;
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
    int retry ;
    time_t rtc ;
    for( retry=0; retry<10; retry++ ) {
        rtc=mcu_r_rtc();
        if( (int)rtc>0 ) {
            tv.tv_sec=rtc ;
            tv.tv_usec=0;
            settimeofday(&tv, NULL);
            // also set onboard rtc
            rtc_set((time_t)tv.tv_sec);
            break;
        }
    }
}

void time_syncgps ()
{
    struct timeval tv ;
    int diff ;
    time_t gpstime ;
    if( p_dio_mmap->gps_valid ) {
        gpstime = (time_t) p_dio_mmap->gps_gpstime ;
        gettimeofday(&tv, NULL);
        diff = (int)gpstime - (int)tv.tv_sec ;
        if( diff>5 || diff<-5 ) {
            tv.tv_sec=gpstime ;
            tv.tv_usec= 0;
            settimeofday( &tv, NULL );
            dvr_log ( "Time sync use GPS time." );
        }
        else if( diff>1 || diff<-1 ) {
            tv.tv_sec =(time_t)diff ;
            tv.tv_usec = 0 ;
            adjtime( &tv, NULL );
        }
        // set mcu time
        mcu_w_rtc(gpstime) ;
        // also set onboard rtc
        rtc_set(gpstime);
    }
}

void time_syncmcu()
{
    int diff ;
    struct timeval tv ;
    time_t rtc ;
    rtc=mcu_r_rtc();
    if( (int)rtc>0 ) {
        gettimeofday(&tv, NULL);
        diff = (int)rtc - (int)tv.tv_sec ;
        if( diff>5 || diff<-5 ) {
            tv.tv_sec=rtc ;
            tv.tv_usec=0;
            settimeofday( &tv, NULL );
            dvr_log ( "Time sync use MCU time." );
        }
        else if( diff>1 || diff<-1 ){
            tv.tv_sec =(time_t)diff ;
            tv.tv_usec = 0 ;
            adjtime( &tv, NULL );
        }
        // set onboard rtc
        rtc_set(rtc);
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

// execute setnetwork script to recover network interface.
void setnetwork()
{
    int status ;
    static pid_t pid_network=0 ;
    if( pid_network>0 ) {
        kill(pid_network, SIGTERM);
        waitpid( pid_network, &status, 0 );
    }
    pid_network = fork ();
    if( pid_network==0 ) {
        static char snwpath[]="/davinci/dvr/setnetwork" ;
        execl(snwpath, snwpath, NULL);
        exit(0);    // should not return to here
    }
}

// bring up wifi adaptor
void wifi_up()
{
    static char wifi_up_script[]="/davinci/dvr/wifi_up" ;
    system( wifi_up_script );
    printf("\nWifi up\n");
}

// bring down wifi adaptor
void wifi_down()
{
    static char wifi_down_script[]="/davinci/dvr/wifi_down" ;
    system( wifi_down_script );
    printf("\nWifi down\n");
}

// Buzzer functions
static int buzzer_timer ;
static int buzzer_count ;
static int buzzer_on ;
static int buzzer_ontime ;
static int buzzer_offtime ;

void buzzer(int times, int ontime, int offtime)
{
    if( times>=buzzer_count ) {
        buzzer_timer = getruntime();
        buzzer_count = times ;
        buzzer_on = 0 ;
        buzzer_ontime = ontime ;
        buzzer_offtime = offtime ;
    }
}

// run buzzer, runtime: runtime in ms
void buzzer_run( int runtime )
{
    if( buzzer_count>0 && (runtime - buzzer_timer)>=0 ) {
        if( buzzer_on ) {
            buzzer_count-- ;
            buzzer_timer = runtime+buzzer_offtime ;
            p_dio_mmap->outputmap &= ~0x100 ;     // buzzer off
        }
        else {
            buzzer_timer = runtime+buzzer_ontime ;
            p_dio_mmap->outputmap |= 0x100 ;     // buzzer on
        }
        buzzer_on=!buzzer_on ;
    }
}    


int smartftp_retry ;
int smartftp_disable ;
int smartftp_reporterror ;

/*
void smartftp_log(char * fmt, ...)
{
    va_list ap ;
    char smlogfile[256] ;
    FILE * fsmlog ;
    
    struct tm t ;
	time_t tt ;
   
    smlogfile[0]=0 ;
    fsmlog = fopen("/var/dvr/dvrcurdisk", "r");
    if( fsmlog ) {
        fscanf(fsmlog, "%s", smlogfile );
        fclose(fsmlog);
    }
    if( smlogfile[0] ) {
        strcat( smlogfile, "/smartlog/smartftplog.txt" );
        fsmlog=fopen(smlogfile, "a");
        if( fsmlog ) {
            tt=time(NULL);
            localtime_r(&tt, &t);
            fprintf( fsmlog, "%04d-%02d-%02d %02d:%02d:%02d ", 
                    t.tm_year+1900, t.tm_mon+1, t.tm_mday ,
                    t.tm_hour, t.tm_min, t.tm_sec );
#ifdef MCU_DEBUG
            printf( "%04d-%02d-%02d %02d:%02d:%02d ", 
                    t.tm_year+1900, t.tm_mon+1, t.tm_mday ,
                    t.tm_hour, t.tm_min, t.tm_sec );
#endif            
            va_start(ap, fmt);
            vfprintf( fsmlog, fmt, ap );
#ifdef MCU_DEBUG
            vprintf( fmt, ap );
#endif            
            va_end(ap);
            fprintf( fsmlog, "\n" );
#ifdef MCU_DEBUG
            printf( "\n" );
#endif            
            fclose(fsmlog);
        }
    }
}
*/

void smartftp_start()
{

    dvr_log( "Start smart server uploading.");
    pid_smartftp = fork();
    if( pid_smartftp==0 ) {     // child process
        char hostname[128] ;
//        char mountdir[250] ;
        
        // get BUS name
        gethostname(hostname, 128) ;
        
        // get mount directory
/*        
        mountdir[0]=0 ;
        FILE * fcurdirname = fopen( "/var/dvr/dvrcurdisk", "r" );
        if( fcurdirname ) {
            fscanf( fcurdirname, "%s", mountdir );
            fclose( fcurdirname );
        }
        if( mountdir[0]==0 ) {
            exit(102);                  // no disk mounted. quit!
        }
*/        

//        system("/davinci/dvr/setnetwork");  // reset network, this would restart wifi adaptor
        wifi_up();                          // bring up wifi adaptor
        
        execl( "/davinci/dvr/smartftp", "/davinci/dvr/smartftp",
              "rausb0",
              hostname,
              "247SECURITY",
              "/var/dvr/disks",
              "510",
              "247ftp",
              "247SECURITY",
              "0",
              smartftp_reporterror?"Y":"N",
              getenv("TZ"), 
              NULL 
              );
        
//        smartftp_log( "Smartftp start failed." );

        dvr_log( "Start smart server uploading failed!");

        exit(101);      // error happened.
    }
    else if( pid_smartftp<0 ) {
        pid_smartftp=0;
    }
//    smartftp_log( "Smartftp started." );
}

int smartftp_wait()
{
    int smartftp_status=0 ;
    pid_t id ;
    int exitcode=-1 ;
    if( pid_smartftp>0 ) {
        id=waitpid( pid_smartftp, &smartftp_status, WNOHANG  );
        if( id==pid_smartftp ) {
            pid_smartftp=0 ;
#ifdef PWII_APP
            if( wifi_on == 0 ) {
                wifi_down();            // bring down wifi adaptor
            }
#endif            
            if( WIFEXITED( smartftp_status ) ) {
                exitcode = WEXITSTATUS( smartftp_status ) ;
                dvr_log( "\"smartftp\" exit. (code:%d)", exitcode );
                if( (exitcode==3 || exitcode==6) && 
                   --smartftp_retry>0 ) 
                {
                    dvr_log( "\"smartftp\" failed, retry...");
                    smartftp_start();
                }
            }
            else if( WIFSIGNALED(smartftp_status)) {
                dvr_log( "\"smartftp\" killed by signal %d.", WTERMSIG(smartftp_status));
            }
            else {
                dvr_log( "\"smartftp\" aborted." );
            }
        }
    }
    return pid_smartftp ;
}

// Kill smartftp in case standby timeout or power turn on
void smartftp_kill()
{
    if( pid_smartftp>0 ) {
        kill( pid_smartftp, SIGTERM );
        dvr_log( "Kill \"smartftp\"." );
        pid_smartftp=0 ;
#ifdef PWII_APP
        if( wifi_on == 0 ) {
            wifi_down();            // bring down wifi adaptor
        }
#endif            
    }
}    

int g_syncrtc=0 ;

float cpuusage()
{
    static float s_cputime=0.0, s_idletime=0.0 ;
    static float s_usage = 0.0 ;
    float cputime, idletime ;
    FILE * uptime ;
    uptime = fopen( "/proc/uptime", "r" );
    if( uptime ) {
        fscanf( uptime, "%f %f", &cputime, &idletime );
        fclose( uptime );
        if( cputime>s_cputime ) {
            s_usage=1.0 -(idletime-s_idletime)/(cputime-s_cputime) ;
            s_cputime=cputime ;
            s_idletime=idletime ;
        }
    }
    return s_usage ;
}

// return 
//        0 : failed
//        1 : success
int appinit()
{
    FILE * pidf ;
    int fd ;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    int i;
    
    // setup time zone
    string tz ;
    string tzi ;
    tz=dvrconfig.getvalue( "system", "timezone" );
    if( tz.length()>0 ) {
        tzi=dvrconfig.getvalue( "timezones", tz.getstring() );
        if( tzi.length()>0 ) {
            p=strchr(tzi.getstring(), ' ' ) ;
            if( p ) {
                *p=0;
            }
            p=strchr(tzi.getstring(), '\t' ) ;
            if( p ) {
                *p=0;
            }
            setenv("TZ", tzi.getstring(), 1);
        }
        else {
            setenv("TZ", tz.getstring(), 1);
        }
    }

    v = dvrconfig.getvalue( "system", "logfile");
    if (v.length() > 0){
        strncpy( logfile, v.getstring(), sizeof(logfile));
    }
    v = dvrconfig.getvalue( "system", "temp_logfile");
    if (v.length() > 0){
        strncpy( temp_logfile, v.getstring(), sizeof(temp_logfile));
    }
        
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

    if( p_dio_mmap->iopid > 0 ) { 				// another ioprocess running?
        pid_t pid = p_dio_mmap->iopid ;
        // kill it
        if( kill( pid,  SIGTERM )==0 ) {
            // wait for it to quit
            waitpid( pid, NULL, 0 );
            sync();
            sleep(2);
        }
        else {
            sleep(5);
        }
    }

    // initialize shared memory
    p_dio_mmap->inputnum = dvrconfig.getvalueint( "io", "inputnum");	
    if( p_dio_mmap->inputnum<=0 || p_dio_mmap->inputnum>32 )
        p_dio_mmap->inputnum = MCU_INPUTNUM ;	
    p_dio_mmap->outputnum = dvrconfig.getvalueint( "io", "outputnum");	
    if( p_dio_mmap->outputnum<=0 || p_dio_mmap->outputnum>32 )
        p_dio_mmap->outputnum = MCU_INPUTNUM ;	

    p_dio_mmap->panel_led = 0;
    p_dio_mmap->devicepower = 0xffff;				// assume all device is power on
    
    p_dio_mmap->iopid = getpid () ;					// io process pid
    
    output_inverted = 0 ;
    
    for( i=0; i<32 ; i++ ) {
        char outinvkey[50] ;
        sprintf(outinvkey, "output%d_inverted", i+1);
        if( dvrconfig.getvalueint( "io", outinvkey ) ) {
            output_inverted |= (1<<i) ;
        }
    }
    
    // check if sync rtc wanted
    
    g_syncrtc = dvrconfig.getvalueint("io", "syncrtc");
    
    // initilize serial port
    v = dvrconfig.getvalue( "io", "serialport");
    if( v.length()>0 ) {
        strcpy( serial_dev, v.getstring() );
    }
    serial_baud = dvrconfig.getvalueint("io", "serialbaudrate");
    if( serial_baud<2400 || serial_baud>115200 ) {
            serial_baud=DEFSERIALBAUD ;
    }
    serial_init ();

    // smartftp variable
    smartftp_disable = dvrconfig.getvalueint("system", "smartftp_disable");

    // turn on wifi ?
    wifi_on = dvrconfig.getvalueint("system", "wifi_on" );
    if( wifi_on ) {
        wifi_up();
    }
        
    // initialize mcu (power processor)
    if( mcu_bootupready () ) {
        printf("MCU UP!\n");
        dvr_log("MCU ready.");
        mcu_readcode();
        // get MCU version
        if( mcu_version( mcu_firmware_version ) ) {
            printf("MCU: %s\n", mcu_firmware_version );
            FILE * mcuversionfile=fopen("/var/dvr/mcuversion", "w");
            if( mcuversionfile ) {
                fprintf( mcuversionfile, "%s", mcu_firmware_version );
                fclose( mcuversionfile );
            }
        }        
    }
    else {
        printf("MCU failed!\n");
        dvr_log("MCU failed.");
    }

#ifdef PWII_APP
    if( mcu_pwii_bootupready() ) {
        printf( "PWII boot up ready!\n");
        // get MCU version
        char pwii_version[100] ;
        if( mcu_pwii_version( pwii_version ) ) {
            printf("PWII version: %s\n", pwii_version );
            FILE * pwiiversionfile=fopen("/var/dvr/pwiiversion", "w");
            if( pwiiversionfile ) {
                fprintf( pwiiversionfile, "%s", pwii_version );
                fclose( pwiiversionfile );
            }
        }     
        
    }

    mcu_pwii_init();    // init pwii variables
    
    pwii_front_ch = dvrconfig.getvalueint( "pwii", "front");
    pwii_rear_ch = dvrconfig.getvalueint( "pwii", "rear");
    
#endif // PWII_APP   
    
    if( g_syncrtc ) {
        mcu_rtctosys();
    }
    
    // gforce sensor setup

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
    p_dio_mmap->gforce_log0=0;
    p_dio_mmap->gforce_log1=0;
    
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
    
    // gsensor trigger setup
    if( mcu_gsensorinit() ) {       // g sensor available
        dvr_log("G sensor detected!");
        pidf = fopen( "/var/dvr/gsensor", "w" );
        if( pidf ) {
            fprintf(pidf, "1");
            fclose(pidf);
        }
    }
    
    usewatchdog = dvrconfig.getvalueint("io", "usewatchdog");
    watchdogtimeout=dvrconfig.getvalueint("io", "watchdogtimeout");
    if( watchdogtimeout<10 )
        watchdogtimeout=10 ;
    if( watchdogtimeout>200 )
        watchdogtimeout=200 ;

    hd_timeout = dvrconfig.getvalueint("io", "hdtimeout");
    if( hd_timeout<=0 || hd_timeout>3000 ) {
        hd_timeout=60 ;             // default timeout 60 seconds
    }

    pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }

    // initialize timer
    initruntime();
    
    return 1;
}

// app finish, clean up
void appfinish()
{
    int lastuser ;

    if( watchdogenabled ) {
        mcu_watchdogdisable();
    }
    
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

int main(int argc, char * argv[])
{
    int i;
    int hdpower=0 ;                             // assume HD power is off
    int hdkeybounce = 0 ;                       // assume no hd
    unsigned int modeendtime = 0;
    unsigned int gpsavailable = 0 ;
    
    if( appinit()==0 ) {
        return 1;
    }

//    signal(SIGUSR1, sig_handler);
//    signal(SIGUSR2, sig_handler);
    
    // disable watchdog at begining
//  mcu_watchdogdisable();

    // check if we need to update firmware
    if( argc>=2 ) {
        for(i=1; i<argc; i++ ) {
            if( strcasecmp( argv[i], "-fw" )==0 ) {
                if( argv[i+1] && mcu_update_firmware( argv[i+1] ) ) {
                    appfinish ();
                    return 0 ;
                }
                else {
                    appfinish ();
                    return 1;
                }
            }
            else if( strcasecmp( argv[i], "-reboot" )==0 ) {
                int delay = 5 ;
                if( argv[i+1] ) {
                    delay = atoi(argv[i+1]) ;
                }
                if( delay<5 ) {
                    delay=5 ;
                }
                else if(delay>100 ) {
                    delay=100;
                }
//                sleep(delay);
//                mcu_reboot();
                watchdogtimeout=delay ;
                mcu_watchdogenable () ;
                sleep(delay+20) ;
                mcu_reboot ();
                return 1;
            }
        }
    }
  
    // setup signal handler	
    app_mode = 1 ;
    signal(SIGQUIT, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR2, sig_handler);
    
    printf( "DVR IO process started!\n");
    
   
    // get default digital input
    mcu_dinput();

    // initialize output
    mcu_doutputmap=~(p_dio_mmap->outputmap) ;
    
    // initialize panel led
    panelled = 0xff ;
    p_dio_mmap->panel_led = 0;
    
    // initialize device power
    devicepower = 0 ;
    p_dio_mmap->devicepower = 0xffff;				// assume all device is power on
    
    while( app_mode ) {

        // do input pin polling
        mcu_input(5000);

        // do digital output
        mcu_doutput();

#ifdef PWII_APP
        // update PWII outputs
        mcu_pwii_output();
#endif            

        unsigned int runtime=getruntime() ;
        
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

        static unsigned int cpuusage_timer ;
        if( (runtime - cpuusage_timer)> 5000 ) {    // 5 seconds to monitor cpu usage
            cpuusage_timer=runtime ;
            static int usage_counter=0 ;
            if( cpuusage()>0.995 ) {
                if( ++usage_counter>12 ) {          // CPU keey busy for 1 minites
                    buzzer( 10, 250, 250 );
                    // let system reset by watchdog
                    dvr_log( "CPU usage at 100%% for 60 seconds, system reset.");
                    app_mode=APPMODE_REBOOT ;
                }
            }
            else {
                usage_counter=0 ;
            }
        }


        // Buzzer functions
        buzzer_run( runtime ) ;
        
        static unsigned int temperature_timer ;
        if( (runtime - temperature_timer)> 10000 ) {    // 10 seconds to read temperature
            temperature_timer=runtime ;

            i=mcu_iotemperature();
#ifdef MCU_DEBUG        
            printf("Temperature: io(%d)",i );
#endif
            if( i > -127 && i < 127 ) {
                static int saveiotemp = 0 ;
                if( i>=75 && 
                   (i-saveiotemp) > 2 ) 
                {
                    dvr_log ( "High system temperature: %d", i);
                    saveiotemp=i ;
                    sync();
                }
                
                p_dio_mmap->iotemperature = i ;
            }
            else {
                p_dio_mmap->iotemperature=-128 ;
            }
            
            i=mcu_hdtemperature();
#ifdef MCU_DEBUG        
            printf(" hd(%d)\n", i );
#endif
            if( i > -127 && i < 127 ) {
                static int savehdtemp = 0 ;

                if( i>=75 && 
                   (i-savehdtemp) > 2 ) 
                {
                    dvr_log ( "High hard disk temperature: %d", i);
                    savehdtemp=i ;
                    sync();
                }

                p_dio_mmap->hdtemperature = i ;
            }
            else {
                p_dio_mmap->hdtemperature=-128 ;
            }
            
            // The Buzzer beep 4time at 0.5s interval every 30s when HDD or system temperature is over 66C
            if( p_dio_mmap->iotemperature > 66 || p_dio_mmap->hdtemperature > 66 ) {
                static int hitempbeep=0 ;
                if( ++hitempbeep>3 ) {
                    buzzer( 4, 250, 250 ) ;
                    hitempbeep=0 ;
                }
            }
            
            static int hightemp_norecord = 0 ;
            if( p_dio_mmap->iotemperature > 83 || p_dio_mmap->hdtemperature > 83 ) {
                if( hightemp_norecord == 0 &&
                   app_mode==APPMODE_RUN &&                      
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrcmd == 0 ) &&
                   ((p_dio_mmap->dvrstatus & DVR_RECORD)!=0) )
                {
                    dvr_log( "Stop DVR recording on high temperature."); 
                    p_dio_mmap->dvrcmd = 3;     // stop recording
                    hightemp_norecord = 1 ;
                }
            }
            else if(  p_dio_mmap->iotemperature < 75 && p_dio_mmap->hdtemperature < 75 ) {
                if( hightemp_norecord == 1 &&
                   app_mode==APPMODE_RUN &&                      
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrcmd == 0 ) &&
                   ((p_dio_mmap->dvrstatus & DVR_RECORD)==0) )
                {
                    p_dio_mmap->dvrcmd = 4;     // start recording
                    hightemp_norecord = 0 ;
                    dvr_log( "Resume DVR recording on lower temperature.");
                }
            }
        }
        
                
        static unsigned int appmode_timer ;
        if( (runtime - appmode_timer)> 3000 ) {    // 3 seconds mode timer
            appmode_timer=runtime ;

            // printf("mode %d\n", app_mode);
            // do power management mode switching
            if( app_mode==APPMODE_RUN ) {                         // running mode
                static int app_run_bell = 0 ;
                if( app_run_bell==0 &&
                   p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrstatus & DVR_RECORD) ) 
                {
                    app_run_bell=1 ;
                    buzzer(1, 1000, 100);
                }
                
                if( p_dio_mmap->poweroff )              // ignition off detected
                {
                    modeendtime = runtime + getshutdowndelaytime()*1000;
                    app_mode=APPMODE_SHUTDOWNDELAY ;                        // hutdowndelay start ;
                    dvr_log("Power off switch, enter shutdown delay (mode %d).", app_mode);
                }
            }
            else if( app_mode==APPMODE_SHUTDOWNDELAY ) {   // shutdown delay
                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay() ;
                    if( runtime>modeendtime  ) {
                        // stop dvr recording
                        if( p_dio_mmap->dvrpid>0 && 
                           (p_dio_mmap->dvrstatus & DVR_RUN) &&
                           (p_dio_mmap->dvrcmd == 0 )) 
                        {
                                p_dio_mmap->dvrcmd = 3;     // stop recording
                        }
                        sync();
                        // stop glog recording
                        if( p_dio_mmap->glogpid>0 ) {
                            kill( p_dio_mmap->glogpid, SIGUSR1 );
                        }
                        modeendtime = runtime + 120000 ;
                        app_mode=APPMODE_NORECORD ;                    // start standby mode
                        p_dio_mmap->iobusy = 1 ;
                        dvr_log("Shutdown delay timeout, to stop recording (mode %d).", app_mode);
                    }
                }
                else {
                    // start dvr recording
                    if( p_dio_mmap->dvrpid>0 && 
                       (p_dio_mmap->dvrstatus & DVR_RUN) &&
                       (p_dio_mmap->dvrcmd == 0 )) 
                    {
                        p_dio_mmap->dvrcmd = 4;     // start recording
                    }
                    p_dio_mmap->devicepower = 0xffff ;
                    app_mode=APPMODE_RUN ;                        // back to normal
                    dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);
                }
            }
            else if( app_mode==APPMODE_NORECORD ) {  // close recording and run smart ftp

                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();
                }

                if( p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   ((p_dio_mmap->dvrstatus & DVR_RECORD)==0) ) 
                {
                        dvr_log("Recording stopped.");
                        modeendtime = runtime ;        
                }
                sync();
                
                if( runtime>=modeendtime ) {
                    app_mode = APPMODE_STANDBY ;
#ifdef PWII_APP                    
                    // standby pwii
                    p_dio_mmap->pwii_output &= ~0x800 ;         // LCD off
                    p_dio_mmap->pwii_output |= 0x1000 ;         // STANDBY mode
#endif                    
                    modeendtime = runtime+getstandbytime()*1000 ;
                    if( smartftp_disable==0 ) {
                        
                        // check video lost report to smartftp.
                        if( (p_dio_mmap->dvrstatus & (DVR_VIDEOLOST|DVR_ERROR) )==0 )
                        {
                            smartftp_reporterror = 0 ;
                        }
                        else {
                            smartftp_reporterror=1 ;
                        }
                        smartftp_retry=3 ;
                        smartftp_start();
                    }
                    dvr_log("Enter standby mode. (mode %d).", app_mode);
                    buzzer( 3, 250, 250);
                    
                    // if standby time is 0, we bypass it, just turn off system.
                    if( modeendtime <= runtime ) {
                        modeendtime = runtime+90000 ;
                        app_mode=APPMODE_SHUTDOWN ;    // turn off mode
                        dvr_log("System shutdown. (mode %d).", app_mode );
                        buzzer( 5, 250, 250 );
                        if( p_dio_mmap->dvrpid > 0 ) {
                            kill(p_dio_mmap->dvrpid, SIGTERM);
                        }
                        if( p_dio_mmap->glogpid > 0 ) {
                            kill(p_dio_mmap->glogpid, SIGTERM);
                        }
                    }
                }
            }
            else if( app_mode==APPMODE_STANDBY ) {                    // standby

                p_dio_mmap->outputmap ^= HDLED ;        // flash LED slowly for standy mode

                if( p_dio_mmap->poweroff ) {
                    mcu_poweroffdelay ();
                }
                
                if( p_dio_mmap->dvrpid>0 && 
                   (p_dio_mmap->dvrstatus & DVR_RUN) &&
                   (p_dio_mmap->dvrstatus & DVR_NETWORK) )
                {
                    p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
                }
                else {
                    p_dio_mmap->devicepower=0 ;        // turn off all devices power
                }

                // continue wait for smartftp
                if( pid_smartftp > 0 ) {
                    if( smartftp_wait()==0 ) {
                        p_dio_mmap->iobusy = 0 ;
                    }
                }
                
                if( p_dio_mmap->poweroff ) {         // ignition off
                    // turn off HD power ?
                    if( standbyhdoff ) {
                        p_dio_mmap->outputmap &= ~HDLED ;   // turn off HDLED
                        if( hdpower ) {
                            hdpower=0;
                            mcu_hdpoweroff ();
                        }
                    }
                    
                    if( runtime>modeendtime  ) {
                        // start shutdown
                        p_dio_mmap->devicepower=0 ;    // turn off all devices power
                        modeendtime = runtime+90000 ;
                        app_mode=APPMODE_SHUTDOWN ;    // turn off mode
                        p_dio_mmap->iobusy = 1 ;
                        dvr_log("Standby timeout, system shutdown. (mode %d).", app_mode );
                        buzzer( 5, 250, 250 );
                        if( p_dio_mmap->dvrpid > 0 ) {
                            kill(p_dio_mmap->dvrpid, SIGTERM);
                        }
                        if( p_dio_mmap->glogpid > 0 ) {
                            kill(p_dio_mmap->glogpid, SIGTERM);
                        }
                        if( pid_smartftp > 0 ) {
                            smartftp_kill();
                        }
                    }
                }
                else {                                          // ignition on
                    if( p_dio_mmap->dvrpid>0 && 
                       (p_dio_mmap->dvrstatus & DVR_RUN) &&
                       p_dio_mmap->dvrcmd == 0 )
                    {
                        p_dio_mmap->dvrcmd = 4 ;             // start recording.
                    }
                    
                    // we should start glog, since it'v been killed.
                    if( p_dio_mmap->glogpid > 0 ) {
                        kill(p_dio_mmap->glogpid, SIGUSR2);
                    }
                    
                    p_dio_mmap->devicepower=0xffff ;    // turn on all devices power
                    app_mode=APPMODE_RUN ;                        // back to normal
                    p_dio_mmap->iobusy = 0 ;
#ifdef PWII_APP
                    // pwii jump out of standby
                    p_dio_mmap->pwii_output |= 0x800 ;      // LCD on                   
                    p_dio_mmap->pwii_output &= ~0x1000 ;    // standby off
#endif                    
                    dvr_log("Power on switch, set to running mode. (mode %d)", app_mode);
                    if( pid_smartftp > 0 ) {
                        smartftp_kill();
                    }
                }
            }
            else if( app_mode==APPMODE_SHUTDOWN ) {                    // turn off mode, no keep power on 
                sync();
                if( runtime>modeendtime ) {     // system suppose to turn off during these time
                    // hard ware turn off failed. May be ignition turn on again? Reboot the system
                    dvr_log("Hardware shutdown failed. Try reboot by software!" );
                    app_mode=APPMODE_REBOOT ;
                }
            }
            else if( app_mode==APPMODE_REBOOT ) {
                static int reboot_begin=0 ;
                sync();
                if( reboot_begin==0 ) {
                    if( p_dio_mmap->dvrpid > 0 ) {
                        kill(p_dio_mmap->dvrpid, SIGTERM);
                    }
                    if( p_dio_mmap->glogpid > 0 ) {
                        kill(p_dio_mmap->glogpid, SIGTERM);
                    }
                    // let MCU watchdog kick in to reboot the system
                    watchdogtimeout=10 ;                    // 10 seconds watchdog time out
                    mcu_watchdogenable() ;
                    usewatchdog=0 ;                         // don't call watchdogenable again!
                    watchdogenabled=0 ;                     // don't kick watchdog ;
                    reboot_begin=1 ;                        // rebooting in process
                    modeendtime=runtime+20000 ;             // 20 seconds time out, watchdog should kick in already!
                }
                else if( runtime>modeendtime ) { 
                    // program should not go throught here,
                    mcu_reboot ();
                    system("/bin/reboot");                  // do software reboot
                    app_mode=APPMODE_QUIT ;                 // quit IOPROCESS
                }
            }
            else if( app_mode==APPMODE_REINITAPP ) {
                dvr_log("IO re-initialize.");
                appfinish();
                appinit();
                app_mode = APPMODE_RUN ;
                p_dio_mmap->iobusy = 0 ;
            }
            else if( app_mode==APPMODE_QUIT ) {
                break ;
            }
            else {
                // no good, enter undefined mode 
                dvr_log("Error! Enter undefined mode %d.", app_mode );
                app_mode = APPMODE_RUN ;              // we retry from mode 1
            }

            // updating watch dog state
            if( usewatchdog && watchdogenabled==0 ) {
                mcu_watchdogenable();
            }

            // kick watchdog
            if( watchdogenabled )
                mcu_watchdogkick () ;

            // DVRSVR watchdog running?
            if( p_dio_mmap->dvrpid>0 && 
               p_dio_mmap->dvrwatchdog >= 0 )
            {      	
                ++(p_dio_mmap->dvrwatchdog) ;
                if( p_dio_mmap->dvrwatchdog == 50 ) {	// DVRSVR dead for 2.5 min?
                    if( kill( p_dio_mmap->dvrpid, SIGUSR2 )!=0 ) {
                        // dvrsvr may be dead already, pid not exist.
                        p_dio_mmap->dvrwatchdog = 110 ;
                    }
                    else {
                        dvr_log( "Dvr watchdog failed, try restart DVR!!!");
                    }
                }
                else if( p_dio_mmap->dvrwatchdog > 110 ) {	
                    buzzer( 10, 250, 250 );
                    dvr_log( "Dvr watchdog failed, system reboot.");
                    app_mode=APPMODE_REBOOT ;
                    p_dio_mmap->dvrwatchdog=1 ;
                }
                
                static int nodatawatchdog = 0;
                if( (p_dio_mmap->dvrstatus & DVR_RUN) && 
                   (p_dio_mmap->dvrstatus & DVR_NODATA ) &&     // some camera not data in
                   app_mode==APPMODE_RUN )
                {
                    if( ++nodatawatchdog > 20 ) {   // 1 minute
                        buzzer( 10, 250, 250 );
                        // let system reset by watchdog
                        dvr_log( "One or more camera not working, system reset.");
                        app_mode=APPMODE_REBOOT ;
                    }
                }
                else 
                {
                    nodatawatchdog = 0 ;
                }
            }
        }

        static unsigned int hd_timer ;
        if( (runtime - hd_timer)> 500 ) {    // 0.5 seconds to check hard drive
            hd_timer=runtime ;
            
            // check HD plug-in state
            if( hdlock && hdkeybounce<10 ) {          // HD lock on
                if( hdpower==0 ) {
                    // turn on HD power
                    mcu_hdpoweron() ;
                }
                hdkeybounce = 0 ;
               
                if( hdinserted &&                      // hard drive inserted
                    p_dio_mmap->dvrpid > 0 &&
                    p_dio_mmap->dvrwatchdog >= 0 &&
                    (p_dio_mmap->dvrstatus & DVR_RUN) &&
                    (p_dio_mmap->dvrstatus & DVR_DISKREADY )==0 )        // dvrsvr is running but no disk detected
                {
                    p_dio_mmap->outputmap ^= HDLED ;
                    
                    if( ++hdpower>hd_timeout*2 )                // 50 seconds
                    {
                        dvr_log("Hard drive failed, system reset!");
                        buzzer( 10, 250, 250 );

                        // turn off HD led
                        p_dio_mmap->outputmap &= ~HDLED ;
                        mcu_hdpoweroff();                   // FOR NEW MCU, THIS WOULD TURN OFF SYSTEM POWER?!
                        hdpower=0;
                    }
                }
                else {
                    // turn on HD led
                    if( app_mode < APPMODE_NORECORD ) {
                        if( hdinserted ) {
                            p_dio_mmap->outputmap |= HDLED ;
                        }
                        else {
                            p_dio_mmap->outputmap &= ~HDLED ;
                        }
                    }
                    hdpower=1 ;
                }
            }
            else {                  // HD lock off
                static pid_t hd_dvrpid_save ;
                p_dio_mmap->outputmap ^= HDLED ;                    // flash led
                if( hdkeybounce<10 ) {                          // wait for key lock stable
                    hdkeybounce++ ;
                    hd_dvrpid_save=0 ;
                }
                else if( hdkeybounce==10 ){
                    hdkeybounce++ ;
                    // suspend DVRSVR process
                    if( p_dio_mmap->dvrpid>0 ) {
                        hd_dvrpid_save=p_dio_mmap->dvrpid ;
                        kill(hd_dvrpid_save, SIGUSR1);         // request dvrsvr to turn down
                        // turn on HD LED
                        p_dio_mmap->outputmap |= HDLED ;
                    }
                }
                else if( hdkeybounce==11 ) {
                    if( p_dio_mmap->dvrpid <= 0 )  {
                        hdkeybounce++ ;
                        sync();
                        // umount disks
                        system("/davinci/dvr/umountdisks") ;
                    }
                }
                else if( hdkeybounce==12 ) {
                    // do turn off hd power
                    sync();
                    mcu_hdpoweroff();       // FOR NEW MCU, THIS WOULD TURN OFF SYSTEM POWER?!
                    hdkeybounce++;
                }
                else if( hdkeybounce<20 ) {
                    hdkeybounce++;
                }
                else if( hdkeybounce==20 ) {
                    hdkeybounce++;
                    if( hd_dvrpid_save > 0 ) {
                        kill(hd_dvrpid_save, SIGUSR2);              // resume dvrsvr
                        hd_dvrpid_save=0 ;
                    }
                    // turn off HD LED
                    p_dio_mmap->outputmap &= ~ HDLED ;

                    // if the board survive (force on cable), we have a new IP address
                    system("ifconfig eth0:247 2.4.7.247") ;
                    
                }
                else {
                    if( hdlock ) {
                        hdkeybounce=0 ;
                    }
                }
            }               // End HD key off
        }

        static unsigned int panel_timer ;
        if( (runtime - panel_timer)> 2000 ) {    // 2 seconds to update pannel
            panel_timer=runtime ;
            unsigned int panled = p_dio_mmap->panel_led ;
            if( p_dio_mmap->dvrpid> 0 && (p_dio_mmap->dvrstatus & DVR_RUN) ) {
                static int videolostbell=0 ;
                if( (p_dio_mmap->dvrstatus & DVR_VIDEOLOST)!=0 ) {
                    panled|=4 ;
                    if( videolostbell==0 ) {
                        buzzer( 5, 500, 500 );
                        videolostbell=1;
                    }
                }
                else {
                    videolostbell=0 ;
                }
            }
            
            if( p_dio_mmap->dvrpid>0 && (p_dio_mmap->dvrstatus & DVR_NODATA)!=0 && app_mode==APPMODE_RUN ) {
                panled|=2 ;     // error led
            }
            
            // update panel LEDs
            if( panelled != panled ) {
                for( i=0; i<PANELLEDNUM; i++ ) {
                    if( (panelled ^ panled) & (1<<i) ) {
                        mcu_led(i, panled & (1<<i) );
                    }
                }
                panelled = panled ;
            }

#ifdef PWII_APP
            // BIT 3: ERROR LED
            if( panelled & 2 ) {
                p_dio_mmap->pwii_output |= 8 ;
            }
            else {
                p_dio_mmap->pwii_output &= ~8 ;
            }
#endif
            
            // update device power
            if( devicepower != p_dio_mmap->devicepower ) {
                for( i=0; i<DEVICEPOWERNUM; i++ ) {
                    if( (devicepower ^ p_dio_mmap->devicepower) & (1<<i) ) {
                        mcu_devicepower(i, p_dio_mmap->devicepower & (1<<i) );
                    }
                }
                devicepower = p_dio_mmap->devicepower ;
                
                // reset network interface, some how network interface dead after device power changes
//                setnetwork();
            }
        }
        
        // adjust system time with RTC
        static unsigned int adjtime_timer ;
        if( gpsvalid != p_dio_mmap->gps_valid ) {
            gpsvalid = p_dio_mmap->gps_valid ;
            if( gpsvalid ) {
                time_syncgps () ;
                adjtime_timer=runtime ;
                gpsavailable = 1 ;
            }
        }
        if( (runtime - adjtime_timer)> 600000 ||
            (runtime < adjtime_timer) )
        {    // call adjust time every 10 minute
            if( g_syncrtc ) {
                if( gpsavailable ) {
                    if( gpsvalid ) {
                        time_syncgps();
                        adjtime_timer=runtime ;
                    }
                }
                else {
                    time_syncmcu();
                    adjtime_timer=runtime ;
                }
            }
        }
    }
    
    appfinish();
    
    printf( "DVR IO process ended!\n");
    return 0;
}

