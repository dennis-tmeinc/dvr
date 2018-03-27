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

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

#include "curl/curl.h"
#include "curl/easy.h"
#include "curl/multi.h"

#define APP_VERSION "2.1"

CURLM * g_curlm ;			// global curl multi interface
int     g_running ;			// global running connections 
string  g_webtun_url ;		// tunnel server url ;
string  g_hostname ;
string  g_phonenumber ;
string  g_logfile ;
static  int     g_maxwaitms ;

fd_set  g_fdr_set;
fd_set 	g_fdw_set;
fd_set 	g_fde_set;

const char default_url[] = "http://tdlive.darktech.org/vlt/vltdir.php" ;

unsigned g_serial ;

int g_timems ;
int gettimems()
{
	struct timespec ts ;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000 ;
}

// write log
void log(char *fmt, ...)
{
	if( *(char *)g_logfile ) {
		long o1=0 , o2=0, r ;
		char linebuf[1024] ;
		FILE * logfile ;
		if( strcmp( (char *)g_logfile, "-" )==0 ) {
			logfile = stdout ;
		}
		else {
			logfile = fopen( g_logfile, "r+" );
			if( logfile == NULL ) {
				logfile = fopen( g_logfile, "w");
			}
			else {
				fseek( logfile, 0, SEEK_END );
				o1 = ftell( logfile ) ;
				if( o1 > 30000 ) {
					// cycle log file
					fseek( logfile, o1-20000, SEEK_SET );		// only 20k stay
					fgets( linebuf, sizeof(linebuf), logfile );	// skip a line
					o1 = 0 ;
					while( ( r=fread( linebuf, 1, sizeof(linebuf), logfile) )>0 ) {
						o2 = ftell( logfile );
						fseek( logfile, o1, SEEK_SET ) ;
						fwrite( linebuf, 1, r, logfile );
						o1 = ftell( logfile );
						fseek( logfile, o2, SEEK_SET ) ;
					}
					fseek( logfile, o1, SEEK_SET );
				}
			}
		}
	    va_list ap ;
		va_start( ap, fmt );
		fprintf(logfile, "%d:", g_timems);
        vfprintf(logfile, fmt, ap );
		va_end( ap );
        fwrite( "\n", 1, 1, logfile );	// end of line
		if( logfile != stdout ) {
			o1 = ftell( logfile );
			fclose( logfile );
			if( o1<o2 )					// log file cycled
				truncate(g_logfile, o1);
		}
	}
}

// load default tunnel server
void loadUrl()
{
	string v ;
    config dvrconfig(CFG_FILE);
    
    // get remote tunnel server
    v = dvrconfig.getvalue("network", "livesetup_url") ;
	if( strncmp( (char *)v, "http://", 7 )==0 ) {
		g_webtun_url = (char *)v ;
		char * u = (char *)g_webtun_url ;
		int l = strlen( u ) ;
		if( u[l-1]=='/' ) {
			// 
			sprintf( v.expand( l+20 ), "%slivetun.php", u ) ;
			g_webtun_url = (char *)v ;
		}
	}
	else {
		g_webtun_url = default_url ;
	}

    v = dvrconfig.getvalue("network", "livesetup_logfile") ;
    if( v.length() > 0 ) {
		g_logfile = (char *)v ;
	}
}

// read phone number
// return 1 for success
//   phone: string buffer to receive phone number, at lease 20 bytes
int getphonenumber( char * phone )
{
	int res = 0 ;
	FILE * fmodeminfo = fopen( "/var/dvr/modeminfo", "r" );
	if( fmodeminfo ) {
		if( fscanf( fmodeminfo, "%s", phone ) == 1 ) {
			res = 1 ;
		}
		fclose( fmodeminfo );
   }
   return res ;
}

// curl tunnel base class
class curltun {
	
protected:
	string m_id ;						// tunnel id (set from remote)
	int  m_eof ;						// 1: local data closed (feof)
	int  m_remote_close ;				// 1: remote tunnel closed
	int  m_active_time ;				// tunnel last active time (in ms)
	
	CURL * m_wcurl ;					// write curl connection, webtun -> dvr (GET)
	struct curl_slist * m_wheader ;
	
	CURL * m_rcurl ;					// read curl connection, dvr -> webtun (PUT)
	struct curl_slist * m_rheader ;

	CURL * m_curl_reuse ;				// save a curl handle for reuse	
	
public:	

	// link
	curltun * next ;

	curltun() {
		m_id = "na" ;
		m_eof = 0 ;						
		m_remote_close = 0 ;			
		m_active_time = g_timems ;
		m_wcurl = NULL ;
		m_wheader = NULL ;
		m_rcurl = NULL ;
		m_rheader = NULL ;
		m_curl_reuse = NULL ;
		next = NULL ;
	}
	
	curltun( const char * id ) {
		m_id = id ;
		m_eof = 0 ;						
		m_remote_close = 0 ;			
		m_active_time = g_timems ;
		m_wcurl = NULL ;
		m_wheader = NULL ;
		m_rcurl = NULL ;
		m_rheader = NULL ;
		m_curl_reuse = NULL ;
		next = NULL ;
	}
	
	virtual ~curltun() {
		if( m_rcurl ) {
			curl_multi_remove_handle(g_curlm, m_rcurl);
			curl_easy_cleanup(m_rcurl);			
			m_rcurl = NULL ;
		}

		if( m_rheader ) {
			curl_slist_free_all(m_rheader);
			m_rheader = NULL ;
		}
		
		if( m_wcurl ) {
			curl_multi_remove_handle(g_curlm, m_wcurl);
			curl_easy_cleanup(m_wcurl);			
			m_wcurl = NULL ;
		}
		
		if( m_wheader ) {
			curl_slist_free_all(m_wheader);
			m_wheader = NULL ;
		}

		if( m_curl_reuse ) {
			curl_easy_cleanup(m_curl_reuse);			
			m_curl_reuse = NULL ;
		}
		
		log("DEL (%s)", (const char *)m_id );
	}
	
	// local side callbacks
		
	virtual size_t headerfunc( void * ptr, size_t si )
	{
		char header[si+2] ;
		memcpy( header, ptr, si );
		header[si] = 0 ;
		// X-Webt-Connection: close	// if present, close the connection
		if( strncasecmp( header , "X-Webt-", 7)==0 ) {

			log("(%s) known header: %s", (char *)m_id, (char *)header );

			if( strncasecmp( header+7 , "Connection:", 11)==0 ) {
				if( strstr( header+18, "close" ) == 0 ) {
					m_remote_close = 1 ;
				}
			}
		}
		return si ;
	}
	
	static size_t s_headerfunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		return ((curltun *)userdata)->headerfunc( ptr, size*nmemb );
	}

	virtual size_t wdatafunc( void *ptr, size_t size )
	{
		// dummy write
		return size ;
	}
	
	static size_t s_wdatafunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		return ((curltun *)userdata)->wdatafunc( ptr, size*nmemb );
	}
	
	// dummy data write, discard all data
	static size_t s_wnullfunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		return size*nmemb ;
	}
	
	virtual size_t rdatafunc( void *ptr, size_t size )
	{
		return 0 ;
	}
	
	static size_t s_rdatafunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		return ((curltun *)userdata)->rdatafunc( ptr, size*nmemb );		
	}
	
	// dummy data read, return 0 for no more data
	static size_t s_rnullfunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		return 0 ;
	}
	
	CURL * curl_new () {
		CURL * curl ;
		if( m_curl_reuse ) {
			curl = m_curl_reuse; 
			m_curl_reuse = NULL ;
			curl_easy_reset( curl );
		}
		else {
			curl = curl_easy_init(); 
		}

		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600 );		// 10 min timeout

		// all default call back handler
		curl_easy_setopt(curl, CURLOPT_PRIVATE, (char *)this);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA,  (void *)this);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,  s_headerfunc);
		curl_easy_setopt(curl, CURLOPT_READDATA,  (void *)this);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION,  s_rdatafunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA,  (void *)this);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  s_wdatafunc);
				
		// add to curl multi interface
		curl_multi_add_handle(g_curlm, curl);
		return curl ;
	}
	
	void curl_release( CURL * curl ) {
		curl_multi_remove_handle(g_curlm, curl);
		// save curl for reuse
		if( m_curl_reuse ) {
			curl_easy_cleanup(m_curl_reuse);
		}
		m_curl_reuse = curl ;
	}

	// setup GET (pull) curl
	CURL * curl_get( char * url = NULL ) {
		if( m_wcurl || m_remote_close || m_eof ) return m_wcurl ;		// only one instance of wcurl allowed
		
		m_wcurl = curl_new();

		char def_url[300] ;
		if( url == NULL ) {
			// default get url
			// url:	 GET http://1.1.1.1/tdc/webtun.php?c=g&t=<tunnel id>&s=<n+1>	
			sprintf( def_url, "%s?c=g&t=%s&s=%x", (char *)g_webtun_url, (char *)m_id, g_serial++ );
			url = def_url ;
		}

		curl_easy_setopt(m_wcurl, CURLOPT_URL, (char *)url );

		// overwrite call back handler
		curl_easy_setopt(m_wcurl, CURLOPT_READFUNCTION,  s_rnullfunc);	// no read 
		
		curl_easy_setopt(m_wcurl, CURLOPT_HTTPHEADER, m_wheader);		// no extra header for get curl
	
		m_active_time = g_timems ;

		log( "GET(%s): %s", (char*)m_id, (char*)url );
		
		return m_wcurl ;
	}
	
	// setup PUT (push) curl
	CURL * curl_put( char * url = NULL ) {
		if( m_rcurl || m_remote_close ) return m_rcurl ;			// only one instance of rcurl allowed
		
		m_rcurl = curl_new();

		char def_url[300] ;
		if( url == NULL ) {
			// default put url
			// url:	 PUT http://1.1.1.1/tdc/webtun.php?c=g&t=<tunnel id>&s=<n+1>	
			sprintf( def_url, "%s?c=p&t=%s&s=%x", (char *)g_webtun_url, (char *)m_id, g_serial++ );
			url = def_url ;			
		}

		curl_easy_setopt(m_rcurl, CURLOPT_URL, (char *)url );
		
		// overwrite call back handler
		curl_easy_setopt(m_rcurl, CURLOPT_WRITEFUNCTION,  s_wnullfunc);	// no write data

		curl_easy_setopt(m_rcurl, CURLOPT_UPLOAD, 1L);
		
		if( m_eof ) {						// local data closed (eof)
			// header to close connection
			m_rheader = curl_slist_append( m_rheader, "X-Webt-Connection: close" );
			m_remote_close = 1 ;
			log( "Final PUT" );
		}
			
		// let use chunked encoding
		// curl_easy_setopt(m_rcurl, CURLOPT_INFILESIZE, (long) m_putsize );
		// remove header:  "Expect: 100-continue" 
		m_rheader = curl_slist_append( m_rheader, "Expect:" );
		
		curl_easy_setopt(m_rcurl, CURLOPT_HTTPHEADER, m_rheader);
		
		m_active_time = g_timems ;

		log( "PUT(%s): %s", (char*)m_id, (char*)url );
		
		return m_rcurl ;
	}

	// a curl request completed
	virtual long curl_complete( CURL * curl, CURLcode result)
	{
		long response_code = 0 ;
		curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &response_code );
			
		m_active_time = g_timems ;

		if( curl==m_rcurl ) {
			m_rcurl = NULL ;
			if( m_rheader ) {
				curl_slist_free_all(m_rheader);
				m_rheader = NULL ;
			}			
			log("PUT (%s) completed : code: %d", (char*)m_id,(int)response_code );
		}
		else if( curl==m_wcurl ) {
			m_wcurl = NULL ;
			if( m_wheader ) {
				curl_slist_free_all(m_wheader);
				m_wheader = NULL ;
			}			
			log("GET (%s) completed : code: %d", (char*)m_id,(int)response_code );
		}
		
		// clean up curl
		curl_release( curl );
		
		// error or closed from peer
		if( result != CURLE_OK || response_code != 200  || m_remote_close ) {	
			m_eof = m_remote_close = 1 ;		// close it!
		}
		
		return response_code ;
		
	}

	virtual void fdset( fd_set * fdr, fd_set * fdw, fd_set * fde, int * maxfd )
	{
	}
	
	virtual int isclosed(){
		return ( m_eof && m_remote_close && m_wcurl == NULL && m_rcurl == NULL ) || (g_timems-m_active_time)>1800000 ;
	}
	
	virtual int process() = 0 ;
};

curltun * g_curlhead ;

void tunnel_int()
{
	g_curlhead = NULL ;
}

void tunnel_destory() 
{
	while( g_curlhead ) {
		curltun * d = g_curlhead ;
		g_curlhead = d->next ;
		delete d ;
	}
}
		
void tunnel_process()
{
	curltun * p = NULL ;
	curltun * t = g_curlhead ;
	curltun * n ;
	while( t ) {
		n = t->next ;
		t->process();
		if( t->isclosed() ) {
			delete t ;
			if( p ) {
				p->next = n ;
			}
			else {
				g_curlhead = n ;
			}
		}
		else {
			p = t  ;
		}
		t = n ;
	}
}

void tunnel_fdset( fd_set * fdr, fd_set * fdw, fd_set * fde, int * maxfd )
{
	curltun * t = g_curlhead ;
	while( t ) {
		t->fdset( fdr, fdw, fde, maxfd );
		t = t->next ;
	}
}

void tunnel_add( curltun * c )
{
	c->next = g_curlhead ;
	g_curlhead = c ;
}

// webtun: main tunnel
// GET curl session format
//    Request:	
//		 GET http://1.1.1.1/tdc/webtun.php?c=g&t=<tunnel id>&ser=<n+1>
//   Response: (may wait up to 60s if no data available)
//       200
//          Content-Length: n
//          X-Webt-Connection: close	// if present, close the connection
//       Contents:
//           binary data of <size> bytes

// PUT curl session format
//    Request:
//		 PUT http://1.1.1.1/tdc/webtun.php?c=p&t=<tunnel id>&ser=<n+1>
//       Content-Length: < total data size >
//       X-Webt-Connection: close	// if present, close the connection
//       Contents:
//           binary data of <Content-Length> bytes
//           or chunked data
// 	      
//   Response: ( response imediately )
//       200
class webtun 
	: public curltun
{
protected:	
	int  m_socket ;						// local socket
	int  m_rsize ;
	int  m_rpaused ;
	
public:	

	webtun( const char * id, const char * host, int port ) 
		:curltun( id )
	{
		log( "New webtun: %s", (char *)m_id );
		
		m_socket = -1 ;
		m_active_time = g_timems ;
		m_rpaused = 0;
		m_rsize = 0 ;
		
		// connect to host:port

		struct addrinfo hints;
		struct addrinfo *res;
		struct addrinfo *ressave;
		char service[20];

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		sprintf(service, "%d", port);
		res = NULL;
		if (getaddrinfo(host, service, &hints, &res) != 0) {
			return ;
		}
		if (res == NULL) {
			return ;
		}
		ressave = res;

		while (res) {
			m_socket = socket(res->ai_family,
							res->ai_socktype, res->ai_protocol);
			
			if (m_socket != -1) {

				if( connect(m_socket, res->ai_addr, res->ai_addrlen)==0 ) {
					break;
				}

				close(m_socket);
				m_socket = -1;
			}
			res = res->ai_next;
		}
		freeaddrinfo(ressave);

		if( m_socket<0 ) 
			m_eof=1 ;
		
	}

	virtual ~webtun(){
		if( m_socket > 0 ) {
			close( m_socket );
			m_socket = -1 ;
		}
	}

	// write data from GET curl
	virtual size_t wdatafunc( void *ptr, size_t size )
	{
		// write curl (GET)
		char * cptr = (char *)ptr ;
		
		if( m_socket>0 ) {
			while( size>0 ) {
				int w = send( m_socket, cptr, size, 0 );
				if( w>0 ) {
					cptr+=w ;
					size-=w ;
				}
				else {
					break;
				}
			}
		}
		return (cptr-(char *)ptr) ;
	}

	// read data for PUT curl
	virtual size_t rdatafunc( void *ptr, size_t size )
	{
		// read curl (PUT)
		if( m_socket>0 && !m_eof ) {
			if( rrdy() ) {
				m_rpaused = 0;
				m_active_time = g_timems ;
				int r = recv( m_socket, ptr, size, 0 ) ;
				if( r>0 ) {
					m_rsize += r ;
					return r ;
				}
				else {
					m_eof = 1 ;
				}
			}
			else if( !m_rpaused && m_rsize < 100000000 ) {
				// log("rcurl Paused");
				if( g_maxwaitms > 10 ) 
					g_maxwaitms = 10 ;
				m_rpaused = 1 ;
				return CURL_READFUNC_PAUSE ;
			}
		}
		m_rpaused = 0;
		return 0 ;
	}
	
	// generic funcitons
	virtual int process()
	{
		if( m_rcurl == NULL  && !m_remote_close && ((m_socket>0 && FD_ISSET(m_socket, &g_fdr_set)) || m_eof ) ) {
			char t ;
			// to detect if local side closed (eof)?
			if( m_eof == 0 && recv( m_socket, &t, 1, MSG_PEEK ) < 1 ) {
				m_eof = 1 ;

				// close socket 
				// close( m_socket );
				// m_socket = -1 ;
			}
			m_rpaused = 0 ;
			m_rsize = 0 ;
			// setup put curl
			curl_put();
		}

		if( m_wcurl == NULL && !m_remote_close && m_socket>0 && FD_ISSET(m_socket, &g_fdw_set) ) {
			curl_get();
		}

		if( m_rpaused && m_rcurl != NULL ) {
			if( (m_socket>0 && FD_ISSET(m_socket, &g_fdr_set)) || (g_timems - m_active_time) > 20 ) {
				// log("rcurl Pause cont");
				curl_easy_pause(m_rcurl, CURLPAUSE_CONT ); 
			}
			else {
				if( g_maxwaitms > 10 ) 
					g_maxwaitms = 10 ;
			}
		}
		
		return 0 ;
	}

    // socket side functions
	virtual void fdset( fd_set * fdr, fd_set * fdw, fd_set * fde, int * maxfd )
	{
		if( m_socket>0 ) {
			if( m_rcurl == NULL || m_rpaused ) {
				FD_SET( m_socket, fdr ) ;
			}
			if( m_wcurl == NULL ) {
				FD_SET( m_socket, fdw ) ;
			}
			if( m_socket>*maxfd ) *maxfd = m_socket ;
		}
	}

protected:
		
	// if read data ready
	int rrdy( int millis = 0 ) {
		if( m_socket>0 ) {
			struct timeval timeout ;
			timeout.tv_sec = millis/1000 ;
			timeout.tv_usec = (millis%1000)*1000 ;
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(m_socket, &fds);
			if (select(m_socket + 1, &fds, NULL, NULL, &timeout) > 0) {
				return FD_ISSET(m_socket, &fds);
			} 
		}
		return 0 ;
	}

	// if write data ready
	int wrdy( int millis = 0 ) {
		if( m_socket>0 ) {
			struct timeval timeout ;
			timeout.tv_sec = millis/1000 ;
			timeout.tv_usec = (millis%1000)*1000 ;
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(m_socket, &fds);
			if (select(m_socket + 1, NULL, &fds, NULL, &timeout) > 0) {
				return FD_ISSET(m_socket, &fds);
			} 
		}
		return 0 ;
	}
	
} ;


// file tunnel
//    one curl connection to retrieve single file
class filetun 
	: public curltun
{
protected:	
	FILE * m_file ;

public:	

	// dummy constructor
	filetun( const char * id )
		: curltun( id )
	{
		m_file = NULL ;
	}

	filetun( const char * id, const char * filename ) 
		: curltun( id )
	{
		m_file = fopen( filename, "rb" );
	}

	virtual ~filetun(){
		if( m_file ) {
			fclose( m_file );
			m_file = NULL ;
		}
	}
	
	virtual size_t rdatafunc( void *ptr, size_t size )
	{
		if( m_file ) {
			int r = fread( ptr, 1, size, m_file );
			if( r>0 ) {
				return r ;
			}
		}
		return 0 ;
	}
	
	virtual int process()
	{
		if( !m_remote_close &&  m_rcurl == NULL ) {
			m_eof = 1 ;		// send one curl only
			curl_put();
		}
		return 0 ;
	}
	
} ;

// Pipe tunnel
//    one curl connection to retrieve pipe output
class pipetun 
	: public filetun
{
public:	

	pipetun( const char * id, const char * command ) 
		: filetun( id )
	{
		log( "Pipe: %s", command );
		m_file = popen( command, "r" );
	}

	virtual ~pipetun(){
		if( m_file ) {
			pclose( m_file ) ;			
			m_file = NULL ;
		}
	}
	
};

// Write file tunnel
//    one curl connection to write file
class fwtun 
	: public curltun
{
protected:	
	FILE * m_file ;
	string m_filename ;
	string m_tmpfilename ;

public:	

	fwtun( const char * id, const char * filename ) 
		: curltun( id )
	{
		m_filename = filename ;
		int len = strlen( filename );
		sprintf( m_tmpfilename.setbufsize( len+5 ), "%s.w", filename );
		m_file = fopen( m_tmpfilename, "rb" );
	}

	virtual ~fwtun(){
		if( m_file ) {
			fclose( m_file );
			m_file = NULL ;
		}
		// final file processing
		if( access( (char *)m_filename, F_OK ) >= 0 ) {
			remove( (char *)m_filename ) ;
		}
		rename( (char *)m_tmpfilename, (char *)m_filename );
	}
	
	virtual size_t wdatafunc( void *ptr, size_t size )
	{
		int w = 0 ;
		if( m_file ) {
			w = fwrite( ptr, 1, size, m_file );
			if( w!=size ) {
				m_eof = 1 ;
			}
		}
		return w ;
	}
	
	// curl side function
	virtual int process()
	{
		if( !m_remote_close &&  m_wcurl == NULL ) {
			curl_get();
		}
		return 0 ;
	}
};

// create a new webtun tunnel
// <cmd:c> <tunnel id> [host] [port]\n
// create new connection
int tunnel_create_webtun( char * createline )
{
	char cmd[32] ;
	char id[256] ;
	char host[256] ;
	int port ;
	if( sscanf( createline, "%s %s %s %d", cmd, id, host, &port)==4 ) {
		tunnel_add( new webtun( id, host, port ) );
		return 1 ;
	}
	return 0;
}

// create filetun tunnel
// <cmd:f> <tunnel id> <filename>\n
int tunnel_create_filetun( char * createline )
{
	char cmd[32] ;
	char id[256] ;
	char filename[256] ;
	if( sscanf( createline, "%s %s %s", cmd, id, filename)==3 ) {
		tunnel_add( new filetun( id, filename ) );
		return 1 ;
	}
	return 0;
}

int tunnel_create_writetun( char * createline ) 
{
	char cmd[32] ;
	char id[256] ;
	char filename[256] ;
	if( sscanf( createline, "%s %s %s", cmd, id, filename)==3 ) {
		tunnel_add( new fwtun( id, filename ) );
		return 1 ;
	}
	return 0 ;
}
					
// create popen tunnel
// <cmd:r> <tunnel id> <command (to the end) >\n
int tunnel_create_pipetun( char * createline )
{
	char cmd[32] ;
	char id[256] ;
	int  ocmd=0 ;
	if( sscanf( createline, "%s %s %n", cmd, id, &ocmd)>=2 && ocmd > 4 ) {
		tunnel_add( new pipetun( id, createline+ocmd ) );
		return 1 ;
	}
	return 0 ;
}

// Master connection (waiting for requests from TDWeb)
// Connect curl session format
//    Request:	
//		 GET http://1.1.1.1/tdc/webtun.php?c=i&p=<phone number>&h=<host name>&ser=<n+1>
//       Content-Length: 0
//   Response: (may wait up to 60s if no data available)
//       200
//			X-Webt-Redir: http://1.1.1.2/sanjose/webtun.php
//       Contents:
//           <cmd:c> <tunnel id> [host] [port]\r\n
//           <cmd:c> <tunnel id> [host] [port]\r\n
//           ...
class connect_curl 
	: public curltun
{
protected:	
	int    m_ready ;
	int    m_idletime ;
	
	char   m_line[1024] ;
	int    m_llen ;
	
public:	

	connect_curl()
		: curltun("master")
	{
		m_llen = 0 ;
		m_idletime = g_timems  ;
		m_ready = 1 ;
		loadUrl();
	}
	
	// received header
	virtual size_t headerfunc( void * ptr, size_t si )	
	{
		string header ;
		char * ph = header.expand( si+4 );
		memcpy( ph, ptr, si );
		ph[si] = 0 ;
		if( strncasecmp( ph, "X-Webt:", 7)==0 ) {
			string v(ph+7) ;
			v.trim();
			if( strcmp( (char *)v, "ready" )==0 ) {
				m_ready = 1 ;			// ready for next request. (right tunnel answer)
			}
		}
		else if( strncasecmp( ph, "X-Webt-Redir:", 13)==0 ) {
			g_webtun_url = ph+13 ;
			g_webtun_url.trim();
		}
		else if( strncasecmp( ph, "X-Webt-Delay:", 13)==0 ) {
			int delaytime = 60 ;
			sscanf( ph+13, "%d", &delaytime );
			if( delaytime > 0 ) {
				m_idletime = g_timems + delaytime*1000 ;
			}
		}
		return si ;
	}
	
	void process_cmd( ) {
		if( m_llen>1 ) {
			m_line[m_llen] = 0 ;
			
			log( "CMD: %s", m_line );
			
			switch( m_line[0] ) {
				case 'c' :			// connect (tcp)
					// <c> <tunnel id> [host] [port]\n
					// create new connection
					tunnel_create_webtun( m_line ) ;
					break;

				case 'f' :			// file read
					// <f> <tunnel id> <filename>\n
					tunnel_create_filetun( m_line ) ;
					break;

				case 'w' :			// file write
					// <w> <tunnel id> <filename>\n
					tunnel_create_writetun( m_line ) ;
					break;
					
				case 'p' :			// pipe read
					// <p> <tunnel id> <command ...>\n
					tunnel_create_pipetun( m_line ) ;
					break;
					
				default :
					break;
			}		
		}
		// reset command line
		m_llen = 0 ;
	}
	
	virtual size_t wdatafunc( void *ptr, size_t size )
	{
		char * cptr = (char *)ptr ;
		while( size>0 ) {
			if( *cptr == '\n' ) {
				m_line[ m_llen ] = 0 ;
				process_cmd();
				m_llen = 0 ;
			}
			else {
				m_line[m_llen++] = *cptr ;
			}
			if( m_llen>1020 ) {
				m_llen = 0 ;
				break;
			}
			cptr++;
			size--;
		}
		return cptr - (char *)ptr ;
	}
		
	virtual long curl_complete( CURL * ch, CURLcode result)
	{
		long response_code = curltun::curl_complete( ch, result );
		
		log("Conn req complete.(code:%d)",(int)response_code);
		
		if( m_remote_close || !m_ready || response_code != 200 ) {
			// failed ? fall back to default url
			loadUrl();
			// idle for 30s
			m_idletime = g_timems + 30000 ;
			m_remote_close = 0 ;
			
			log( "Go Idle, response: %d, reqclose: %d,  ready: %d", response_code, m_remote_close, m_ready );
			
		}
		else {
			m_idletime = g_timems ;		// no idle
		}
		
		return response_code ;
	}

	// send http request
	void send_request() 
	{
		string phonenumber ;
		if( getphonenumber( phonenumber.expand(128) ) ) {
			
			m_llen = 0 ;
			m_ready = 0 ;
			
			string url ;
			// url : http://1.1.1.1/tdc/webtun.php?c=i&p=<phone number>&ser=<n+1>
			sprintf( url.expand(512), "%s?c=i&p=%s&s=%x", (char *)g_webtun_url, (char *)phonenumber, g_serial++ );
			curl_get((char *)url);
			
			m_idletime = g_timems ;
		}
		else {
			m_idletime = g_timems + 30000 ;
		}
		
	}
	
	virtual int process() 
	{
		m_eof = 0 ;						// never close this
		
		if( (g_timems - m_idletime) < 0 ) {
			if( g_maxwaitms>5000 ) 
				g_maxwaitms = 5000 ;	// idling, came back in 5s
			return 0 ; 					
		}
		if( m_wcurl == NULL )
			send_request();
			
		return 0 ;
	} 

};

static void sig_handler(int signum)
{
	g_running = 0 ;
}

int webtun_run()
{
	int  curl_run ;
	long curl_timeo = -1;

    // setup signal
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	
	curl_global_init( CURL_GLOBAL_ALL );
	g_curlm = curl_multi_init() ;
	
	printf("libcurl version: %s \n", curl_version() );

	g_timems = gettimems() ;
	
	tunnel_int();

	// main connection curl tunnel
	tunnel_add( new connect_curl() );
	
	g_running = 1 ;
	while( g_running ) {
		g_timems = gettimems() ;
		g_maxwaitms = 300000 ;
		
		curl_multi_perform(g_curlm, &curl_run);
		CURLMsg *msg;
		int Q=0 ;
		while ((msg = curl_multi_info_read(g_curlm, &Q))) {
			if (msg->msg == CURLMSG_DONE) {
				CURL * ch = msg->easy_handle ;
				curltun * tun = NULL ;
				curl_easy_getinfo(ch, CURLINFO_PRIVATE, &tun);
				if( tun!=NULL ) {
					tun->curl_complete(ch, msg->data.result);
				}
				else {
					// unknown error ?
					curl_multi_remove_handle(g_curlm, ch);
					curl_easy_cleanup(ch);
					log("Unknow curl handle !" );
				}
			}
			else {
				log("Unknow multi curl message : %d",  msg->msg );
			}
		}
		tunnel_process();
		
		curl_timeo = g_maxwaitms;
		curl_multi_timeout(g_curlm, &curl_timeo);
	
		if( curl_timeo < 0 || curl_timeo > g_maxwaitms ) 
			curl_timeo = g_maxwaitms ;
		if( curl_timeo > 0 ) {
			int maxfd = -1;
			struct timeval timeout;

			timeout.tv_sec  = curl_timeo / 1000;
			timeout.tv_usec = (curl_timeo % 1000) * 1000;
		 
			FD_ZERO(&g_fdr_set);
			FD_ZERO(&g_fdw_set);
			FD_ZERO(&g_fde_set);

			curl_multi_fdset(g_curlm, &g_fdr_set, &g_fdw_set, &g_fde_set, &maxfd);
			tunnel_fdset( &g_fdr_set, &g_fdw_set, &g_fde_set, &maxfd);

			// wait for new event
			if( select(maxfd+1, &g_fdr_set, &g_fdw_set, &g_fde_set, &timeout)<0 ) {
				if( g_running )
					sleep(5);
			}
		}
	} 
	
	tunnel_destory();

	curl_multi_cleanup(g_curlm);
	curl_global_cleanup();

	return 0;
}

int main()
{
    printf("Webtun : v%s\n", APP_VERSION );
	
#ifdef RCMAIN
	void rc_main();
	rc_main();
#endif

	webtun_run();
	
	return 0 ;
}
