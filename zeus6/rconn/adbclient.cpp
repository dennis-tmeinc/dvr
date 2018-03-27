#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#include "rconn.h"
#include "net.h"
#include "adbclient.h"

#define ADB_PORT  5037
#define ADB_PORT_S "5037"

static int	adb_track_socket = 0;
static int  adb_device_num = 0 ;		// number of online device
static struct pollfd * adb_track_sfd ;
#define MAX_ADBCONN	(4)
static adb_conn * adbconn[MAX_ADBCONN] ;

int usb_resetdev(int bus, int dev)
{
    char usbf[64];
    int fd;
    int rc;
    
    sprintf(usbf, "/dev/bus/usb/%03d/%03d", bus, dev );

    fd = open(usbf, O_WRONLY);
    if (fd < 0) {
        perror("Error opening output file");
        return 1;
    }

    rc = ioctl(fd, USBDEVFS_RESET, 0);
    if (rc < 0) {
        perror("Error in ioctl");
    }
    else {
		printf("Reset successful\n");
	}

    close(fd);
    return 0;
}

int usb_reset()
{
	usb_resetdev(1,1);
}

static int adb_local_connect()
{
	return net_connect("127.0.0.1", ADB_PORT);
}

static void adb_senddata(int fd, char * data)
{
	int len ;
	char lpre[8] ;
	len = strlen( data ) ;
	if( len<1 || len>1000 ) {
		return ;
	}
	sprintf( lpre, "%04x", len ) ;
	
	net_sendx( fd, lpre, 4 ) ;
	net_sendx( fd, data, len );
}

// return -1 for error
static int adb_recvdata(int fd, char * data, int maxsize)
{
	int len ;
	char lpre[8] ;
	len = net_recvx( fd, lpre, 4 );
	if( len!=4 ) return -1 ;
	lpre[4] = 0 ;
	sscanf(lpre, "%x", &len);
	if( len<0 && len>=maxsize ) return -1 ;
	if( len>0 ) 
		return net_recvx( fd, data, len );
	else 
		return 0 ;
}

static int adb_status(int fd)
{
    unsigned char buf[4];
	if( net_rrdy( fd, 2000000 ) && net_recvx( fd, buf, 4 ) == 4 ) {
		return (memcmp(buf, "OKAY", 4)==0) ;
	}
	else {
		return 0 ;
	}
}

static int adb_service( int fd, char * service, char * device = NULL )
{
	if( device && strncmp( service, "host", 4 ) != 0) {
		// set transport
		char transport[128] ;
		sprintf( transport, "host:transport:%s", device );
		if( !adb_service( fd, transport, NULL ) ) {
			return 0 ;
		}
	}
	adb_senddata(fd, service) ;
	return adb_status(fd) ;
}

static void adb_startserver()
{
	printf("Start adb server.\n");
	system("/davinci/dvr/adb start-server");
}

// scan/wait for android device
static int adb_scan_device()
{
	int i;
	int online = 0 ;
	if( net_rrdy( adb_track_socket ) ) {
	
		char data[1024] ;
		int l, n ;
		l = adb_recvdata( adb_track_socket, data, 1023 );
		if( l>=0 ) {

			adb_conn * nadbconn[MAX_ADBCONN] ;
			int na = 0 ;

			data[l] = 0 ;
			
			printf( "Track-Data:%s\n", data );
			
			n=0;
			while( l>n && na<MAX_ADBCONN ) {
				int x = 0 ;
				char device_serial[100] ;
				char device[32] ;
				if( sscanf( data+n, "%99s %31s%n", device_serial, device, &x ) >= 2 ) {
					if( strcasecmp( device, "device" )==0 ) {
						// found online device
						online ++ ;
						
						printf("Found online device: %s\n", device_serial );
						
						int found=0;
						for( i=0; i<MAX_ADBCONN; i++ ) {
							if( adbconn[i] != NULL && strcmp( adbconn[i]->m_serialno,  device_serial )==0 ) {
								nadbconn[na++] = adbconn[i] ;
								adbconn[i]=NULL ;
								found=1 ;
								break;
							}
						}
						
						if( !found ) {
							// create a new connection
							nadbconn[na++] = new adb_conn(device_serial);
						}
						
					}
					n+=x ;
				}
				else {
					break ;
				}
			}
			
			for( i=0; i<MAX_ADBCONN; i++ ) {
				if( adbconn[i] ) {
					delete adbconn[i] ;
				}
				if( i< na ) 
					adbconn[i] = nadbconn[i] ;
				else
					adbconn[i] = NULL ;

			}

		}
		else {
			adb_reset();
		}
		
		printf("Online: %d\n", online );

	}
	
	return online ;
}

static int adb_test( char * serialno )
{
	int s = adb_local_connect();
	if( s>0 ) {
		if( adb_service( s, "shell:echo ghfc", serialno ) ) {
			// give pwv service some time to start
			if( net_rrdy( s, 3000000) ) {
				char rdata[10] ;
				if( net_recv( s, rdata, 10 )>=4 ) {
					if( memcmp( rdata, "ghfc", 4)==0 ) {
						close(s);
						return 1 ;
					}
				}
			}
		}
		close(s);
	}
	return 0 ;
}

int adb_conn::connect_server()
{
	int s = -1 ;
	if( *m_serialno ) {
		s = adb_local_connect();
		if( s>0 ) {
			// tcp connect to local
			char adbcmd[20] ;
			sprintf(adbcmd, "tcp:%d", g_port );
			if( !adb_service( s, adbcmd, m_serialno ) ) {
				close( s ) ;
				s = -1 ;

				// tcp connection error!
				printf("USB TCP failed!\n");
				
				if( adb_test(m_serialno)==0 ) {
					// adb echo test failed!
					usb_reset();
				}
				*m_serialno = 0;

			}
			else {
				// connected
				g_maxwaitms=10;
				printf("server :%d connected\n", g_port);
				activetime = g_runtime ;
			}
		}
	}

	return s ;
}

adb_conn::adb_conn( char * serialno )
	:rconn()
{
	int l = strlen( serialno );
	m_serialno = new char [l+2] ;
	strcpy( m_serialno, serialno ) ;
	
	maxidle=10000;
	
	if( adb_test( m_serialno )==0 ) {
		*m_serialno = 0 ;
		return ;
	}

	// use shell command to start pwv service
	int s = adb_local_connect();
	if( s>0 ) {
		if( adb_service( s, "shell:svc power stayon usb ;am broadcast -n com.tme_inc.pwv/.PwBootReceiver;echo --eos", m_serialno ) ) {
			// give pwv service some time to start
			char rsp[256] ;

			while( net_rrdy( s, 10000000) ) {
				int l = net_recv( s, rsp, 255 ) ;
				if( l>0 ) {
					rsp[l] = 0;
					if( strstr(rsp,"--eos") ) {
						break;
					}
				}
				else {
					break;
				}
			}
			printf( "Android device (%s) connected!\n", m_serialno);
		}
		else {
			printf("Device service failed!\n");
		}
		close( s ) ;
	}
}

adb_conn::~adb_conn()
{
	if( m_serialno )
		delete m_serialno ;
}

int adb_setfd( struct pollfd * pfd, int max )
{
	int nfd = 0 ;
	adb_track_sfd=NULL;

	if( adb_track_socket<= 0 ) {
		
		usb_reset();
		
		// create tracking socket
		
		adb_track_socket = adb_local_connect();
		if( adb_track_socket<=0 ) {
			adb_startserver();					// start adb server, just in case it's not up yet
			return 0;
		}

		if( !adb_service( adb_track_socket, "host:track-devices", NULL ) ) {
			adb_reset();
			return 0;
		}

	}
	
	if( max > nfd && adb_track_socket > 0 ) {
		pfd->events = POLLIN ;
		pfd->revents = 0 ;
		pfd->fd = adb_track_socket ;
		adb_track_sfd = pfd ;
		nfd ++ ;
	}
	
	for( int i=0; i<MAX_ADBCONN && max>nfd ; i++ ) {
		if( adbconn[i] ) {
			nfd += adbconn[i]->setfd( pfd+nfd, max-nfd ); 
		}
	}

	return nfd ;
}

void adb_process()
{
	for( int i=0; i<MAX_ADBCONN; i++ ) {
		if( adbconn[i] ) {
			adbconn[i]->process();
			
			// new created tracking socket
			if( g_maxwaitms > 20000 ) 
				g_maxwaitms = 20000 ;			
		}
	}

	if( adb_track_socket>0 && adb_track_sfd && adb_track_sfd->fd == adb_track_socket && adb_track_sfd->revents ) {
		adb_scan_device();
	}
	
}


void adb_reset()
{
	for( int i=0; i<MAX_ADBCONN; i++ ) {
		if( adbconn[i] ) {
			delete adbconn[i] ;
			adbconn[i]=NULL;
		}
	}
	
	if( adb_track_socket>0 ) {
		close(adb_track_socket);
		adb_track_socket = -1 ;
	}
}
