#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "net.h"
#include "adbclient.h"

#define ADB_PORT  5037
#define ADB_PORT_S "5037"

static int adb_ready = 0  ;				// 0: not-ready, 1: wait-for-device, 2: ready
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

static int adb_recvdata(int fd, char * data, int maxsize)
{
	int len ;
	char lpre[8] ;
	net_recvx( fd, lpre, 4 );
	lpre[4] = 0 ;
	sscanf(lpre, "%x", &len);
	if( len>maxsize ) return 0 ;
	return net_recvx( fd, data, len );
}

static int adb_status(int fd)
{
    unsigned char buf[4];
	buf[0]=0;
	net_recvx( fd, buf, 4 );
    return (memcmp(buf, "OKAY", 4)==0) ;
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
//	system("adb start-server");
	if( fork()==0 ) {
		char * argv[] = {"adb", "-P", ADB_PORT_S, "fork-server", "server", NULL} ;
		execvp( argv[0], argv );
		exit(0);
	}
}

static int adb_wait()
{
	int s ;
	
	s = adb_local_connect();
	if( s<0 ) {
		adb_startservice();					// start adb server, just in case it's not up yet
		return 0 ;
	}

	if( !adb_service( s, "host:track-devices" ) ) {
		close( s );
		return 0 ;
	}
	
	// now waiting for device reponse
	char data[512] ;
	int online = 0 ;		// device online ?
	while( online==0 && net_rrdy( s, 1800*1000000 ) > 0 ) {
		int l, n ;
		l = adb_recvdata( s, data, 511 );
		data[l] = 0 ;
		n=0;
		while( l>n ) {
			int x = 0 ;
			char device[32] ;
			if( sscanf( data+n, "%63s %31s%n", adb_device, device, &x ) >= 2 ) {
				if( strcasecmp( device, "device" )==0 ) {
					// found online device
					online=1 ;
					break ;
				}
				n+=x ;
			}
			else {
				break ;
			}
		}
	}
	close(s);
	
	if( online == 0 ) {
		// no device, time out ?
		adb_device[0] = 0 ;
		return 0 ;
	}
	
	// device ready,

	// to start pwv service
	// use shell to start pwv service
	s = adb_local_connect();
	if( s>=0 ) {
		if( !adb_service( s, "shell:am broadcast -n com.tme_inc.pwv/.PwBootReceiver" ) ) {
			close( s ) ;
			return 0 ;
		}
		// give pwv service some time to start
		while( net_rrdy( s, 5000000) ) {
			if( net_recv( s, data, sizeof(data) )<=0 ) break ;
		}
		close( s );

		// android device ready!
		return 1 ;
	}
	else {
		return 0 ;
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

static void * wait_for_device( void * )
{
	int s ;
	adb_ready = 1 ;			// wait-for-device
	if( adb_wait() ) {
		adb_ready = 2 ;		// ready
	}
	else {
		sleep(1);
		adb_ready = 0 ;		// not-ready, will restart later
	}
	return NULL ;
}

int adb_conn::connect_server()
{
	if( adb_ready == 0 ) {
		// to start wait-for-device thread
		pthread_t wait_for_device_threadid ;
		pthread_attr_t attr;
		pthread_attr_init(&attr);

		adb_ready = 1 ;		// waiting
		if( pthread_create(&wait_for_device_threadid, &attr, wait_for_device, (void *)NULL) == 0 ) {
			pthread_detach(wait_for_device_threadid);
		}
		else {
			adb_ready = 0 ;	
		}
		return -1 ;				// not yet ready
	}
	else if( adb_ready == 1 ) {
		// keep waiting
		usleep(1000);
		return -1 ;
	}
	else {
		// adb_ready == 2 
		// wait - for - device completed
		int s = adb_connect(g_port) ;
		if( s<0 ) {
			adb_ready = 0 ;
			return -1 ;
		}
		else {
			return s;
		}
	}
}
