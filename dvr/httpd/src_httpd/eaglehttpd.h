#ifndef __HTTPD_H__
#define __HTTPD_H__

#include "cfg.h"

#define SERVER_NAME "Eagle HTTP 0.33"
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define CACHE_MAXAGE        (86400)
#define KEEP_ALIVE          (60)

// HTTP buffering output
#define HTTP_MIN_BUFSIZE	(256*1024)
#define HTTP_LINESIZE		(4*1024)

extern int	http_keep_alive ;
extern int	http_cacheable ;
extern int 	http_status ;

// trim string
char * trimtail( char * str );
char * trimhead( char * str );
char * trim( char * str );

// http document 
int document_stat(const char *doc, struct stat * st = NULL, char * docname = NULL );
FILE * document_open( char * doc );
int alt_root();

void parseRequest();
char * getRequest( const char * req, const char * type = NULL );	// type: "GPC" : Get, Post, Cookie

void http_setheader(char * name, char * cvalue );
void http_nocache();
void http_cacheupdate(char * file);
void http_setstatus( int status );
void http_set_contenttype( char * mime_type );
void http_set_contentlength( int length );
void http_setcookie(char * name, char * value);

// return non NULL if buffer allocated and big enough
char * http_buffer_request( int si );
// return remain free buffer size
int http_buffer_remain();
// buffered output writing
void http_buffer_write( void * buf, int si);
// buffered output print
int http_buffer_print( const char *fmt, ... );
// buffered output file content
int http_buffer_writefile( FILE * f );
// advance buffer position
void http_buffer_advance( int si );
void http_buffer_flush();


// small ssi support
void smallssi_include_file( char * ifilename );

#endif
