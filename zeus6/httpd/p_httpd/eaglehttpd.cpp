#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>

#include "eaglehttpd.h"

#define DENIED_PAGE ("/denied.html")
#define DEFAULT_PAGE ("/system.html")
#define DEFAULT_ALT_ROOT ("/davinci/dvr/www" )

const char EOL[] = "\r\n" ;

struct http_cfg {
	int    standalone ;
	int    keepalive ;
	int    cache_maxage ;
	int    min_bufsize ;			// minimum buffers size 
	int    listen_port ;			// on which port to listen
	char * alt_root ;
	char * default_page ;
	char * denied_page ;
} ;

// global data
http_cfg cfg ;

// thread local data
struct http_data_t {
	// handle for HTTP io
	int    in ;
	int    out ;

	// http run status
	int    status ;		// http response status
	int    headerdumped ;	// header already dumped?
	int    keep_alive ;

	// cache control
	unsigned long  	etag ;   			// Etag of request document
	time_t 			modtime ;        	// last modified time
	int    			cachemaxage ;    	// cache max age (in seconds)

	// http output buffer
	char *	obuffer ;
	int		osize ;
	int		opos ;
	int		omark ;	
	int		chunked ;					//  chunked transfer
	
	char 	** environ ;

};

#ifdef _WINDOWS
#define	__thread __declspec( thread )
#endif

// http_data must be local thread safe data
__thread struct http_data_t *http_data ;

static char * strclone( char * s )
{
	char * v ;
	int l = strlen( s );
	v = (char *)malloc( l+1 );
	strcpy( v, s );
	return v ;
}

void http_resetenv();

void http_data_init() 
{
	http_data = new http_data_t ;
	memset( http_data, 0, sizeof( http_data ) );
	
	// init environ variable
	http_data->environ = (char 	**)malloc( 50 * sizeof( char *) ) ;
	http_data->environ[0] = NULL ;
}

void http_data_release() 
{
	int ienv ;
	if( http_data->in > 2 ) 
		close( http_data->in );
	if( http_data->out > 2 ) 
		close( http_data->out );

	// release env
	http_resetenv();
	free( http_data->environ );
	
	delete http_data ;
	http_data = NULL ;
}

static int __find_env( const char * name )
{
	int i ;
	int ln = strlen( name );
	for( i=0; http_data->environ[i] != NULL ;i++ ) {
		if( strncmp( name, http_data->environ[i], ln )==0 && http_data->environ[i][ln]=='=' ) {
			return i ;
		}
	}
	return -1 ;
}

char * http_getenv(const char * name)
{
	int ln = strlen( name );
	int i ;
	for( i=0; http_data->environ[i] != NULL ;i++ ) {
		if( strncmp( name, http_data->environ[i], ln )==0 && http_data->environ[i][ln]=='=' ) {
			return http_data->environ[i]+ln+1 ;
		}
	}
	return NULL ;
}

static void __http_setenv(int idx, const char * name, const char *value)
{
	char * nv ;
	int ln, lv ;
	ln = strlen(name);
	lv = strlen(value);
	if( http_data->environ[idx] ) 
		free( http_data->environ[idx] );
	nv = (char *)malloc( ln+lv+2 );
	if( nv ) {
		strcpy( nv, name );
		nv[ln] = '=' ;
		strcpy( nv+ln+1, value );
		http_data->environ[idx] = nv ;
	}
}

int http_setenv(const char *name, const char *value, int overwrite)
{
	char 	** nenv ;
	int ln = strlen( name );
	int i ;
	for( i=0; http_data->environ[i] != NULL ;i++ ) {
		if( strncmp( name, http_data->environ[i], ln )==0 && http_data->environ[i][ln]=='=' ) {
			if( overwrite ) {
				__http_setenv(i, name, value);
			}
			return 0 ;
		}
	}
	nenv = (char **)realloc( http_data->environ, (i*2+2)* sizeof( char *) ) ;
	if( nenv ) {
		http_data->environ = nenv ;
		__http_setenv(i, name, value);
		http_data->environ[i+1] = NULL ;
		return 0;
	}
	return -1 ;
}

int http_unsetenv(const char * name)
{
	int ln = strlen( name );
	int i ;
	for( i=0; http_data->environ[i] != NULL ;i++ ) {
		if( strncmp( name, http_data->environ[i], ln )==0 && http_data->environ[i][ln]=='=' ) {
			free( http_data->environ[i] ); 
			break;
		}
	}
	for( ; http_data->environ[i] != NULL ;i++ ) {
		http_data->environ[i] = http_data->environ[i+1] ;
	}
	return 0 ;
}

// reset all env variables
void http_resetenv()
{
	int ienv ;
	for( ienv=0; http_data->environ[ienv] != NULL; ienv++ ) {
		free( http_data->environ[ienv] );
		http_data->environ[ienv] = NULL ;
	} 
}

static void cfg_init()
{
	memset( &cfg, 0, sizeof(cfg));
	
	cfg.standalone = 0 ;
	cfg.keepalive = KEEP_ALIVE ;
	cfg.cache_maxage = CACHE_MAXAGE ;
	cfg.min_bufsize = HTTP_MIN_BUFSIZE ;
	cfg.listen_port = 80 ;			// default listen port
	
	cfg.alt_root = strclone( DEFAULT_ALT_ROOT );
	cfg.default_page = strclone( DEFAULT_PAGE );
	cfg.denied_page = strclone( DENIED_PAGE );
}

static void cfg_release()
{
	if( cfg.alt_root )
		free( cfg.alt_root );
	if( cfg.default_page )
		free( cfg.default_page );
	if( cfg.denied_page )
		free( cfg.denied_page );
}

// string trimming

// remove white space on tail of string
char * trimtail( char * str )
{
    int l ;
    l=strlen(str);
    while( l>0 && str[l-1]<=' ' && str[l-1]>0 ) {
		str[--l]=0;
    }
    return str ;
}

// skip space on head of string
char * trimhead( char * str )
{
    while( *str <= ' ' && *str > 0 ) {
		str++ ;
    }
    return str ;
}

// trim space on head and tail of the string
char * trim( char * str )
{
	return trimtail( trimhead(str) );
}

// return value
//		0: not exist, 1: in document root, 2: in alt document root, 3: in gz, 4: alt gz
int document_stat(const char *doc, struct stat * st, char * docname )
{
	struct stat xst ;
	char xdocname[260] ;

	if( st==NULL ) st=&xst ;		// dummy stat
	if( docname == NULL ) docname = xdocname ;
	
	strcpy( docname, doc );
	if( stat( docname, st )==0 ) 
		return 1 ;

	// check altroot
	sprintf( docname, "%s/%s", cfg.alt_root, doc );		// try open it from alt root ( in flash )
	if( stat( docname, st )==0 ) 
		return 2 ;
		
	char * accept_enc = http_getenv("HTTP_ACCEPT_ENCODING");
	if( accept_enc && strstr( accept_enc, "gzip" ) ) {		// gzip supported ?
		// check alt root gz file
		sprintf( docname, "%s/%s.gz", cfg.alt_root, doc );	// test alt gz file
		if( stat( docname, st )==0 ) 
			return 4 ;
			
		// check gz file
		sprintf( docname, "%s.gz", doc );					// test gz file
		if( stat( docname, st )==0 ) 
			return 3 ;
	}
	
	return 0;
}

FILE * document_open( char * doc )
{
	char docname[260] ;
	if( document_stat( doc, NULL, docname )>0 ) {
		return fopen( docname, "r");
	}
	return NULL ;
}

// direct input/output
int http_ready( int timeout ) 
{
	struct timeval tv ;
    tv.tv_usec = 0 ;
    tv.tv_sec = timeout ;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET( http_data->in, &fds);
    if (select( http_data->in + 1, &fds, NULL, NULL, &tv ) > 0) {
        return FD_ISSET(http_data->in, &fds);
    } 
    return 0 ;
}

int http_read( void * buf, int si )
{
	int tr = 0;
	while( tr<si ) {
		int r = (int)read( http_data->in, ((char *)buf) + tr, si-tr );
		if( r<=0 ) break ;
		tr+=r ;
	}
	return tr ;
}

int http_getc() 
{
	unsigned char c ;
	if( http_read( &c, 1 )==1 ) {
		return (int) (unsigned int) c ;
	}
	else {
		return EOF ;
	}
}

int http_gets( char * buf, int bsize )
{
	int tr = 0;
	bsize-- ;
	while( tr<bsize && http_read( buf+tr, 1 ) ) {
		if( buf[tr++] == '\n' ) {
			break ;
		}
	}
	buf[tr]=0 ;
	return tr ;
}
		
int http_write( const void * buf, int si )
{
	int tw=0 ;	// total written
	int w ;
	while( tw<si ) {
		w = write( http_data->out, ((char *)buf)+tw, (si-tw) );
		if( w<=0 ) {
			break ;
		}
		tw+=w ;
	}
	return tw ;
}

static void http_buffer_init()
{
	http_data->chunked = 0 ;				// default no chunked transfer
	http_data->opos=0 ;
	http_data->osize=0 ;
	http_data->omark=0 ;
	http_data->obuffer = NULL ;
}

// return remain free buffer size
int http_buffer_remain()
{
	if( http_data->obuffer != NULL ) {
		return (http_data->osize-http_data->opos) ;
	}
	return 0 ;
}

// return non NULL if buffer allocated or remaining buffer is big enough
char * http_buffer_request( int si )
{
	if(http_buffer_remain()>=si) {
		// buffer big enough
		return http_data->obuffer+http_data->opos ;
	}
	// retry after flush buffer out
	http_buffer_flush() ;
	if(http_buffer_remain()>=si) {
		// buffer big enough
		return http_data->obuffer+http_data->opos ;
	}	
		
	// current buffer is too small, free it 
	if( http_data->obuffer != NULL ) {
		free( http_data->obuffer );
	}
	// allocate buffer
	http_data->opos=0 ;
	http_data->omark=0 ;
	http_data->osize=cfg.min_bufsize ;
	if( http_data->osize<si ) http_data->osize = si ;
	http_data->obuffer = (char *)malloc( http_data->osize );
	return http_data->obuffer ;
}

// advance buffer position
void http_buffer_advance( int si )
{
	if( si>0 && http_data->obuffer !=NULL ) {
		http_data->opos += si ;
		if( http_data->opos>http_data->osize ) http_data->opos = http_data->osize ;
	}
}

// buffered output writing
void http_buffer_write( void * buf, int si)
{
	if( si>0 && http_buffer_request(si) ) {
		memcpy( http_data->obuffer+http_data->opos, buf, si );
		http_data->opos+=si ;
	}
}

// buffered output print
int http_buffer_print( const char *fmt, ... )
{
	char * httpbuf = http_buffer_request(HTTP_LINESIZE);
	if( httpbuf!=NULL ) {
		va_list ap ;	
		int n ;
		va_start( ap, fmt );
		n = vsnprintf(httpbuf, http_buffer_remain(), fmt, ap );
		if( n>0 ) {
			http_data->opos += n ;
		}
		va_end( ap );
	}
	return 1 ;
}

// buffered output file content
int http_buffer_writefile( FILE * f )
{
	int total = 0 ;
	int n ;
	if( f==NULL ) 
		return 0;
	while( http_buffer_request(HTTP_LINESIZE) ) {
		n = fread( http_data->obuffer+http_data->opos, 1, http_data->osize-http_data->opos, f );
		if( n>0 ) {
			total+=n ;
			http_data->opos+=n ;
		}
		else {
			break ;
		}
	}
	return total ;
}

void http_dumpheader();

// mark current position, restore the marked position by http_buffer_reset()
void http_buffer_mark()
{
	http_data->omark = http_data->opos ;
}
			
// reset buffer pos to marked position, original mark is 0
void http_buffer_reset()
{
	http_data->opos = http_data->omark ;
}
			
// flush buffer before http transfer complete
void http_buffer_flush()
{
	if( http_data->obuffer != NULL && http_data->opos>0 ) {
		// write headers if not done yet
		if( ! http_data->headerdumped ) {
			http_data->chunked  = 1 ;
			http_dumpheader();
		}
		
		if( http_data->chunked ) {
			// chunked output
			int n ;
			char chunkedheader[32] ;
			n = sprintf( chunkedheader, "%x\r\n", http_data->opos );
			http_write( chunkedheader, n );
			http_write( http_data->obuffer, http_data->opos );
			http_write( EOL, 2 );
		}
		else {
			http_write( http_data->obuffer, http_data->opos );
		}
		
		http_data->opos = 0 ;
		http_data->omark = 0 ;
	}
}

// flush buffer to complete transfer
void http_buffer_complete()
{
	// write headers if not done yet. Do this first to enable Content-Length
	if( ! http_data->headerdumped ) {
		if( !http_data->chunked ) {
			if( http_data->obuffer==NULL ) {
				http_data->opos = 0 ;
			}
			http_set_contentlength( http_data->opos );
		}
		http_dumpheader();
	}
	http_buffer_flush();
	if( http_data->chunked ) {
		http_write("0\r\n\r\n", 5 );		// end of chunked transfer, use direct write
	}
	if( http_data->obuffer != NULL ) {
		free( http_data->obuffer );
		http_data->obuffer = NULL ;
	}
	
	http_buffer_flush();
}

// only list types might be used by myself.
static char * mime_type[][2] =
{
    {"html", "text/html"},
    {"htm",  "text/html"},
    {"shtml","text/html"},
    {"shtm", "text/html"},
    {"txt",  "text/plain"},
    {"css",  "text/css"},
    {"png",  "image/png"},
    {"jpg",  "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif",  "image/gif"},
    {"ico",  "image/ico"},
    {"js",   "application/javascript"},
    {"json", "application/json"},
    {"wav",  "audio/wav"},
    {"mp3",  "audio/mpeg"},
    {"mp4",  "video/mp4"},
    {"mpeg", "video/mpeg"},
    {"mpg",  "video/mpeg"},
    {"264",  "video/h264"},
    {"bin",  "application/octet-stream" },
    {NULL,   "text/html"}
} ;

char * get_mime_type( char* name )
{
    int i;
    char * dot ;
    dot = strrchr(name,'.');
    if( dot==NULL ) {
		dot=name;
	}
	else {
		dot++;
	}
	for(i=0; i<100; i++ ) {
		if( mime_type[i][0]==NULL ||
			strcasecmp(mime_type[i][0], dot)==0 ) {
			return mime_type[i][1] ;
		}
	}
    // use binary stream as default mime type instead of text/plain
    return "application/octet-stream" ;
}

char * http_status_msg( int status )
{
	static struct {
		int status ;
		char * msg ;
	} http_msg_table [] =
	{
		{ 100, "Continue" },
		{ 200, "OK" },
		{ 300, "Multiple Choices" },
		{ 304, "Not Modified" },
		{ 400, "Bad Request" },
		{ 403, "Forbidden" },
		{ 404, "Not Found" },
		{ 0, "Unknown" },
	} ;

    int i;
    for( i=0; i<100; i++ ) {
        if( http_msg_table[i].status == status || http_msg_table[i].status<=0 ) {
            return http_msg_table[i].msg ;
        }
    }
    return "Unknown" ;
}

void http_setheader(char * name, char * cvalue )
{
    char buf[200] ;
    if( http_data->headerdumped ) 		
		return ;
    sprintf( buf, "HEADER_%s", name );
    http_setenv( buf, cvalue, 1 );
}

void http_setheader(char * name, int nvalue )
{
    char buf[200] ;
    if( http_data->headerdumped ) 		
		return ;    
    sprintf( buf, "%d", nvalue );
    http_setheader(name, buf );
}

void http_setcookie(char * name, char * value)
{
	char buf[200] ;
    if( http_data->headerdumped ) 		// too late?
		return ;
    sprintf( buf, "COOKIE_%s", name );
    http_setenv( buf, value, 1 );
}

// dumpout http headers
void http_dumpheader()
{
	time_t now;
	char * hbuf ;			// header line buffer
	int i, n;
	char * hname ;
	char * hval ;

	if( http_data->headerdumped ) 
		return ;
		
	hbuf = (char *)malloc(HTTP_LINESIZE) ;
	if( hbuf==NULL ) return ;
		
	n = snprintf(hbuf, HTTP_LINESIZE, "%s %d %s\r\n", PROTOCOL, http_data->status, http_status_msg(http_data->status) );
	http_write( hbuf, n );

	//reset Server
	http_setheader( "Server", SERVER_NAME );    //  Ignore server name from CGI
	//reset Date
    now = time( (time_t*) NULL );
    strftime( hbuf, HTTP_LINESIZE, RFC1123FMT, gmtime( &now ) );
	http_setheader( "Date", hbuf );     		//  Ignore server name from CGI
	
    if( http_data->chunked ) {
		// no content length
		http_unsetenv("HEADER_Content-Length"); 
		http_setheader( "Transfer-Encoding" , "chunked" );		// chunked transfer
	}
	
	// cache ?
	if( http_data->cachemaxage>0 ) {
	    sprintf( hbuf, "max-age=%d",  http_data->cachemaxage );
		http_setheader( "Cache-Control", hbuf );

		// Modified time
		if( http_data->modtime != 0 ) {
			strftime( hbuf, HTTP_LINESIZE, RFC1123FMT, gmtime( &http_data->modtime ) );
			http_setheader( "Last-Modified", hbuf );
		}

		//  etag
		if( http_data->etag != 0 ) {
			sprintf(hbuf, "\"%x\"", (unsigned int)http_data->etag);
			http_setheader( "ETag", hbuf);
		}
		
	}
	else {
		http_setheader( "Cache-Control" , "no-cache" );	
	}
	
    if( http_data->keep_alive>0 ) {
		http_setheader("Connection", "keep-alive" );
		sprintf( hbuf, "timeout=%d, max=99", http_data->keep_alive );
		http_setheader("Keep-Alive", hbuf );
	}
	
	// check extra header from cgi, (cgi/ssi may use this file to set headers)
    hname = http_getenv("POST_FILE_EXHEADER");
    if( hname ) {
        FILE * fexhdr = fopen( hname, "r" );
        if( fexhdr ) {
            while( fgets( hbuf, HTTP_LINESIZE, fexhdr ) )
            {
                if( (hval=strchr(hbuf, ':' )) ) {
                    *hval=0 ;
                    http_setheader(trim(hbuf), trim(hval+1)) ;
                }
            }
            fclose( fexhdr );
        }
    }
	
	char ** env = http_data->environ ;
	// dumpout headers
	for(i=0; i<500; i++ ) {
        if( env[i]==NULL || env[i][0]==0 ) {
            break;
        }
        if( strncmp( env[i], "HEADER_", 7 )==0 )
        {
			hname = env[i]+7 ;
            hval = strchr( hname, '=' ) ;
            if( hval!=NULL ) {
                int ln = hval - hname ;
                http_write( hname, ln );
                http_write( ": ", 2 );
                ++hval ;
                http_write( hval, strlen(hval) );
       			http_write( EOL, 2 );
            }
        }
    }

	for(i=0; i<500; i++ ) {
        if( env[i]==NULL || env[i][0]==0 ) {
            break;
        }
        if( strncmp( env[i], "HEADER_", 7 )==0 )
        {
			hname = env[i]+7 ;
            hval = strchr( hname, '=' ) ;
            if( hval!=NULL ) {
                int ln = hval - hname ;
                http_write( hname, ln );
                http_write( ": ", 2 );
                ++hval ;
                http_write( hval, strlen(hval) );
       			http_write( EOL, 2 );
            }
        }
    }

	// dumpout cookies
	for(i=0; i<500; i++ ) {
        if( env[i]==NULL || env[i][0]==0 ) {
            break;
        }
        if( strncmp( env[i], "COOKIE_", 7 )==0 )
        {
			hname = env[i]+7 ;
			http_write("Set-Cookie: ", 12 );
			http_write( hname, strlen(hname) );
			http_write( EOL, 2 );
		}
    }

    // empty line, finishing the http header
	http_write( EOL, 2 );
    
    free( hbuf );
    http_data->headerdumped = 1 ;
}

// output http header
void http_setstatus( int status )
{
	http_data->status = status ;
}

void http_set_contenttype( char * mime_type )
{
   if( mime_type!=NULL ) {
		http_setheader( "Content-Type" , mime_type );
    }
}

void http_set_contentlength( int length )
{
    if( length>=0 ) {
		http_setheader( "Content-Length" , length );
    }
}

void http_error( int status )
{
    http_data->cachemaxage=0 ;
	http_data->status = status ;
    http_set_contenttype( "text/html" );
    http_buffer_print( "<html><head><title>%d %s</title></head>\n<body bgcolor=\"#66ffff\"><h4>%d %s</h4></body></html>\r\n",
           status, http_status_msg( status ), status, http_status_msg( status ) );
}

// update page modified time and etag for cache control
void http_cacheupdate(char * file)
{
    struct stat filest ;
    if( http_data->cachemaxage > 0 && file != NULL && document_stat( file, &filest )>0 ) {
        if( http_data->modtime<filest.st_mtime ) {
            http_data->modtime=filest.st_mtime ;
        }
        http_data->etag+=(unsigned long)filest.st_ino ;
        http_data->etag+=(unsigned long)filest.st_size ;
        http_data->etag+=(unsigned long)filest.st_mtime ;
    }
}

void http_nocache()
{
	http_data->cachemaxage=0 ;
}

// return true if request cache is good
int http_checkcache()
{
    char tbuf[120];
    char * s ;

	if( http_data->cachemaxage<=0 ) 
		return 0;

    // check etag
    if( (s=http_getenv("HTTP_IF_NONE_MATCH"))) {
		//  etag
		sprintf(tbuf, "\"%x\"", (unsigned)http_data->etag);
        if( strcmp( s, tbuf )==0 ) {        // Etag match?
            if( (s=http_getenv("HTTP_IF_MODIFIED_SINCE"))) {
				// Modified time
				strftime( tbuf, sizeof(tbuf), RFC1123FMT, gmtime( &http_data->modtime ) );
                if( strcmp( s, tbuf )==0 ) {   // Modified time match?
                    return 1 ;                 // cache is fresh
                }
            }
        }
    }
    return 0 ;
}

// run cgi program
void cgi_run( char * cgi )
{
	FILE * fin = NULL  ;		// cgi program input stream
	char  * p ;
    char * linebuf ;	

    http_nocache();             // default no cache for cgi result

	linebuf = http_buffer_request(HTTP_LINESIZE) ;
	
	if( linebuf==NULL ) return ;		// no memory ?
	
	if( document_stat( cgi, NULL, linebuf ) ) {
		fin = popen( linebuf, "r" );
	}

	if( fin!=NULL ) {
        // parse cgi output headers
		while( (linebuf = http_buffer_request(HTTP_LINESIZE)) != NULL ) {
			if( fgets( linebuf, http_buffer_remain(), fin ) ) {
				if( *linebuf == '\r' || *linebuf == '\n' || *linebuf=='\0' ) {		// empty line 
					break;
				}
				else if( strncmp(linebuf, "HTTP/1.", 7)==0 ) {
					continue ;          // ignor HTTP responds
				}
				else if( (p=strchr(linebuf, ':' )) ) {
					*p=0 ;
					http_setheader(trim(linebuf), trim(p+1)) ;
				}
			}
			else {
				break ;
			}
		}
		
		// dump all remaining contents
		http_buffer_writefile(fin);
		pclose( fin );
	}
	
}

// set http headers as environment variable.
void sethttpenv(char * headerline)
{
    char * pn ;
    int i;
    char z ;
	char envname [260] ;
    strcpy(envname,"HTTP_");
    pn = &envname[5] ;
    for(i=0;i<252;i++){
		z = headerline[i] ;
        if(z>='a' && z<='z') {
            pn[i]=z-'a'+'A' ;
        }
        else if( z=='-' ) {
            pn[i]='_' ;
        }
        else if( z==0 ) {
            break ;
        }
        else if( z==':' ) {
            pn[i]=0 ;
            http_setenv( envname, trim( headerline+(i+1) ), 1 );
            break ;
        }
        else {
            pn[i] = z ;
        }
    }
}

// un-set http headers as environment variable.
/*
void unsethttpenv()
{
    int i ;
    int l ;
    char * eq ;
    char envname[200] ;

    for(i=0; i<500 && environ[i]!=NULL; ) {
        if ( strncmp( environ[i], "POST_FILE_", 10 )==0 )
        {
            eq = strchr( environ[i], '=' ) ;
            if( eq ) {
                // delete posted file
                unlink(eq+1);
            }
        }
        if( strncmp( environ[i], "HTTP_", 5 )==0 ||
            strncmp( environ[i], "QUERY_", 6 )==0 ||
            strncmp( environ[i], "POST_", 5 )==0 ||
            strncmp( environ[i], "VAR_", 4 )==0 ||
			strncmp( environ[i], "COOKIE_", 7 )==0 ||
            strncmp( environ[i], "HEADER_", 7 )==0 )
        {
            eq = strchr( environ[i], '=' ) ;
            if( eq ) {
                l = eq-environ[i] ;
                if( l<200 ) {
					strncpy( envname, environ[i], l);
					envname[l]=0 ;
					http_unsetenv( envname );
				}
                continue ;
            }
        }
        i++ ;
    }

    return  ;
}
*/

// set multipart part header env
static void setpartheaderenv( int partno, char * headerline)
{
    char * pn ;
    int i;
    char z ;
	char envname [260] ;
	i=sprintf( envname, "POST_PART%d_", partno ) ;
	pn = envname + i ;
    for(i=0;i<200;i++){
		z = headerline[i] ;
        if(z>='a' && z<='z') {
            pn[i]=z-'a'+'A' ;
        }
        else if( z=='-' ) {
            pn[i]='_' ;
        }
        else if( z==0 ) {
            break ;
        }
        else if( z==':' ) {
            pn[i]=0 ;
            http_setenv( envname, trim( headerline+(i+1) ), 1 );
            break ;
        }
        else {
            pn[i] = z ;
        }
    }
}

static char * getpartenv( int partno, const char * key )
{
	char * v ;
	char envname [260] ;
	sprintf( envname, "POST_PART%d_%s", partno, key ) ;
	v = http_getenv( envname );
	if( v==NULL ) return "" ;
	else return v ;
}
    
// process mulitpart post input (file uploads)
// return 1: success, 0: failed to received all content
int http_inputmultipart( int content_length )
{
    FILE * postfile ;
    char * boundary ;
    char * p ;
    char * p2 ;
    char * linebuf ;
    int  i ;
    int  d ;
    int lbdy=0 ;            // boundary length ;
    int datatype = 1 ;      // 0: dataend, 1: data, 2: databoundary
    int res=0;

    p = http_getenv("HTTP_CONTENT_TYPE");
    if( p ) {
        boundary = strstr(p, "boundary=" ) ;
        if( boundary ) {
            boundary += 9 ;
            lbdy = strlen(boundary);
        }
        else {
            return res;
        }
    }
    else {
        return res;
    }

    if( content_length <= 2*lbdy ) {
        return res;
    }

    linebuf = (char *)malloc(HTTP_LINESIZE) ;
    
    // get first boundary
    if( http_gets( linebuf, HTTP_LINESIZE) == 0 ) {
		free( linebuf );
        return res;
	}
        
    if( linebuf[0]!='-' ||
        linebuf[1]!='-' ||
        strncmp( &linebuf[2], boundary, lbdy )!=0 ){
		free( linebuf );
        return res;
	}

	// file uploading progress file
    FILE * ulprogfile = fopen("uploading", "w");
    int  ulbytes = 0 ;

    fprintf(ulprogfile,"0" );
    fflush(ulprogfile);
    
    int partnum = 0 ;

    while(ulbytes<content_length) {
        char part_name[256] ;
        ++partnum;
        
        // default part name
        sprintf( linebuf, "NAME: part%d", partnum );
        setpartheaderenv( partnum, linebuf ) ;
        
        // mulitpart headers
        while ( http_gets( linebuf, HTTP_LINESIZE )  )
        {
			ulbytes += strlen( linebuf ) ;
			
            if( linebuf[0] == '\r' ||
				linebuf[0] == '\n' ||
				linebuf[0] == 0 ) {
                break;
            }
            else {
				setpartheaderenv( partnum, linebuf ) ;
			}
		}
		
		// process content disposition header
		char * cdisp = getpartenv( partnum, "CONTENT_DISPOSITION" );
		if( cdisp && strncmp( cdisp, "form-data;", 10)==0 ) {
			cdisp+=10 ;
			// process content disposition 
			p=strstr(cdisp, " name=\"");
			if( p ) {
				p+=7 ;
				p2=strchr(p, '\"');
				if( p2 ) {
					strcpy( linebuf, "NAME: " );
					i=p2-p ;
					strncpy( linebuf+5, p, i );
					linebuf[5+i]=0 ;
					// over write NAME env
					setpartheaderenv( partnum, linebuf );
				}
			}
			
			p=strstr(cdisp, " filename=\"");
			if( p ) {
				p+=11 ;
				p2=strchr(p, '\"');
				if( p2 ) {
					i=p2-p ;
					strncpy( part_name, p, i );
					part_name[i]=0 ;
					
					sprintf( linebuf, "POST_FILENAME_%s", getpartenv( partnum, "NAME" ) );
					http_setenv( linebuf, part_name, 1 );
				}
			}
		}
        
		sprintf(part_name, "post_file_%d_%d",  (int)getpid(), partnum );
        postfile = fopen(part_name, "w");
        if( postfile ) {
            // set POST_FILE_* env
            sprintf( linebuf, "POST_FILE_%s", getpartenv( partnum, "NAME" ) );
            http_setenv( linebuf, part_name, 1 );

            // get content data
            i = 0 ;
            datatype=1 ;
            while( datatype==1 ) {
                d=http_getc();
                
                // update download progress
                ulbytes++ ;
                if( (ulbytes%(100+content_length/150))==0 ) {
                    rewind(ulprogfile);
                    fprintf(ulprogfile,"%d\r\n", ulbytes*100/content_length );
                    fflush(ulprogfile);
                }

                if( d==EOF ) {
                    if( i>0 ) {
                        fwrite( linebuf, 1, i, postfile);
                        i=0 ;
                    }
                    datatype=0 ;
                    break;
                }
                else if( d=='\r' ) {
                    if( i>0 ) {
                        fwrite( linebuf, 1, i, postfile);
                        i=0 ;
                    }
                    linebuf[i++]=d ;
                }
                else if( d=='\n' && i==1 ) {
                    linebuf[i++]=d ;
                }
                else if( d=='-' && (i==2 || i==3) ) {
                    linebuf[i++]=d ;
                }
                else if( i>3 && d==boundary[i-4] ) {
                    if(boundary[i-3]=='\0') { // boundary found
                        datatype=2 ;
                        break;
                    }
                    linebuf[i++]=d ;
                }
                else {
                    linebuf[i++]=d ;
                    fwrite( linebuf, 1, i, postfile);
                    i=0 ;
                }
            }
            fclose( postfile );
        }
        else {
            break;                              // can't open file
        }


		if( http_gets(linebuf, HTTP_LINESIZE) ) {
			ulbytes += strlen( linebuf ) ;
			
			if( linebuf[0]=='\r' || linebuf[0]=='\n' || linebuf[0]==0 ) {
				// empty line, go for next part
				continue ;
			}
			else if( linebuf[0]=='-' && linebuf[1]=='-' ) {
				// end of multipart
				res=1;
				break;
			}
			else {
				break ;
			}
		}
		else {
			break;
		}
    }
    
    free( linebuf );
    
    // complete upload progress
    rewind(ulprogfile);
    fprintf(ulprogfile,"100");
    fclose( ulprogfile );
    
    return res ;
}

// process post data
// return 1: success, 0: failed to received all content
int http_inputpost()
{
    char * 	request_method ;
    int    	content_length=0 ;
    char * 	content_type ;
    char * 	post_string;
    char * 	p ;
    int		res = 0 ;

    // check input
    p = http_getenv("HTTP_CONTENT_LENGTH") ;
    if( p ) {
        if( sscanf( p, "%d", &content_length ) !=1 ) {
			content_length=0 ;
		}
    }

    if( content_length<=0 ) {       // no contents
        return 1 ;                  // return success.
    }

    content_type = http_getenv("HTTP_CONTENT_TYPE");
    request_method = http_getenv("REQUEST_METHOD") ;
    if( strcmp( request_method, "POST" )==0 ) 	// support "POST" contents only
    {
        if( content_length<200000 &&
			content_type &&
            strncasecmp( content_type, "application/x-www-form-urlencoded", 32)==0 )
        {
            post_string = (char *)malloc( content_length + 1 );
            if( post_string!=NULL ) {
                if( http_read( post_string, content_length ) == content_length ) {
                    post_string[content_length]=0;
                    http_setenv( "POST_STRING", post_string, 1 );
                    res = 1 ;
                }
                free( post_string );
            }
        }
        else if( content_length<20000000 && 
			content_type &&
			strncasecmp( content_type, "multipart/form-data", 19 )==0 ) {
            res = http_inputmultipart(content_length);
        }
        else {
			// unknown POST
			// discard all contents
			while( content_length-->0 ) {
				http_getc();
			}
			res = 1 ;
		}
    }
    return res ;
}

// output requested documents
void http_document()
{
    struct stat sb;
    char * uri ;
    int len ;
    FILE * fp ;
    int doctype ;

    // use linebuf to store document name.
    uri = http_getenv("REQUEST_URI");

    if( uri==NULL ) {
        http_error( 404 );
        return ;
    }
	uri++ ;

#ifdef MOD_SUPPORT
	// in app module support
    if( strncmp(uri, "mod/", 4 )==0 )
    {
        // execute module 
        extern void mod_run();
        http_nocache();
        mod_run();
        return ;
    }
#endif

    // is file available ?
    doctype = document_stat( uri, &sb ) ;

    if( doctype == 0 ||  S_ISDIR( sb.st_mode ) ) { 	// not support directory list
        http_error( 404 );
        return ;
	}

    len = strlen( uri );
    // check file type by extension
    if( doctype <= 2  &&							// not .gz file
		(strcasecmp(&uri[len-5], ".html")==0 ||   	// to run small ssi
		strcasecmp(&uri[len-6], ".shtml")==0 ) )
    {
	    http_set_contenttype("text/html; charset=UTF-8");

		// bug fix for IE8, add IE document mode support
		// http_setheader("X-UA-Compatible", "IE=EmulateIE8" );

		smallssi_include_file( uri );
		
		// try check cache if header not dumped yet
		if( !http_data->headerdumped ) {					// if headers has been sent out, then we can do nothing
			if( http_checkcache() ) {       		// check if cache fresh?
				// reset http buffer
				http_data->opos = 0 ;						// discard output buffer
				http_setstatus( 304 );				// let browser use cache
				return ;
			}
		}
    }
    else if( doctype <= 2 && 
			( strncmp(uri, "cgi/", 4 )==0 ||
			  strcasecmp(&uri[len-4], ".cgi")==0 ))
    {
		// no cache
        http_nocache();
        // execute cgi.
        cgi_run( uri );
    }
    else {

		if( doctype > 2 ) {			// this is a .gz file
			http_setheader( "Content-Encoding", "gzip");
		}
		
		// check if to use cache ?
		http_cacheupdate( uri );
		if( http_checkcache() ) {       		// check if cache fresh?
			http_setstatus( 304 );
			return ;
		}

        fp = document_open( uri );

        if ( fp == NULL ) {
            http_error( 403 );
            return ;
        }
        
        http_set_contenttype(get_mime_type( uri ));
        
        http_buffer_writefile(fp);
        
        fclose( fp );
    }

}

// process_input (header part) of http protocol
// return 1: success, 0: failed
int http_input()
{
    char * method ;		// request method
    char * uri ;		// request URI
    char * query ;		// request query string
    char * protocol ;	// request protocol
    char * linebuf ;
    linebuf = (char *)malloc( HTTP_LINESIZE ) ;

    if ( http_gets( linebuf, HTTP_LINESIZE ) == 0 )
    {
		free(linebuf);
        return 0;
    }

    method = trimhead(linebuf);
    uri = strchr(method, ' ');
    if( uri==NULL ) {
		free(linebuf);
        return 0;
    }
    *uri=0;
    // set request method, support POST and GET only
    if( *method != 'G' && *method != 'P' ) {
		free(linebuf);
		return 0;
	}
	http_setenv( "REQUEST_METHOD", method, 1 );

    uri=trimhead(uri+1);
    // check for invalid uri
    if( uri[0] != '/' ||
        uri[1] == '/' ||
        strstr( uri, "/.." ) )
    {
		free(linebuf);
        return 0;
    }

    protocol=strchr(uri, ' ');
    if( protocol==NULL ) {
		free(linebuf);
        return 0;
    }

    *protocol=0;
    protocol=trimhead(protocol+1);
    // check for valid protocol
    if( strncasecmp(protocol, "HTTP/1.", 7)!=0 ) {
		free(linebuf);
        return 0;
    }

    // get query string
    query=strchr(uri, '?');
    if( query ){
        *query=0;
        http_setenv( "QUERY_STRING", query+1, 1);
    }

    if( uri[0]=='/' && uri[1]=='\0' ) {        	// default page
        http_setenv( "REQUEST_URI", cfg.default_page, 1 );
    }
    else {
        http_setenv( "REQUEST_URI", uri, 1 );
    }

    // read request headers
    while ( http_gets( linebuf, HTTP_LINESIZE )  )
    {
		if( linebuf[0] == '\r' ||
			linebuf[0] == '\n' ||
			linebuf[0] == 0 ) {
			// empty line, header completed
			break;
		}
        // set header as env HTTP_???
        sethttpenv(linebuf);
    }
    
	free(linebuf);
	
	// success!
    return 1 ;
}

int access_check();


void initenv()
{
	char host[256] ;
	char port[32] ;
	
    http_resetenv();

    // set document root dir
    http_setenv( "DOCUMENT_ROOT", getcwd( host, 256 ), 1);
    http_setenv( "ALT_ROOT", cfg.alt_root, 1);
	http_setenv( "SERVER_PROTOCOL", PROTOCOL, 1 );
	http_setenv( "SERVER_SOFTWARE", SERVER_NAME, 1 );
    
    struct sockaddr_in sname ;
	socklen_t slen ;
	slen = sizeof(sname);
	if( getsockname( http_data->in, (struct sockaddr *)&sname, &slen)==0 ) {
		if( getnameinfo( (struct sockaddr *)&sname, slen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV )==0 ) {
			http_setenv( "SERVER_ADDR", host, 1 );
			http_setenv( "SERVER_PORT", port, 1 );
		}
	}
	slen = sizeof(sname);
	if( getpeername( http_data->in, (struct sockaddr *)&sname, &slen)==0 ) {
		if( getnameinfo( (struct sockaddr *)&sname, slen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV )==0 ) {
			http_setenv( "REMOTE_ADDR", host, 1 );
			http_setenv( "REMOTE_PORT", port, 1 );
		}
	}
}

void http()
{
	char * checkheader ;

    initenv();	

	if( !http_ready( http_data->keep_alive+5 ) ) {
		http_data->keep_alive=0 ;
        return ;
    }
    
    http_data->status = 200 ;			// default status, http OK
    http_data->keep_alive=0 ;
    http_data->headerdumped = 0 ;
    http_data->etag=0 ;
    http_data->modtime=0 ;
    http_data->cachemaxage=cfg.cache_maxage ;		// enable cache default, call nocache() to disable

    if( http_input()==0 ) {
        http_error(400);
        return ;
    }

    // save post data
    if( http_inputpost()==0 ) {
        http_error(400 );
        return ;
    }
    // all inputs processed!
    
    // check keep alive header 
    checkheader = http_getenv("HTTP_CONNECTION");
	if( checkheader && strcasestr( checkheader, "keep-alive" )!=NULL ) {
        http_data->keep_alive=cfg.keepalive   ;
	}

	// check cache control
    checkheader = http_getenv("HTTP_CACHE_CONTROL");
	if( checkheader && strcasestr( checkheader, "no-cache" )!=NULL ) {
		http_nocache();
	}
    checkheader = http_getenv("REQUEST_METHOD");
	if( checkheader && strcasecmp( checkheader, "GET" )!=0 ) {
		http_nocache();
	}
	
    // setup extra header files. (for inline exec support)
    char ehd[20] ;
    sprintf( ehd, "ehd%d", getpid() );
    http_setenv("POST_FILE_EXHEADER", ehd, 1 );
	
	// init http output buffer
    http_buffer_init();
    
	// parse All request, (GET, POST, COOKIE)
	parseRequest();
	
    // access check
    if( access_check() == 0 ) { 		// access failed!
		http_setenv("REQUEST_URI", cfg.denied_page, 1);          // replace with denied page
		http_nocache();										// don't cache denied page
	}

    // output http document
    http_document() ;
    
    // complete/flush out http output buffer
    http_buffer_complete();

    // remove all HTTP_* environment and POST files
    // unsethttpenv() ;

    return ;
}

void sigpipe(int sig)
{
    _exit(0);
}

// listen on port 80 for http connections, make this program a stand alone http server
int http_listen()
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    union {
        struct sockaddr_in sin ;
        struct sockaddr_in6 sin6 ;
    } saddr ;
    socklen_t saddrlen;
    int sockfd;
    int val;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = PF_INET ;
    hints.ai_socktype = SOCK_STREAM ;

    res = NULL;
    char port[16] ;
    sprintf(port, "%d", cfg.listen_port );
    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        exit(1);
    }
    if (res == NULL) {
        exit(1);
    }
    ressave = res;

    /*
     Try open socket with each address getaddrinfo returned,
     until getting a valid listening socket.
     */
    sockfd = -1;
    while (res) {
        sockfd = socket(res->ai_family,
                        res->ai_socktype, res->ai_protocol);

        if (sockfd != -1) {
            val = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val,
                       sizeof(val));
            if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
                break;
            }
            close(sockfd);
            sockfd = -1;
        }
        res = res->ai_next;
    }

    freeaddrinfo(ressave);

    if (sockfd == -1) {
        exit(1);
    }
    listen(sockfd, 10);
    return sockfd ;
}

void readcfg( char * fcfg ) 
{
	char * xkey ;
	char * xvalue ;
	FILE * cfgfile ;
	cfgfile = fopen( fcfg, "r" );
	if( cfgfile!=NULL ) {
		char * lbuf = (char *)malloc( HTTP_LINESIZE );
		while( fgets( lbuf, HTTP_LINESIZE, cfgfile ) ) {
			xvalue = strchr( lbuf, '=' ) ;
			if( xvalue ) {
				*xvalue = 0 ;
				xvalue++ ;
				xkey = trim( lbuf );
				
				// check keys
				if( strcmp( xkey, "httpd.standalone" ) == 0 ) {
					cfg.standalone = atoi( xvalue );
				}
				else if(strcmp( xkey, "httpd.keepalive" ) == 0 ) {
					cfg.keepalive = atoi( xvalue );
				}
				else if(strcmp( xkey, "httpd.cache_maxage" ) == 0 ) {
					cfg.cache_maxage = atoi( xvalue );
				}			
				else if(strcmp( xkey, "httpd.min_bufsize" ) == 0 ) {
					cfg.min_bufsize = atoi( xvalue );
				}	
				else if(strcmp( xkey, "httpd.listen_port" ) == 0 ) {
					cfg.listen_port = atoi( xvalue );
				}		
				else if(strcmp( xkey, "httpd.document_root" ) == 0 ) {
				    chdir( trim(xvalue) );			// change dir directly
				}		
				else if(strcmp( xkey, "httpd.alt_root" ) == 0 ) {
					if( cfg.alt_root ) free( cfg.alt_root ) ;
					cfg.alt_root = strclone( trim(xvalue) ) ;
				}		
				else if(strcmp( xkey, "httpd.default_page" ) == 0 ) {
					if( cfg.default_page ) free( cfg.default_page ) ;
					cfg.default_page = strclone( trim(xvalue) ) ;
				}		
				else if(strcmp( xkey, "httpd.denied_page" ) == 0 ) {
					if( cfg.denied_page ) free( cfg.denied_page ) ;
					cfg.denied_page = strclone( trim(xvalue) ) ;
				}		
			}
		}
		free( lbuf );
		fclose( cfgfile );
	}
}

// process arguments
void arg( int argc, char * argv[] ) 
{
	int i;
	char * ar ;
	for( i=1; i<argc; i++ ) {
		ar = argv[i] ;
		if( *ar == '-' ) {
			switch( ar[1] ) {
				case 'l' :			// listening (standalone server)
					cfg.standalone = 1 ;
					break;

				case 'p' :			// port
					// imply standalone server
					cfg.standalone = 1 ;
					cfg.listen_port = atoi( ar+2 );
					break;

				case 'k' :			// keep alive time out
					cfg.keepalive = atoi( ar+2 );
					break;

				case 'b' :			// buffer size
					cfg.min_bufsize = atoi( ar+2 );
					break;

				case 'r' :			// document root
					chdir( ar+2 );
					break;

				case 'a' :			// alt root
					if( cfg.alt_root ) free( cfg.alt_root ) ;
					cfg.alt_root = strclone( ar+2 ) ;
					break;
										
				case 'd' :			// default page
					if( cfg.default_page ) free( cfg.default_page ) ;
					cfg.default_page = strclone( ar+2 ) ;
					break;

				case 'n' :			// denied_page
					if( cfg.denied_page ) free( cfg.denied_page ) ;
					cfg.denied_page = strclone( ar+2 ) ;
					break;
					
				case 'f' :			// read config file
					readcfg( ar+2 );
					break;

				default :
					break;
			}
		}
		else {	// document root
			chdir( ar );
		}
	}
}

// single http instance ( to be modified for mulit-threading )
void http_run( int s_in, int s_out )
{
	int runloop=20 ;

	http_data_init() ;
	http_data->in = s_in ;		
	http_data->out = s_out ;	
    http_data->keep_alive=120 ;		// longer initial waits
    
    while( --runloop > 0 && http_data->keep_alive>0 ) {
	    http();
    }

	http_data_release() ;
}


int main(int argc, char * argv[])
{
    // do some cleaning on SIGPIPE
    signal(SIGPIPE,  sigpipe );

	// init cfg
	cfg_init() ;
	arg( argc, argv );
	
	if( cfg.standalone ) {
		int s_server = http_listen() ;
		if( s_server > 0 ) {
			int s_http ;
			while( (s_http = accept(s_server,NULL,NULL)) > 0 ) {
				http_run(s_http,s_http);
			}
		}
		else {
			printf("Can't start server.\n");
		}
    }
    else {
		// executed from inetd
		http_run(0,1) ;		// stdin, stdout
	}

	// release configure memory
	cfg_release();
		
    return 0 ;
}
