#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <signal.h>

#include "net.h"

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

#include "rconn.h"

#define APPNKEY "AQ7Ynq23JYCtyHWATwS9"

#define TUNNEL_BUFSIZE (4096)

string	g_server ;
int     g_port ;

static int     g_nointernetaccess ;	// no internet access
static int     s_running ;				// global running connections 
static string  g_did ;					// device id ( use mac addr )
static string  g_internetkey ;			// internet accessing key

const char default_server[] = "pwrev.us.to" ;
const int  default_port = 15600 ;

// return runtime in seconds
int runtime()
{
	struct timespec ts ;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int)(ts.tv_sec) ;
}

// tunneling
void rconn_loop( int ssock, int tsock )
{
	int  tbufptr=0, tbufsize=0 ;
	int  sbufptr=0, sbufsize=0 ;
	int  r;
	
	char * tbuffer = new char [TUNNEL_BUFSIZE] ;
	char * sbuffer = new char [TUNNEL_BUFSIZE] ;
	
	while( (ssock>=0 || tsock>=0) && s_running ) {
		if( sbufptr>=sbufsize && ssock<0 ) {
			break ;
		}
		if( tbufptr>=tbufsize && tsock<0 ) {
			break ;
		}

		struct pollfd fds[2];
		
		fds[0].events = 0 ;
		fds[0].revents = 0 ;
		fds[0].fd = ssock ;
		if( ssock>=0 ) {
			if( tbufsize>tbufptr )
				fds[0].events |= POLLOUT ;
			if( sbufsize<TUNNEL_BUFSIZE )
				fds[0].events |= POLLIN ;
		}
		
		fds[1].events = 0 ;
		fds[1].revents = 0 ;
		fds[1].fd = tsock ;
		if( tsock>=0 ) {
			if( sbufsize>sbufptr )
				fds[1].events |= POLLOUT ;
			if( tbufsize<TUNNEL_BUFSIZE )
				fds[1].events |= POLLIN ;
		}
		
		r = poll( fds, 2, 600000 );
		if( r<=0 ) {
			// time out or error
			break;
		}

		// r>0 

		// source receiving
		if( ssock>=0 &&  ( fds[0].revents & POLLIN ) && sbufsize<TUNNEL_BUFSIZE ) {
			r = net_recv( ssock, sbuffer+sbufsize, TUNNEL_BUFSIZE-sbufsize ) ;
			if( r>0 ) {
				sbufsize += r ;
			}
			else {
				close( ssock ) ;
				ssock = -1 ;
			}
		} 
		
		// target receiving
		if( tsock>=0 && ( fds[1].revents & POLLIN ) && tbufsize<TUNNEL_BUFSIZE ) {
			r = net_recv( tsock, tbuffer+tbufsize, TUNNEL_BUFSIZE-tbufsize ) ;
			if( r>0 ) {
				tbufsize += r ;
			}
			else {
				close( tsock ) ;
				tsock = -1 ;
			}
		} 
		
		// source sending
		if( ssock>=0 && ( fds[0].revents & POLLOUT ) && tbufsize>tbufptr ) {
			r = net_send( ssock, tbuffer+tbufptr, tbufsize-tbufptr ) ;
			if( r>=0 ) {
				tbufptr+=r ;
				if( tbufptr>=tbufsize ) {	// complete this buffer
					tbufsize = 0 ;
					tbufptr = 0 ;
				}
			}
			else {
				close( ssock ) ;
				ssock = -1 ;
			}
		}

		// target sending
		if( tsock>=0 &&  ( fds[1].revents & POLLOUT ) && sbufsize>sbufptr ) {
			r = net_send( tsock, sbuffer+sbufptr, sbufsize-sbufptr ) ;
			if( r>=0 ) {
				sbufptr+=r ;
				if( sbufptr>=sbufsize ) {	// complete this buffer
					sbufsize = 0 ;
					sbufptr = 0 ;
				}
			}
			else {
				close( tsock ) ;
				tsock = -1 ;
			}
		}
				
	}
	
	if( tsock >=0 )
		close( tsock );
	if( ssock >=0 )
		close( ssock );
	
	delete tbuffer ;
	delete sbuffer ;

}

static void *rconn_thread(void *param)
{
	char cmd[20] ;
	char id[80] ;
	char source[80], target[80] ;
	int  sport=0, tport=0 ;
	int  tsock, ssock ;		// target, source socket
	
	sscanf( (char *)(*(string *)param) , "%19s %79s %79s %d %79s %d", cmd, id, target, &tport, source, &sport ) ;
	delete (string *)param ;			// release parameter
	
	if( sport==0 ) sport = g_port ;		// use default port
	if( source[0] == '*' ) {			// use default source
		ssock = net_connect( g_server, sport ) ;
	}
	else {
		ssock = net_connect( source, sport ) ;
	}
	
	if( target[0] == '*' ) {			// connect to localhost
		tsock = net_connect( "127.0.0.1", tport ) ;
	}
	else {
		tsock = net_connect( target, tport ) ;
	}
	
	// initial command
	if( ssock>=0 ) {
		if( tsock>=0 ) {
			net_sendx( ssock, (void *)"connected ", 10);
		}
		else {
			net_sendx( ssock, (void *)"close ", 6);
		}
		net_sendx( ssock, id, strlen(id));
		net_sendx( ssock, (void *)"\n", 1);
	}
	
	rconn_loop( ssock, tsock );

	return NULL;
}


int g_keepalive = 60 ;

#define LINEBUFSIZE (1024)

// received a command from server, string terminated by "\0"
// 	command list
//      connect <id> <target> <tport> <source> <sport> 
//      ok	(empty command)
void rconn_received( char * str )
{
	if( strncmp(str, "connect ", 8)==0 ) {		// this is the only command supported now
		// to create a new connection
		string * argv = new string ( str );
		pthread_t   newthreadid ;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if( pthread_create(&newthreadid, &attr, rconn_thread, (void *)argv) == 0 ) {
			pthread_detach( newthreadid ) ;	
		}
		else {
			delete argv ;
		}
	}
	// ignore everything else
}

void app_init();

int rconn_reg(int s)
{
	char hostname [256] ;
	char buf[512] ;
	gethostname(hostname, 255);
	hostname[255]=0 ;
	sprintf( buf, "unit %s %s %s\n", hostname , (char *)g_did, (char *)g_internetkey );
	net_sendx( s, (void *)buf, strlen(buf) );
}

int rconn_run()
{
	int res ;
	int r ;
	char linebuf[LINEBUFSIZE] ;
	int  lineptr = 0 ;
	int  acttime, rtime ;	
	int s_rconn = -1  ;
	int  xcount = 0 ;

	rtime = acttime = runtime();
	
	s_running = 1 ;
	while( s_running ) {
		if( s_rconn < 0 ) {
			s_rconn = net_connect( g_server, g_port ) ;
			
			if( s_rconn<0 ) {
				sleep(30);
				continue;
			}
			
			rconn_reg( s_rconn );
			lineptr = 0 ;
		}
		
		res = net_rrdy( s_rconn, g_keepalive*1000000 ) ;
		rtime = runtime();

		if( res > 0 ) {
			// data ready
			if( lineptr<LINEBUFSIZE  ) {
				r=net_recv( s_rconn, &linebuf[lineptr], 1 ) ;
				if( r>0 ) {
					acttime = rtime ;
					if( linebuf[lineptr] == '\n' ) {		// end of line
						linebuf[lineptr] = '\0' ;			// replace with eol
						rconn_received( linebuf ) ;
						lineptr = 0 ;						// reset line buffer
					}
					else {
						lineptr++ ;
					}
				}
				else {
					// failed ?
					close( s_rconn );						// try reconnect
					s_rconn = -1 ;
				}
			}
			else {											// line buffer overrun, 
				close( s_rconn ) ;
				s_rconn = -1 ;
			}
		}
		else if( res==0 ) {
			// time out
			if( rtime-acttime > 300 ) {
				close( s_rconn ) ;
				s_rconn = -1 ;
			}

			// time out, send a PING
			if( xcount++ < 15 ) {
				net_sendx( s_rconn, (void *)"p\n", 2 );
			}
			else {
				xcount=0 ;
				rconn_reg(s_rconn) ;
			}
			// reset line buffer
			lineptr = 0 ;

		}
		else {
			// error 
			close( s_rconn );
			s_rconn = -1 ;
		}
	}
	
	if( s_rconn >=0 ) close( s_rconn );
	
	return 0 ;
}

void rc_init()
{
	int    iv ;
	string v ;

    config dvrconfig(CFG_FILE);

    // get remote tunnel server
    v = dvrconfig.getvalue("network", "rcon_server") ;
    if( v.isempty() ) {
		g_server = default_server ;
	}
	else {
		g_server = v ;
	}
	
    // get remote tunnel port
    iv = dvrconfig.getvalueint("network", "rcon_port") ;
    if( iv==0 ) {
		g_port = default_port ;
	}
	else {
		g_port = iv ;
	}

	g_nointernetaccess = dvrconfig.getvalueint("system", "nointernetaccess") ;
	if(g_nointernetaccess) {
		g_internetkey = APPNKEY ;
	}
	else {
		g_internetkey = dvrconfig.getvalue("system", "internetkey") ;
	}

	// device id
	FILE * fdid = fopen( APP_DIR"/did", "r");
	
	if( fdid==NULL ) {
		fdid = fopen( APP_DIR"/did", "w" );
		if( fdid ) {
			FILE * fr ;
			fr = fopen("/sys/class/net/eth0/address", "r");
			if( fr ) {
				fscanf( fr, "%50s", (char *)v.expand( 60 ) );
				fprintf( fdid, "%s", (char *)v );
				fclose( fr );
			}
			
			fr = fopen("/dev/urandom", "r" );
			if( fr ) {
				fread( &iv, sizeof(iv), 1, fr );
				fprintf( fdid, "%x", iv );
				fread( &iv, sizeof(iv), 1, fr );
				fprintf( fdid, "%x", iv );
				fclose( fr );
			}
			fclose( fdid );
		}
		fdid = fopen( APP_DIR"/did", "r");
	}
	
	if( fdid ) {
		fscanf( fdid, "%250s", (char *)v.expand( 256 ) );
		g_did = v ;
		fclose( fdid );
	}
		
}

static void s_handler(int signum)
{
	s_running = 0 ;
}

#ifdef RCMAIN
void rc_main()
{
	if( fork()== 0 ) {
		signal(SIGINT, s_handler);
		signal(SIGTERM, s_handler);		
		rc_init();
		g_internetkey = "V5d0DgUgu?f51u5#i3FV" ;
		g_did = g_did+"V" ;
		rconn_run();
	}
}
#else 
int main() 
{
	// setup signal
	signal(SIGINT, s_handler);
	signal(SIGTERM, s_handler);
	rc_init();
#ifdef ANDROID_CLIENT
	adb_client_init();
#endif	
	rconn_run();
#ifdef ANDROID_CLIENT
	adb_client_cleanup();
#endif	
	return 0 ;
}
#endif
