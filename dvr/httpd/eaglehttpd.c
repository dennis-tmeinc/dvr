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

#include "../cfg.h"

#define SERVER_NAME "Eagle HTTP 0.22"
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

#define DENIED_PAGE ("/denied.html")

// we need to excess environment
extern char **environ ;

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

char * document_root="/home/www" ;

#define CACHE_MAXAGE        (600)
#define KEEP_ALIVE          (30)

int    http_keep_alive ;
long   http_etag ;           // Etag of request document
time_t http_modtime ;        // last modified time
int    http_cachemaxage ;    // cache max age (in seconds)

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

void http_setheader(char * header, char *header_txt)
{
    char hdenv[160] ;
    sprintf( hdenv, "HEADER_%s", header );
    setenv( hdenv, header_txt, 1 );
}

void setcookie(char * key, char * value)
{
    char cookie[200] ;
    sprintf( cookie, "%s=%s", key, value );
    http_setheader("Set-Cookie", cookie );
}

// output http header
void http_header( int status, char * title, char * mime_type, int length)
{
    time_t now;
    char hbuf[120];
    int i ;
    char * resp ;
    char * fn ;

    if( title==NULL ) {
        // use default title
        title=http_msg(status);
    }
    
    printf( "%s %d %s\r\n", PROTOCOL, status, title );
    unsetenv("HEADER_Server");                  //  Not allowed for CGI to change server
    unsetenv("HEADER_Date");                    //  Not allowed for CGI to change date
    printf( "Server: %s\r\n", SERVER_NAME );
    now = time( (time_t*) NULL );
    strftime( hbuf, sizeof(hbuf), RFC1123FMT, gmtime( &now ) );
    printf( "Date: %s\r\n", hbuf );

    if( mime_type!=NULL ) {
        setenv("HEADER_Content-Type", mime_type, 1 );
    }
    if( length>=0 ) {
        sprintf(hbuf, "%d", length);
        setenv("HEADER_Content-Length",hbuf,0);         // don't overwrite if env exist
    }

    // check extra header (may be set by online cgi)
    fn = getenv("POST_FILE_EXHEADER");
    if( fn ) {
        FILE * fexhdr = fopen( fn, "r" );
        if( fexhdr ) {
            while( fgets( hbuf, sizeof(hbuf), fexhdr ) )
            {
                if( (fn=strchr(hbuf, ':' )) ) {
                    *fn=0 ;
                    http_setheader(cleanstring(hbuf), cleanstring(fn+1)) ;
                }
            }
            fclose( fexhdr );
        }
    }

    setenv("HEADER_Cache-Control", "no-cache", 0);  // don't overwrite it
    
    for(i=0; i<200; ) {
        if( environ[i]==NULL || environ[i][0]==0 ) {
            break;
        }
        if( strncmp( environ[i], "HEADER_", 7 )==0 )
        {
            resp = strchr( environ[i], '=' ) ;
            if( resp ) {
                int l = resp-environ[i] ;
                strncpy( hbuf, environ[i], l);
                hbuf[l]=0 ;
                printf( "%s: %s\r\n", &(hbuf[7]), resp+1 );
                unsetenv( hbuf );
                continue ;
            }
        }
        i++ ;
    }

    // empty line, finishing the http header
    printf("\r\n");
}

void http_error( int status, char * title )
{
    http_keep_alive=0  ;
    http_cachemaxage=0 ;
    if( title==NULL ) {
        title = http_msg( status );
    }
    http_header( status, title, "text/html", -1 );
    printf("<html><head><title>%d %s</title></head>\n<body bgcolor=\"#66ffff\"><h4>%d %s</h4></body></html>\r\n",
           status, title, status, title );
}

// update page modified time and etag
void cache_upd(char * pagefile)
{
    struct stat filest ;
    if( pagefile != NULL && stat( pagefile, &filest ) ==0 ) {
        if( http_modtime<filest.st_mtime ) {
            http_modtime=filest.st_mtime ;
        }
        http_etag+=(long)filest.st_ino ;
        http_etag+=(long)filest.st_mode ;
        http_etag+=(long)filest.st_size ;
        http_etag+=(long)filest.st_ctime ;
    }
    else {
        http_modtime=time(NULL);
        http_cachemaxage=0;
    }
}

int http_nocache()
{
    http_setheader( "Cache-Control", "no-cache" );
    return 0 ;
}    

// return true if document is fresh
int http_checkcache()
{
    char tbuf[120];
    char etagstr[16] ;
    char * s ;

    if( (s=getenv( "HTTP_CACHE_CONTROL")) ) {
        if( strcmp(s, "no-cache")==0 ) {
            return http_nocache();
        }
    }

    if( (s=getenv( "REQUEST_METHOD" )) ) {
        if( strcmp(s, "POST")==0 ) {
            return http_nocache();
        }
    }

    if( http_modtime==(time_t)0 || http_etag==0 ) {
        return http_nocache();
    }
    
    sprintf( tbuf, "max-age=%d",  http_cachemaxage );
    http_setheader( "Cache-Control", tbuf );

    // Modified time
    strftime( tbuf, sizeof(tbuf), RFC1123FMT, gmtime( &http_modtime ) );
    http_setheader( "Last-Modified", tbuf );

    //  etag
    sprintf(etagstr, "\"%x\"", (unsigned int)http_etag);
    http_setheader( "Etag", etagstr);

    // check etag
    if( (s=getenv("HTTP_IF_NONE_MATCH"))) {
        if( strcmp( s, etagstr )==0 ) {        // Etag match?
            if( (s=getenv("HTTP_IF_MODIFIED_SINCE"))) {
                if( strcmp( s, tbuf )==0 ) {   // Modified time match?
                    return 1 ;                 // cache is fresh
                }
            }
        }
    }
    return 0 ;
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

int cgi_exec()
{
    char  cgibuf[512] ;
    fflush(stdout);
    http_keep_alive=0  ;        // quit process if exec failed

    // cgi file name
    sprintf( cgibuf, "%s%s", document_root, getenv("REQUEST_URI") );
    execl( cgibuf, cgibuf, NULL );
    return 0;
}

int cgi_run()
{
    pid_t childpid ;
    char  cgibuf[200] ;

    // make output file name for cgi
    sprintf( cgibuf, "%s/cgiout%d", document_root, getpid() );
    setenv("POST_FILE_TMPCGIOUT", cgibuf, 1 );
    
    fflush(stdout);
    childpid=fork();
    if( childpid==0 ) {
        // open cgi output file and set it as stdout for cgi
        int hcgifile = open(cgibuf, O_RDWR|O_CREAT, S_IRWXU );
        if( hcgifile ) {
            dup2( hcgifile, 1 ) ;
            dup2( hcgifile, 2 ) ;
            close( hcgifile );
        }
        // cgi file name
        sprintf( cgibuf, "%s%s", document_root, getenv("REQUEST_URI") );
        execl( cgibuf, cgibuf, NULL );
        exit(0) ;
    }
    else if( childpid>0) {
        int   chstatus ;
        int   len ;

        waitpid(childpid, &chstatus, 0);      // wait cgi to finish
        
        FILE * fp = fopen( cgibuf, "r" );
        if( fp==NULL ) {
            http_error( 403, NULL );
            return 0;
        }
        fseek( fp, 0, SEEK_END ) ;
        len = ftell(fp);
        if( len<=0 ) {
            http_error( 403, NULL );
            return 0;
        }
        fseek( fp, 0, SEEK_SET );

        http_nocache();             // default no cache for cgi result
        
        // parse cgi output header
        char * p ;
        while ( fgets( cgibuf, sizeof(cgibuf), fp )  )
        {
            if( cgibuf[0] < ' ' ) {
                break;
            }
            else if( strncmp(cgibuf, "HTTP/", 5)==0 ) {
                continue ;          // ignor HTTP responds
            }
            else if( (p=strchr(cgibuf, ':' )) ) {
                *p=0 ;
                http_setheader(cleanstring(cgibuf), cleanstring(p+1)) ;
            }
        }

        http_header( 200, NULL, NULL, len-ftell(fp) );

        // output cgi
        int ch ;
        while( (ch = fgetc( fp ))!=EOF ) {
            fputc( ch, stdout ) ;
        }
        fclose( fp ) ;
    }
    return 0 ;
}

int smallssi_gettoken(FILE * fp, char * token, int bufsize)
{
    int r=0;
    int c = fgetc(fp);
    if( c==EOF ) return 0;
    http_etag+=(long)c ;
    token[r] = c ;
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
                        while( r<bufsize-10 ) {
                            r++;
                            token[r]=fgetc(fp) ;
                            if( token[r]=='>' && token[r-1]=='-' && token[r-2]=='-' ) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    token[r+1]=0;
    return r+1 ;
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
    cache_upd(NULL);
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

// ex: <!--#exec cmd="ls" -->
void smallssi_exec( char * ssicmd )
{
    char * eq ;
    char * ifile ;
    cache_upd(NULL);
    eq = strchr( ssicmd, '=' );
    if( eq ) {
        ifile=strchr(eq, '\"');
        if( ifile ) {
            ifile++;
            eq=strchr(ifile, '\"');
            if( eq ) {
                *eq=0;
                fflush(stdout);
                system(ifile);		// execute ssi cgi
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
    cache_upd(ifilename);
    while( (r=smallssi_gettoken(fp, token, sizeof(token) ) )>0 ) {
        if( r<8 ) {
            fputs(token, stdout);
        }
        else if( strncasecmp(token, "<!--#include ", 13) == 0 ) {		// found ssi include command
            smallssi_include(token);
        }
        else if( strncasecmp(token, "<!--#echo ", 10) == 0 ) {		// found ssi echo command
            smallssi_echo(token);
        }
        else if( strncasecmp(token, "<!--#exec ", 10) == 0 ) {		// found ssi exec command
            smallssi_exec(token);
        }
        else {
            fputs(token, stdout);		// pass to client
        }
    }
    
    fclose( fp );
}

char serfile[]=WWWSERIALFILE ;

int checkserialno( char * serialno )
{
    time_t t ;
    unsigned int ui ;
    FILE * sfile ;
    char buf[1024];
    sfile = fopen(serfile, "r");
    if( sfile ) {
        fscanf(sfile, "%u %s",&ui, buf);
        time(&t);
        fclose( sfile ) ;
        if( strcmp(serialno, buf)==0 && ((unsigned int)t)-ui<600 ) {
            return 1 ;
        }
    }
    return 0;
}

// update valid serialno time 
int updateserialno(char * serialno)
{
    time_t t ;
    FILE * sfile ;
    sfile = fopen(serfile, "w");
    if( sfile ) {
        time(&t);
        fprintf(sfile, "%u %s", (unsigned int)t, serialno);
        fclose( sfile ) ;
    }
    return 0;
}

int cleanserialno()
{
    remove( serfile );
    return 0 ;
}

// return 1: success, 0: failed to received all content
int savepostfile()
{
    char postfilename[1024] ;
    FILE * postfile ;
    char * boundary ;
    char * p ;
    char * p2 ;
    char linebuf[4096] ;
    int  i ;
    int  content_length=0 ;
    int  d ;
    int lbdy=0 ;            // boundary length ;
    int datatype = 1 ;      // 0: dataend, 1: data, 2: databoundary
    int res=0;

    p = getenv("HTTP_CONTENT_LENGTH" );
    if( p ) {
        sscanf(p,"%d", &content_length);
    }

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
    if( fgets( linebuf, sizeof(linebuf), stdin)== NULL )
        return res;
    if( linebuf[0]!='-' ||
        linebuf[1]!='-' ||
        strncmp( &linebuf[2], boundary, lbdy )!=0 )
        return res;

    
    FILE * ulprogfile = fopen("uploading", "w");
    int  ulbytes = 0 ;

    fprintf(ulprogfile,"0" );
    fflush(ulprogfile);
    
    while(1) {
        char part_name[128] ;
        char part_filename[256] ;
        part_name[0]=0;
        part_filename[0]=0;
        // get form data headers
        while ( fgets( linebuf, sizeof(linebuf), stdin )  )
        {
            if( linebuf[0] == 0 ||
                strcmp( linebuf, "\n" ) == 0 ||
                strcmp( linebuf, "\r\n" ) == 0 ){
                break;
            }
            else if( strncmp(linebuf, "Content-Disposition: form-data", 30)==0 ) {
                p=strstr(&linebuf[30], " name=");
                if( p && p[6]=='\"') {
                    p2=strchr(&p[7], '\"');
                    if( p2 ) {
                        *p2='\0';
                        strncpy( part_name, &p[7], sizeof(part_name)-1 );
                        *p2='\"' ;
                    }
                }

                p=strstr(&linebuf[30], " filename=");
                if( p && p[10]=='\"') {
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
        
        sprintf(postfilename, "%s/post_file_%d_%s", document_root, (int)getpid(), part_name );
        postfile = fopen(postfilename, "w");
        if( postfile ) {
            // set POST_FILE_* env
            sprintf( linebuf, "POST_FILE_%s", part_name );
            setenv( linebuf, postfilename, 1 );

            // set POST_FILENAME_* env
            if( strlen(part_filename)>0 ) {
                sprintf( linebuf, "POST_FILENAME_%s", part_name );
                setenv( linebuf, part_filename, 1 );
            }

            // get content data
            i = 0 ;
            datatype=1 ;
            while( datatype==1 ) {
                d=fgetc(stdin);
                ulbytes++ ;
                if( (ulbytes%(100+content_length/150))==0 ) {
                    rewind(ulprogfile);
                    fprintf(ulprogfile,"%d\r\n", ulbytes*100/content_length );
                    fflush(ulprogfile);
                }
                
                if( d==EOF ) {
                    if( i>0 ) {
                        fwrite( linebuf, 1, i, postfile);
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
                else if( i==1 && d=='\n' ) {
                    linebuf[i++]=d ;
                }
                else if( (i==2 || i==3) && d=='-' ) {
                    linebuf[i++]=d ;
                }
                else if( i>=4 && d==boundary[i-4] ) {
                    if(boundary[i-3]=='\0') { // end of boundary
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

        if( datatype==0 )
            break;
        else {
            fgets(linebuf,sizeof(linebuf), stdin);
            if( linebuf[0]=='\r' && linebuf[1]=='\n' ) {
                continue ;
            }
            else if( linebuf[0]=='-' && linebuf[1]=='-' ) {
                res=1;
                break;
            }
            else {
                break ;
            }
        }
    }
    rewind(ulprogfile);
    fprintf(ulprogfile,"100");
    fclose( ulprogfile );
    return res ;
}

int check_access()
{
    char * p ;
    char * uri ;
    int res=1 ;
    char access[128] ;
    FILE * accessfile = fopen( "access", "r" );
    if( accessfile ) {
        uri = getenv("REQUEST_URI");
        while( fgets( access, 128, accessfile ) ) {
            if( strcmp( cleanstring(access), uri )==0 ) {
                // access check required
                p = getquery("ser") ;
                if( p ) {
                    setcookie("ser",p);
                }
                else {
                    p = getcookie("ser");
                }

                if( p && checkserialno (p)) {
                    updateserialno(p);
                }
                else {
                    setenv("REQUEST_URI", DENIED_PAGE, 1);          // replace denied page
                    setenv("HTTP_CACHE_CONTROL", "no-cache", 1 );   // force no cache on denied page
                    res=0 ;
                }
                break;
            }
        }
        fclose( accessfile );
    }
    return res ;
}

void smallssi_run()
{
    FILE * fp ;
    char ssioutfile[200] ;

    char * ssiname = getenv("REQUEST_URI");
    ssiname++ ;
    
    fp = fopen( ssiname, "r" );
    if ( fp == NULL ) {
        http_error( 403, NULL );
        return ;
    }
    fclose( fp );

    // setup extra header files. (for inline exec support)
    sprintf( ssioutfile, "%s/exhdr%d", document_root, getpid() );
    setenv("POST_FILE_EXHEADER", ssioutfile, 1 );

    fflush(stdout);

    // save old stdout
    int oldstdout = dup(1);
    int oldstderr = dup(2);

    // set ssi output file
    sprintf( ssioutfile, "%s/ssiout%d", document_root, getpid() );
    setenv("POST_FILE_TMPSSI", ssioutfile, 1 );
    
    int hssifile = open(ssioutfile, O_RDWR|O_CREAT, S_IRWXU );
    dup2( hssifile, 1 ) ;
    dup2( hssifile, 2 ) ;
    close( hssifile );

    smallssi_include_file( ssiname );
    
    // restore stdout
    fflush( stdout );
    dup2( oldstdout, 1 );
    close( oldstdout );
    dup2( oldstderr, 2 );
    close( oldstderr );

    // add IE document mode support
    http_setheader("X-UA-Compatible", "IE=EmulateIE8" );
    
    if( http_checkcache() ) {       // check if cache fresh?
        // use cahce
        http_header( 304, NULL, NULL, 0 );        // let browser to use cache
    }
    else {
        fp = fopen( ssioutfile, "r" );
        fseek( fp, 0, SEEK_END );
        http_header( 200, NULL, "text/html; charset=UTF-8", ftell(fp) );
        fseek( fp, 0, SEEK_SET );
        int ch ;
        while( (ch=fgetc(fp))!=EOF ) {
            fputc( ch, stdout );
        }
        fclose( fp ) ;
    }
}

// set http headers as environment variable. 
void sethttpenv(char * headerline)
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
            setenv( envname, cleanstring(&headerline[i+1]), 1 );
            break ;
        }
        else {
            pn[i] = headerline[i] ;
        }
    }
}

// un-set http headers as environment variable. 
void unsethttpenv()
{
    int i ;
    int l ;
    char * eq ;
    char envname[105] ;

    // un-set regular HTTP env
    unsetenv( "QUERY_STRING" );
    
    for(i=0; i<200; ) {
        if( environ[i]==NULL || environ[i][0]==0 ) {
            break;
        }
        if ( strncmp( environ[i], "POST_FILE_", 10 )==0 )
        {
            eq = strchr( environ[i], '=' ) ;
            if( eq ) {
                // remove post file
                unlink(eq+1);
            }
        }
        if( strncmp( environ[i], "HTTP_", 5 )==0 ||
            strncmp( environ[i], "POST_", 5 )==0 )
        {
            eq = strchr( environ[i], '=' ) ;
            if( eq ) {
                l = eq-environ[i] ;
                strncpy( envname, environ[i], l);
                envname[l]=0 ;
                unsetenv( envname );
                continue ;
            }
        }
        i++ ;
    }

    return  ;
}

// return 1: success, 0: failed to received all content
int savepost()
{
    int res = 0 ;
    char * request_method ;
    int    content_length=0 ;
    char * content_type ;
    char * post_string;
    char * p ;
    int  r ;
    
    // check input
    p = getenv("HTTP_CONTENT_LENGTH") ;
    if( p ) {
        sscanf( p, "%d", &content_length );
    }
    if( content_length<=0 ) {       // no contents
        return 1 ;                  // return success.
    }
    
    content_type = getenv("HTTP_CONTENT_TYPE");
    request_method = getenv("REQUEST_METHOD") ;
    if( strcmp( request_method, "POST" )==0 &&      // support "POST" contents only
        content_type &&                              // content-type must be provided by client
        content_length<20000000 )                    // can't handle huge contents
    {
        if( content_length<100000 &&
            strncasecmp( content_type, "application/x-www-form-urlencoded", 32)==0 )
        {
            post_string = (char *)malloc( content_length + 1 );
            if( post_string ) {
                r = fread( post_string, 1, content_length, stdin );
                if( r == content_length ) {
                    post_string[r]=0;
                    setenv( "POST_STRING", post_string, 1 );
                    res = 1 ;
                }
                free( post_string );
            }
        }
        else if( strncasecmp( content_type, "multipart/form-data", 19 )==0 ) {
            res = savepostfile();
        }
    }
    return res ;
}

void http_document()
{
    struct stat sb;
    char * uri ;
    char * fname ;
    int len ;
    FILE * fp ;
    int ch ;

    // use linebuf to store document name.
    uri = getenv("REQUEST_URI");

    if( uri==NULL ) {
        http_error( 404, NULL );
        return ;
    }
    fname=uri+1 ;
    // if file available ?
    if ( stat( fname, &sb ) < 0 ) {
        http_error( 404, NULL );
        return ;
    }
    if ( S_ISDIR( sb.st_mode ) )    // do not support directory list
    {
        http_error( 403, NULL );
        return ;
    }

    uri = getenv("HTTP_KEEP_ALIVE");
    if( uri ) {
        sscanf(uri, "%d", &http_keep_alive );
        if(http_keep_alive>KEEP_ALIVE) {
            http_keep_alive=KEEP_ALIVE  ;
        }
    }
    else {
        http_keep_alive=KEEP_ALIVE  ;
    }

    http_cachemaxage=CACHE_MAXAGE ;

    len = strlen( fname );

    // check file type by extension
    if(strcasecmp(&fname[len-5], ".html")==0 ||   // to run small ssi
       strcasecmp(&fname[len-6], ".shtml")==0 )
    {
        smallssi_run();
    }
    else if( strncmp(fname, "cgi/mp4", 7 )==0 )
    {
        // execute cgi for mp4 streaming.
        cgi_exec();
    }
    else if( strncmp(fname, "cgi/", 4 )==0 ||
             strcasecmp(&fname[len-4], ".cgi")==0 )
    {
        // execute cgi.
        cgi_run();
    }
    else {
        // if we are here, no cgi is executed, dump out requested file.
        cache_upd( fname );
        if( http_checkcache() ) {       // check if cache fresh?
            http_header( 304, NULL, NULL, 0 );        // to let browser use cache
            return ;
        }

        fp = fopen( fname, "r" );

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

        http_header( 200, NULL, get_mime_type( fname ), len );

        // output whole file
        while ( ( ch = getc( fp ) ) != EOF ) {
            putchar( ch );
        }

        fclose( fp );
    }
    
}

// process_input (header part) of http protocol 
// return 1: success, 0: failed
int http_input()
{
    char * method ;	// request method
    char * uri ;	// request URI
    char * query ;	// request query string
    char * protocol ;	// request protocol
    char linebuf[1024] ;

    if ( fgets( linebuf, sizeof(linebuf), stdin ) == NULL )
    {
        return 0;
    }
    
    method = cleanstring(linebuf);
    uri = strchr(method, ' ');
    if( uri==NULL ) {
        return 0;
    }
    *uri=0;
    uri=cleanstring(uri+1);

    // set request method, support POST and GET only
    if( strcasecmp(method, "POST")==0 ) {
        setenv( "REQUEST_METHOD", "POST", 1 );
    }
    else if( strcasecmp(method, "GET")==0 ) {
        setenv( "REQUEST_METHOD", "GET", 1 );
    }
    else {
        return 0 ;
    }
    
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
    protocol=cleanstring(protocol+1);
    // check protocol
    if( strncasecmp(protocol, "HTTP/1.", 7)!=0 ) {
        return 0 ;
    }
    setenv("SERVER_PROTOCOL", protocol, 1 );

    // get query string
    query=strchr(uri, '?');
    if( query ){
        *query=0;
        query++;
        setenv( "QUERY_STRING", query, 1);
    }

    if( uri[1]=='\0' ) {        // default page
        setenv( "REQUEST_URI", "/system.html", 1 );
    }
    else {
        setenv( "REQUEST_URI", uri, 1 );
    }

    // analyz request header
    while ( fgets( linebuf, sizeof(linebuf), stdin )  )
    {
        if( linebuf[0] >= ' ' )
        {
            // set header as env HTTP_???
            sethttpenv(linebuf);
        }
        else {
            break;
        }
    }
    return 1 ;
}

void http()
{
    if( recvok(0, http_keep_alive*1000000)<=0 ) {
        http_keep_alive=0 ;
        return ;
    }
    http_keep_alive=0 ;
    http_etag=0 ;
    http_modtime=0 ;
    http_cachemaxage=0 ;

    if( http_input()==0 ) {
        http_error(400, NULL);
        goto http_done ;
    }

    // save post data
    if( savepost()==0 ) {
        http_error(400, NULL );
        goto http_done ;
    }
    // all inputs processed!

    // access check
    check_access() ;

    // output http document
    http_document() ;

http_done:
    
    fflush(stdout);

    // remove all HTTP_* environment and POST files
    unsethttpenv() ;

    return ;
}

void sigpipe(int sig)
{
    unsethttpenv() ;
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
            dup2( asockfd, 0 );	// dup to 0, 1
            dup2( asockfd, 1 );
            close( sockfd );	// close listening socket
            close( asockfd );
            break;
        }
        close( asockfd ) ;
        while( waitpid(0, NULL, WNOHANG)>0 );
    }


    return ;
}



int main(int argc, char * argv[])
{
    int maxkeepalive=50 ;

    if( argc>1 ) {
        document_root=argv[1] ;
    }
    if( argc>2 ) {
        if( argv[2][0]=='-' && argv[2][1]=='l' ) {
            http_listen();
        }
    }

    // set document root dir
    setenv( "DOCUMENT_ROOT", document_root, 1);
    chdir(document_root);

    // do some cleaning on SIGPIPE
    signal(SIGPIPE,  sigpipe );

    http_keep_alive=KEEP_ALIVE ;
    while( http_keep_alive && maxkeepalive-->0 ) {
        http();
    }
    return 0 ;
}

