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

#include "httpd.h"

static char * mime_type[][2] =
{
    {"html", "text/html; charset=UTF-8"},
    {"htm", "text/html; charset=UTF-8"},
    {"shtml", "text/html; charset=UTF-8"},
    {"shtm", "text/html; charset=UTF-8"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"png", "image/png"},
    {"ico", "image/ico"},
    {"js",  "text/javascript"},
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
    {"mp4", "video/mp4"},
    {"", "stream/octem"}
} ;

char document_root[256] ;
char temp_root[256] ;

int		http_keep_alive ;
int		http_cacheable ;
int 	http_resp_done ;
int 	http_status ;
int		request_content_length ;

int		request_time ;
FILE *  fd_in ;
FILE *  fd_out ;

// internal functions
void xtest1()
{
    http_output("<html><head><title>xtest1</title></head>\n<body bgcolor=\"#66ffff\"><h4>XTEST1</h4></body></html>", -1 );
}

// internal functions table
struct internal_fn_t internal_fn[] = { { "xtest1",  xtest1 }, {NULL,NULL} } ;

// check if data ready to read
int recvok(int fd, int usdelay)
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

// remove white space on head and tail of string
char * stringtrim( char * instr )
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

// remove white space on head and tail of string
char * stringupper( char * str )
{
	char * s = str ;
	while( *s ) {
		if( islower(*s) ) {
			*s = toupper(*s) ;
		}
		s++ ;
	}
	return str ;
}

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

// convert hex to int
int hextoint(int c)
{
    if( c>='0' && c<='9' )
        return c-'0' ;
    if( c>='a' && c<='f' )
        return 10+c-'a' ;
    if( c>='A' && c<='F' )
        return 10+c-'A' ;
    return 0;
}

// decode url encoded string
int decode(const char * in, char * out, int osize )
{
    int i ;
    int o ;
    for(i=0, o=0; o<osize-1; o++ ) {
        if( in[i]=='%' && in[i+1] && in[i+2] ) {
            out[o] = hextoint( in[i+1] ) * 16 + hextoint( in[i+2] );
            i+=3 ;
        }
        else if( in[i]=='+' ) {
            out[o] = ' ' ;
            i++ ;
        }
        else if( in[i]=='&' || in[i]=='\0' ) {
            break ;
        }
        else {
            out[o]=in[i++] ;
        }
    }
    out[o] = 0 ;
    return i;
}


// get query value
char * getquery( const char * qname )
{
    static char qvalue[1024] ;
    char * query ;
    int l = strlen(qname);
    int x ;
    query = getenv("QUERY_STRING");
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( strncmp(qname, qvalue, l)==0 && qvalue[l]=='=' ) {
            return &qvalue[l+1] ;
        }
        if( query[x]=='\0' )
            break;
        else {
            query+=x+1;
        }
    }
    query = getenv("POST_STRING" );
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( strncmp(qname, qvalue, l)==0 && qvalue[l]=='=' ) {
            return &qvalue[l+1] ;
        }
        if( query[x]=='\0' )
            break;
        else {
            query+=x+1;
        }
    }
    // see if it is multipart value
    sprintf(qvalue, "POST_VAR_%s", qname);
    query = getenv(qvalue);
	if( query ) 
		return query ;

	// POST FILE multipart
    sprintf(qvalue, "POST_FILE_%s", qname);
    query = getenv(qvalue);
    if( query ) {
        FILE * fquery = fopen( query, "r" );
        if( fquery ) {
            x = fread(fquery, 1, sizeof(qvalue)-1, fquery );
            fclose( fquery );
            if( x>0 ) {
                qvalue[x]=0 ;
                return qvalue ;
            }
        }
    }

    return NULL ;
}

char * getcookie(char * key)
{
    static char cookievalue[1024] ;
    char * cookie ;
    char * needle ;
    int  l = strlen(key);
    int i;

    cookie=getenv("HTTP_COOKIE");
    while( cookie ) {
        needle = strstr(cookie, key) ;
        if( needle ) {
            if( needle[l]=='=' ) {
                needle+=l+1 ;
                for( i=0; i<1023; i++ ) {
                    if( needle[i]==';' || needle[i]<' ' ) break;
                    cookievalue[i]=needle[i] ;
                }
                while( cookievalue[i-1]<=' ' ) {
                    i-- ;
                }
                cookievalue[i]=0;
                i=0;
                while( cookievalue[i]>0 && cookievalue[i]<=' ' ) {      // skip white space
                    i++;
                }
                return &cookievalue[i] ;
            }
            cookie = needle+l+1 ;
        }
        else {
            break;
        }
    }
    return NULL;
}

struct http_msg_t {
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
{ 0, "" },
} ;

char * http_msg( int status )
{
    int i;
    for( i=0; i<100; i++ ) {
        if( http_msg_table[i].status == status || http_msg_table[i].status<=0 ) {
            return http_msg_table[i].msg ;
        }
    }
    return "" ;
}

void http_header(const char * header, const char *value)
{
    char hdenv[256] ;
    sprintf( hdenv, "HEADER_%s", header );
	if( value==NULL || *value==0 ) {
		unsetenv(hdenv);
	}
	else {
		setenv( hdenv, value, 1 );
	}
}

void setcookie(char * key, char * value)
{
    char cookie[256] ;
    sprintf( cookie, "COOKIE_%s", key );
    setenv( cookie, value, 1 );
}

void setcontenttype( char * script_name )
{
    setenv("HEADER_Content-Type", get_mime_type( script_name ), 1 );
}

void setcontentlength( int length )
{
	char cclen[32] ;
    sprintf(cclen, "%d", length);
    setenv("HEADER_Content-Length",cclen,1);
}

// output http response and headers
void http_response( int status, char * title )
{
    time_t now;
    char hbuf[120];
    int i ;
    char * resp ;
    char * fn ;

	if( http_resp_done ) return ;

    if( title==NULL ) {
        // use default title
        title=http_msg(status);
    }

    fprintf( fd_out, "%s %d %s\r\n", PROTOCOL, status, title );
	http_header("Server",NULL);		//  Not allowed for CGI to change server
	http_header("Date",NULL);      	//  Not allowed for CGI to change date
    fprintf( fd_out, "Server: %s\r\n", SERVER_NAME );
    now = time( (time_t*) NULL );
    strftime( hbuf, sizeof(hbuf), RFC1123FMT, gmtime( &now ) );
    fprintf( fd_out, "Date: %s\r\n", hbuf );

	if( http_cacheable && status < 400 ) {
		http_header( "Cache-Control", "max-age=3600");
	}
	else {
		// remove cache related header
		http_header( "Etag", NULL );
		http_header( "Last-Modified", NULL );
		http_header( "Cache-Control", "no-cache");
	}
	
	http_keep_alive = 0 ;
	if( request_content_length==0 && getenv("HEADER_Content-Length")!=NULL ) {
		char * httpconnection = getenv( "HTTP_CONNECTION" );
		if( httpconnection && strcasestr( httpconnection, "Keep-Alive" ) ) {
			http_header( "Connection", "Keep-Alive" );
			http_keep_alive = MAX_KEEP_ALIVE ;
			sprintf(hbuf, "timeout=%d,max=%d", 15, http_keep_alive );
			http_header( "Keep-Alive", hbuf );
		}
	}
	if( http_keep_alive == 0 ) {
		http_header( "Connection", "close" );
	}

    for(i=0; i<500; i++ ) {
        if( environ[i]==NULL || environ[i][0]==0 ) {
            break;
        }
        else if( strncmp( environ[i], "HEADER_", 7 )==0 )
        {
            resp = strchr( environ[i], '=' ) ;
            if( resp ) {
                int l = resp-environ[i] ;
                strncpy( hbuf, environ[i], l);
                hbuf[l]=0 ;
                fprintf( fd_out, "%s: %s\r\n", &(hbuf[7]), resp+1 );
            }
        }
        else if( strncmp( environ[i], "COOKIE_", 7 )==0 )
        {
            resp = strchr( environ[i], '=' ) ;
            if( resp ) {
                int l = resp-environ[i] ;
                strncpy( hbuf, environ[i], l);
                hbuf[l]=0 ;
                fprintf( fd_out, "Set-Cookie: %s=%s\r\n", &(hbuf[7]), resp+1 );
            }
        }
    }

    // empty line, finishing the http header
	fprintf( fd_out, "\r\n");
	http_resp_done = 1 ;
}

void http_output( char * buf, int buflen )
{
	if( !http_resp_done ) {
		http_response( http_status, NULL );
	}
	if( buflen<0 ) {
		buflen=strlen(buf);
	}
	if( buflen>0 ) {
		fwrite( buf, 1, buflen, fd_out );	
	}
}

void http_error( int status, char * title )
{
	char errormsg[256] ;
    http_response( status, title );
	sprintf( errormsg, "<html><head><title>%d %s</title></head>\n<body bgcolor=\"#66ffff\"><h4>%d %s</h4></body></html>\r\n",
           status, title, status, title );
	http_output( errormsg, -1);
}

// return true if document match cache condition (match etag and modtime)
int http_checkcache()
{	
	char * s ;
	if( http_cacheable ) {
		char * ims = getenv("HTTP_IF_MODIFIED_SINCE");
		char * inm = getenv("HTTP_IF_NONE_MATCH");
		if( ims==NULL && inm==NULL ) {
			return 0 ;
		}

		if( ims ) {
			s = getenv("HEADER_Last-Modified");
			if( s==NULL || strcasecmp( s, ims )!=0 ) {
				return 0;
			}
		}

		if( inm ) {
			s = getenv("HEADER_Etag");
			if( s==NULL || strcasecmp( s, inm )!=0 ) {
				return 0;
			}
		}

		return 1 ;
	}
	return 0 ;
}

int cgi_exec()
{
	char * script_filename ;
    fflush(fd_out);

	script_filename = getenv( "SCRIPT_FILENAME" );
	if( script_filename ) {
	    pid_t childpid=fork();
		if( childpid==0 ) {		// child process
			if( fileno( fd_in ) != 0 ) {
				dup2( fileno(fd_in), 0 );
			}
			if( fileno(fd_out) != 1 ) {
				dup2( fileno(fd_out), 1 );
			}
			execl( script_filename, script_filename, NULL );
			exit(0);
		}
		else if( childpid>0) {	// parent process
		    int   chstatus ;
		    waitpid(childpid, &chstatus, 0);      // wait cgi to finish
		}
	}
	return 0;
}

// SMALL SSI functions

FILE * ssi_fd ;			// ssi temprary output file

// token ex: <!--#exec cmd="ls" -->
int smallssi_gettoken(FILE * fp, char * token, int bufsize)
{
	const char ssi_begin[]="<!--#" ;
    int r=0;
	int c ;

	while( r<5 ) {
		c = fgetc(fp);
		if( c == EOF ) {
			return r ;
		}
		else if( c == ssi_begin[r] ) {
			token[r++] = c ;
		}
		else {
			token[r++] = c ;
			return r ;
		}
	}

	// get remain of the token
	while( r<bufsize-2 ) {
		c = fgetc(fp);
		if( c == EOF ) {
			break;
		}
		else if( c=='>' && token[r-1]=='-' && token[r-2]=='-' ) {
			token[r++] = c ;
			break;
		}
		else {
			token[r++] = c ;
		}
	}
    token[r]=0;
    return r ;
}

void smallssi_include_file( char * ifilename);

// ex: <!--#include file="includefile" -->
void smallssi_include( char * ssicmd )
{
    char * eq ;
    char * ifile ;
    eq = strchr( ssicmd, '=' );
    if( eq ) {
        ifile=strchr(eq, '\"');
        if( ifile ) {
            ifile++;
            eq=strchr(ifile, '\"');
            if( eq ) {
                *eq=0;
                smallssi_include_file( ifile ) ;
            }
        }
    }
}

// ex: <!--#echo var="envname" -->
void smallssi_echo( char * ssicmd )
{
    char * eq ;
    char * ifile ;
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
                    fputs( eq, ssi_fd );
                }
            }
        }
    }
}

// token example: <!--#exec cmd="ls" -->
void smallssi_exec( char * ssicmd )
{
    char * eq ;
    char * cmd ;
	FILE * ossi ;
	int    c ;
	char cmdline[256] ;

    eq = strchr( ssicmd, '=' );
    if( eq ) {
        cmd=strchr(eq, '\"');
        if( cmd ) {
            cmd++;
            eq=strchr(cmd, '\"');
            if( eq ) {
                *eq=0;

                // execute ssi cgi
				sprintf(cmdline, "%s > %s/ssiexec%d", cmd, temp_root, (int)getpid() );
				system(cmdline);

				sprintf(cmdline, "%s/ssiexec%d", temp_root, (int)getpid() );
				ossi = fopen( cmdline, "r" );
				if( ossi ) {
					while( (c=fgetc(ossi))!=EOF ) {
						fputc(c, ssi_fd);
					}
					fclose( ossi );
				}
				unlink( cmdline );
			}
        }
    }
}

void smallssi_include_file( char * ifilename )
{
    FILE * fp ;
    int r ;
    char token[1024] ;
    fp = fopen( ifilename, "r" ) ;
    if( fp==NULL ) {
        return ;
    }
    while( (r=smallssi_gettoken(fp, token, sizeof(token) ) )>0 ) {
		if( r>19 && strncmp(token, "<!--#include ", 13) == 0 ) {		// found ssi include command
            smallssi_include(token+13);
        }
        else if( r>16 && strncmp(token, "<!--#echo ", 10) == 0 ) {		// found ssi echo command
            smallssi_echo(token+10);
        }
        else if( r>16 && strncmp(token, "<!--#exec ", 10) == 0 ) {		// found ssi exec command
            smallssi_exec(token+10);
        }
        else {
			fwrite(token, 1, r, ssi_fd);
        }
    }

    fclose( fp );
}

// small ssi output
void smallssi_run()
{
	char buf[4096] ;
    char ssioutfile[200] ;
	int  rlen ;
	char * script_filename = getenv("SCRIPT_FILENAME");

    ssi_fd = fopen( script_filename, "r" );
    if ( ssi_fd == NULL ) {
        http_error( 403, NULL );
        return ;
    }
    fclose( ssi_fd );

    // set ssi output file
    sprintf( ssioutfile, "%s/ssiout%d", temp_root, getpid() );

	ssi_fd = fopen( ssioutfile, "w+b" );

    smallssi_include_file( script_filename );

	// content type as if .html
	setcontenttype(".html");

	// output ssi file ;
	setcontentlength( ftell(ssi_fd) );
    fseek( ssi_fd, 0, SEEK_SET );

	while( (rlen=fread(buf, 1, 4096, ssi_fd))>0 ) {
		http_output( buf, rlen );
	}
    fclose( ssi_fd ) ;
	unlink( ssioutfile );
}

// set server (cgi) variables
void sethttpheadervar( char * headerline)
{
    char envname[105] ;
    char * pn ;
    int i;
    strcpy(envname,"HTTP_");
    pn = &envname[5] ;
    for(i=0;i<100;i++){
        if(headerline[i]>='a' && headerline[i]<='z') {
            pn[i]=headerline[i]-'a'+'A' ;
        }
        else if( headerline[i]=='-' ) {
            pn[i]='_' ;
        }
        else if( headerline[i]==0 ) {
            break ;
        }
        else if( headerline[i]==':' ) {
            pn[i]=0 ;
            setenv( envname, stringtrim(&headerline[i+1]), 1 );
            break ;
        }
        else {
            pn[i] = headerline[i] ;
        }
    }
}

// clear server variables
void clrservervar()
{
    int i ;
    int l ;
    char * eq ;
    char envname[105] ;

    // un-set regular HTTP env
    unsetenv( "QUERY_STRING" );

    for(i=0; i<500; i++ ) {
        if( environ[i]==NULL || environ[i][0]==0 ) {
            break;
        }
		else if ( strncmp( environ[i], "POST_FILE_", 10 )==0 )
        {
            eq = strchr( environ[i], '=' ) ;
            if( eq ) {
                // remove post file
                unlink(eq+1);
                l = eq-environ[i] ;
                strncpy( envname, environ[i], l);
                envname[l]=0 ;
                unsetenv( envname );
				i--;
            }
        }
        else if( strncmp( environ[i], "HTTP_", 5 )==0 ||
            strncmp( environ[i], "POST_", 5 )==0 || 
            strncmp( environ[i], "HEADER_", 7 )==0 ||
            strncmp( environ[i], "COOKIE_", 7 )==0 )
        {
            eq = strchr( environ[i], '=' ) ;
            if( eq ) {
                l = eq-environ[i] ;
                strncpy( envname, environ[i], l);
                envname[l]=0 ;
                unsetenv( envname );
				i--;
            }
        }
    }

    return  ;
}

// return 1: success, 0: failed to received all content
int	http_input_multipart()
{
    FILE * postfile ;
	FILE * postprogressfile ;
    char * boundary ;
    int    lbdy ;            	// boundary length ;
    char * p ;
    char * p2 ;
    char linebuf[1024] ;
	char post_filename[256] ;
    char part_name[256] ;
    char part_filename[256] ;\
    int  i ;
	int  c ;
    int  res=0;
	int  content_length = request_content_length;
	const char sbdy[8]="\r\n--" ;

    p = getenv("HTTP_CONTENT_TYPE");
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

    // get first boundary
    if( fgets( linebuf, sizeof(linebuf), fd_in)== NULL )
        return res;
    if( linebuf[0]!='-' ||
        linebuf[1]!='-' ||
        strncmp( &linebuf[2], boundary, lbdy )!=0 )
        return res;

	request_content_length -= strlen( linebuf ) ;

    while( request_content_length > 0) {
        char part_name[128] ;
        char part_filename[256] ;
        part_name[0]=0;
        part_filename[0]=0;
        // get form data headers
        while ( fgets( linebuf, sizeof(linebuf), fd_in )  )
        {
			l = strlen( linebuf );
			request_content_length -= l ;
			
            if( l <= 0 ||
                strcmp( linebuf, "\n" ) == 0 ||
                strcmp( linebuf, "\r\n" ) == 0 ){
				// end of part headers
                break;
            }
            else if( strncmp(linebuf, "Content-Disposition: form-data;", 31)==0 ) {
                p=strstr(&linebuf[30], " name=\"");
                if( p ) {
                    p2=strchr(&p[7], '\"');
                    if( p2 ) {
                        *p2='\0';
                        strncpy( part_name, &p[7], sizeof(part_name)-1 );
                        *p2='\"' ;
                    }
                }

                p=strstr(&linebuf[30], " filename=\"");
                if( p ) {
                    p2=strchr(&p[11], '\"');
                    if( p2 ) {
                        *p2='\0';
                        strncpy( part_filename, &p[11], sizeof(part_filename)-1 );
                        *p2='\"' ;
                    }
                }
            }
        }

        if( strlen(part_name)<=0 )
            break ;

		// initiate file upload progress
		sprintf(linebuf, "%s/http_post_progress_%s", temp_root, part_name );
		postprogressfile = fopen("linebuf", "w");
		fprintf(postprogressfile,"%d", (content_length-request_content_length)*100/content_length);
		fflush(postprogressfile);

        // set POST_FILENAME_* env
        if( strlen(part_filename)>0 ) {
            sprintf( linebuf, "POST_FILENAME_%s", part_name );
            setenv( linebuf, part_filename, 1 );
        }

        sprintf(post_filename, "%s/post_file_%s_%d", temp_root, part_name, (int)getpid() );
        postfile = fopen(post_filename, "w");
        if( postfile ) {

            // get content data
            i = 0 ;
            while( request_content_length>0 ) {

                c=fgetc(fd_in);

                if( c==EOF ) {
					datetype=0;
					break;
                }

				request_content_length--;

				// update progress files
				if( request_content_length%10000 == 0 ) {
                    rewind(postprogressfile);
					fprintf(postprogressfile,"%d", (content_length-request_content_length)*100/content_length);
					fflush(postprogressfile);
				}

				if( i<4 && c==sbdy[i] ) {
	                linebuf[i++]=c ;
				}
				else if( i>=4 && i<4+lbdy && c==boundary[i-4] ) {
	                linebuf[i++]=c ;
					if( i==4+lbdy ) {			// found a boundary
						if( fgets(linebuf, sizeof(linebuf), fd_in) ) {
							l = strlen( linebuf );
							request_content_length -= l ;
							if( linebuf[0]=='\r' && linebuf[1]=='\n' ) {
								continue ;
							}
							else if( linebuf[0]=='-' && linebuf[1]=='-' &&  ) {
								request_content_length = 0 ;
								res = 1 ;
								break;
							}
						}
						break;
					}
                }
                else {
                    linebuf[i++]=c ;
                    fwrite( linebuf, 1, i, postfile);
                    i=0 ;
                }
            }

			// close post file
            fclose( postfile );

            // set POST_FILE_* env
            sprintf( linebuf, "POST_FILE_%s", part_name );
            setenv( linebuf, post_filename, 1 );
        }
    }

    rewind(ulprogfile);
    fprintf(ulprogfile,"100");
    fclose( ulprogfile );
    return res ;
}

// process input contents
// return 1: success, 0: failed to received all content
int http_input_content()
{
    int res = 0 ;
    char * request_method ;
    char * content_type ;
    char * post_string;
    int  r ;

    if( request_content_length<=0 ) { 		// no contents
        return 1 ;                  		// return success.
    }

    content_type = getenv("HTTP_CONTENT_TYPE");
    request_method = getenv("REQUEST_METHOD") ;
    if( strcmp( request_method, "POST" )==0 && 		// support "POST" contents only
        content_type &&                  			// content-type must be provided by client
        request_content_length<20000000 )     		// can't handle huge contents
    {
        if( request_content_length<100000 &&
            strncasecmp( content_type, "application/x-www-form-urlencoded", 32)==0 )
        {
            post_string = (char *)malloc( request_content_length + 1 );
            if( post_string ) {
                r = fread( post_string, 1, request_content_length, fd_in );
                if( r == request_content_length ) {
                    post_string[r]=0;
                    setenv( "POST_STRING", post_string, 1 );
					request_content_length = 0 ;		// completed input contents
                    res = 1 ;
                }
                free( post_string );
            }
        }
        else if( strncasecmp( content_type, "multipart/form-data;", 20 )==0 ) {
			return http_input_multipart() ;
        }
    }
    return res ;
}

// output http content
void http_document()
{
    struct stat sb;
	int len ;
    char * uri ;
	char * s ;
	char * script_name ;
	char * script_filename ;
    FILE * fp ;
    int ch ;
	int i;
	char buf[4096] ;

    // use linebuf to store document name.
	script_name = getenv("SCRIPT_NAME" );
	// check internal funciton
	for( i=0; internal_fn[i].cname && i<100; i++ ) {
		if( strcmp( internal_fn[i].cname, script_name )== 0 ) {	// match internal name
			internal_fn[i].func();
			return ;
		}
	}
	
	script_filename = getenv("SCRIPT_FILENAME");
    // if file available ?
    if ( stat( script_filename, &sb ) != 0 ) {
        http_error( 404, NULL );
        return ;
    }
    if ( S_ISDIR( sb.st_mode ) )    // do not support directory list
    {
        http_error( 403, NULL );
        return ;
    }

    len = strlen( script_filename );

    // check file type by extension
    if(strcmp(&script_filename[len-6], ".shtml")==0 ||   // to run small ssi
       strcmp(&script_filename[len-4], ".ssi")==0 )
    {
        smallssi_run();
    }
    else if( strcmp(&script_filename[len-4], ".cgi")==0 ||
			 strncmp(script_name, "/cgi/", 5 )==0 ) {
        cgi_exec();
    }
    else {
        // dump out script file
        fp = fopen( script_filename, "rb" );
        if ( fp == NULL ) {
            http_error( 403, NULL );
            return ;
        }

        fseek( fp, 0, SEEK_END );
        len = (size_t) ftell( fp );
        fseek( fp, 0, SEEK_SET );

        if( len<=0 ) {
            http_error( 404, NULL );
            fclose( fp );
            return ;
        }

		// cacheable ?
	    if( strcmp(getenv( "REQUEST_METHOD" ), "GET")==0 ) {
			http_cacheable = 1 ;
			if( (s=getenv( "HTTP_CACHE_CONTROL")) ) {
				if( strcasestr(s, "no-cache") || strcasestr(s, "no-store") ) {
					http_cacheable = 0 ;
				}
			}
	    }
		if( http_cacheable ) {
			strftime( buf, sizeof(buf), RFC1123FMT, gmtime( &sb.st_mtime ) );
		    http_header( "Last-Modified", buf );
			unsigned int etag = (unsigned int)sb.st_ino ;
			etag+=(unsigned int)sb.st_size ;
			etag+=(unsigned int)sb.st_mtime ;
			sprintf(buf, "\"%x\"", etag );
			http_header( "Etag", buf );
			// check cacheable ?
		    if( http_checkcache() ) {       	// check if cache fresh?
		        http_response( 304, NULL );     // to let browser use cache
		        return ;
		    }
		}

		setcontentlength( len );
		setcontenttype( script_name );

        // output whole file
        while ( ( len = fread( buf, 1, sizeof(buf),  fp ) )>0 ) {
			http_output( buf, len );
        }

        fclose( fp );
    }

}

// process_input (header part) of http protocol
// return 1: success, 0: failed
int http_input()
{
	char * p ;
    char * method ;	// request method
    char * uri ;	// request URI
    char * query ;	// request query string
    char * protocol ;	// request protocol
    char linebuf[2048] ;

	// request line
    if ( fgets( linebuf, sizeof(linebuf), fd_in ) == NULL )
    {
        return 0;
    }

    method = stringtrim(linebuf);
    uri = strchr(method, ' ');
    if( uri==NULL ) {
        return 0;
    }
    *uri=0;
    uri=stringtrim(uri+1);

    // set request method, support POST and GET only
    setenv( "REQUEST_METHOD", stringupper( method ), 1 );

    // check uri
    if( uri[0] != '/' ||
        uri[1] == '/' ||
        strstr( uri, "/../" ) )
    {
        return 0 ;
    }

    protocol=strchr(uri, ' ');
    if( protocol==NULL ) {
        return 0 ;
    }

    *protocol=0;
    protocol=stringtrim(protocol+1);
    // check protocol
    if( strncasecmp(protocol, "HTTP/1.", 7)!=0 ) {
        return 0 ;
    }
    setenv("SERVER_PROTOCOL", protocol, 1 );

    // get query string
	uri = stringtrim(uri);
	setenv("REQUEST_URI", uri, 1);

    query=strchr(uri, '?');
    if( query ){
        *query=0;
        query++;
        setenv( "QUERY_STRING", query, 1);
    }

	// SCRIPT NAME
	setenv( "SCRIPT_NAME", uri, 1 );

	// script file name
	sprintf( linebuf, "%s%s", document_root, getenv("SCRIPT_NAME") );
	setenv( "SCRIPT_FILENAME", linebuf, 1 );

    // get request header
    while ( fgets( linebuf, sizeof(linebuf), fd_in )  )
    {
        if( linebuf[0] >= ' ' )
        {
            // set header as env HTTP_???
            sethttpheadervar(linebuf);
        }
        else {
            break;
        }
    }

	// process some known header
	request_content_length = -1 ;
	p = getenv( "HTTP_CONTENT_LENGTH" ) ;
	if( p ) {
		sscanf( p, "%d", &request_content_length );
	}

    return 1 ;
}

void http()
{
	char host[64] ;
	char port[32] ;
    if( recvok(fileno(fd_in), http_keep_alive*1000000)<=0 ) {
        http_keep_alive=0 ;
        return ;
    }
    http_keep_alive=0 ;		// default no keepalive
	http_cacheable=0 ;		// default no cacheable
	http_resp_done=0 ;
 	http_status=200 ;		// assume OK
	request_content_length=-1 ;		// not request content length.

	struct sockaddr_in sname ;
	socklen_t slen ;
	slen = sizeof(sname);
	if( getsockname(fileno(fd_in), (struct sockaddr *)&sname, &slen)==0 ) {
		if( getnameinfo( (struct sockaddr *)&sname, slen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV )==0 ) {
			setenv( "SERVER_ADDR", host, 1 );
			setenv( "SERVER_NAME", host, 1 );
			setenv( "SERVER_PORT", port, 1 );
		}
	}
	slen = sizeof(sname);
	if( getpeername(fileno(fd_in), (struct sockaddr *)&sname, &slen)==0 ) {
		if( getnameinfo( (struct sockaddr *)&sname, slen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV )==0 ) {
			setenv( "REMOTE_ADDR", host, 1 );
			setenv( "REMOTE_PORT", port, 1 );
		}
	}

    if( http_input()==0 ) {
        http_error(400, NULL);
        goto http_done ;
    }

    // save post data
    if( http_input_content()==0 ) {
        http_error(400, NULL );
        goto http_done ;
    }
    // all inputs processed!

    // access check
//    check_access() ;

    // output http document
    http_document() ;

http_done:

    // remove all HTTP_* environment and POST files
    clrservervar() ;

	fflush( fd_out );

    return ;
}

void sigpipe(int sig)
{
    clrservervar() ;
    _exit(0);
}


// listen on port 80 for http connections, make this program a stand alone http server
void http_listen()
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
    int asockfd ;
    int val;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM ;

    res = NULL;
    if (getaddrinfo(NULL, "80", &hints, &res) != 0) {
        printf("Error:getaddrinfo!");
        exit(1);
    }
    if (res == NULL) {
        printf("Error:getaddrinfo!");
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
        printf("Error:listen!");
        exit(1);
    }
    listen(sockfd, 10);

    while( (asockfd=accept( sockfd, (struct sockaddr *)&saddr, &saddrlen  ))>0 ) {
        if( fork()==0 ) {
            // child process
			fd_in = fdopen( asockfd, "r+b" );
			fd_out = fd_in ;
            close( sockfd );	// close listening socket
            break;
        }
        close( asockfd ) ;
        while( waitpid(0, NULL, WNOHANG)>0 );
    }


    return ;
}

int main(int argc, char * argv[])
{
	// max keep alive request in one connection
    int maxkeepalive=50 ;

	fd_in = stdin ;
	fd_out = stdout ;

    if( argc>1 ) {
		strcpy( document_root, argv[1] );
    }
    if( argc>2 ) {
        if( argv[2][0]=='-' && argv[2][1]=='l' ) {
            http_listen();
        }
    }

    // set document root dir
    setenv( "DOCUMENT_ROOT", document_root, 1);
	setenv( "SERVER_PROTOCOL", PROTOCOL, 1 );
	setenv( "SERVER_SOFTWARE", SERVER_NAME, 1 );
    chdir(document_root);
	
	// temperary directory
	strcpy( temp_root, document_root );

    // do some cleaning on SIGPIPE
    signal(SIGPIPE,  sigpipe );

    http_keep_alive=MAX_KEEP_ALIVE ;
    while( http_keep_alive>0 && maxkeepalive-->0 ) {
	    http();
    }

    return 0 ;
}

