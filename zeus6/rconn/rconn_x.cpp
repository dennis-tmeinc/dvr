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

int     g_nointernetaccess ;	// no internet access
int     g_running ;			// global running connections 
string  g_server ;
int     g_port ;
string  g_hostname ;
int     g_id ;				// random generated id, to be replace with uuid

const char default_server[] = "pwrev.us.to" ;
const int  default_port = 15600 ;

// send a string to sever, string should terminated by '\n'
void rconn_send( int s, char * str )
{
	net_send( s, (void *)str, strlen(str) );
}

#define TUNNEL_BUFSIZE (4096)

void *rconn_thread(void *param)
{
	string * command = (string *)param ;
	char  *tbuffer ;
	int  tbufptr=0, tbufsize=0 ;

	char *sbuffer ;
	int  sbufptr=0, sbufsize=0 ;
	
	int  r ;
	char cmd[20] ;
	char source[80], target[80] ;
	int  id ;
	int  sport=0, tport=0 ;
	int  tsock, ssock ;		// target, source socket
	sscanf( *command, "%s %d %s %d %s %d", cmd, &id, target, &tport, source, &sport ) ;
	delete command ;	// release parameter
	
	if( sport==0 ) sport = g_port ;		// use default port
	if( source[0] == '*' ) {			// use default source
		ssock = net_connect( g_server, sport ) ;
	}
	else {
		ssock = net_connect( source, sport ) ;
	}
	
	if( ssock<0 ) {
		return NULL ;
	}
	
	if( target[0] == '*' ) {	// use default target
		tsock = net_connect( "127.0.0.1", tport ) ;
	}
	else {
		tsock = net_connect( target, tport ) ;
	}

	if( tsock<0 ) {
		close( ssock ) ;
		return NULL ;
	}
	
	tbuffer = new char [TUNNEL_BUFSIZE] ;
	sbuffer = new char [TUNNEL_BUFSIZE] ;
	
	sprintf( tbuffer, "connected %d\n", id );
	rconn_send( ssock, tbuffer );
	
	// now tunneling every over
	while( g_running ) {
		struct pollfd fds[2];
		fds[0].fd = tsock ;
		fds[0].events = 0 ;
		if( tbufsize==0 )
			fds[0].events |= POLLIN ;
		if( sbufsize>0 )
			fds[0].events |= POLLOUT ;
		fds[0].revents = 0 ;

		fds[1].fd = ssock ;
		fds[1].events = 0 ;
		if( sbufsize==0 )
			fds[1].events |= POLLIN ;
		if( tbufsize>0 )
			fds[1].events |= POLLOUT ;
		fds[1].revents = 0 ;
		
		r = poll( fds, 2, 10000 );
		if( r==0 ) continue ;
		else if( r<0 ) break ;
		// r>0 
			
		if( tbufsize==0 && ( fds[0].revents & POLLIN ) ) {
			r = net_recv( tsock, tbuffer, TUNNEL_BUFSIZE ) ;
			if( r>0 ) {
				tbufsize = r ;
				tbufptr = 0 ;
			}
			else {
				break ;
			}
		} 
		
		if( sbufsize==0 && ( fds[1].revents & POLLIN ) ) {
			r = net_recv( ssock, sbuffer, TUNNEL_BUFSIZE ) ;
			if( r>0 ) {
				sbufsize = r ;
				sbufptr = 0 ;
			}
			else {
				break ;
			}
		} 
		
		if( sbufsize>0 && ( fds[0].revents & POLLOUT ) ) {
			r = net_send( tsock, sbuffer+sbufptr, sbufsize-sbufptr ) ;
			if( r>=0 ) {
				sbufptr+=r ;
				if( sbufptr>=sbufsize ) {	// complete this buffer
					sbufsize = 0 ;
					sbufptr = 0 ;
				}
			}
			else {
				break;
			}
		}

		if( tbufsize>0 && ( fds[1].revents & POLLOUT ) ) {
			r = net_send( ssock, tbuffer+tbufptr, tbufsize-tbufptr ) ;
			if( r>=0 ) {
				tbufptr+=r ;
				if( tbufptr>=tbufsize ) {	// complete this buffer
					tbufsize = 0 ;
					tbufptr = 0 ;
				}
			}
			else {
				break;
			}
		}
	}
	close( tsock );
	close( ssock );
	
	delete tbuffer ;
	delete sbuffer ;

	return NULL;
}


int s_rconn = -1  ;
int g_keepalive = 60 ;
int g_watchdog = 0 ;

#define LINEBUFSIZE (1500)

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
			pthread_detach( newthreadid ) ;		// detach the thread
		}
	}
	// ignore everything else
	
	// clear watchdog
	g_watchdog = 0 ;
}

int rconn_run()
{
	int res ;
	int r ;
	char linebuf[LINEBUFSIZE] ;
	int  lineptr = 0 ;

	s_rconn = -1 ;

	g_running = 1 ;
	while( g_running ) {
		if( s_rconn < 0 ) {
			
			if( g_nointernetaccess ) {
				sleep(600) ;
				app_init();
			}
			
			s_rconn = net_connect( g_server, g_port ) ;
			
			printf("Connected! %s %d %d\n", (char *)g_server, g_port, s_rconn);
			
			if( s_rconn<0 ) {
				sleep(15);
				continue;
			}
			
			string v ;
			sprintf( v.setbufsize(256), "unit %s %d\n", (char *)g_hostname , g_id  );
			rconn_send( s_rconn, (char *)v );
			lineptr = 0 ;
		}
		
		res = net_rrdy( s_rconn, g_keepalive*1000000 ) ;
		if( res > 0 ) {
			// data ready
			while( lineptr<LINEBUFSIZE && net_rrdy(s_rconn)>0 ) {
				r=net_recv( s_rconn, &linebuf[lineptr], 1 ) ;
				if( r>0 ) {
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
					break ;
				}
			}
			
			
			if( lineptr>=LINEBUFSIZE ) {					// line buffer overrun, 
				close( s_rconn ) ;
				s_rconn = -1 ;
			}
		}
		else if( res==0 ) {
			// time out, send PING package
			if( ++g_watchdog > 5 ) {
				close( s_rconn ) ;
				s_rconn = -1 ;
			}
			else {
				rconn_send( s_rconn, "p \n" );
			}
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
		
void sig_handler(int signum)
{
	g_running = 0 ;
}

void app_init()
{
    // setup signal
//	signal(SIGINT, sig_handler);
//	signal(SIGTERM, sig_handler);
	
	int    iv ;
	string v ;
    config dvrconfig(CFG_FILE);

	g_server = default_server ;
	g_port = default_port ;
	
    // get remote tunnel server
    v = dvrconfig.getvalue("network", "rcon_server") ;
    if( ! v.isempty() ) {
		g_server = v ;
	}
	else {
		g_server = default_server ;
	}
	
    // get remote tunnel port
    iv = dvrconfig.getvalueint("network", "rcon_port") ;
    if( iv==0 ) {
		g_port = default_port ;
	}
	else {
		g_port = iv ;
	}

    v = dvrconfig.getvalue("system", "hostname") ;
    if( !v.isempty() ) {
		g_hostname = v ;
	}
	else {
		g_hostname = "PW" ;
	}
	
	g_nointernetaccess = getvalueint("system", "nointernetaccess") ;
		
	g_id = random();
	
}

int main()
{
	// wait until modem is up
	app_init();

	rconn_run();
	
	return 0 ;
}
