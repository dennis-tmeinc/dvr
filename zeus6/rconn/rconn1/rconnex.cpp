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

#define TUNNEL_MAX	(100)
#define TUNNEL_BUFSIZE (4000)

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
	net_sendall( s, str, strlen(str) );
}

#define TUNNEL_BUFSIZE (4096)


// return runtime in seconds
int runtime()
{
	struct timespec ts ;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
	return (int)(ts.tv_sec) ;
}

class tunnel;
tunnel * g_tunnel[TUNNEL_MAX] ;
struct pollfd   g_fds[TUNNEL_MAX*2+1] ;		// server socket use first entry

class tunnel {
public:
	int idx ;
protected:
	int  activetime ;					// active time

	int  tsock ;
	char * tbuffer ;
	int  tbufptr, tbufsize ;
	struct pollfd * tfd ;

	int  ssock ;
	char * sbuffer ;
	int  sbufptr, sbufsize ;
	struct pollfd * sfd ;
	
public:
	tunnel( int i, int initsocket ) {
		idx = i ;
		stage = STAGE_CMD ;
		tbuffer = new char [TUNNEL_BUFSIZE] ;
		tbufptr = tbufsize = 0;
		sbuffer = new char [TUNNEL_BUFSIZE] ;
		sbufptr = sbufsize = 0;
		tsock = -1 ;
		ssock = initsocket ;
		activetime = g_runtime ;
	}
	
	~tunnel() {
		if( tsock>=0 ) 	close( tsock );
		if( ssock>=0 ) 	close( ssock );
		if( tbuffer != NULL )
			delete tbuffer ;
		if( sbuffer != NULL )
			delete sbuffer ;
	}
	
	int setfd( struct pollfd * pfd ) {
		int n=0 ;
		sfd=NULL;
		if( ssock>=0 ) {
			sfd = pfd ;
			sfd->fd = ssock ;
			sfd->events = 0 ;
			if( sbufsize<TUNNEL_BUFSIZE )
				sfd->events |= POLLIN ;
			if( tbufsize>tbufptr )
				sfd->events |= POLLOUT ;
			sfd->revents = 0 ;
			n++ ;
			pfd++ ;
		}

		tfd = NULL ;
		if( tsock>=0 ) {
			tfd = pfd ;
			tfd->fd = tsock ;
			tfd->events = 0 ;
			if( tbufsize<TUNNEL_BUFSIZE )
				tfd->events |= POLLIN ;
			if( sbufsize>tbufptr )
				tfd->events |= POLLOUT ;
			tfd->revents = 0 ;
			n++ ;
		}
		
		return n ;
	}
	
	// process 
	//    return 1 success
	//    return 0 closed or failed
	int process() {
		int r ;
		// validate fds
		if( ssock>=0 && sfd!=NULL && sfd->fd == ssock ) {
			
			if( sbufsize<TUNNEL_BUFSIZE && ( sfd->revents & POLLIN ) ) {
				r = net_recv( ssock, sbuffer+sbufsize, TUNNEL_BUFSIZE-sbufsize ) ;
				if( r>0 ) {
					activetime = g_runtime ;
					sbufsize += r ;
							
					if( stage == STAGE_CMD ) {
						process_cmd();
					}
					
				}
				else {
					return 0 ;
				}
			}
			
			if( tbufsize>tbufptr && ( sfd->revents & POLLOUT ) ) {
				r = net_send( ssock, tbuffer+tbufptr, tbufsize-tbufptr ) ;
				if( r>0 ) {
					tbufptr+=r ;
					if( tbufptr>=tbufsize ) {	// buffer completed
						tbufsize = 0 ;
						tbufptr = 0 ;
					}
				}
				else {
					return 0 ;
				}
			}
		}
		
		// validate fds
		if( tsock>=0 && tfd!=NULL && tfd->fd == tsock ) {

			if( tbufsize<TUNNEL_BUFSIZE && ( tfd->revents & POLLIN ) ) {
				r = net_recv( tsock, tbuffer+tbufsize, TUNNEL_BUFSIZE-tbufsize ) ;
				if( r>0 ) {
					activetime = g_runtime ;
					tbufsize += r ;
				}
				else {
					return 0 ;
				}
			}
			
			if( sbufsize>sbufptr && ( tfd->revents & POLLOUT ) ) {
				r = net_send( tsock, sbuffer+sbufptr, sbufsize-sbufptr ) ;
				if( r>0 ) {
					sbufptr+=r ;
					if( sbufptr>=sbufsize ) {	// buffer completed
						sbufsize = 0 ;
						sbufptr = 0 ;
					}
				}
				else {
					return 0 ;
				}
			}
		}
		
		if( (g_runtime - activetime) > 900 ) {
			// consider idling tunnel, close it
			return 0;
		}
		
		return stage ;
	}
	
	// hard push all output buffer to source sock
	void push_s()
	{
		int s ;
		while(ssock>=0 && tbufsize>tbufptr) {
			s = net_send( ssock, tbuffer+tbufptr, tbufsize-tbufptr ) ;
			if( s>0 ) {
				tbufptr+=s ;
				if( tbufptr>=tbufsize ) {	// buffer completed
					tbufsize = 0 ;
					tbufptr = 0 ;
					break;
				}
			}
			else {
				stage = STAGE_CLOSED ;
				break;
			}
		}
	}
	
	// request reversed connecting
	void req_connect( int ix, int port )
	{
		if( ssock>=0 && tbufsize< (TUNNEL_BUFSIZE / 2 ) ) {
			sprintf( tbuffer+tbufsize, "connect %d * %d * 0\n", ix, port );
			tbufsize+=strlen( tbuffer+tbufsize );
		}
	}
	
	// reverse connection connected
	void connected( int targetsocket, char * data, int dsize )
	{
		tsock = targetsocket ;
		if( dsize>0 ) {
			if( tbufsize+dsize > TUNNEL_BUFSIZE ) {
				push_s();
			}
			memcpy( tbuffer+tbufsize, data, dsize );
			tbufsize+=dsize ;
		}
		stage = STAGE_CONNECTED ;
	}
	
	// process init command 
	void process_cmd(){
		// buffer check
		int eol ;
		for( eol = sbufptr ; eol<sbufsize; eol++ ) {
			if( sbuffer[eol] == '\n' ) {		// found a command line
				
				// support command list:
				//    validate:    validate sessionid       ( validate session id )
				//    session:     session user key	        ( register a client session )
				//    list:        list	sessionid	*|hostname  ( request a list of PW unit )
				//    remote:      remote sessionid idx port  ( request remote connection )
				//    unit:        unit name uuid   		( PW unit register )
				//    vserver:     vserver idx port         ( request virtual server to remote **** to be support in future )
				//    connected:   connected idx            ( reverse connection from PW )
				//    ping:        ping                     ( null ping, ignor it)
				//    
				char * cmd = sbuffer+sbufptr ;
				sbuffer[eol] = 0 ;
				sbufptr = eol+1 ;				// command "connected" may transfer remain buffer to other tunnel

				printf("Command: %s\n", cmd );
				
								
				if( strncmp( cmd, "validate ", 9)==0 ) {
					cmd_validate( cmd+9 );
				}
				else if(strncmp( cmd, "session ", 8)==0 ) {
					cmd_session( cmd+8 );
				}
				else if(strncmp( cmd, "list ", 5)==0 ) {
					cmd_list( cmd+5 );
				}
				else if(strncmp( cmd, "remote ", 7)==0 ) {
					cmd_remote( cmd+7 );
				}
				else if(strncmp( cmd, "unit ", 5)==0 ) {
					cmd_unit( cmd+5 );
				}
				else if(strncmp( cmd, "vserver ", 8)==0 ) {
					cmd_vserver( cmd+9 );
				}
				else if(strncmp( cmd, "connected ", 10)==0 ) {
					cmd_connected( cmd+10 );
				}
				else {
					// unknown cmd
					sprintf( tbuffer+tbufsize, "e\n" );
					tbufsize+=strlen( tbuffer+tbufsize );					
				}
				
				if( sbufptr>=sbufsize ) {	// buffer completed
					sbufsize = 0 ;
					sbufptr = 0 ;
				}
				
				break ;
			}
		}
	}

};


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


// received a command from server, string terminated by "\0"
// 	command list
//      connect <id> <target> <tport> <source> <sport> 
//      ok	(empty command)
void rconn_cmd( char * cmd )
{
	if( strncmp(str, "connect ", 8)==0 ) {		// this is the only command supported now
		// to create a new connection

		// find new tunnel slot
		int i;
		for( i=0; i<TUNNEL_MAX; i++ ) {
			if( g_tunnel[i] == NULL ) {
				g_tunnel[i] = new tunnel( cmd+8 );
				break;
			}
		}
	}
}

int rconn_run()
{
	int res ;
	int r ;
	int nfd ;
	char linebuf[LINEBUFSIZE] ;
	char * lbuf ;
	int  lineptr = 0 ;

	s_rconn = -1 ;

	g_watchdog = 0 ;
	g_running = 1 ;
	while( g_running ) {
		if( s_rconn < 0 ) {
			s_rconn = net_connect( g_server, g_port ) ;
			
			printf("Connected! %s %d %d\n", (char *)g_server, g_port, s_rconn);
			
			if( s_rconn<0 ) {
				sleep(30);
				continue;
			}
			
			lbuf = new char [512] ;
			sprintf( lbuf, "unit %s %d\n", (char *)g_hostname , g_id  );
			net_sendall( s_rconn, lbuf, strlen(lbuf) );
			delete lbuf ;

			lineptr = 0 ;
		}

		g_fds[0].fd = s_rconn ;
		g_fds[0].events = POLLIN ;
		g_fds[0].revents = 0 ;
		
		nfd = 1 ;
		
		for( i=0; i<TUNNEL_MAX; i++ ) {
			if( g_tunnel[i] !=NULL ) {
				nfd += g_tunnel[i]->setfd( &g_fds[nfd] ) ;
			}
		}

		r = poll( g_fds, nfd, 100000 );
		if( r==0 ) {
			// time out, send PING package
			if( ++g_watchdog > 5 ) {
				close( s_rconn ) ;
				s_rconn = -1 ;
				continue ;
			}
			else {
				lbuf = "p\n" ;
				net_sendall( s_rconn, lbuf, strlen(lbuf) );
			}
		}
		else if( r<0 ) {
			// ? what to do ?
			sleep(1);
			
			printf("poll error!\n");
			
			close( s_rconn );
			s_rconn = -1 ;
			
			continue ;
		}
		// r>0
		
		g_runtime = runtime() ;
		g_watchdog = 0 ;
		
		// check master connections
		if( (g_fds[0].revents & POLLIN) != 0 ) {
			// data ready
			while( lineptr<LINEBUFSIZE && net_rrdy(s_rconn)>0 ) {
				r=net_recv( s_rconn, &linebuf[lineptr], 1 ) ;
				if( r>0 ) {
					if( linebuf[lineptr] == '\n' ) {		// end of line
						linebuf[lineptr] = '\0' ;			// replace with eol
						rconn_cmd( linebuf ) ;
						lineptr = 0 ;						// reset line buffer
					}
					else {
						lineptr+=r ;
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
		
		for( i=0; i<TUNNEL_MAX; i++ ) {
			if( g_tunnel[i] !=NULL ) {
				if( g_tunnel[i]->process()==0 ) {
					
					printf("Killed %d\n", i);
					
					delete g_tunnel[i] ;
					g_tunnel[i] = NULL ;
				}
			}
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
