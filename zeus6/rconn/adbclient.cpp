#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "rconn.h"
#include "net.h"
#include "adbclient.h"

#define ADB_PORT  5037
#define ADB_PORT_S "5037"

static char adb_device[64] ;

static int adb_local_connect()
{
	return net_connect("127.0.0.1", ADB_PORT);
}

static void adb_senddata(int fd, char * data)
{
	int len ;
	char lpre[8] ;
	len = strlen( data ) ;
	if( len<1 || len>200 ) {
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
	if( net_recvx( fd, buf, 4 ) == 4 ) {
		return (memcmp(buf, "OKAY", 4)==0) ;
	}
	else {
		return 0 ;
	}
}

static int adb_service( int fd, char * service )
{
	if( strncmp( service, "host", 4 ) != 0  && adb_device[0] != 0 ) {
		// set transport
		char transport[128] ;
		sprintf( transport, "host:transport:%s", adb_device );
		if( !adb_service( fd, transport ) ) {
			return 0 ;
		}
	}
	adb_senddata(fd, service) ;
	return adb_status(fd) ;
}

static void adb_startservice()
{
	printf("Start adb service.\n");
	
	system("/davinci/dvr/adb start-server");
}

static int adb_ready ;				// 1: ready, 0: waiting
static int adb_wait_socket = -1 ;

// scan/wait for android device
static void adb_wait_for_device()
{
	int s ;
	if( adb_wait_socket <= 0 ) {
		adb_device[0] = 0 ;					
		s = adb_local_connect();
		if( s<=0 ) {
			adb_startservice();					// start adb server, just in case it's not up yet
			return ;
		}

		if( !adb_service( s, "host:track-devices" ) ) {
			close( s );
			return ;
		}
		adb_wait_socket = s ;
	}
	
	int online = 0 ;
	char data[256] ;
	int l, n ;
	if( net_rrdy( adb_wait_socket ) ) {
		
		l = adb_recvdata( adb_wait_socket, data, 255 );
		if( l<0 ) {
			close(adb_wait_socket);
			adb_wait_socket = -1 ;
		}		
		else if( l>=0 ) {
			data[l] = 0 ;
			n=0;
			while( l>n ) {
				int x = 0 ;
				char device[32] ;
				if( sscanf( data+n, "%63s %31s%n", adb_device, device, &x ) >= 2 ) {
					if( strcasecmp( device, "device" )==0 ) {
						// found online device
						online = 1 ;
						
						printf("Found online device: %s\n", adb_device );
						
						break ;
					}
					n+=x ;
				}
				else {
					break ;
				}
			}
		}
	}
	if( online ) {
	
		// use shell command to start pwv service
		s = adb_local_connect();
		if( s>=0 ) {
			if( !adb_service( s, "shell:am broadcast -n com.tme_inc.pwv/.PwBootReceiver" ) ) {
				close( s ) ;
				return ;
			}
			// give pwv service some time to start
			while( net_rrdy( s, 2000000) ) {
				if( net_recv( s, data, sizeof(data) )<=0 ) break ;
			}
			close( s );

			// android device ready!
			adb_ready = 1 ;
		
			printf( "Android device ready!\n");
			
			// cloase waiting socket
			if( adb_wait_socket>0 ) {
				close(adb_wait_socket);
				adb_wait_socket = -1 ;
			}
		}
	}
}

static int adb_connect( int port )
{
	int s ;
	s = adb_local_connect();
	if( s>=0 ) {
		// tcp connect to local
		char adbcmd[16] ;
		snprintf(adbcmd, 20, "tcp:%d", port );
		if( !adb_service( s, adbcmd ) ) {
			close( s ) ;
			return -1 ;
		}	
		// connected
	}
	return s ;
}

int adb_conn::connect_server()
{
	if( adb_ready ) {
		// adb_ready == 2 
		// wait - for - device completed
		int s = adb_connect(g_port) ;
		if( s>=0 ) {
			return s ;
		}
		// not ready
		adb_ready = 0 ;
		if( adb_wait_socket>0 ) {
			close(adb_wait_socket);
			adb_wait_socket = -1 ;
		}
	}
			
	if( adb_wait_socket>0 ) {
		adb_wait_for_device() ;
		activetime = g_runtime ;
	}
	else if( g_runtime-activetime > 5 ) {		// don't retry scan in 5 sec
		adb_wait_for_device() ;
		activetime = g_runtime ;
	}
	// max wait time
	if( g_maxwaitms > 1000 ) {
		g_maxwaitms = 1000 ;
	}	
	return -1 ;
}
