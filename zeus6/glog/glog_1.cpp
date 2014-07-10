
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
char serial_dev[256] = "/dev/ttyS0" ;
int baudrate ;
int serial_handle = 0;
int glog_poweroff=1 ;           // start from power off
string dvrcurdisk;

int inputdebounce=3 ;

#define MAXSENSORNUM    (32)

string sensorname[MAXSENSORNUM] ;
int    sensor_invert[MAXSENSORNUM] ;
int    sensor_value[MAXSENSORNUM] ;
double sensorbouncetime[MAXSENSORNUM] ;
double sensorbouncevalue[MAXSENSORNUM] ;

#define DEFSERIALBAUD	(4800)

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;
char * pidfile = "/var/dvr/glog.pid" ;

int app_state ;     // 0: quit, 1: running: 2: restart

void sig_handler(int signum)
{
    if( signum == SIGUSR2 ) {
        app_state = 2 ;
    }
    else {
        app_state = 0 ;
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

int serial_open()
{
	int port = 0;
	int i;
	if( serial_handle>0 ) {
		return serial_handle ;		// opened already
	}
	if( strcmp( serial_dev, "/dev/ttyS1")==0 ) {
		port=1 ;
	}
	serial_handle = open( serial_dev, O_RDWR | O_NOCTTY | O_NDELAY );
	if( serial_handle > 0 ) {
 		fcntl(serial_handle, F_SETFL, 0);

		struct termios tios ;
		speed_t baud_t ;
		memset(&tios, 0, sizeof(tios));
		tcgetattr(serial_handle, &tios);
		// set serial port attributes
		tios.c_cflag = CS8|CLOCAL|CREAD;
		tios.c_iflag = IGNPAR;
		tios.c_oflag = 0;
		tios.c_lflag = 0;		// non - canonical
		tios.c_cc[VMIN]=0;		// minimum char 0
		tios.c_cc[VTIME]=0;		// 0.1 sec time out
		baud_t = B115200 ;
		for( i=0; i< (int)(sizeof(baud_table)/sizeof(baud_table[0])); i++ ) {
			if( baudrate >= baud_table[i].baudrate ) {
				baud_t = baud_table[i].baudv ;
				break;
			}
		}
		cfsetispeed(&tios, baud_t);
		cfsetospeed(&tios, baud_t);

		tcflush(serial_handle, TCIOFLUSH);
		tcsetattr(serial_handle, TCSANOW, &tios);

		if( port==1 ) {		// port 1
			// Use Hikvision API to setup serial port (RS485)
			InitRS485(serial_handle, baudrate, DATAB8, STOPB1, NOPARITY, NOCTRL);
		}
		tcflush(serial_handle, TCIOFLUSH);
	}
	else {
		serial_handle = 0 ;		// failed
	}
	return serial_handle ;
}

void serial_close()
{
	if( serial_handle ) {
		tcflush(serial_handle, TCIFLUSH);
		close( serial_handle );
		serial_handle = 0 ;
	}
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

int gps_readline( char * line, int linesize )
{
//	strcpy( line, "$GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68" );
	int i; 
	char c ;
	if( serial_open() <= 0 ) {					// open serial port
		sleep(1);
		return 0 ;					
	}
	for( i=0 ;i<linesize; ) {
		if( serial_dataready(2000000) ) {
			if( serial_read( &c, 1)==1 ) {
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
	serial_close();
    usleep(10000);
	return 0;
}

unsigned int gps_checksum( char * line )
{
    unsigned char sum = 0 ;
	int i;
	for( i=1; i<300; i++ ) {
		if( line[i]=='*' ) {
			return (unsigned int)sum ;
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
//   0: no GPRMC sentence or sentence error
int gps_read(struct gps_data * gpsdata)
{
	char line[300] ;		
	int i, f;
	int field[20] ;
	f=0 ;		
	if( gps_readline( line, sizeof(line) ) ) {
		if( memcmp(line, "$GPRMC,", 7)==0 ) {
			field[0] = 7 ;
			for( i=7; i<300; i++ ) {
				if( f>17 ) {
					break;
				}
				else if( line[i]==',' ) {		// field breaker
					f++ ;
					field[f]=i+1;
				}
				else if( line[i]=='*' ) {		// check sum field
					f++ ;
					field[f] = i ;
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

FILE * gps_logfile ;
static int gps_logday ;
struct gps_data validgpsdata ;

void gps_logclose()
{
	if( gps_logfile ) {
		fclose( gps_logfile );
		gps_logfile=NULL;
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
        
        if( gpslogfilename[0] != 0 ) {
            // try rename exist "*N.001" to "L.001", so we can reuse same file
            strncpy( curdisk, gpslogfilename, sizeof(curdisk) );
            i=strlen( gpslogfilename ) ;
            curdisk[i-5] = 'N' ;
            rename( curdisk, gpslogfilename );
            
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
	
    // log g sensor value
    if( p_dio_mmap->g_sensor_log0 ) {
        gps_logprintf(16, ",%.1f,%.1f,%.1f", 
                      p_dio_mmap->g_sensor_forward_0,
                      p_dio_mmap->g_sensor_right_0,
                      p_dio_mmap->g_sensor_down_0 );
        p_dio_mmap->g_sensor_log0=0;
    }
    if( p_dio_mmap->g_sensor_log1 ) {
        gps_logprintf(16, ",%.1f,%.1f,%.1f", 
                      p_dio_mmap->g_sensor_forward_1,
                      p_dio_mmap->g_sensor_right_1,
                      p_dio_mmap->g_sensor_down_1 );
        p_dio_mmap->g_sensor_log1=0;
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

int tab102b_init()
{
    
}

// log sensor event
int	tab102b_log()
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
	
    // log g sensor value
    if( p_dio_mmap->g_sensor_log0 ) {
        gps_logprintf(16, ",%.1f,%.1f,%.1f", 
                      p_dio_mmap->g_sensor_forward_0,
                      p_dio_mmap->g_sensor_right_0,
                      p_dio_mmap->g_sensor_down_0 );
        p_dio_mmap->g_sensor_log0=0;
    }
    if( p_dio_mmap->g_sensor_log1 ) {
        gps_logprintf(16, ",%.1f,%.1f,%.1f", 
                      p_dio_mmap->g_sensor_forward_1,
                      p_dio_mmap->g_sensor_right_1,
                      p_dio_mmap->g_sensor_down_1 );
        p_dio_mmap->g_sensor_log1=0;
    }
    
	return 1;
}


static void * tab102b_log_thread( void * param)
{
    while( app_state ) {
        if( app_state==1 ) {
            tab102b_log();
        }
        usleep(20000);
    }
    return NULL ;
}

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

    // init gps log 
    p_dio_mmap->usage++;
    p_dio_mmap->glogpid = getpid();
    p_dio_mmap->gps_valid = 0 ;
    p_dio_mmap->gps_speed = 0 ;			// knots
    p_dio_mmap->gps_direction = 0 ;     // degree
    p_dio_mmap->gps_latitude=0 ;		
    p_dio_mmap->gps_longitud=0 ;
    p_dio_mmap->gps_gpstime=0 ;
    
	// get serial port setting
	serialport = dvrconfig.getvalue( "glog", "serialport");
	if( serialport.length()>0 ) {
		strcpy( serial_dev, serialport.getstring() );
	}
	baudrate = dvrconfig.getvalueint("glog", "serialbaudrate");
	if( baudrate<1200 || baudrate>115200 ) {
		baudrate=DEFSERIALBAUD ;
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

    printf( "GLOG: started!\n");

	return 1;
}


// app finish, clean up
void appfinish()
{
	// close serial port
	serial_close();

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
    struct gps_data gpsdata ;
    pthread_t sensorthreadid ;

    app_state=1 ;
    // setup signal handler	
	signal(SIGQUIT, sig_handler);
	signal(SIGINT,  sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR2, sig_handler);

	if( appinit()==0 ) {
		return 1;
	}
    
	glog_poweroff = 1 ;         // start from power off
    
    memset( &gpsdata, 0, sizeof(gpsdata) );
    validgpsdata = gpsdata ;

    pthread_create(&sensorthreadid, NULL, sensorlog_thread, NULL); 

	while( app_state ) {
        if( app_state==2 ) {    // restart app ?
            appfinish();    
            appinit();
            app_state=1 ;
        }
		if( gps_read(&gpsdata) ) {      // read a GPRMC sentence?
            if( gpsdata.valid ) {
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
                dio_unlock();
				// log gps position
                validgpsdata = gpsdata ;
				gps_log();
            }
        }
        p_dio_mmap->gps_valid=gpsdata.valid ;
	}

    // end of sensor log thread,
    pthread_join( sensorthreadid, NULL );

	appfinish();

	return 0;
}
