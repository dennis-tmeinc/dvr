#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#define SERVER_NAME "thttp"
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

#define closesocket   close

static char * mime_type[][2] = 
{
    {"htm", "text/html; charset=UTF-8"},
    {"html", "text/html; charset=UTF-8"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"png", "image/png"},
    {"css", "text/css"},
    {"au", "audio/basic"},
    {"wav", "audio/wav"},
    {"avi", "video/x-msvideo"},
    {"mov", "video/quicktime"},
    {"qt", "video/quicktime"},
    {"mpeg", "video/mpeg"},
    {"mpe","video/mpeg"},
    {"midi", "audio/midi"},
    {"mid", "audio/midi"},
    {"mp3", "audio/mpeg"},
    {"ogg", "application/ogg"},
    {"txt", "text/plain"},
    {"", "stream/octem"}
} ;

char linebuf[4096] ;

char * get_mime_type( char* name )
{
    int i;
    char * dot ;
    dot = strrchr(name,'.');
    if( dot ) {
        for(i=0; i<100; i++ ) {
            if( mime_type[i][0][0]==0 ) break;
            if( strcasecmp(mime_type[i][0], dot+1)==0 ) {
                return mime_type[i][1] ;
            }
        }
    }
    // use binary stream as default mime type instead of text/plain
    return "application/octet-stream" ;
}

void http_header( int status, char * title, char * mime_type, int length )
{
    time_t now;
    char timebuf[100];

    printf( "%s %d %s\r\n", PROTOCOL, status, title );
    printf( "Server: %s\r\n", SERVER_NAME );
    now = time( (time_t*) 0 );
    strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    printf( "Date: %s\r\n", timebuf );
    printf( "Cache-Control: no-cache\r\n" );
    if ( mime_type != NULL ) {
	    printf( "Content-Type: %s\r\n", mime_type );
    }
    if ( length > 0 ) {
	    printf( "Content-Length: %ld\r\n", (long) length );
    }
}

void http_error( int status, char * title )
{
    http_header( status, title, "text/html", -1 );
    printf( "\r\n<html><head><title>%d %s</title></head>\n<body bgcolor=\"#cc9999\"><h4>%d %s</h4></body></html>\n", status, title, status, title );
    fflush( stdout );
}

// remove whilespace on head and tail of input string 
int copycleanstring( char * instr, char * outstr )
{
    int len = 0 ;
    while( *instr > 0 &&
           *instr<=' ' ) {
            instr++;
           }
    while( *instr ) {
        *outstr++=*instr++ ;
        len++ ;
    }
    *outstr=0 ;
    while(len>0) {
        outstr-- ;
        if( *outstr > 0 && 
           *outstr <= ' ' ) {
               *outstr=0 ;
               len-- ;
        }
        else {
            break;
        }
    }
    return len ;
}

char ssi_script[]="ssi.cgi" ;

void cgi_run( char * execfile )
{
    pid_t childid ;
    fflush(stdout);
    if( (childid=fork())==0 ) {
	execl( execfile, execfile, NULL );
	exit(1) ;		
    }
    else {
	waitpid(childid, NULL, 0);
    }
}

// remove white space on head and tail of string
char * cleanstring( char * instr )
{
    int len ;
    while( *instr>0 && *instr<=' ' ) {
	instr++;
    }
    len=strlen(instr);
    while( len>0 && instr[len-1]>0  && instr[len-1]<=' ') {
	len--;
    }
    instr[len]=0;
    return instr ;    
}

int smallssi_gettoken(FILE * fp, char * token, int bufsize)
{
    int r=0;
	int c ;
	c = fgetc(fp);
    token[r] = c ;
    if( c == EOF ) return 0;
    if( token[r] == '<' ) {
	r++;
	token[r] = getc(fp);
    if( token[r] == '!' ) {
	r++;
	token[r] = getc(fp);
    if( token[r] == '-' ) {
	r++;
	token[r] = getc(fp);
    if( token[r] == '-' ) {
	r++;
	token[r] = getc(fp);
    if( token[r] == '#' ) {
	while( r<bufsize-2 ) {
            r++;
	    token[r]=getc(fp) ;
	    if( token[r]=='>' && token[r-1]=='-' && token[r-2]=='-' ) {
		break;
	    }
        }
    }}}}}
    token[r+1]=0;
    return r+1 ;
}

// cmd : <--!#include tag="includefile"
void smallssi_include( char * ssicmd )
{
    char * eq ;
    char * ifile ;
    FILE * pifile ;
    int r ;
    eq = strchr( ssicmd, '=' );
    if( eq ) {
         ifile=strchr(eq, '\"');
	if( ifile ) {
		ifile++;
		eq=strchr(ifile, '\"');
		if( eq ) {
			*eq=0;
			pifile = fopen(ifile, "r");
			if( pifile ) {
				while( (r=fread(linebuf, 1, sizeof(linebuf), pifile))>0 ) {
					fwrite(linebuf, 1, r, stdout);
				}
				fclose( pifile );			
			}
		}
	}
    }
 
}

void smallssi_echo( char * ssicmd )
{
    char * eq ;
    char * ifile ;
    FILE * pifile ;
    int r ;
    eq = strchr( ssicmd, '=' );
    if( eq ) {
         ifile=strchr(eq, '\"');
	if( ifile ) {
		ifile++;
		eq=strchr(ifile, '\"');
		if( eq ) {
			*eq=0;
			eq=getenv(ifile);
			if( eq ) {
				fputs( eq, stdout );
			}
		}
	}
    }
}

void smallssi_exec( char * ssicmd )
{
    char * eq ;
    char * ifile ;
    FILE * pifile ;
    int r ;
    eq = strchr( ssicmd, '=' );
    if( eq ) {
         ifile=strchr(eq, '\"');
	if( ifile ) {
		ifile++;
		eq=strchr(ifile, '\"');
		if( eq ) {
			*eq=0;
			cgi_run(ifile);
		}
	}
    }
}

void smallssi_run()
{
    int r ;
    FILE * fp ;
    fp = fopen( getenv("PATH_TRANSLATED"), "r" );
    if ( fp == NULL ) {
	http_error( 403, "Forbidden" );
        return ;
    }

    http_header( 200, "Ok", "text/html; charset=UTF-8", -1 );
    printf("\r\n");                                 // empty line.
    
    while( (r=smallssi_gettoken(fp, linebuf, sizeof(linebuf) ) )>0 ) {
	if( r<8 ) {
	    fputs(linebuf, stdout);
	}
	else if( strncasecmp(linebuf, "<!--#include ", 13) == 0 ) {		// found ssi include command
	    smallssi_include(linebuf);
	}
	else if( strncasecmp(linebuf, "<!--#echo ", 10) == 0 ) {		// found ssi echo command
	    smallssi_echo(linebuf);
	}
	else if( strncasecmp(linebuf, "<!--#exec ", 10) == 0 ) {		// found ssi exec command
	    smallssi_exec(linebuf);
	}
	else {
	    fputs(linebuf, stdout);		// pass to client
	}
    }

    fclose( fp );
    printf("\r\n");
}


int http_run()
{
    char method[11], protocol[21];
    char path[301], file[300];
    char * ext ;
    char * pc ;
    size_t len;
    int ch;
    FILE* fp;
    int i;
    struct stat sb;

    if ( fgets( linebuf, sizeof(linebuf), stdin ) == NULL )
    {
        http_error(400, "Bad Request" );
        return -1 ;
    }
    if ( sscanf( linebuf, "%10s %300s %20s", method, path, protocol ) != 3 ) {
        http_error(400, "Bad Request" );
        return -1 ;
    }

    // check method
    if( strcasecmp(method, "POST")==0 ) {
	setenv( "REQUEST_METHOD", "POST", 1 );
    }
    else if( strcasecmp(method, "GET")==0 ) {
	setenv( "REQUEST_METHOD", "GET", 1 );
    }
    else {
	unsetenv( "REQUEST_METHOD" );
        http_error(300, "Bad Query");
        return -1 ;
    }

    // check path
    if( path[0] != '/' ) {
        http_error(400, "Bad Request" );
        return -1;
    }
    if( path[1] == '/' ||
        strstr( path, "/.." ) ) {
        http_error(404, "Not Found" );    
    }
    setenv("PATH_INFO", path, 1 );
    for( i=1; i<300 ;i++ ) {
        if( path[i] == '?' || path[i]==0 ) {
            break;
        }
        file[i-1]=path[i] ;
    }
    file[i-1] = 0 ;
    setenv("PATH_TRANSLATED", file, 1);
    
    if ( stat( file, &sb ) < 0 ) {
        http_error( 404, "Not Found");
        return -1;
    }
    if ( S_ISDIR( sb.st_mode ) )    // not supporting directory list
    {
        http_error( 403, "Forbidden" );
        return -1;
    }
    
    // get query string
    unsetenv( "QUERY_STRING" );
    if( path[i]=='?' ) {
	setenv("QUERY_STRING", path+i+1, 1 );
    }
    
    // check protocol
    if( strncasecmp(protocol, "HTTP/1.", 7)!=0 ) {
        http_error( 300, "Bad Request");
        return -1 ;
    }
    setenv("SERVER_PROTOCOL", protocol, 1 );

    // unset header related environment
    unsetenv("HTTP_HOST");
    unsetenv("SERVER_PORT");
    unsetenv("SERVER_NAME");
    unsetenv("HTTP_USER_AGENT");
    unsetenv("HTTP_COOKIE");
    unsetenv("HTTP_ACCEPT");
    unsetenv("CONTENT_TYPE");
    unsetenv("CONTENT_LENGTH");
    
    // analyz request header
    while ( fgets( linebuf, sizeof(linebuf), stdin )  )
	{
        // skip all headders.
        if( linebuf[0] == 0 ||
            strcmp( linebuf, "\n" ) == 0 || 
            strcmp( linebuf, "\r\n" ) == 0 ){
            break;
	}
        else if( strncmp(linebuf, "Host:", 5)==0 ) {
	    pc = cleanstring( linebuf+5 );
	    setenv("HTTP_HOST", pc, 1);
	    ext = strrchr( pc, ':' ) ;
	    if( ext ) {
		setenv("SERVER_PORT", ext+1, 1 );
	        *ext=0;
            }
	    setenv("SERVER_NAME", pc, 1 );
        }
        else if( strncmp(linebuf, "User-Agent:", 11)==0 ) {
	    setenv("HTTP_USER_AGENT", cleanstring(linebuf+11), 1);
        }
        else if( strncmp(linebuf, "Cookie:", 7)==0 ) {
            setenv("HTTP_COOKIE", cleanstring(linebuf+7), 1);
        }
	else if( strncmp(linebuf, "Accept:", 7)==0 ) {
	    setenv("HTTP_ACCEPT", cleanstring(linebuf+7), 1);
	}
        else if( strncmp(linebuf, "Content-Type:", 13)==0 ) {
	    setenv("CONTENT_TYPE", cleanstring(linebuf+13), 1 );
        }
        else if( strncmp(linebuf, "Content-Length:", 15)==0 ) {
            setenv("CONTENT_LENGTH", cleanstring(linebuf+15), 1 );
        }
    }
    
    // check if we need to run a cgi
    ext=strrchr( file, '.' ) ;
    if( ext ) {
        if( strcasecmp(ext, ".sshtml")==0 ||
            strcasecmp(ext, ".sshtm")==0 ) {   // to run small ssi
	    smallssi_run();
	    fflush( stdout );
            return 1 ;
	}
        else if( strcasecmp(ext, ".cgi")==0 ) {
	    // execute cgi ?
	    cgi_run( file );
            fflush( stdout );
            return 1;
        }
        else if( strcasecmp(ext, ".shtml")==0 ||
		 strcasecmp(ext, ".shtm")==0 ) {   // to run ssi
	    // it is a ssi script
            cgi_run( ssi_script );
            fflush( stdout );
            return 1;
        }
    }
    
    // if we are here, no cgi is executed
    fp = fopen( file, "r" );	    

    if ( fp == NULL ) {
	http_error( 403, "Forbidden" );
        return -1;
    }
    
    fseek( fp, 0, SEEK_END );
    len = (size_t) ftell( fp );
    fseek( fp, 0, SEEK_SET );
    
    if( len<=0 ) {
        http_error( 404, "Not Found" );
        fclose( fp );
        return -1;
    }

    
    http_header( 200, "Ok", get_mime_type( file ), len );
    printf("\r\n");                                 // empty line.
    
    while ( ( ch = getc( fp ) ) != EOF ) {
	putchar( ch );    
    }
    
    printf("\r\n"); 
    fclose( fp );
    fflush( stdout );

    return 0 ;
}    

static int serverfd;

int net_recvok(int fd, int usdelay)
{
    struct timeval timeout ;
    timeout.tv_usec = usdelay % 1000000 ;
    timeout.tv_sec = usdelay/1000000 ;
    fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &timeout ) > 0) {
		return FD_ISSET(fd, &fds);
	} else {
		return 0;
	}
}

int net_sendok(int fd, int usdelay)
{
	struct timeval timeout ;
    timeout.tv_usec = usdelay % 1000000 ;
    timeout.tv_sec = usdelay/1000000 ;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, NULL, &fds, NULL, &timeout) > 0) {
		return FD_ISSET(fd, &fds);
	} else {
		return 0;
	}
}

int net_listen(int port, int socktype)
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ressave;
	int sockfd;
	int val;
	char service[20];

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = socktype;

	sprintf(service, "%d", port);

	res = NULL;
	if (getaddrinfo(NULL, service, &hints, &res) != 0) {
		fprintf(stderr, "Error:net_listen!");
		return -1;
	}
	if (res == NULL) {
        fprintf(stderr, "Error:net_listen!");
		return -1;
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
			if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0)
				break;
			closesocket(sockfd);
			sockfd = -1;
		}
		res = res->ai_next;
	}

	freeaddrinfo(ressave);

	if (sockfd == -1) {
        fprintf(stderr, "Error:net_listen!");
		return -1;
	}
	if (socktype == SOCK_STREAM) {
		// turn on TCP_NODELAY, to increase performance
		int flag = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
		listen(sockfd, 8);
	}

	return sockfd;
}

int app_quit ;
int net_port ;

void net_run()
{
    int fd ;
    int savestdin, savestdout ;
    while( app_quit == 0  ) {
        if( net_recvok( serverfd, 1000000 ) ) {
            fd = accept( serverfd, NULL, NULL ) ;
            if( fd > 0 ) {
                savestdin = dup( 0 );
                savestdout = dup( 1 );
                dup2( fd, 0 );          // dup fd to stdin
                dup2( fd, 1 );          // dup fd to stdout
		setvbuf( stdin, NULL, _IONBF, 0);	// unbuffered mode
                closesocket(fd);
                http_run();
                dup2( savestdin, 0 );   // restore stdin, (closed fd)
                close( savestdin );
                dup2( savestdout, 1);   // restore stdout
                close( savestdout );
            }
        }
    }
}


// initialize network
void net_init()
{
	app_quit = 0;				// assume not runninghttp://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.9.2
	if (net_port == 0)
		net_port = 80;

	serverfd = net_listen(net_port, SOCK_STREAM);
	if (serverfd == -1) {
        exit(1);
	}
}

void net_finish()
{
    app_quit=1;
	closesocket(serverfd);
}

int main( int argc, char * argv)
{
    net_port=11151 ;
    net_init();
    net_run();
    net_finish();
    return 0;
}
