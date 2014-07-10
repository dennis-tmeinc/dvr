#ifndef __HTTPD_H__
#define __HTTPD_H__

#define SERVER_NAME "Eagle HTTP 0.30"
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define CACHE_MAXAGE        (86400)
#define MAX_KEEP_ALIVE      (100)

extern char **environ ;

extern int	http_keep_alive ;
extern int	http_cacheable ;
extern int 	http_status ;

// internal functions table
struct internal_fn_t {
	char * cname ;
	void (* func)() ;
} ;

// get query value
char * getquery( const char * qname );
char * getcookie(char * key);
void http_header(const char * header, const char *value);
void setcookie(char * key, char * value);
void setcontenttype( char * script_extension );
void setcontentlength( int length );
void http_response( int status, char * title );
void http_output( char * buf, int buflen );

#endif
