
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

#include "../dvrsvr/dir.h"


static struct baud_table_t {
    speed_t baudv ;
    int baudrate ;
} baud_table[] = {
    { B4800,	4800},
    { B9600,	9600},
    { B19200,	19200},
    { B38400,	38400},
    { B57600,	57600},
    { B115200,	115200},
    { 0, 0 }
} ;


#define MAXPORTNUM (100) 
char port_name[MAXPORTNUM][256] ;
int  port_handles[MAXPORTNUM] ;
int  port_num=0 ;


#define gps_port_handle ( mhandles[0] )
#define tab102b_port_handle ( mhandles[1] )

char gps_port_dev[256] = "/dev/ttyS0" ;
int gps_port_baudrate = 4800 ;      // gps default baud

int app_state ;     // 0: quit, 1: running: 2: restart

void sig_handler(int signum)
{
	app_state = 0 ;
}

int serial_open(int * handle, char * serial_dev, int serial_baudrate )
{
	int i;
    int serial_handle = *handle ;
	if( serial_handle > 0 ) {
		return serial_handle ;		// opened already
	}
    
    // check if serial device is stdin ?
    struct stat stdinstat ;
    struct stat devstat ;
    int r1, r2 ;
    r1 = fstat( 0, &stdinstat ) ; // get stdin stat
    r2 = stat( serial_dev, &devstat ) ;
    
    if( r1==0 && r2==0 && stdinstat.st_dev == devstat.st_dev && stdinstat.st_ino == devstat.st_ino ) { // matched stdin
        printf("Stdin match serail port!\n");
        serial_handle = dup(0);     // duplicate stdin
 		fcntl(serial_handle, F_SETFL, O_RDWR | O_NOCTTY );
    }
    else {
        serial_handle = open( serial_dev, O_RDWR | O_NOCTTY );
/*
        if(serial_handle>0) {
            close(serial_handle);       // do reopen to fix ttyUSB port bugs
            usleep(20000);
        }
        serial_handle = open( serial_dev, O_RDWR | O_NOCTTY );
*/
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
        baud_t = serial_baudrate ;
        i=0 ;
        while( baud_table[i].baudrate ) {
            if( serial_baudrate == baud_table[i].baudrate ) {
                baud_t = baud_table[i].baudv ;
                break;
            }
            i++ ;
        }
        cfsetispeed(&tios, baud_t);
        cfsetospeed(&tios, baud_t);
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
    int n=0;
	if( serial_handle>=0 ) {
        n = read( serial_handle, buf, bufsize);
    }
    return n ;
}

int serial_write(int serial_handle, void * buf, size_t bufsize)
{
    int n=0 ;
	if( serial_handle>=0 ) {
        n = write( serial_handle, buf, bufsize);
	}
	return n;
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

// Check if data ready to read on multiple serial ports
//     return 0: no data
//            1: yes
int serial_mready(int microsecondsdelay)
{
    int i ;
    int maxhandle=0 ;
	struct timeval timeout ;
	fd_set fds;
	timeout.tv_sec = microsecondsdelay/1000000 ;
	timeout.tv_usec = microsecondsdelay%1000000 ;
	FD_ZERO(&fds);
    for( i=0;i<port_num;i++) {
        if( port_handles[i]>0 ) {
            FD_SET( port_handles[i], &fds);
			if( port_handles[i]>maxhandle ) {
				maxhandle=port_handles[i] ;
			}
        }
    }
	if( maxhandle==0 ) {
		return 0 ;
	}
    return ( select(maxhandle + 1, &fds, NULL, NULL, &timeout) > 0) ;
}

// check gps line check sum
// 0: failed, 1: success
int gps_checksum( char * line )
{
    unsigned int checksum=0x55 ;
    unsigned char sum = 0 ;
	int i;
	if( *line != '$' ) 
		return 0 ;
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

//  Check if a complete GPS line can be read
//	GPS line example: "$GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68"
int gps_readline( int serial_handle )
{
	int  idx = 0 ;
    char gpsline[300] ;

	gpsline[0]=0 ;

    while( serial_dataready(serial_handle, 0) ) {
		if( serial_read(serial_handle, (void *)gpsline, 1)==1 ) {
			if( gpsline[0]=='$' ) {
				break;
			}
		}
	}	
	if( gpsline[0]!='$' )  {
		return 0 ;
	}

	while( serial_dataready(serial_handle, 500000) && ++idx<299 ) {
        if( serial_read(serial_handle, (void *)&gpsline[idx], 1)==1 ) {
            if( gpsline[idx]=='\n' ) {
				gpsline[idx]=0;
				return gps_checksum( gpsline ) ;   // got a gps line
            }
        }
		else {
			break ;
		}
    }

    return 0 ;                           // gps not ready!
}

// return time in seconds from first call to this function
unsigned long gettime()
{
	struct timeval tv ;
    gettimeofday( &tv, NULL );
    return (unsigned long)tv.tv_sec ;
}


int main()
{
	unsigned long t1, t2 ;
	
    app_state=1 ;
    // setup signal handler	

	signal(SIGTERM, sig_handler);

	printf("Testing : ");
	       
	port_num = 0 ;
	dir_find devfind ;
	devfind.open( "/dev/" ) ;
	while( port_num<MAXPORTNUM && devfind.find() ) {
		if( devfind.isdir() ) {
			continue ;
		}
		if( fnmatch( "ttyS*", devfind.filename(), 0 )==0 ) {
			strcpy( port_name[ port_num ], devfind.pathname() );
			port_num++ ;
			printf( "%s ", devfind.filename() );
		}
		else if( fnmatch( "ttyUSB*", devfind.filename(), 0 )==0 ) {
			strcpy( port_name[ port_num ], devfind.pathname() );
			port_num++ ;
			printf( "%s ", devfind.filename() );
		}
	}
	devfind.close();

	printf("\n...\n");

	int found = 0 ;
	int idx_baud ;
	for( idx_baud=0; idx_baud<10; idx_baud++ ) {
		int baud ;
		int numdev=0 ;
		baud = baud_table[idx_baud].baudrate ;
		if( baud < 100 ) {
			break;
		}
		printf("Testing baud rate : %d\n", baud );

		for( numdev=0; numdev<port_num; numdev++) {
			serial_open( &port_handles[numdev], port_name[numdev], baud );
		}

		t1=gettime();
		
		while( serial_mready(5000000) ) {
			int gpsport;
			for( gpsport=0; gpsport<port_num; gpsport++ ) {
				if( gps_readline( port_handles[gpsport] ) ) {
					printf("GPS found! on %s, baud rate %d\n", port_name[gpsport], baud );
					found = 1 ;
					break;
				}
			}
			if( found ) 
				break ;
			t2 = gettime();
			if( (t2-t1 )>6 ) 
				break;
		}

		for( numdev=0; numdev<port_num; numdev++) {
			serial_close( &port_handles[numdev] );
		}

		if( found ) break;
	}

	if( !found ) {
		printf( "Can't find GPS device!\n");
	}

	return 0;
}
