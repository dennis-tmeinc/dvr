#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "net.h"
#include "adbclient.h"

static int adb_local_connect()
{
    int s;
    struct sockaddr_un addr;
    socklen_t alen;
    size_t namelen;
    const char ads[]="ads0";

    s = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(s < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    namelen  = strlen(ads);

    addr.sun_path[0] = 0;
    memcpy(addr.sun_path + 1, ads, namelen);
    addr.sun_family = AF_LOCAL;
    alen = namelen + offsetof(struct sockaddr_un, sun_path) + 1;

    if(connect(s, (struct sockaddr *) &addr, alen) < 0) {
		close(s);
		return -1 ;
    }

    return s;
}

static int adb_status(int fd)
{
    unsigned char buf[5];
	buf[3]=0;
	net_recvx( fd, buf, 4 );
    return (memcmp(buf, "OKAY", 4)==0) ;
}

static void adb_send(int fd, char * data)
{
	int len ;
	char lpre[5] ;
	len = strlen( data ) ;
	if( len<1 || len>1024 ) {
		return ;
	}
	sprintf( lpre, "%04x", len ) ;
	
	net_sendx( fd, lpre, 4 ) ;
	net_sendx( fd, data, len );
}

static int adb_service( int fd, char * service )
{
	if( strncmp( service, "h-", 2 ) != 0 ) {
		// transport to adbd over usb
		if( !adb_service( fd, "h-u:t-u" ) ) {
			return 0 ;
		}
	}
	adb_send(fd, service) ;
	return adb_status(fd);
}

// Note : timeout cannot be more than 1800
static int adb_wait( int timeout )
{
	int s ;
	s = adb_local_connect();
	if( s<0 ) return 0 ;

	if( !adb_service( s, "h-u:w-f-u" ) ) {
		close( s );
		return 0 ;
	}
	// now waiting ...
	if( timeout<0 || timeout>1800 ) timeout=1800 ;
	if( net_rrdy( s, timeout*1000000 ) <=0 ) {
		// time out ?
		close( s );
		return 0 ;
	}
	// ready?
	if( ! adb_status(s) ) {
		close( s );
		return 0 ;
	}
	// device ready,
	// to start pwv service
	close( s );		// adservice would close the connection already

	// use shell to start pwv service
	s = adb_local_connect();
	if( s>=0 ) {
		if( !adb_service( s, "shell:am broadcast -n com.tme_inc.pwv/.PwBootReceiver" ) ) {
			close( s ) ;
			return 0 ;
		}
		// give pwv service some time to start
		while( net_rrdy( s, 3000000) ) {
			char tbuf[16] ;
			if( net_recv( s, tbuf, 16 )<=0 ) break ;
		}
		close( s );
		sleep(2);
	}
	else {
		return 0 ;
	}

	// android device ready!
	return 1 ;
}

static int adb_connect( int port )
{
	int s ;
	char tmp[20] ;
	s = adb_local_connect();
	if( s>=0 ) {
		// tcp connect to local
		snprintf(tmp, 20, "tcp:%d", port );
		if( !adb_service( s, tmp ) ) {
			close( s ) ;
			return -1 ;
		}	
		return s ;		// connected
	}
	return -1 ;
}

static int adb_ready = 0  ;				// 0: not-ready, 1: wait-for-device, 2: ready

static void * wait_for_device( void * )
{
	int s ;
	adb_ready = 1 ;			// wait-for-device
	if( adb_wait( 1800 ) ) {
		adb_ready = 2 ;		// ready
	}
	else {
		sleep(2);
		adb_ready = 0 ;		// not-ready
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
		usleep(1000);
		return -1 ;
	}
	else {
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
