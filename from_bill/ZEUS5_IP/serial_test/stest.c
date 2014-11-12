#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include "eagle32/davinci_sdk.h"

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

char serial_devout[256] = "/dev/ttyS1" ;
char serial_devin[256] = "/dev/ttyUSB1" ;
int serial_out ;
int serial_in ;
int baud ;

int serial_dataready(int fd, int microsecondsdelay)
{
	struct timeval timeout ;
	fd_set fds;
	if( fd<=0 ) {
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
		if( baud<2400 || baud>115200 ) {
			baud=115200 ;
		}
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
			for( i=0; i<7; i++ ) {
				if( baud == baud_table[i].baudrate ) {
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

//        0 : failed
//        1 : success
int appinit()
{
	serial_out = openport( serial_devout, baud );
	serial_in  = openport( serial_devin,  baud );
	return 1 ;
}

// app finish, clean up
void appfinish()
{
	close( serial_out );
	close( serial_in );
}

int main(int argc, char * argv[])
{
	char buf1[100]  ;
	char buf2[100] ;

	if( argc>1 ) {
		strcpy( serial_devout, argv[1] );
	}
	if( argc>2 ) {
		strcpy( serial_devin, argv[2] );
	}
	if( argc>3 ) {
		baud = atoi( argv[3] );
	}

	if( appinit()==0 ) {
		return 1;
	}
	
	memset( buf1, 0x55, 100 );
	strcpy( buf1, "UU Hello!" );
	memset( buf2, 0, 100 );
	write( serial_out, buf1, 100 );
	if( serial_dataready( serial_in, 10000 ) ) {
		read( serial_in, buf2, 100 );
		printf( "%s\n", buf2 );
	}

	appfinish();

	return 0;
}

