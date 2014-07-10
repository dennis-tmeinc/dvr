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

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

#include "curl/curl.h"
#include "curl/easy.h"
#include "curl/multi.h"


CURLM * g_curlm ;			// global curl multi interface
int     g_running ;			// global running connections 
string  g_webtun_url ;		// tunnel server url ;
string  g_hostname ;
string  g_phonenumber ;
string  g_tmpdir ;
string g_logfile ;

int     g_urlidx ;
const char * default_url[2] = {
	"http://tdlive.darktech.org/vlt/vltdir.php",
	"http://64.40.243.198/vlt/vltdir.php"
	};

unsigned g_serial ;

#define WT_FLAG_MORE	(1)		// more data to go
#define WT_FLAG_INIT	(2)		// initial connection
#define WT_FLAG_CLOSE	(4)		// closed connection

#define MAX_TUNNEL	(100)

// write log
void log(char *fmt, ...)
{
	long o1=0 , o2=0, r ;
	char linebuf[1024] ;
	if( *(char *)g_logfile ) {
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

static time_t starttime = (time_t)0 ;
int gettime()
{
	struct timespec ts ;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)(ts.tv_sec - starttime) ;
}

int inittime()
{
	starttime = gettime();
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

// Connect curl session format
//    Request:	
//		 GET http://1.1.1.1/tdc/webtun.php?c=i&p=<phone number>&ser=<n+1>
//       Content-Length: 0
//   Response: (may wait up to 60s if no data available)
//       200
//			X-Webt-Redir: http://1.1.1.2/sanjose/webtun.php
//          Content-Length: n
//       Contents:
//           <cmd:c> <tunnel id> [host] [port]\r\n
//           <cmd:d> <tunnel id> size\r\n
//           binary data of <size> bytes
//           <cmd:d> <tunnel id> size\r\n
//           binary data of <size> bytes
//           ...

// GET curl session format
//    Request:	
//		 GET http://1.1.1.1/tdc/webtun.php?c=g&t=<tunnel id>&ser=<n+1>
//       Content-Length: 0
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
// 	      
//   Response: ( response imediately )
//       200


class webtun {
protected:	
	
	string m_id ;
	
	int  m_socket ;						// socket to local host
	CURL * m_wcurl ;					// write curl connection, webtun -> dvr (GET)
	int  m_wend ;						// 1: no more GET request
	
	CURL * m_rcurl ;					// read curl connection, dvr -> webtun (PUT)
	int    m_putsize ;
	
	int m_active_time ;
	
	struct curl_slist * m_rheader ;

public:	

	webtun( const char * id, const char * host, int port ) {
		m_id = id ;

		m_wcurl = NULL ;
		m_wend = 0 ;
		m_rcurl = NULL ;
		
		m_rheader = NULL ;
		
		m_socket = -1 ;
		m_active_time = gettime();
		
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
		
	}

	~webtun(){
		if( m_rcurl ) {
			curl_multi_remove_handle(g_curlm, m_rcurl); 
			curl_easy_cleanup(m_rcurl);
			m_rcurl = NULL ;
		}
		if( m_wcurl ) {
			curl_multi_remove_handle(g_curlm, m_wcurl); 
			curl_easy_cleanup(m_wcurl);
			m_wcurl = NULL ;
		}
		if( m_rheader ) {
			curl_slist_free_all(m_rheader);
		}

		if( m_socket > 0 ) {
			close( m_socket );
		}
	}
	
	const char * id()
	{
		return (const char *)m_id ;
	}
	
	size_t wheaderfunc( CURL * eh, void * ptr, size_t si )
	{
		// X-Webt-Connection: close	// if present, close the connection
		if( strncasecmp( (char *)ptr , "X-Webt-Connection:", 18)==0 ) {

			string header ;
			char * ph = header.expand( si );
			memcpy( ph, ((char *)ptr)+18, si-18 );
			ph[si-18] = 0 ;
			header.trim();

			log("(%s) x-webt: %s", (char *)m_id, (char *)header );
			
			if( strcasecmp( header, "close" ) == 0 ) {
				// connection closed.
				if( m_socket>0 ) {
					close( m_socket );
					m_socket = -1 ;
				}
			}
			else if( strcasecmp( header, "gend" ) == 0 ) {
				// no more GET request
				m_wend = 1 ;
			}
		}
		return si ;
	}
	
	static size_t s_wheaderfunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		CURL * eh = (CURL *)userdata ; 	// curl easy handle
		char * priv ;
		curl_easy_getinfo( eh, CURLINFO_PRIVATE, &priv);
		return ((webtun *)priv)->wheaderfunc( eh, ptr, size*nmemb );
	}

	size_t wdatafunc( CURL * eh, void *ptr, size_t size )
	{
		if( eh == m_wcurl && size>0 && m_socket>0 ) {		// write curl (GET)
			write( ptr, size );
		}
		else if( eh == m_rcurl && size>0 && m_socket>0 ) {	// read curl (PUT)
			log("WDATA:PUT:---");
		}
		return size ;
	}
	
	static size_t s_wdatafunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		CURL * eh = (CURL *)userdata ; 	// curl easy handle
		char * priv ;
		curl_easy_getinfo( eh, CURLINFO_PRIVATE, &priv);
		return ((webtun *)priv)->wdatafunc( eh, ptr, size*nmemb );
	}
	
	size_t rdatafunc( CURL * eh, void *ptr, size_t size )
	{
		int r = 0 ;
		if( eh == m_rcurl && m_socket>0 ) {		// read curl (PUT)

			if( size> m_putsize ) {
				size = m_putsize ;
			}
			if( size > 0 ) {
				r =  read( ptr, size ) ;
			}
			m_putsize -= r ;
		}
		else {
			memset( ptr, 0, size) ;			// empty padding data
			r = size ;
		}
		return r ;
	}
	
	static size_t s_rdatafunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		CURL * eh = (CURL *)userdata ; 	// curl easy handle
		char * priv ;
		curl_easy_getinfo( eh, CURLINFO_PRIVATE, &priv);
		return ((webtun *)priv)->rdatafunc( eh, ptr, size*nmemb );		
	}
	
// curl side function

	// send data out use PUT request through m_rcurl
	void rcurl_send()
	{
		if( m_socket>0 && m_rcurl == NULL ) {
			m_rcurl = curl_easy_init(); 

			string url ;
			// url:	 PUT http://1.1.1.1/tdc/webtun.php?c=p&t=<tunnel id>&s=<n+1>
			sprintf( url.expand(512), "%s?c=p&t=%s&s=%x", (char *)g_webtun_url, (char *)m_id, g_serial++ );
			// use data file handle
			
			curl_easy_setopt(m_rcurl, CURLOPT_URL, (char *)url );
			curl_easy_setopt(m_rcurl, CURLOPT_TIMEOUT, 60 );		// 1 minute timeout
			
			// call back handler
			curl_easy_setopt(m_rcurl, CURLOPT_PRIVATE, (char *)this);
			curl_easy_setopt(m_rcurl, CURLOPT_READDATA,  m_rcurl);
			curl_easy_setopt(m_rcurl, CURLOPT_READFUNCTION,  s_rdatafunc);
			curl_easy_setopt(m_rcurl, CURLOPT_WRITEHEADER,  m_rcurl);
			curl_easy_setopt(m_rcurl, CURLOPT_HEADERFUNCTION,  s_wheaderfunc);
			curl_easy_setopt(m_rcurl, CURLOPT_WRITEDATA,  m_rcurl);
			curl_easy_setopt(m_rcurl, CURLOPT_WRITEFUNCTION,  s_wdatafunc);

			curl_easy_setopt(m_rcurl, CURLOPT_UPLOAD, 1L);
			curl_easy_setopt(m_rcurl, CURLOPT_PUT, 1L);
			
			if( m_rheader ) {
				curl_slist_free_all(m_rheader);
			}
			m_rheader = NULL ;
			m_rheader = curl_slist_append( m_rheader, "Accept:" );
			
			m_putsize = 0 ;
			ioctl(m_socket, FIONREAD, &m_putsize);
			if( m_putsize <= 0 ) {	// closed or error?
				// close socket 
				close( m_socket );
				m_socket = -1 ;
				
				m_putsize = 0 ;
				// header to close connection
				m_rheader = curl_slist_append( m_rheader, "X-Webt-Connection: close" );
				
				log("PUT(%s):closed",(char*)m_id);
				
			}
			curl_easy_setopt(m_rcurl, CURLOPT_HTTPHEADER, m_rheader);
			curl_easy_setopt(m_rcurl, CURLOPT_INFILESIZE_LARGE,(curl_off_t)m_putsize);			
			
			// add to curl multi interface
			curl_multi_add_handle(g_curlm, m_rcurl);
			
			log( "PUT(%s): %s  (%d)", (char*)m_id,(char*)url, (int)m_putsize );

		}
	}

	// send date receiving request use GET request through m_wcurl
	void wcurl_send()
	{
		if( m_socket>0 &&  m_wcurl == NULL && m_wend == 0 ) {
			
			string url ;
			// url:	 GET http://1.1.1.1/tdc/webtun.php?c=g&t=<tunnel id>&s=<n+1>				
			sprintf( url.expand(512), "%s?c=g&t=%s&s=%x", (char *)g_webtun_url, (char *)m_id, g_serial++ );
			
			m_wcurl = curl_easy_init(); 

			curl_easy_setopt(m_wcurl, CURLOPT_URL, (char *)url );
			curl_easy_setopt(m_wcurl, CURLOPT_TIMEOUT, 600 );		// 10 min timeout

			// call back handler
			curl_easy_setopt(m_wcurl, CURLOPT_PRIVATE, (char *)this);
			curl_easy_setopt(m_wcurl, CURLOPT_READDATA,  m_wcurl);
			curl_easy_setopt(m_wcurl, CURLOPT_READFUNCTION,  s_rdatafunc);
			curl_easy_setopt(m_wcurl, CURLOPT_WRITEHEADER,  m_wcurl);
			curl_easy_setopt(m_wcurl, CURLOPT_HEADERFUNCTION,  s_wheaderfunc);
			curl_easy_setopt(m_wcurl, CURLOPT_WRITEDATA,  m_wcurl);
			curl_easy_setopt(m_wcurl, CURLOPT_WRITEFUNCTION,  s_wdatafunc);

			curl_easy_setopt( m_wcurl, CURLOPT_HTTPGET, 1);

			// add to curl multi interface
			curl_multi_add_handle(g_curlm, m_wcurl);
			
			log( "GET(%s): %s", (char*)m_id,(char*)url );
		}
	}

	void curl_complete( CURL * e, CURLcode result)
	{
		long response_code = 0 ;

		curl_multi_remove_handle(g_curlm, e);

		curl_easy_getinfo( e, CURLINFO_RESPONSE_CODE, &response_code );
			
		m_active_time = gettime();

		if( e==m_rcurl ) {
			
			log("PUT(%s): completed : code: %d", (char*)m_id,(int)response_code );
			
			m_rcurl = NULL ;
			if( result == CURLE_OK && response_code == 200 ) {	// success
			}
			else {						// failed
				if( m_socket>0 ) {
					close( m_socket ) ;
					m_socket = -1 ;
				}			
			}
		}
		else if( e==m_wcurl ) {


			double download ;
			curl_easy_getinfo( e, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &download );
			log("GET(%s): completed : code: %d, size: %d", (char*)m_id,(int)response_code,(int)download );
			


			m_wcurl = NULL ;
			if( result == CURLE_OK && response_code == 200 ) {	// success
			}
			else {				// failed
				if( m_socket>0 ) {
					close( m_socket ) ;
					m_socket = -1 ;
				}
			}
		}

		// clean up curl handle
		curl_easy_cleanup(e);

	}

// generic funcitons
	
	int checkid( char * id ) {
		return strcmp( id, m_id )==0 ;
	}

	int isclosed() {
		return ( m_socket <= 0 && m_wcurl == NULL && m_rcurl == NULL ) || (gettime()-m_active_time)>1800 ;
	}
	
	int process()
	{
		if( m_socket>0 && m_rcurl == NULL && rrdy() ) {
			rcurl_send() ;
		}
		if( m_socket>0 && m_wcurl == NULL && wrdy() ) {
			wcurl_send() ;
		}
		return 0 ;
	}

// socket side functions
	void fdset( fd_set * fdr, fd_set * fdw, fd_set * fde, int * maxfd )
	{
		if( m_socket>0 ) {
			if( m_rcurl == NULL ) {
				FD_SET( m_socket, fdr ) ;
			}
			if( m_wcurl == NULL ) {
				FD_SET( m_socket, fdw ) ;
			}
			FD_SET( m_socket, fde ) ;
			if( m_socket>*maxfd ) *maxfd = m_socket ;
		}
	}
		
	// if read data ready
	int rrdy( int millis = 0 ) {
		struct timeval timeout ;
		timeout.tv_sec = millis/1000 ;
		timeout.tv_usec = (millis%1000)*1000 ;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(m_socket, &fds);
		if (select(m_socket + 1, &fds, NULL, NULL, &timeout) > 0) {
			return FD_ISSET(m_socket, &fds);
		} 
		return 0 ;
	}

	// read 
	int read( void * ptr, int si ) {
		char * cptr = (char *)ptr ;
		while ( m_socket>0 && si>0 && rrdy(10) ) {
			int r = recv( m_socket, cptr, si, 0 );
			if( r>0 ) {
				cptr+=r ;
				si-=r ;
			}
			else {	// socket closed?
				close( m_socket );
				m_socket = -1 ;
			}		
		}
		return cptr - (char *)ptr ;
	}

	// if write data ready
	int wrdy( int millis = 0 ) {
		struct timeval timeout ;
		timeout.tv_sec = millis/1000 ;
		timeout.tv_usec = (millis%1000)*1000 ;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(m_socket, &fds);
		if (select(m_socket + 1, NULL, &fds, NULL, &timeout) > 0) {
			return FD_ISSET(m_socket, &fds);
		} 
		return 0 ;
	}
	
	int write( void * data, int size ) {
		
		char * cdata = (char *)data ;
		while( size>0 && m_socket > 0 ) {
			if( wrdy( 10000 ) ){
				int w = send( m_socket, cdata, size, 0 );
				if( w>=0 ) {
					cdata+=w ;
					size-=w ;
				}
				else {
					// error!
					close(m_socket);
					m_socket = -1 ;
					break;
				}
			}
			else {
				// time out
				close(m_socket);
				m_socket = -1 ;
				break;
			}
		}
		return (cdata-(char *)data) ;
	}
	
} ;

webtun * webtun_array[MAX_TUNNEL] ; 
webtun * tunnel_create( char * id, char * host, int port )
{
	int i ;
	for( i=0; i<MAX_TUNNEL; i++ ) {
		if( webtun_array[i] == NULL ) {
			webtun_array[i] = new webtun( id, host, port );
			return webtun_array[i] ;
		}
	}
	return NULL ;
}

webtun * tunnel_byid( char * id )
{
	int i ;
	for( i=0; i<MAX_TUNNEL; i++ ) {
		if( webtun_array[i] && webtun_array[i]->checkid( id ) ) {
			return webtun_array[i] ;
		}
	}
	return NULL ;
}

void tunnel_process()
{
	int i;
	for( i=0; i<MAX_TUNNEL; i++ ) {
		if( webtun_array[i] ) {
			webtun_array[i]->process();
			if( webtun_array[i]->isclosed() ) {
				
				log("DEL (%s)", webtun_array[i]->id() );
				
				delete webtun_array[i] ;
				webtun_array[i] = NULL ;
			}
		}
	}
}

void tunnel_fdset( fd_set * fdr, fd_set * fdw, fd_set * fde, int * maxfd )
{
	int i;
	for( i=0; i<MAX_TUNNEL; i++ ) {
		if( webtun_array[i] ) 
			webtun_array[i]->fdset( fdr, fdw, fde, maxfd );
	}
}

// Connect curl session format
//    Request:	
//		 GET http://1.1.1.1/tdc/webtun.php?c=i&p=<phone number>&h=<host name>&ser=<n+1>
//       Content-Length: 0
//   Response: (may wait up to 60s if no data available)
//       200
//			X-Webt-Redir: http://1.1.1.2/sanjose/webtun.php
//          Content-Length: n
//       Contents:
//           <cmd:c> <tunnel id> [host] [port]\r\n
//           <cmd:d> <tunnel id> size\r\n
//           binary data of <size> bytes
//           <cmd:d> <tunnel id> size\r\n
//           binary data of <size> bytes
//           ...
class connect_curl {
protected:	
	FILE * m_datafile ;
	int    m_ready ;
	int    m_idletime ;
	
public:	

	CURL * m_curl ;					// write curl connection, webtun -> dvr

	connect_curl() {
		
		m_curl = NULL ;
		m_datafile = NULL ;
		m_idletime = gettime();
		request();

	}
	
	~connect_curl() {
		if( m_curl ) {
			curl_multi_remove_handle(g_curlm, m_curl);			
			curl_easy_cleanup(m_curl);
		}
		if( m_datafile ) {
			fclose(m_datafile);
		}
	}
	
	size_t headerfunc( void * ptr, size_t si )
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
				m_idletime = gettime() + delaytime ;
			}
		}
		return si ;
	}
	
	static size_t s_wheaderfunc( void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		return ((connect_curl *)userdata)->headerfunc(ptr, size*nmemb);
	}
	
	void request() {
		
		if( m_curl ) return ;	// request already sent
		if( gettime()<m_idletime ) {
			return ; 		// idling
		}
		
		string phonenumber ;

		if( getphonenumber( phonenumber.expand(128) ) ) {
			string url ;
			
			// url : http://1.1.1.1/tdc/webtun.php?c=i&p=<phone number>&ser=<n+1>
			sprintf( url.expand(512), "%s?c=i&p=%s&s=%x", (char *)g_webtun_url, (char *)phonenumber, g_serial++ );
			// use data file handle
			
			m_curl = curl_easy_init(); 
			curl_easy_setopt(m_curl, CURLOPT_URL, (char *)url );
			curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 600 );		// 10 min timeout
			
			curl_easy_setopt(m_curl, CURLOPT_WRITEHEADER, (void *)this);
			curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION,  connect_curl::s_wheaderfunc);
			
			string tmpdatafile ;
			sprintf( tmpdatafile.expand(256), "%s/webtun.conn", (char *)g_tmpdir );
			m_datafile = fopen( tmpdatafile, "w+");
			curl_easy_setopt(m_curl, CURLOPT_WRITEDATA,  (void *)m_datafile);
			curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1);
			// add to curl multi interface
			curl_multi_add_handle(g_curlm, m_curl);		
			m_ready = 0 ;
			
			log("req: %s", (char *)url);
		}
		else {
			m_idletime = gettime() + 30 ;
		}
	}

	void curl_complete( CURL * ch, CURLcode result)
	{
		long response_code = 0 ;
		curl_multi_remove_handle(g_curlm, ch);
		curl_easy_getinfo( m_curl, CURLINFO_RESPONSE_CODE, &response_code );
		
		log("req complete.(code:%d)",(int)response_code);
			
		if( result == CURLE_OK && response_code == 200 && m_ready ) {
			
			if( ftell( m_datafile) > 0 ) {

				char buf[4096] ;
				fseek( m_datafile, 0, SEEK_SET );
				while( fgets( buf, sizeof(buf)-1, m_datafile ) ) {
					int r ;
					char id[256] ;
					int chunksize ;
					int port ;
					char host[256] ;
					
					switch( buf[0] ) {
						case 'c' :
							// <cmd:c> <tunnel id> [host] [port]\r\n
							// create new connection
							if( sscanf( buf+1, "%s %s %d", id, host, &port)==3 ) {
								tunnel_create( id, host, port ) ;
							}
							break;

						case 'd' :
							// <cmd:d> <tunnel id> size\r\ne
							// send data for tunnel 				
							chunksize = 0 ;
							if( sscanf( buf+1, "%s %d", id, &chunksize )==2 && chunksize>0 ) {
								webtun * wt = tunnel_byid( id );
								if( wt ) {
									while( chunksize > 0 ) {
										int wsi = chunksize ;
										
										if( wsi>(int)sizeof(buf) ) {
											wsi = (int)sizeof(buf) ;
										}
										fread( buf, 1, wsi, m_datafile );
										if( wsi>0 ) {
											wt->write( buf, wsi );
										}
										
										chunksize-=wsi ;
									}
								}
								else {
									// no such connection, discard data
									fseek( m_datafile, chunksize, SEEK_CUR );
								}
							}
							break;
							
						default :
							break;
					}
				}
			}
		}
		else {
			// failed ? fall back to default url
			g_urlidx = (g_urlidx+1)%2 ;
			g_webtun_url = default_url[g_urlidx] ;
			m_idletime = gettime() + 100 ;
		}

		if( m_datafile ) {
			fflush( m_datafile );
			ftruncate( fileno( m_datafile ), 0 );
			fclose(m_datafile);
			m_datafile = NULL ;
		}

		if( result == CURLE_OK && response_code == 200 && m_ready ){
			curl_easy_cleanup(m_curl);
			m_curl = NULL ;
			request();
		}
		else {
			curl_easy_cleanup(m_curl);
			m_curl = NULL ;
		}

	}

};


int run()
{
	int  curl_run ;
	long curl_timeo = -1;
	
	g_running = 1 ;
	
	connect_curl * cc = new connect_curl ;
	
	while( g_running ) {
		cc->request();
		curl_multi_perform(g_curlm, &curl_run);
		CURLMsg *msg;
		int Q ;
		while ((msg = curl_multi_info_read(g_curlm, &Q))) {
			if (msg->msg == CURLMSG_DONE) {
				CURL * ch = msg->easy_handle ;
				if( ch == cc->m_curl ) {
					cc->curl_complete(ch, msg->data.result);
				}
				else {
					char * priv ;
					curl_easy_getinfo(ch, CURLINFO_PRIVATE, &priv);
					(( webtun * ) priv )->curl_complete(ch, msg->data.result);
				}
			}
			else {
				log("Unknow multi message : %d",  msg->msg );
			}
		}

		tunnel_process();

		curl_multi_timeout(g_curlm, &curl_timeo);
		
		if( curl_timeo !=0 ) {
			
			struct timeval timeout;
			int rc; 
			int maxfd = -1;
		 
			/* set a suitable timeout to play around with */ 
			timeout.tv_sec = 5 ;
			timeout.tv_usec = 0;
			
			if(curl_timeo > 0) {
			  timeout.tv_sec = curl_timeo / 1000;
			  if(timeout.tv_sec > 5)
				timeout.tv_sec = 5;
			  else
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
			}
			
			fd_set fdr;
			fd_set fdw;
			fd_set fde;
		 
			FD_ZERO(&fdr);
			FD_ZERO(&fdw);
			FD_ZERO(&fde);

			curl_multi_fdset(g_curlm, &fdr, &fdw, &fde, &maxfd);
			tunnel_fdset( &fdr, &fdw, &fde, &maxfd);
		 
			// wait for new event
			rc = select(maxfd+1, &fdr, &fdw, &fde, &timeout);
			if( rc<0 ) {
				sleep(10);
			}
		}
	} 
	delete cc ;
	return 0;
}

// return true if modem is up
int app_init()
{
	int res = 0 ;
	string v ;
    config dvrconfig(CFG_FILE);
    
    // get remote tunnel server
    v = dvrconfig.getvalue("network", "livesetup_url") ;
	if( strncmp( (char *)v, "http://", 7 )!=0 ) {
		g_webtun_url = default_url[0] ;
	}
	else {
		g_webtun_url = (char *)v ;
		char * u = (char *)g_webtun_url ;
		int l = strlen( u ) ;
		if( u[l-1]=='/' ) {
			// 
			sprintf( v.expand( l+20 ), "%slivetun.php", u ) ;
			g_webtun_url = (char *)v ;
		}
	}

    v = dvrconfig.getvalue("network", "livesetup_logfile") ;
    if( v.length() > 0 ) {
		g_logfile = (char *)v ;
	}

    v = dvrconfig.getvalue("network", "livesetup_tmpdir") ;
    if( v.length() > 0 ) {
		g_tmpdir = (char *)v ;
	}
	else {
		g_tmpdir = "/tmp" ;
	}
	
	inittime();
	return res ;

}

int main()
{
	// wait until modem is up
	app_init();
	curl_global_init( CURL_GLOBAL_ALL );
	g_curlm = curl_multi_init() ;

	run();
	
	curl_multi_cleanup(g_curlm);
	curl_global_cleanup();
	return 0 ;
}
