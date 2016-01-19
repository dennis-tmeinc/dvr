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
#include <sys/un.h>

#include "net.h"

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

#include "rconn.h"

#ifdef ANDROID_CLIENT
#include "adbclient.h"
#endif	

#define APPNKEY "AQ7Ynq23JYCtyHWATwS9"

#define TUNNEL_BUFSIZE (4096)

string	g_server ;
int     g_port ;
int     g_runtime ;
int		g_maxwaitms  ;
	
static int     g_nointernetaccess ;		// no internet access
static string  g_did ;					// device id ( use mac addr )
static string  g_internetkey ;			// internet accessing key

static int     s_running ;				// global running connections 

const char default_server[] = "pwrev.us.to" ;
const int  default_port = 15600 ;

// return runtime in seconds
int runtime()
{
	struct timespec ts ;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int)(ts.tv_sec) ;
}

rconn::rconn()
{
	stage = STAGE_REG ;
	datalen = 0 ;
	cmdptr=0;
	r_xoff = 0 ;		
	m_block = 0 ;
}

rconn::~rconn()
{
	// remove all channels relate to this connection
	while( channel_list.first()!=NULL ) {
		delete (channel *)(channel_list.first()->item) ;
		channel_list.remove(channel_list.first());
	}
}

void rconn::closechannel(void) 
{
	target=NULL ;
	channel::closechannel();
}

int rconn::process() 
{
	if( sfd!=NULL && sfd->fd == sock ) {

		if( g_runtime - activetime > 100 ) {
			sendLine("p\n");
		}

		if( sock>=0 && ( sfd->revents & POLLIN ) ) {
			if( datalen>0 ) {
				int r = do_read( datalen>CHANNEL_BUFSIZE ? CHANNEL_BUFSIZE : datalen );
				if( r>0 ) {
					datalen-=r ;
					if( datalen<=0 ) {
						target = NULL ;
					}
				}
			}
			else {
				do_cmd();
			}
		}

		if( sock>=0 && ( sfd->revents & POLLOUT ) ) {
			do_send();
		}
	}
	m_block = (sfifo.first()!=NULL) ;
	
	// process sub connections
	list_node * li = channel_list.first() ;
	while( li ) {
		list_node * n = li->next ;
		channel * ch = (channel *)(li->item);
		if( ch->process() == 0 ) {
			delete ch ;
			channel_list.remove(li);
		}
		li = n;
	}

	return 1 ;		// never die
}

int rconn::connect_server()
{
	return  net_connect( g_server, g_port ) ;
}
	
int  rconn::setfd( struct pollfd * pfd, int max )
{
	// try reopen server socket
	if( sock < 0 ) {
		sock = 	connect_server();
		if( sock >= 0 ) {
			char hostname [256] ;
			gethostname(hostname, 255);
			hostname[255]=0 ;
			sendLineFormat("unit %s %s %s\n", hostname , (char *)g_did, (char *)g_internetkey );
		}
		else {
			if( g_maxwaitms>2000 ) g_maxwaitms = 2000 ;		// to retry connection in 2 seconds
		}
	}
				
	int nfd = 0 ;
	
	sfd=NULL;
	if( max>0 && sock>=0 ) {
		pfd->events = POLLIN ;
		if( sfifo.first()!=NULL ) {
			pfd->events |= POLLOUT ;
		}
		pfd->fd = sock ;
		pfd->revents = 0 ;
		sfd = pfd ;
		nfd++ ;
	}
	
	// setfd sub connections
	list_node * li = channel_list.first() ;
	while( li && nfd<max ) {
		nfd += ((channel *)(li->item))->setfd( &pfd[nfd], max-nfd );
		li=li->next ;
	}
	
	return nfd ;

}

void rconn::closepeer( channel * peer ) 
{
	if( target == peer ) target = NULL ;
	sendLineFormat("close %s\n", peer->id );
}

// sending data requested from peer
int rconn::sendpacket( packet * p, channel * from )
{
	sendLineFormat( "mdata %s %d\n", from->id, p->len() );
	sfifo.add( new packet(p) );
	return 1 ;
}

void rconn::xoff( channel * peer ) 
{
	sendLineFormat( "xoff %s\n", peer->id );
}

void rconn::xon( channel * peer ) 
{ 
	sendLineFormat( "xon %s\n", peer->id );
}	

// do the command line receiving
void rconn::do_cmd()
{
	while( sock>=0 && net_rrdy(sock) ) {
		int r = net_recv( sock, cmdline+cmdptr, 1 );
		if( r>0 ) {
			if( cmdline[cmdptr] == '\n' ) {
				cmdline[cmdptr] = 0;
				process_cmd(cmdline);
				cmdptr=0 ;
				break ;
			}
			cmdptr++;
			if( cmdptr>(sizeof(cmdline)-4) ) {
				// command line buffer full
				close(sock);
				sock=-1 ;
				break ;
			}
		}
		else {
			close(sock);
			sock=-1 ;
			break ;
		}
	}
}

channel * rconn::findbyId( char * id ) 
{
	list_node * li = channel_list.first() ;
	while( li ) {
		channel * ch = (channel * )li->item ;
		if( strcmp( id, ch->id )==0 ) {	// found
			return ch ;
		}
		li=li->next ;
	}
	return NULL ;
}

// reversed connecting from PW
// command:
//    mconn id target tport
void  rconn::cmd_mconn( int argc, char * argv[] ) 
{
	if( argc<2 ) {
		return ;			
	}
	
	if( argc<4 ) {
		sendLineFormat("close %s\n", argv[1] );
	}
	
	// target
	if( argv[2][0] == '*' ) {
		argv[2] = "127.0.0.1" ;
	}

	int tsock = net_connect( argv[2], atoi( argv[3]) );
	if( tsock<0 ) {
		sendLineFormat("close %s\n", argv[1]);
		return ;
	}
				
	channel * tch = new channel(tsock);
	channel_list.add( tch );
	tch->setid( argv[1] );
	tch->connect( this );
	sendLineFormat( "connected %s\n", argv[1] );
}
	
// reversed connecting from PW
// command:
//    connect id target tport source sport 
void  rconn::cmd_connect( int argc, char * argv[] ) 
{
	if( argc<2 ) {
		return ;			
	}
	
	if( argc<4 ) {
		sendLineFormat("close %s\n", argv[1] );
	}
	
	// target
	if( argv[2][0] == '*' ) {
		argv[2] = "127.0.0.1" ;
	}

	int tsock = net_connect( argv[2], atoi( argv[3]) );
	if( tsock<0 ) {
		sendLineFormat("close %s\n", argv[1]);
		return ;
	}

	// source
	int ssock ;
	if( argc<5 || argv[4][0] == '*' ) {
		ssock = connect_server();		// connect to default server
	}
	else {
		int sport = 0 ;
		if( argc>5 ) {
			sport = atoi( argv[5] );
		}
		if( sport <= 0 ) {
			sport = g_port ;
		}
		// source socket
		ssock = net_connect( argv[4], sport );
	}
	
	if( ssock<0 ) {
		close( tsock );
		return;
	}
				
	channel * tch = new channel(tsock);
	channel * sch = new channel(ssock);
	tch->connect( sch );
	sch->connect( tch );
	sch->sendLineFormat( "connected %s\n", argv[1] );
	channel_list.add( tch );
	channel_list.add( sch );
}

// reversed disconnect from PW
// command:
//    close id
void  rconn::cmd_close( int argc, char * argv[] ) 
{
	if( argc<2 ) {
		return ;			
	}
	
	channel * ch = findbyId( argv[1] );
	if( ch ) {
		ch->closepeer(this);
	}
}

// receiving data from device
// command:
//    mdata id size
void  rconn::cmd_data( int argc, char * argv[] ) 
{
	if( argc<3 ) {
		return ;			
	}
	
	target = findbyId( argv[1] );
	datalen = atoi(argv[2]);
}

// to start a p2p detection
// commands:
//      p2p peer port
void rconn::cmd_p2p( int argc, char * argv[] ) 
{
	if( argc<3 ) {
		return ;			
	}	
	
	return ;
}

// xon channel
// command:
//    xon id 
void  rconn::cmd_xon( int argc, char * argv[] ) 
{
	if( argc<2 ) {
		return ;			
	}	

	channel * ch = findbyId( argv[1] );
	if( ch ) {
		ch->xon( this );
	}
	
}

// xoff channel
// command:
//    xoff id 
void  rconn::cmd_xoff( int argc, char * argv[] ) 
{
	if( argc<2 ) {
		return ;			
	}

	channel * ch = findbyId( argv[1] );
	if( ch ) {
		ch->xoff( this );
	}
}

void rconn::cmd( int argc, char * argv[] )
{
	if( strcmp( argv[0], "mconn")==0 ) {			// multi- connection
		cmd_mconn( argc, argv );
	}
	else if(strcmp( argv[0], "connect")==0 ) {		// single connection
		cmd_connect( argc, argv );
	}
	else if( strcmp( argv[0], "close")==0 ) {		// close connection
		cmd_close( argc, argv );
	}
	else if(strcmp( argv[0], "p2p")==0 ) {			// to start p2p detection
		cmd_p2p( argc, argv );
	}
	else if(strcmp( argv[0], "mdata")==0 ) {		// multi-plex connection
		cmd_data( argc, argv );
	}
	else if(strcmp( argv[0], "xon")==0 ) {		// multi-plex connection
		cmd_xon( argc, argv );
	}
	else if(strcmp( argv[0], "xoff")==0 ) {
		cmd_xoff( argc, argv );
	}
}

void rconn::process_cmd(char * c)
{
	char * argv[10] ;
	int    argc=0 ;
	int    brk=1 ;
	
	// breaking arguments
	while( *c && argc<9 ) {
		if( *c<=' ' && *c>0 ) {		// found space
			*c = 0 ;
			brk=1 ;
		}
		else {
			if(brk) {
				brk=0 ;
				argv[argc++] = c ;
			}
		}
		c++ ;
	}
	
	if( argc>0 ) {
		cmd( argc, argv );
	}
}
	
void rconn_run()
{
	int r ;
	
	struct pollfd * pfd ;
	int nfd, pfdsize, pfdresize ;
	
	// starting pollfd size
	pfdresize = 0 ;
	pfdsize = 40 ;
	pfd = new struct pollfd [pfdsize+2] ;

	s_running = 1 ;

	g_runtime = runtime() ;
	g_maxwaitms =  120000 ;
	
	// main channel 
	rconn * mainconn  = new rconn();
	
#ifdef ANDROID_CLIENT
	adb_conn * adbconn = new adb_conn();
#endif	

	while( s_running ) {
		if( pfdresize > 0 ) {		// to adjust pollfd size 
			delete pfd ;
			pfd = new struct pollfd [pfdresize+2] ;
			pfdsize = pfdresize ;
			pfdresize = 0 ;
		}
		nfd = 0 ;
		nfd += mainconn->setfd( pfd+nfd, pfdsize-nfd ); 

#ifdef ANDROID_CLIENT
		nfd += adbconn->setfd( pfd+nfd, pfdsize-nfd ); 
#endif			

		// to adjust pollfd size
		if( nfd>=pfdsize ) {
			pfdresize = pfdsize * 2 ;
			g_maxwaitms = 1 ;
		}
		else if( pfdsize>40 && nfd<(pfdsize/4) ) {
			pfdresize = pfdsize / 2 ;
		}
		if( nfd>0 ) {
			r = poll( pfd, nfd, g_maxwaitms );
		}
		else {
			sleep(2);
			r = 0;
		}
		g_runtime = runtime() ;
		g_maxwaitms =  120000 ;
		
		if( r>=0 ) {
			mainconn->process() ;

#ifdef ANDROID_CLIENT
			adbconn->process();
#endif				

		}
		else if( r<0 ) {	// error!!!
			// ? what to do ?
			
			// reset main connection
			delete mainconn ;
			mainconn = new rconn();

#ifdef ANDROID_CLIENT
			delete adbconn ;
			adbconn = new adb_conn();
#endif	

		}
	}
	
	// delete all channel
	delete mainconn ;

#ifdef ANDROID_CLIENT
	delete adbconn ;
#endif	
	
	delete [] pfd ;
	return  ;
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
		fdid = fopen( APP_DIR"/did", "w+" );
		if( fdid ) {
			FILE * fr ;
			fr = fopen("/sys/class/net/eth0/address", "r");
			if( fr ) {
				fscanf( fr, "%40s", (char *)v.expand( 80 ) );
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
			rewind(fdid);
		}
	}
	
	if( fdid ) {
		fscanf( fdid, "%79s", (char *)v.expand( 80 ) );
		g_did = v ;
		fclose( fdid );
	}
		
}

#ifdef RCMAIN
void rc_main()
{
	if( fork()== 0 ) {
		printf("RC Start!\n");
		rc_init();
		g_internetkey = "V5d0DgUgu?f51u5#i3FV" ;
		g_did = g_did+"V" ;
		signal(SIGPIPE, SIG_IGN );
		rconn_run();
	}
}
#else 

static void s_handler(int signum)
{
	s_running = 0 ;
}

int main() 
{
	// setup signal
	signal(SIGINT, s_handler);
	signal(SIGTERM, s_handler);
	signal(SIGPIPE, SIG_IGN );
	rc_init();
	rconn_run();
	return 0 ;
}
#endif
