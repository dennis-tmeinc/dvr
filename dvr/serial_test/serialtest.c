#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include "eagle32/davinci_sdk.h"

#define BAUDTABLESIZE (23)

struct baud_table_t {
	speed_t baudv ;
	int baudrate ;
} baud_table[BAUDTABLESIZE] = {
	{ B1200, 	1200},
    { B2400, 	2400},
	{ B4800,	4800},
	{ B9600,	9600},
	{ B19200,	19200},
	{ B38400,	38400},
	{ B57600,	57600},
	{ B115200,	115200},
    { B230400,  230400},
    { B460800,  460800},
    { B500000,  500000},
    { B576000,  576000},
    { B921600,  921600},
    { B1000000, 1000000},
    { B1152000, 1152000},
    { B1000000, 1000000},
    { B1500000, 1500000},
    { B2000000, 2000000},
    { B2500000, 2500000},
    { B3000000, 3000000},
    { B3500000, 3500000},
    { B2000000, 2000000},
    { B4000000, 4000000}
} ;

int recvready(int fd, int microsecondsdelay)
{
	struct timeval timeout ;
	fd_set fds;
	if( fd<0 ) {
		return 0;
	}
	timeout.tv_sec = microsecondsdelay/1000000 ;
	timeout.tv_usec = microsecondsdelay%1000000 ;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
		return FD_ISSET(fd, &fds);
	} else {
		return 0;
	}
}

int sendready(int fd, int microsecondsdelay)
{
	struct timeval timeout ;
	fd_set fds;
	if( fd<0 ) {
		return 0;
	}
	timeout.tv_sec = microsecondsdelay/1000000 ;
	timeout.tv_usec = microsecondsdelay%1000000 ;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, NULL, &fds, NULL, &timeout) > 0) {
		return FD_ISSET(fd, &fds);
	} else {
		return 0;
	}
}

int openport( char * port, int baud )
{
	int i;
	int fd ;
	int eagle485=0;

	// initilize serial port
	if( strcmp( port, "/dev/ttyS1")==0 ) {
		eagle485=1 ;
	}
	fd = open( port, O_RDWR | O_NOCTTY | O_NDELAY );
	if( fd > 0 ) {
 		fcntl(fd, F_SETFL, 0);
		if( eagle485 ) {		// eagle485
			// Use Hikvision API to setup serial port (RS485)
			InitRS485(fd, baud, DATAB8, STOPB1, NOPARITY, NOCTRL);
		}
		else {
			struct termios tios ;
			speed_t baud_t ;
			memset( &tios, 0, sizeof(tios) );
			tcgetattr(fd, &tios);
			// set serial port attributes
			tios.c_cflag = CS8|CLOCAL|CREAD;
			tios.c_iflag = IGNPAR;
			tios.c_oflag = 0;
			tios.c_lflag = 0;       // ICANON;
			tios.c_cc[VMIN]=0;		// minimum char 0
			tios.c_cc[VTIME]=2;		// 0.2 sec time out
			baud_t = B115200 ;
			for( i=0; i<BAUDTABLESIZE; i++ ) {
				if( baud <= baud_table[i].baudrate ) {
					baud_t = baud_table[i].baudv ;
					break;
				}
			}

			cfsetispeed(&tios, baud_t);
			cfsetospeed(&tios, baud_t);
			
			tcflush(fd, TCIOFLUSH);
			tcsetattr(fd, TCSANOW, &tios);
		}
	}
	else {
		// even no serail port, we still let process run
		printf("Serial port failed!\n");
	}
	return fd;
}


unsigned int getms()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    return ( (tv.tv_sec%(3600*48))*1000 + tv.tv_usec/1000 ) ;
}

int main(int argc, char * argv[])
{
    static  char serial_dev[256] = "/dev/ttyS1" ;
    int serial_handle ;
    int baud = 115200;
    char buf[300]  ;
    int n, i ;
    int testwrite=0 ;	// default no write test.
    unsigned int t1, t2 ;

	if( argc>1 ) {
		strcpy( serial_dev, argv[1] );
	}
	if( argc>2 ) {
		baud = atoi( argv[2] );
	}
        if( argc>3 ) {
                if( argv[3][0]=='w' ) {
                     testwrite=1 ;
                }
         }

	serial_handle = openport( serial_dev, baud );
	if( serial_handle<=0 ) {
		return 1;
	}
    

    t1=getms();
    
     while( 1 ) {
        if( recvready( serial_handle, 1000 ) ) {
            n = read( serial_handle, buf, 300 );
            if( n>0 ) {
                write( 1, buf, n ); 
            }
        }
        if( recvready( 0, 1000 ) ) {
            n = read( 0, buf, 300 );
            if( n>0 ) {
                write( serial_handle, buf, n );
            }
        }
        if( testwrite &&
            sendready( serial_handle, 1000 )) 
        {
            t2 = getms();
            if( (t2-t1) > 1 ) {
            // generate data
            i++ ; 
            if( i>70 ) i=0 ;
            for( n=0 ; n<70; n++ ) {
               buf[n] = '0' + (n+i)%10 ;
            }
            buf[n++]='\r' ;
            buf[n++]='\n' ;
            buf[n]=0 ;
            write( serial_handle, buf, n ) ;
            }
        }
     }
  
	close( serial_handle );

	return 0;
}

