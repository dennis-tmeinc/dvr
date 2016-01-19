#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "net.h"

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

#include "rconn.h"

#define TUNNEL_BUFSIZE (4096)

static int adb_local_connect()
{
    int s;
    struct sockaddr_un addr;
    socklen_t alen;
    size_t namelen;
    const char * name="ads0";

    s = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(s < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    namelen  = strlen(name);

    addr.sun_path[0] = 0;
    memcpy(addr.sun_path + 1, name, namelen);
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
		adb_send(fd, "h-u:t-u" );
		if( !adb_status(fd) ) {
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
	if( s>=0 ) {
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
		close( s );		// adservice would close the connection
	}
	else {
		return 0 ;
	}

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

void * adb_conn_thread(void *param)
{
	char cmd[20] ;
	char id[80] ;
	char source[20], target[20] ;
	int  sport=0, tport=0 ;
	int  tsock, ssock ;		// target, source socket
	
	sscanf( (char *)param , "%19s %79s %19s %d %19s %d", cmd, id, target, &tport, source, &sport  ) ;
	delete (char *)param ;				// release parameter
	
	if( sport==0 ) sport = g_port ;		// use default port
	ssock = adb_connect( sport ) ;

	// initial command
	if( ssock>=0 ) {
			
		if( target[0] == '*' ) {			// connect to localhost
			tsock = net_connect( "127.0.0.1", tport ) ;
		}
		else {
			tsock = net_connect( target, tport ) ;
		}
		
		if( tsock>=0 ) {
			net_sendx( ssock, (void *)"connected ", 10);
		}
		else {
			net_sendx( ssock, (void *)"close ", 6);
		}
		net_sendx( ssock, id, strlen(id));
		net_sendx( ssock, (void *)"\n", 1);
		
		rconn_loop( ssock, tsock );
		
	}
	
	return NULL;

}


// received a command from server, string terminated by "\0"
// 	command list
//      connect <id> <target> <tport> <source> <sport> 
//      ok	(empty command)
void adb_received( char * str )
{
	if( strncmp(str, "connect ", 8)==0 ) {		// this is the only command supported now
		// to create a new connection
		char * nstr = new char [strlen(str)+1] ;
		strcpy( nstr, str );
		pthread_t newthreadid ;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if( pthread_create(&newthreadid, &attr, adb_conn_thread, (void *)nstr) == 0 ) {
			pthread_detach( newthreadid ) ;	
		}
		else {
			delete nstr ;
		}
	}
	// ignore everything else
}

#define LINEBUFSIZE (1024)

int adb_client_running ;

void * adb_client_threadfunc(void *)
{
	int res ;
	int r ;
	char linebuf[LINEBUFSIZE] ;
	int  lineptr = 0 ;
	int  acttime, rtime ;	
	int  adb_conn = -1  ;
	int  xcount = 0 ;

	rtime = acttime = runtime();
	
	adb_client_running = 1 ;
	while( adb_client_running ) {
		if( adb_conn < 0 ) {
			
			printf("Wait for android device!\n");
			if( adb_wait(1200) ) {							// would wait for Android device ready
				adb_conn = adb_connect( g_port ) ;		
			}
		
			if( adb_conn<0 ) {
				sleep(2);
				continue;
			}
			printf("Connection ready!\n");
			
			rconn_reg( adb_conn );
			lineptr = 0 ;
		}
		
		res = net_rrdy( adb_conn, g_keepalive*1000000 ) ;
		rtime = runtime();

		if( res > 0 ) {
			// data ready
			if( lineptr<LINEBUFSIZE  ) {
				r=net_recv( adb_conn, &linebuf[lineptr], 1 ) ;
				if( r>0 ) {
					
					acttime = rtime ;

					if( linebuf[lineptr] == '\n' ) {		// end of line
						linebuf[lineptr] = '\0' ;			// replace with eol
						adb_received( linebuf ) ;
						lineptr = 0 ;						// reset line buffer
					}
					else {
						lineptr++ ;
					}
				}
				else {
					// failed ?
					close( adb_conn );						// try reconnect
					adb_conn = -1 ;
				}
			}
			else {											// line buffer overrun, 
				close( adb_conn ) ;
				adb_conn = -1 ;
			}
		}
		else if( res==0 ) {
			// time out
			if( rtime-acttime > 300 ) {
				close( adb_conn ) ;
				adb_conn = -1 ;
			}

			// time out, send a PING
			if( xcount++ < 15 ) {
				net_send( adb_conn, (void *)"p\n", 2 );
			}
			else {
				xcount=0 ;
				rconn_reg(adb_conn) ;
			}
			// reset line buffer
			lineptr = 0 ;

		}
		else {
			// error 
			close( adb_conn );
			adb_conn = -1 ;
		}
	}
	
	if( adb_conn >=0 ) close( adb_conn );
	
	return 0 ;
}

static pthread_t adb_client_threadid ;
void adb_client_init()
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(&adb_client_threadid, &attr, adb_client_threadfunc, (void *)NULL) ;
}

void adb_client_cleanup()
{
	adb_client_running=0 ;
	pthread_cancel(adb_client_threadid);
	pthread_join(adb_client_threadid, NULL);
}
