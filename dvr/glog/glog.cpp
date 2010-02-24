
// GPS LOGING 


#include <sys/mman.h>
#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <memory.h>
#include <dirent.h>
#include <pthread.h>
#include <termios.h>
#include <stdarg.h>
#include <pthread.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../dvrsvr/eagle32/davinci_sdk.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "../ioprocess/diomap.h"

#include "../cfg.h"

struct baud_table_t {
	speed_t baudv ;
	int baudrate ;
} baud_table[] = {
	{ B230400,	230400},
	{ B115200,	115200},
	{ B57600,	57600},
	{ B38400,	38400},
	{ B19200,	19200},
	{ B9600,	9600},
	{ B4800,	4800},
	{ B2400, 	2400},
	{ B1800,    1800},
	{ B1200, 	1200},
	{ B600,     600},
	{ B0,       0}
} ;

#define SPEEDTABLESIZE (10)

struct speed_table_t {
    float  speed ;
    float  distance ;
} speed_table [SPEEDTABLESIZE] ;

// max distance in degrees
float max_distance=500 ;
// converting parameter
float degree1km = 0.012 ;

struct dio_mmap * p_dio_mmap ;

char dvriomap[256] = "/var/dvr/dvriomap" ;
char gpslogfilename[512] ;
int glog_poweroff=1 ;           // start from power off
string dvrcurdisk;
int glog_ok=0 ;                 // logging success.

int gps_port_disable=0 ;
int gps_port_handle = -1;
char gps_port_dev[256] = "/dev/ttyS0" ;
int gps_port_baudrate = 4800 ;      // gps default baud
int gps_fifofile ;               // gps data debugging fifo (pipe file)

int gforce_log_enable = 0 ;
int tab102b_enable = 0 ;
int tab102b_port_handle = -1;
char tab102b_port_dev[256] = "/dev/ttyUSB1" ;
int tab102b_port_baudrate = 19200 ;     // tab102b default baud
int tab102b_data_enable=0 ;

int inputdebounce=3 ;

#ifdef MCU_DEBUG
int mcu_debug_out ;
#endif

#define MAXSENSORNUM    (32)

string sensorname[MAXSENSORNUM] ;
int    sensor_invert[MAXSENSORNUM] ;
int    sensor_value[MAXSENSORNUM] ;
double sensorbouncetime[MAXSENSORNUM] ;
double sensorbouncevalue[MAXSENSORNUM] ;

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;
char * pidfile = "/var/dvr/glog.pid" ;

int app_state ;     // 0: quit, 1: running: 2: restart

unsigned int sigcap = 0 ;

char disksroot[128] = "/var/dvr/disks" ;

class dir_find {
    protected:
        DIR * m_pdir ;
        struct dirent * m_pent ;
        struct dirent m_entry ;
        char m_dirname[PATH_MAX] ;
        char m_pathname[PATH_MAX] ;
        struct stat m_statbuf ;
    public:
        dir_find() {
            m_pdir=NULL ;
        }

        // close dir handle
        void close() {
            if( m_pdir ) {
                closedir( m_pdir );
                m_pdir=NULL ;
            }
        }
        // open an dir for reading
        void open( const char * path ) {
            int l ;
            close();
            m_pent=NULL ;
            strncpy( m_dirname, path, sizeof(m_dirname) );
            l=strlen( m_dirname ) ;
            if( l>0 ) {
                if( m_dirname[l-1]!='/' ) {
                    m_dirname[l]='/' ;
                    m_dirname[l+1]='\0';
                }
            }
            m_pdir = opendir(m_dirname);
        }
        ~dir_find() {
            close();
        }
        dir_find( const char * path ) {
            m_pdir=NULL;
            open( path );
        }
        int isopen(){
            return m_pdir!=NULL ;
        }
        // find directory.
        // return 1: success
        //        0: end of file. (or error)
        int find() {
            struct stat findstat ;
            if( m_pdir ) {
                while( readdir_r( m_pdir, &m_entry, &m_pent)==0  ) {
                    if( m_pent==NULL ) {
                        break;
                    }
                    if( (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='\0') || 
                       (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='.' && m_pent->d_name[2]=='\0') ) {
                           continue ;
                       }
                    if( m_pent->d_type == DT_UNKNOWN ) {                   // d_type not available
                        strcpy( m_pathname, m_dirname );
                        strcat( m_pathname, m_pent->d_name );
                        if( stat( m_pathname, &findstat )==0 ) {
                            if( S_ISDIR(findstat.st_mode) ) {
                                m_pent->d_type = DT_DIR ;
                            }
                            else if( S_ISREG(findstat.st_mode) ) {
                                m_pent->d_type = DT_REG ;
                            }
                        }
                    }
                    return 1 ;
                }
            }
            m_pent=NULL ;
            return 0 ;
        }
        char * pathname()  {
            if(m_pent) {
                strcpy( m_pathname, m_dirname );
                strcat( m_pathname, m_pent->d_name );
                return (m_pathname) ;
            }
            return NULL ;
        }
        char * filename() {
            if(m_pent) {
                return (m_pent->d_name) ;
            }
            return NULL ;
        }
        
        // check if found a dir
        int    isdir() {
            if( m_pent ) {
                return (m_pent->d_type == DT_DIR) ;
            }
            else {
                return 0;
            }
        }
        
        // check if found a regular file
        int    isfile(){
            if( m_pent ) {
                return (m_pent->d_type == DT_REG) ;
            }
            else {
                return 0;
            }
        }
        
        // return file stat
        struct stat * filestat(){
            char * path = pathname();
            if( path ) {
                if( stat( path, &m_statbuf )==0 ) {
                    return &m_statbuf ;
                }
            }
            return NULL ;
        }
};

void sig_handler(int signum)
{
    if( signum == SIGUSR1 ) {
        sigcap |= 1 ;
    }
    else if( signum == SIGUSR2 ) {
        sigcap |= 2 ;
    }
    else if( signum == SIGPIPE ) {
        sigcap |= 3 ;
    }
    else {
        sigcap |= 0x8000 ;
    }
}

// pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;

static pthread_mutex_t log_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;

void log_lock()
{
    pthread_mutex_lock(&log_mutex);
}

void log_unlock()
{
    pthread_mutex_unlock(&log_mutex);
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
        sched_yield();      // or sleep(0)
    }
}

void dio_unlock()
{
    p_dio_mmap->lock=0;
}

struct buzzer_t_type {
    int times ;
    int ontime ;
    int offtime ;
} ;

void * buzzer_thread(void * p)
{
    struct buzzer_t_type * pbuzzer = (struct buzzer_t_type *)p ;
    while( pbuzzer->times-- ) {
        p_dio_mmap->outputmap |= 0x100 ;     // buzzer on
        usleep( pbuzzer->ontime*1000 ) ;
        p_dio_mmap->outputmap &= (~0x100) ;     // buzzer off
        usleep( pbuzzer->offtime*1000 ) ;
    }
    delete pbuzzer ;
    return NULL ;
}

void buzzer(int times, int ontime, int offtime)
{
    pthread_t buzzerthread ;
    struct buzzer_t_type * pbuzzer = new struct buzzer_t_type ;
    pbuzzer->times=times ;
    pbuzzer->ontime=ontime ;
    pbuzzer->offtime=offtime ;
    if( pthread_create( &buzzerthread, NULL, buzzer_thread, (void *)pbuzzer )==0 ) {
        pthread_detach( buzzerthread );
    }
    else {
        delete pbuzzer ;
    }
}

int serial_open(int * handle, char * serial_dev, int serial_baudrate )
{
	int i;
    int serial_handle = *handle ;
	if( serial_handle >= 0 ) {
		return serial_handle ;		// opened already
	}
    
    // check if serial device is stdin ?
    struct stat stdinstat ;
    struct stat devstat ;
    int r1, r2 ;
    r1 = fstat( 0, &stdinstat ) ; // get stdin stat
    r2 = stat( serial_dev, &devstat ) ;
    
    if( r1==0 && r2==0 && stdinstat.st_dev == devstat.st_dev && stdinstat.st_ino == devstat.st_ino ) { // matched
        printf("Stdin match serail port!\n");
        serial_handle=0 ;
 		fcntl(serial_handle, F_SETFL, O_RDWR | O_NOCTTY );
    }
    else {
        serial_handle = open( serial_dev, O_RDWR | O_NOCTTY );
    }
    
	if( serial_handle >= 0 ) {
		struct termios tios ;
		speed_t baud_t ;
		memset(&tios, 0, sizeof(tios));
		tcgetattr(serial_handle, &tios);
		// set serial port attributes
        tios.c_cflag = CS8|CLOCAL|CREAD;
        tios.c_iflag = IGNPAR;
        tios.c_oflag = 0;
        tios.c_lflag = 0;       // ICANON;
        tios.c_cc[VMIN]=50;  	// minimum char
        tios.c_cc[VTIME]=1;		// 0.1 sec time out
		baud_t = B115200 ;
		for( i=0; i< (int)(sizeof(baud_table)/sizeof(baud_table[0])); i++ ) {
			if( serial_baudrate >= baud_table[i].baudrate ) {
				baud_t = baud_table[i].baudv ;
				break;
			}
		}
		cfsetispeed(&tios, baud_t);
		cfsetospeed(&tios, baud_t);

		tcflush(serial_handle, TCIOFLUSH);
		tcsetattr(serial_handle, TCSANOW, &tios);
		tcflush(serial_handle, TCIOFLUSH);
	}
    *handle = serial_handle ;
	return serial_handle ;
}

void serial_close(int * serial_handle)
{
	if( * serial_handle > 0 ) {
		tcflush(* serial_handle, TCIFLUSH);
		close( * serial_handle );
	}
    * serial_handle= -1 ;
}

// Check if data ready to read on serial port
//     return 0: no data
//            1: yes
int serial_dataready(int serial_handle, int microsecondsdelay)
{
	struct timeval timeout ;
	fd_set fds;
	if( serial_handle<0 ) {
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

int serial_read(int serial_handle, void * buf, size_t bufsize)
{
	if( serial_handle>=0 ) {
#ifdef MCU_DEBUG
        if( mcu_debug_out ) {
            int i, n ;
            char * cbuf ;
            cbuf = (char *)buf ;
            n=read( serial_handle, buf, bufsize);
            for( i=0; i<n ; i++ ) {
                printf("%02x ", (int)cbuf[i] );
            }
            return n ;
        }
        else {
            return read( serial_handle, buf, bufsize);
        }
#else        
        return read( serial_handle, buf, bufsize);
#endif
    }
    return 0;
}

int serial_write(int serial_handle, void * buf, size_t bufsize)
{
	if( serial_handle>=0 ) {
#ifdef MCU_DEBUG
        if( mcu_debug_out ) {
            int i ;
            char * cbuf ;
            cbuf = (char *)buf ;
            for( i=0; i<(int)bufsize ; i++ ) {
                printf("%02x ", (int)cbuf[i] );
            }
        }
#endif
        return write( serial_handle, buf, bufsize);
	}
	return 0;
}

int serial_clear(int serial_handle)
{
    unsigned char buf[100] ;
	if( serial_handle>=0 ) {
        while( serial_dataready(serial_handle, 1000 ) ) {
            serial_read( serial_handle, buf, sizeof(buf) );
        }
    }
	return 0;
}

#define GPS_BUFFER_TSIZE (2000)
static char gps_buffer[GPS_BUFFER_TSIZE] ;
static int  gps_buffer_len = 0 ;
static int  gps_buffer_pointer ;

#define gps_open() serial_open(&gps_port_handle, gps_port_dev, gps_port_baudrate )
#define gps_close() serial_close(&gps_port_handle)
// #define gps_dataready(microsecondsdelay) serial_dataready(gps_port_handle, microsecondsdelay)
// #define gps_read(buf,bufsize) serial_read(gps_port_handle, buf, bufsize)
#define gps_write(buf,bufsize) serial_write(gps_port_handle, buf, bufsize)
#define gps_clear() ( gps_buffer_len=0, serial_clear(gps_port_handle))

int gps_dataready(int microsecondsdelay)
{
    if( gps_buffer_len>0 ) { // buffer available
        return 1 ;
    }
    else {
        return serial_dataready(gps_port_handle, microsecondsdelay);
    }
}

int gps_read(char * buf, int bufsize)
{
    if( gps_buffer_len<=0 ) {   // gps buffer empty
        gps_buffer_pointer=0 ;
        gps_buffer_len = serial_read(gps_port_handle, gps_buffer, GPS_BUFFER_TSIZE);
    }
    if( gps_buffer_len>0 ) { // buffer available
        int cpbytes = (gps_buffer_len > bufsize)?bufsize:gps_buffer_len ;
        memcpy( buf, &gps_buffer[gps_buffer_pointer], cpbytes );
        gps_buffer_pointer+=cpbytes ;
        gps_buffer_len-=cpbytes ;
        return cpbytes ;
    }
    return 0 ;                  // err, no data.
}

unsigned char gps_calcchecksum( char * line )
{
    unsigned char sum = 0 ;
	int i;
	for( i=1; i<400; i++ ) {
        if( line[i]==0 || line[i]=='\r' || line[i]=='\n' ) {    // no check sum field
            break ;
        }
		else if( line[i]=='*' ) {
            return sum ;
		}
		sum^=(unsigned char)line[i] ;
	}
	return 0 ;
} 

// set SiRF sentense rate
void gps_setsirfrate(int sentense, int rate)
{
    char buf1[100], buf2[100] ;
    sprintf( buf1, "$PSRF103,%d,0,%d,1*", sentense, rate );
    sprintf( buf2, "%s%02x\r\n", buf1, gps_calcchecksum( buf1 ) );
    gps_write(buf2, strlen(buf2));
}

int gps_readline( char * line, int linesize )
{
//	strcpy( line, "$GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68" );
	int i; 
	char c ;
    static int errcount=0 ;
    
    if( gps_port_disable ) {                // gps disabled ?
        return 0;
    }

    if( gps_port_handle<0 ) {
        if( gps_open() < 0 ) {					// open serial port
            sleep(1);
            return 0 ;					
        }
        // set SiRF sentences rates
        gps_setsirfrate(0,0);       // turn off GGA
        gps_setsirfrate(1,0);       // turn off GLL
        gps_setsirfrate(2,0);       // turn off GSA
        gps_setsirfrate(3,0);       // turn off GSV
        gps_setsirfrate(5,0);       // turn off VTG
        gps_setsirfrate(8,0);       // turn off ZDA
        gps_setsirfrate(4,1);       // turn on RMC
    }

    for( i=0 ;i<linesize; ) {
		if( gps_dataready(1200000) ) {
			if( gps_read(&c, 1)==1 ) {
				if( c=='\n' ) {
					line[i]=0;
				    return i;
				}
				else if( c<=1 || c>=127 ) {		// GPS dont give these chars
					// for unknown reason, serial port mess up all the time??????
					break;
				}
				else {
					line[i++]=c ;
				}
			}
			else {
				break;
			}
		}
		else {
    	    break;
    	}
	}
    
	// line too long, or error
    if( errcount++ > 10 ) {
	gps_close();
        usleep(10000);
        errcount=0 ;
    }
	return 0;
}

// 0: failed, 1: success
int gps_checksum( char * line )
{
    unsigned int checksum=0x55 ;
    unsigned char sum = 0 ;
	int i;
	for( i=1; i<400; i++ ) {
        if( line[i]==0 || line[i]=='\r' || line[i]=='\n' ) {    // no check sum field
            return 0 ;
        }
		else if( line[i]=='*' ) {
            sscanf( &line[i+1], "%2x", &checksum );
            return ( sum==checksum);
		}
		sum^=(unsigned char)line[i] ;
	}
	return 0 ;
}

struct gps_data {
	int valid ;
	int year ;
	int month ;
	int day ;
	int hour ;
	int minute ;
	float second ;
	float latitude ;
	float longitude ;
	float speed ;
	float direction ;
} ;

// read and parse GPS sentence
//   gps sentence example: $GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68
// return
//   1: receive GPRMC sentence
//   2: receive other GPS sentence
//   0: no GPS sentence or sentence error
int gps_readdata(struct gps_data * gpsdata)
{
	char line[300] ;		
	int l;
	if( (l=gps_readline( line, sizeof(line) ))>10 ) {
        // fifo ouput for debuging               
        if( gps_fifofile>0 ) {
            write( gps_fifofile, line, l );
            write( gps_fifofile, "\n", 1 );
        }
        // end of debug
		if( memcmp(line, "$GPRMC,", 7)==0 && gps_checksum(line) )
        {
            char fix = 0 ;
            int lat_deg, long_deg ;
            char lat_dir, long_dir ;
            sscanf( &line[7], "%2d%2d%f,%c,%2d%f,%c,%3d%f,%c,%f,%f,%2d%2d%d",
                   &gpsdata->hour,
                   &gpsdata->minute,
                   &gpsdata->second,
                   &fix,
                   &lat_deg, &gpsdata->latitude, &lat_dir,
                   &long_deg, &gpsdata->longitude, &long_dir,
                   &gpsdata->speed,
                   &gpsdata->direction,
                   &gpsdata->day,
                   &gpsdata->month,
                   &gpsdata->year 
                   );   
            
            gpsdata->valid = ( fix == 'A' ) ;
            
            gpsdata->latitude = lat_deg + gpsdata->latitude/60.0 ;	
            if( lat_dir != 'N' ) {
                gpsdata->latitude = - gpsdata->latitude ;
            }
            
            gpsdata->longitude = long_deg + gpsdata->longitude/60.0 ;	
            if( long_dir != 'E' ) {
                gpsdata->longitude = - gpsdata->longitude ;
            }

            if( gpsdata->year>=80 ) {
                gpsdata->year+=1900 ;
            }
            else {
                gpsdata->year+=2000 ;
            }
            return 1 ;
        }
        else {
            return 2 ;
        }
    }
    else {
    	gpsdata->valid=0;               // serial line broken or no data
    }
    return 0 ;
}
    
/*

// read and parse GPS sentence
//   gps sentence example: $GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68
// return
//   1: receive GPRMC sentence
//   0: no GPRMC sentence or sentence error
int gps_readdata(struct gps_data * gpsdata)
{
	char line[300] ;		
	int i, f, l;
	int field[40] ;
	f=0 ;		
	if( (l=gps_readline( line, sizeof(line) ))>10 ) {
		if( memcmp(line, "$GPRMC,", 7)==0 ) {
			field[0] = 7 ;
			for( i=7; i<300 && f<39; i++ ) {
				if( line[i]==',' ) {		// field breaker
					field[++f]=i+1;
				}
				else if( line[i]=='*' ) {		// check sum field
					field[++f] = i ;
					break;
				}
				else if( line[i]=='\0' ) {		// should not reach here
					return 0;
				}
			}
		}
	}
    else {
    	gpsdata->valid=0;               // serial line broken
        return 0;
    }
    
	if( f>9 ) {
		// last field checksum
		if( line[field[f]]!='*' ) {
			return 0;
		}

		unsigned int checksum ;
		sscanf(&(line[field[f]+1]), "%2x", &checksum );
		if( checksum != gps_checksum(line) ) {
			// check sum error!
			return 0;
		}

// debuging                
        static int gpsfifo=0 ;
        if( gpsfifo<=0 ) {
            gpsfifo=open( "/davinci/gpsfifo", O_WRONLY | O_NONBLOCK );
        }
        if( gpsfifo>0 ) {
            if( write( gpsfifo, line, l )!=l ) {
                close(gpsfifo);
                gpsfifo=0;
            }
        }
// end of debug

		// field 0 : UTC time
		sscanf( &line[field[0]], " %2d%2d%f", 
				&gpsdata->hour,
				&gpsdata->minute,
				&gpsdata->second );

		// field 1 : A = gps fix valid
		gpsdata->valid = ( line[field[1]] == 'A' ) ;
 
		// field 2 , 3 Latitude
		sscanf( &line[field[2]], " %2d%f", &i, &gpsdata->latitude );
		gpsdata->latitude = i + gpsdata->latitude/60.0 ;	
        if( line[field[3]] != 'N' ) {
            gpsdata->latitude = - gpsdata->latitude ;
        }

		// field 4,  5 Longitude
		sscanf( &line[field[4]], " %3d%f", &i, &gpsdata->longitude );
		gpsdata->longitude = i + gpsdata->longitude/60.0 ;	
        if( line[field[5]] != 'E' ) {
            gpsdata->longitude = - gpsdata->longitude ;
        }

		// field 6 speed
		sscanf( &line[field[6]], "%f", &gpsdata->speed );

		// field 7 direction, degree
		sscanf( &line[field[7]], "%f", &gpsdata->direction );

		// field 8 Date (DDMMYY)
		sscanf( &line[field[8]], " %2d%2d%d", 
				&gpsdata->day,
				&gpsdata->month,
				&gpsdata->year );
		if( gpsdata->year>=80 ) {
			gpsdata->year+=1900 ;
		}
		else {
			gpsdata->year+=2000 ;
		}

		return 1 ;
	}
	return 0 ;		
}
*/

FILE * gps_logfile ;
static int gps_logday ;
struct gps_data validgpsdata ;

static int gps_close_fine = 0 ;

void gps_logclose()
{
	if( gps_logfile ) {
		fclose( gps_logfile );
		gps_logfile=NULL;
        gps_close_fine=1 ;
	}
}


FILE * gps_logopen(struct tm *t)
{
	char curdisk[256] ;
	char hostname[128] ;
	int r ;
	int i;
	// check if day changed?
	if( gps_logday != t->tm_mday ) {
		gps_logclose();
		gps_logday = t->tm_mday ;
        gpslogfilename[0]=0 ;               // change file
	}

    if( gps_logfile==NULL ) {
        if( gpslogfilename[0]==0 ) {
            gethostname(hostname, 127);
            struct stat smlogstat ;
            dir_find mountdir( disksroot );
            while( mountdir.find() ) {
                if( mountdir.isdir() ) {
                    sprintf(gpslogfilename, "%s/smartlog/%s_%04d%02d%02d_N.001",
                            mountdir.pathname(),
                            hostname,
                            (int)t->tm_year+1900,
                            (int)t->tm_mon+1,
                            (int)t->tm_mday );
                    if( stat( gpslogfilename, &smlogstat )==0 ) {
                        if( S_ISREG( smlogstat.st_mode ) ) {
                            // rename exist "*N.001" to "L.001", so we can reuse same file
                            strncpy( curdisk, gpslogfilename, sizeof(curdisk) );
                            i=strlen( gpslogfilename ) ;
                            gpslogfilename[i-5] = 'L' ;
                            rename( curdisk, gpslogfilename );
                            break;
                        }
                    }
                    i=strlen( gpslogfilename ) ;
                    gpslogfilename[i-5] = 'L' ;
                    if( stat( gpslogfilename, &smlogstat )==0 ) {
                        if( S_ISREG( smlogstat.st_mode ) ) {
                            break;
                        }
                    }
                    gpslogfilename[0]=0 ;
                }
            }
            
            if( gpslogfilename[0]==0 ) {
                
                FILE * diskfile = fopen( dvrcurdisk.getstring(), "r");
                if( diskfile ) {
                    r=fread( curdisk, 1, 255, diskfile );
                    fclose( diskfile );
                    if( r>2 ) {
                        curdisk[r]=0 ;  // null terminate char
                        sprintf(gpslogfilename, "%s/smartlog", curdisk );
                        mkdir(gpslogfilename, 0777);
                        gethostname(hostname, 127);
                        sprintf(gpslogfilename, "%s/smartlog/%s_%04d%02d%02d_L.001",
                                curdisk,
                                hostname,
                                (int)t->tm_year+1900,
                                (int)t->tm_mon+1,
                                (int)t->tm_mday );
                    }
                }
            }
        }
        
        if( gpslogfilename[0] != 0 ) {
            gps_logfile = fopen( gpslogfilename, "a" );
        }

        if( gps_logfile ) {
            fseek( gps_logfile, 0, SEEK_END ) ;
            if( ftell( gps_logfile )<10 ) {		// new file, add file headers
                fseek(gps_logfile, 0, SEEK_SET);
                fprintf(gps_logfile, "%04d-%02d-%02d\n99", 
                        (int)t->tm_year+1900,
                        (int)t->tm_mon+1,
                        (int)t->tm_mday );
                
                for( i=0; i<MAXSENSORNUM && i<p_dio_mmap->inputnum; i++ ) {
                    fprintf(gps_logfile, ",%s", sensorname[i].getstring() );
                }
                fprintf(gps_logfile, "\n");
            }
            else if( gps_close_fine==0 ) {
                fprintf(gps_logfile, "\n");         // add a newline, in case power interruption happen and file no flush correctly.
            }
        }
        else {
            gpslogfilename[0]=0 ;
        }
    }
    return gps_logfile;
}

int gps_logprintf(int logid, char * fmt, ...)
{
    FILE * logfile ;
    va_list ap ;
    
    struct tm t ;
	time_t tt ;
    
    tt=time(NULL);
    localtime_r(&tt, &t);
    
    // no record before 2005
    if( t.tm_year < 105 || t.tm_year>150 ) {
        return 0 ;
    }
    
    log_lock();
    
	logfile = gps_logopen(&t);
    
	if( logfile==NULL ) {
        log_unlock();
        glog_ok=0 ;
		return 0;
	}
    
    fprintf(logfile, "%02d,%02d:%02d:%02d,%09.6f%c%010.6f%c%.1fD%06.2f", 
                          logid,
                          (int)t.tm_hour,
                          (int)t.tm_min,
                          (int)t.tm_sec,
                          validgpsdata.latitude<0.0000000 ? -validgpsdata.latitude : validgpsdata.latitude ,
                          validgpsdata.latitude<0.0000000 ? 'S' : 'N' ,
                          validgpsdata.longitude<0.0000000 ? -validgpsdata.longitude: validgpsdata.longitude,
                          validgpsdata.longitude<0.0000000 ? 'W' : 'E' ,
                          (float)(validgpsdata.speed * 1.852),      // in km/h.
                          (float)validgpsdata.direction ) ;
    
    va_start(ap, fmt);
    vfprintf( logfile, fmt, ap );
    fprintf(logfile, "\n");
    va_end(ap);

    if( validgpsdata.speed < 1.0 ) {
        gps_logclose ();
    }

    log_unlock();
    glog_ok=1 ;
    return 1;
}


// log gps position
int	gps_log()
{
    static float logtime=0.0 ;
//    static float logdirection=0.0 ;
//    static float logspeed=0.0 ;
    static int stop = 0 ;
//    static float disttime = 0 ;
//    static float dist=0 ;
    
    float ti ;
    float tdiff ;
    int log = 0 ;

    ti = 3600.0 * validgpsdata.hour +
        60.0 * validgpsdata.minute +
        validgpsdata.second ;
    
//    dist+=gpsdata->speed * (1.852/3.6) * (ti-disttime) ;
//    disttime=ti ;
    tdiff = ti-logtime ;
    if( tdiff<0.0 ) tdiff=1000.0 ;

    if( validgpsdata.speed<0.2 ) {
        if( stop==0 || tdiff>60.0 ) {
            stop=1 ;
            log=1 ;
        }
    }
    else if( tdiff>3.0 ) {
        stop=0 ;
        log=1;
    }

    if( log ) {
        if( gps_logprintf(1, "" ) ) {
            logtime = ti ;
//            logdirection = gpsdata->direction ;
//            logspeed = gpsdata->speed ;
//            dist=0.0 ;
        }
	}

	return 1;
}

// log sensor event
int	sensor_log()
{
	int i;

    if( validgpsdata.year < 1900 ) return 0 ;

    // log ignition changes
    if( glog_poweroff != p_dio_mmap->poweroff ) {
        if( gps_logprintf(2, ",%d", p_dio_mmap->poweroff?0:1 ) )
        {
            glog_poweroff = p_dio_mmap->poweroff ;
        }
        else {
            return 0 ;
        }
    }

	// log sensor changes
	unsigned int imap = p_dio_mmap->inputmap ;

    int sensorid = 3 ;			// start from 03
    
    double btime ;
    struct timeval tv ;
    gettimeofday( &tv, NULL);
    btime = ((double)(unsigned long)tv.tv_sec) + (double)(tv.tv_usec)/1000000.0 ;
    
    for( i=0; i<p_dio_mmap->inputnum; i++ ) {
        int sv ;
        
        // sensor debouncing
        sv = (( imap & (1<<i) )!=0) ;
        if( sensor_invert[i] ) {
            sv=!sv ;
        }

        if( sv != sensor_value[i] && (btime-sensorbouncetime[i]) > 2.5 ) {
            if( gps_logprintf( (sv)? (20+i*2):(20+i*2+1), "" ) )
            {
                sensor_value[i]=sv ;
            }
        }
        
        if( sv != sensorbouncevalue[i] ) {  // sensor value bouncing
            sensorbouncetime[i]=btime ;
            sensorbouncevalue[i] = sv ;
        }
    }

    // log g sensor value
    if( gforce_log_enable ) {
        if( p_dio_mmap->gforce_log0 ) {
            // we record forward/backward, right/left acceleration value, up/down as g-force value
            gps_logprintf(16, ",%.1f,%.1f,%.1f", 
                          (double) - p_dio_mmap->gforce_forward_0,
                          (double) - p_dio_mmap->gforce_right_0,
                          (double) p_dio_mmap->gforce_down_0 );
            p_dio_mmap->gforce_log0=0;
        }
        if( p_dio_mmap->gforce_log1 ) {
            // we record forward/backward, right/left acceleration value, up/down as g-force value
            gps_logprintf(16, ",%.1f,%.1f,%.1f", 
                          (double) - p_dio_mmap->gforce_forward_1,
                          (double) - p_dio_mmap->gforce_right_1,
                          (double) p_dio_mmap->gforce_down_1 );
            p_dio_mmap->gforce_log1=0;
        }
    }

    for( i=0; i<p_dio_mmap->inputnum; i++ ) {
        int sv ;
        
        // sensor debouncing
        sv = (( imap & (1<<i) )!=0) ;
        if( sensor_invert[i] ) {
            sv=!sv ;
        }

        if( sv != sensor_value[i] && (btime-sensorbouncetime[i]) > 2.5 ) {
            if( gps_logprintf( (sv)? (sensorid+i*2):(sensorid+i*2+1), "" ) )
            {
                sensor_value[i]=sv ;
            }
        }
        
        if( sv != sensorbouncevalue[i] ) {  // sensor value bouncing
            sensorbouncetime[i]=btime ;
            sensorbouncevalue[i] = sv ;
        }
    }


	return 1;
}

static void * sensorlog_thread(void *param)
{
    while( app_state ) {
        if( app_state==1 ) {
            sensor_log();
        }
        usleep(20000);
    }
    return NULL ;
}

// TAB102B sensor log

#define TAB102B_BUFFER_TSIZE (2000)
static char tab102b_buffer[TAB102B_BUFFER_TSIZE] ;
static int  tab102b_buffer_len ;
static int  tab102b_buffer_pointer ;

#define tab102b_open() serial_open(&tab102b_port_handle, tab102b_port_dev, tab102b_port_baudrate )
#define tab102b_close() serial_close(&tab102b_port_handle)
//#define tab102b_dataready(microsecondsdelay) serial_dataready(tab102b_port_handle, microsecondsdelay)
//#define tab102b_read(buf,bufsize) serial_read(tab102b_port_handle, buf, bufsize)
#define tab102b_write(buf,bufsize) serial_write(tab102b_port_handle, buf, bufsize)
#define tab102b_clear() (tab102b_buffer_len = 0, serial_clear(tab102b_port_handle))

int tab102b_dataready(int microsecondsdelay)
{
    if( tab102b_buffer_len>0 ) { // buffer available
        return 1 ;
    }
    else {
        return serial_dataready(tab102b_port_handle, microsecondsdelay);
    }
}

int tab102b_read(unsigned char * buf, int bufsize)
{
    if( tab102b_buffer_len<=0 ) {   // gps buffer empty
        tab102b_buffer_pointer=0 ;
        tab102b_buffer_len = serial_read(tab102b_port_handle, tab102b_buffer, TAB102B_BUFFER_TSIZE);
    }
    if( tab102b_buffer_len>0 ) { // buffer available
        int cpbytes = (tab102b_buffer_len > bufsize)?bufsize:tab102b_buffer_len ;
        memcpy( buf, &gps_buffer[gps_buffer_pointer], cpbytes );
        gps_buffer_pointer+=cpbytes ;
        gps_buffer_len-=cpbytes ;
        return cpbytes ;
    }
    return 0 ;                  // err, no data.
}

// read with timeout in microseconds
int tab102b_read_withtimeout(unsigned char * buf, int bufsize, int microsecondtimeout)
{
    unsigned char * buf_pointer = buf ;
    int    n_toread = bufsize ;
    int    n_read ;
    while( n_toread > 0 ) {
        if( tab102b_dataready(microsecondtimeout) ) {
            n_read = tab102b_read(buf_pointer, n_toread) ;
            if( n_read>0 ) {
                n_toread-=n_read ;
                buf_pointer+=n_read ;
            }
            else {          // err? 
                return 0 ;
            }
        }
        else {              // time out
            return bufsize-n_toread ; 
        }
    }
    return bufsize ;
}

// check data check sum
// return 0: checksum correct
int tab102b_checksun( unsigned char * data ) 
{
    unsigned char ck;
    int i ;
    if( data[0]<5 || data[0]>40 ) {
        return -1 ;
    }
    ck=0;
    for( i=0; i<data[0]; i++ ) {
        ck+=data[i] ;
    }
    return (int)ck;
}

// calculate check sum of data
void tab102b_calchecksun( unsigned char * data )
{
    unsigned char ck = 0 ;
    unsigned int si = (unsigned int) *data ;
    while( --si>0 ) {
        ck-=*data++ ;
    }
    *data = ck ;
}

int tab102b_senddata( unsigned char * data )
{
    if( *data<5 || *data>50 ) { 	// wrong data
        return 0 ;
    }
    tab102b_calchecksun(data);

#ifdef MCU_DEBUG
    // for debugging 
    struct timeval tv ;
    gettimeofday(&tv, NULL);
    struct tm * ptm ;
    ptm = localtime(&tv.tv_sec);
    double sec ;
    sec = ptm->tm_sec + tv.tv_usec/1000000.0 ;
    
    printf("%02d:%02d:%02.3f tab102b senddata: ", ptm->tm_hour, ptm->tm_min, sec);
    mcu_debug_out = 1 ;
    int n = tab102b_write(data, (int)*data);
    mcu_debug_out = 0 ;
    printf("\n");
    return n ;
#else    
    return tab102b_write(data, (int)*data);
#endif    
} 

int tab102b_recvmsg( unsigned char * data, int size )
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
    
    printf("%02d:%02d:%02.3f tab102b recvmsg: ", ptm->tm_hour, ptm->tm_min, sec);
    mcu_debug_out = 1 ;
#endif    
    
    if( tab102b_read(data, 1) ) {
        n=(int) *data ;
        if( n>=5 && n<=size ) {
            n=tab102b_read_withtimeout(data+1, n-1, 1000000)+1 ;
            if( n==*data && 
                data[1]==0 && 
                data[2]==4 &&
                tab102b_checksun( data ) == 0 ) 
            {
#ifdef MCU_DEBUG
    mcu_debug_out = 0 ;
    printf("\n");
#endif                    
                return n ;
            }
        }
    }
#ifdef MCU_DEBUG
    mcu_debug_out = 0 ;
#endif                    

    tab102b_clear();
    return 0 ;
}

int tab102b_x_0g, tab102b_y_0g, tab102b_z_0g, tab102b_st ;

void tab102b_log( int x, int y, int z)
{
    float x_v, y_v, z_v ;
    
    x_v = (x-tab102b_x_0g) * (3.3/1024.0/0.12) ;
    y_v = (y-tab102b_y_0g) * (3.3/1024.0/0.12) ;
    z_v = (z-tab102b_z_0g) * (3.3/1024.0/0.12) ;

#ifdef MCU_DEBUG    
    printf( "tab102b: x %05x :%05x : %.2f  y %05x :%05x : %.2f  z %05x :%05x : %.2f\n", 
           x, tab102b_x_0g, x_v, 
           y, tab102b_y_0g, y_v, 
           z, tab102b_z_0g, z_v );
#endif

    if( validgpsdata.year < 1900 ) return ;

    // log tab102b g sensor value
    // we record forward/backward, right/left acceleration value, up/down as g-force value
    if( gforce_log_enable ) {
        gps_logprintf(16, ",%.1f,%.1f,%.1f", 
                      (double) - x_v ,
                      (double) - y_v ,
                      (double) z_v );
    }
    
}

void tab102b_checkinput(unsigned char * ibuf)
{
    int x, y, z ;
    
    switch( ibuf[3] ) {
        case '\x1e' :                   // tab102b peak data
            x = 256*ibuf[5] + ibuf[6] ;
            y = 256*ibuf[7] + ibuf[8] ;
            z = 256*ibuf[9] + ibuf[10] ;
            tab102b_log( x, y, z );
            break;
        
        default :
            // ignor other input messages
            break;
    }
}

int tab102b_error ;
int tab102b_response( unsigned char * recv, int size, int usdelay=500000 );

// receiving respondes message from tab102b
int tab102b_response( unsigned char * recv, int size, int usdelay )
{
    int n;
    if( size<5 ) {				// minimum packge size?
        return 0 ;
    }
    
    while( tab102b_dataready(usdelay) ) {
        n = tab102b_recvmsg (recv, size) ;
        if( n>0 ) {
            if( recv[4]=='\x02' ) {             // is it a input message ?
                tab102b_error=0 ;
                tab102b_checkinput(recv) ;
                continue ;
            }
            else if( recv[4]=='\x03' ){         // is it a response?
                tab102b_error=0 ;
                return n ;                      // it could be a response message
            }
            else {
                break;
            }
        }
        else {
            break;
        }
    }

    if( ++tab102b_error>5 ) {
        tab102b_close();
        sleep(1);
        tab102b_open();
    }

    return 0 ;
}


static unsigned char bcd_v( unsigned char v)
{
    return (v/10)*16 + v%10 ;
}

int tab102b_cmd_sync_rtc()
{
    int retry ;
    static unsigned char cmd_sync_rtc[15]="\x0d\x04\x00\x07\x02\x01\x02\x03\x04\x05\x06\x07\xcc" ;
    unsigned char response[40] ;
    if( tab102b_open()<0 ) {
        return 0;
    }
    // build rtc
    struct tm t ;
	time_t tt ;
    
    tt=time(NULL);
    localtime_r(&tt, &t);
    cmd_sync_rtc[5]= bcd_v(t.tm_sec) ;
    cmd_sync_rtc[6]= bcd_v(t.tm_min) ;
    cmd_sync_rtc[7]= bcd_v(t.tm_hour) ;
    cmd_sync_rtc[8]= bcd_v(t.tm_wday) ;
    cmd_sync_rtc[9]= bcd_v(t.tm_mday) ;
    cmd_sync_rtc[10]=bcd_v(t.tm_mon+1) ;
    cmd_sync_rtc[11]=bcd_v(t.tm_year-100) ;        // need convert to bcd???
    
    for( retry=0; retry<10; retry++) {
        tab102b_senddata( cmd_sync_rtc );
    
        if( tab102b_response( response, sizeof(response))>0 ) {
            break;
        }
    }
    
    return 1;
}

int tab102b_datauploadconfirmation(int res)
{
    static unsigned char cmd_data_conf[15]="\x07\x04\x00\x1a\x02\xdd\xcc" ;
    unsigned char response[40] ;
    if( tab102b_open()<0 ) {
        return 0;
    }
    cmd_data_conf[5]=(unsigned char)res ;
    tab102b_senddata( cmd_data_conf );
    if( tab102b_response( response, sizeof(response))>0 ) {
        return 1 ;
    }
    
    return 0;
}

int tab102b_logdata( unsigned char * tab102b_data, int datalength )
{
    int len ;
    char hostname[128] ;
    char tab102b_datafilename[256] ;
    struct tm t ;
    time_t tt ;
    unsigned char dbuf[256] ;
    FILE * tab102b_datafile = NULL ;
    FILE * diskfile = fopen( dvrcurdisk.getstring(), "r");
    if( diskfile ) {
        len=fread( dbuf, 1, 255, diskfile );
        fclose( diskfile );
        if( len>2 ) {
            dbuf[len]=0 ;  // null terminate char
            tt=time(NULL);
            localtime_r(&tt, &t);
            gethostname( hostname, 127 );
            sprintf(tab102b_datafilename, "%s/_%s_/%04d%02d%02d/%s_%04d%02d%02d%02d%02d%02d_TAB102log_L.log",
                    dbuf,
                    hostname,
                    t.tm_year+1900,
                    t.tm_mon+1,
                    t.tm_mday,
                    hostname,
                    t.tm_year+1900,
                    t.tm_mon+1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec );
            tab102b_datafile = fopen(tab102b_datafilename, "wb");
        }
    }

    if( tab102b_datafile ) {
        fwrite( tab102b_data, 1, datalength, tab102b_datafile );
        fclose( tab102b_datafile );
    }

    return 1 ;
}


int tab102b_cmd_req_data()
{
    int res=0;
    int datalength ;
    unsigned char * udatabuf ;
    static unsigned char cmd_req_data[8]="\x06\x04\x00\x19\x02\xcc" ;
    unsigned char response[40] ;
    
    if( tab102b_data_enable == 0 ) {
        return 0 ;
    }
    
    if( tab102b_open()<0 ) {
        return 0;
    }

    tab102b_senddata( cmd_req_data );
    
    if( tab102b_response( response, sizeof(response))>0 ) {
        if( response[3]=='\x19' ) {             // data response?
            datalength = 0x1000000 * response[5] + 0x10000* response[6] + 0x100 * response[7] + response[8] ;
            if( datalength>0 && datalength<0x100000) {
    
#ifdef MCU_DEBUG    
    printf( "tab102b: uploading data , length: %d\n", datalength );
#endif                
                udatabuf=(unsigned char *)malloc( datalength+0x12 );
                if( udatabuf ) {
                    int readlen = 0;
                    int toread=datalength+0x12 ;
                    int len ;
                    while( tab102b_dataready(10000000) && app_state ) {
                        len = tab102b_read( &udatabuf[readlen], toread-readlen );
                        if( len>0 ) {
                            readlen+=len ;
                            if( readlen>=toread ) {
                                res=1 ;
                                break;
                            }
                        }
                        else {
                            break;
                        }
                    }
                    if( res==1 ) {
                        tab102b_logdata( udatabuf, toread );
                    }
                    free( udatabuf );
                }
            }
        }
    }

    if( res==0 ) {
        while( tab102b_dataready(1000000) && app_state ) {
            tab102b_clear();
        }
    }

    if( app_state ) {
        tab102b_datauploadconfirmation(res);
    }
    
    return res;
}

int tab102b_cmd_get_0g()
{
    static unsigned char cmd_get_0g[8]="\x06\x04\x00\x18\x02\xcc" ;
    unsigned char response[40] ;
    if( tab102b_open()<0 ) {
        return 0;
    }

    tab102b_senddata( cmd_get_0g );
    
    if( tab102b_response( response, sizeof(response))>0 ) {
        if( response[3]=='\x18' ) {             // data response?
            tab102b_x_0g = 0x100*response[5] + response[6] ;
            tab102b_y_0g = 0x100*response[7] + response[8] ;
            tab102b_z_0g = 0x100*response[9] + response[10] ;
            tab102b_st   = response[11] ;
        }
    }
    
    return 1;
}

int tab102b_cmd_ready()
{
    int retry ;
    static unsigned char cmd_ready[8]="\x06\x04\x00\x08\x02\xcc" ;
    unsigned char response[40] ;
    if( tab102b_open()<0 ) {
        return 0;
    }

    for( retry=0; retry<10; retry++) {
        tab102b_senddata( cmd_ready );
    
        if( tab102b_response( response, sizeof(response))>0 ) {
            return 1 ;
        }
    }
    return 0;
}

int tab102b_cmd_enablepeak()
{
    static unsigned char cmd_enablepeak[8]="\x06\x04\x00\x0f\x02\xcc" ;
    unsigned char response[40] ;
    if( tab102b_open()<0 ) {
        return 0;
    }

    tab102b_senddata( cmd_enablepeak );
    
    if( tab102b_response( response, sizeof(response))>0 ) {
            return 1 ;
    }
    return 0;
}

int tab102b_cmd_disablepeak()
{
    static unsigned char cmd_disablepeak[8]="\x06\x04\x00\x10\x02\xcc" ;
    unsigned char response[40] ;
    if( tab102b_open()<0 ) {
        return 0;
    }

    tab102b_senddata( cmd_disablepeak );
    
    if( tab102b_response( response, sizeof(response))>0 ) {
            return 1 ;
    }
    return 0;
}

int tab102b_init()
{
    if( tab102b_open()<0 ) {
        return 0;
    }
    tab102b_cmd_sync_rtc();
    
    tab102b_cmd_req_data();
        
    tab102b_cmd_get_0g();
    
    tab102b_cmd_ready();
    
    if( gforce_log_enable ) {
        tab102b_cmd_enablepeak();
    }
    
    return 1;
}

int tab102b_finish()
{
    if( tab102b_open()<0 ) {
        return 0;
    }
    tab102b_cmd_disablepeak();
    tab102b_close();
    return 1;
}

static void * tab102b_thread(void *param)
{
    int tab102b_start=0;
    unsigned char recvbuf[50] ;
    while( app_state ) {
        if( app_state==1 ) {
            if( tab102b_start==0 ) {
                if( glog_ok ) {
                    tab102b_init();         // delay tab102b log until log available
                    tab102b_start=1;
                }
                usleep(100000);
            }
            else if( tab102b_port_handle<0 ) {
                tab102b_open();
            }
            else if( tab102b_dataready(2000000) ) {
                tab102b_response(recvbuf, sizeof(recvbuf), 1000000);
            }
        }
        else {
            if( app_state==2 && tab102b_port_handle>0 ) {
                if( gforce_log_enable ) {
                    tab102b_cmd_disablepeak();
                }
                tab102b_cmd_req_data();
                if( gforce_log_enable ) {
                    tab102b_cmd_enablepeak();
                }
            }
            tab102b_close();
            usleep(10000);
        }
    }
    if( tab102b_start ) {
        tab102b_finish();
    }
    return NULL ;
}

pthread_t sensorthreadid=0 ;
pthread_t tab102b_threadid=0 ;

// return 
//        0 : failed
//        1 : success
int appinit()
{
	int fd ;
	int i;
	char * p ;
    FILE * pidf ;
	string serialport ;
	string iomapfile ;
    string tstr ;
	config dvrconfig(dvrconfigfile);
	iomapfile = dvrconfig.getvalue( "system", "iomapfile");
	
	p_dio_mmap=NULL ;
	if( iomapfile.length()==0 ) {
		return 0;						// no DIO.
	}

	for( i=0; i<10; i++ ) {				// retry 10 times
		fd = open(iomapfile.getstring(), O_RDWR );
		if( fd>0 ) {
			break ;
		}
		sleep(1);
	}
	if( fd<=0 ) {
		printf("GLOG: IO module not started!");
		return 0;
	}

	p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	close( fd );								// fd no more needed
	if( p==(char *)-1 || p==NULL ) {
		printf( "GLOG:IO memory map failed!");
		return 0;
	}
	p_dio_mmap = (struct dio_mmap *)p ;   
    
    if( p_dio_mmap->glogpid > 0 ) {
        kill( p_dio_mmap->glogpid, SIGTERM );
        for(i=0; i<10; i++) {
            if(p_dio_mmap->glogpid<=0 ) break;
        }
    }

    // init gps log 
    p_dio_mmap->usage++;
    p_dio_mmap->glogpid = getpid();
    p_dio_mmap->gps_valid = 0 ;
    p_dio_mmap->gps_speed = 0 ;			// knots
    p_dio_mmap->gps_direction = 0 ;     // degree
    p_dio_mmap->gps_latitude=0 ;		
    p_dio_mmap->gps_longitud=0 ;
    p_dio_mmap->gps_gpstime=0 ;
    
	// get GPS port setting
    gps_port_disable = dvrconfig.getvalueint( "glog", "gpsdisable");
	serialport = dvrconfig.getvalue( "glog", "serialport");
	if( serialport.length()>0 ) {
		strcpy( gps_port_dev, serialport.getstring() );
	}
	i = dvrconfig.getvalueint("glog", "serialbaudrate");
	if( i>=1200 && i<=115200 ) {
		gps_port_baudrate=i ;
	}

    tstr = dvrconfig.getvalue( "glog", "gpsfifo" ); 
    if( tstr.length()>0 ) {
        gps_fifofile = open( tstr.getstring(), O_WRONLY | O_NONBLOCK );
    }
    else {
        gps_fifofile=0 ;
    }
    
    // get tab102b serial port setting
	serialport = dvrconfig.getvalue( "glog", "tab102b_port");
	if( serialport.length()>0 ) {
		strcpy( tab102b_port_dev, serialport.getstring() );
	}
    
	i = dvrconfig.getvalueint("glog", "tab102b_baudrate");
	if( i>=1200 && i<=115200 ) {
		tab102b_port_baudrate=i ;
	}
    
	// initialize time zone
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

    // initialize gps log speed table
    tstr = dvrconfig.getvalue( "glog", "degree1km");
    if( tstr.length()>0 ) {
        sscanf(tstr.getstring(), "%f", &degree1km );
    }
    
    tstr = dvrconfig.getvalue( "glog", "dist_max");
    if( tstr.length()>0 ) {
        sscanf( tstr.getstring(), "%f", &max_distance );
    }
    else {
        max_distance=500 ;
    }
    // convert distance to degrees
    max_distance *= degree1km/1000.0;
    
    for( i=0; i<SPEEDTABLESIZE; i++ ) {
        char iname[200] ;
        sprintf( iname, "speed%d", i);
        tstr = dvrconfig.getvalue( "glog", iname );
        if( tstr.length()>0 ) {
            sscanf( tstr.getstring(), "%f", &(speed_table[i].speed) );
            sprintf( iname, "dist%d", i );
            tstr = dvrconfig.getvalue( "glog", iname );
            if( tstr.length()>0 ) {
                sscanf( tstr.getstring(), "%f", &(speed_table[i].distance) );
            }
            else {
                speed_table[i].distance=100 ;
            }
            speed_table[i].distance *= degree1km/1000.0 ;
        }
        else {
            speed_table[i].speed=0 ;
            break;
        }
    }
    
    tstr = dvrconfig.getvalue( "glog", "inputdebounce");
    if( tstr.length()>0 ) {
        sscanf( tstr.getstring(), "%d", &inputdebounce );
    }

    // gforce logging enabled ?
    gforce_log_enable = dvrconfig.getvalueint( "glog", "gforce_log_enable");

    tab102b_enable = dvrconfig.getvalueint( "glog", "tab102b_enable");

    tab102b_data_enable = dvrconfig.getvalueint( "glog", "tab102b_data_enable");

        
    // get dvr disk directory
	dvrcurdisk = dvrconfig.getvalue("system", "currentdisk" );

	// get sensor name
	for( i=0; i<MAXSENSORNUM && i<p_dio_mmap->inputnum; i++ ) {
		char sec[10] ;
		sprintf( sec, "sensor%d", i+1);
		sensorname[i] = dvrconfig.getvalue(sec, "name");
        sensor_invert[i] = dvrconfig.getvalueint(sec, "inverted");
	}
    
    gpslogfilename[0]=0 ;
    
    pidf = fopen( pidfile, "w" );
	if( pidf ) {
		fprintf(pidf, "%d", (int)getpid() );
		fclose( pidf );
	}

    // create sensor log thread
    pthread_create(&sensorthreadid, NULL, sensorlog_thread, NULL); 
    
    // create tab102 log thread
    if( tab102b_enable ) {
        pthread_create(&tab102b_threadid, NULL, tab102b_thread, NULL); 
    }
    
    printf( "GLOG: started!\n");

	return 1;
}


// app finish, clean up
void appfinish()
{
    // end of sensor log thread,
    if( sensorthreadid != 0 ) {
        pthread_join( sensorthreadid, NULL );
    }
    sensorthreadid = 0 ;
    
    if( tab102b_threadid!=0 ) {
        pthread_join( tab102b_threadid, NULL );
    }
    tab102b_threadid = 0 ;
    
	// close serial port
	gps_close();

    // close log file
    gps_logclose();

    // clean up shared memory
    p_dio_mmap->gps_valid = 0 ;
    p_dio_mmap->gps_speed = 0 ;			// knots
    p_dio_mmap->gps_direction = 0 ;     // degree
    p_dio_mmap->gps_latitude=0 ;		
    p_dio_mmap->gps_longitud=0 ;
    p_dio_mmap->gps_gpstime=0 ;
    p_dio_mmap->glogpid=0;
    p_dio_mmap->usage--;

    munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    
    // delete pid file
	unlink( pidfile );
    
	printf( "GPS logging process ended!\n");
}

int main()
{
    int r ;
    int validgpsbell=0 ;
    struct gps_data gpsdata ;

    app_state=1 ;
    // setup signal handler	
	signal(SIGQUIT, sig_handler);
	signal(SIGINT,  sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGPIPE, sig_handler);

	if( appinit()==0 ) {
		return 1;
	}
    
	glog_poweroff = 1 ;         // start from power off
    
    memset( &gpsdata, 0, sizeof(gpsdata) );
    validgpsdata = gpsdata ;

	while( app_state ) {
        if( sigcap ) {
            p_dio_mmap->gps_valid = 0 ;
            if( sigcap&0x8000 ) {       // Quit signal ?
                app_state=0 ;
            }
            else {
                if( sigcap&1 ) {       // SigUsr1? 
                    app_state=2 ;           // suspend running
                }
                if( sigcap&2 ) {
                    if( app_state==1 ) {
                        app_state=0 ;       // to let sensor thread exit
                        appfinish();    
                        appinit();
                    }
                    app_state=1 ;
                }
            }
            sigcap=0 ;
        }

        if( app_state==1 && gps_port_disable==0 ) {
            r=gps_readdata(&gpsdata) ;
            if( r==1 ) {      // read a GPRMC sentence?
                // update dvrsvr OSD speed display
                dio_lock();
                p_dio_mmap->gps_speed=gpsdata.speed ;
                p_dio_mmap->gps_direction=gpsdata.direction ;
                p_dio_mmap->gps_latitude=gpsdata.latitude ;
                p_dio_mmap->gps_longitud=gpsdata.longitude ;
                struct tm ttm ;
                ttm.tm_sec = 0;
                ttm.tm_min = gpsdata.minute ;
                ttm.tm_hour = gpsdata.hour ;
                ttm.tm_mday = gpsdata.day ;
                ttm.tm_mon = gpsdata.month-1 ;
                ttm.tm_year = gpsdata.year-1900 ;
                ttm.tm_wday=0 ;
                ttm.tm_yday=0 ;
                ttm.tm_isdst=-1 ;
                p_dio_mmap->gps_gpstime = (double) timegm(&ttm) + gpsdata.second ;
                p_dio_mmap->gps_valid = gpsdata.valid ;
                dio_unlock();
                // log gps position
                if( gpsdata.valid ) {
                    validgpsdata = gpsdata ;
                    gps_log();
                }
            }
            else if( r==0 ) {
                p_dio_mmap->gps_valid=0 ;
            }
            
            // sound the bell
            if( gpsdata.valid ) {
                if( validgpsbell==0 ) {
                    buzzer( 2, 300, 300 );
                    validgpsbell=1;
                }
            }
            else {
                if( validgpsbell==1 ) {
                buzzer( 3, 300, 300 );
                validgpsbell=0;
                }
            }
        }
        else {
            gps_close();
            gps_logclose();
            usleep(10000);
        }
    }
    
	appfinish();

	return 0;
}
