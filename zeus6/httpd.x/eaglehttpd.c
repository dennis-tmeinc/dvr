
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define SERVER_NAME "Eagle HTTP 0.1"
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

static char * mime_type[][2] = 
{
    {"htm", "text/html; charset=UTF-8"},
    {"html", "text/html; charset=UTF-8"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"png", "image/png"},
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
    {"", "stream/octem"}
} ;

char * document_root="/home/www" ;

char linebuf[8192] ;

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

void setcookie(char * key, char * value)
{
    printf("Set-Cookie: %s=%s\r\n", key, value );
}

void http_header( int status, char * title, char * mime_type, int length )
{
    time_t now;
    char timebuf[120];
    
    printf( "%s %d %s\r\n", PROTOCOL, status, title );
    printf( "Server: %s\r\n", SERVER_NAME );
    now = time( (time_t*) NULL );
    strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    printf( "Date: %s\r\n", timebuf );
    printf( "Connection: close\r\n" );
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
    printf("\r\n<html><head><title>%d %s</title></head>\n<body bgcolor=\"#66ffff\"><h4>%d %s</h4></body></html>\r\n",
           status, title, status, title );
}

void http_cache( int age, time_t mtime )
{
    char timebuf[120];
    time_t now ;
    if( age<=0 ) {
        now = time( (time_t*) NULL );
        strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
        printf( "Last-Modified: %s\r\n", timebuf );
        printf( "Cache-Control: no-cache\r\n" );        // no cache on ssi html file
    }
    else {
        strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &mtime ) );
        printf( "Last-Modified: %s\r\n", timebuf );
        printf( "Cache-Control: max-age=%d \r\n", age );
    }
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

void runapp( char * execfile, char * arg )
{
    pid_t childpid ;
    fflush(stdout);
    childpid=fork();
    if( childpid==0 ) {
        execlp(execfile, execfile, arg, NULL);
        exit(0);
    }
    else if( childpid>0 ) {
        waitpid( childpid, NULL, 0 );
    }
}

void cgi_run(char * execfile )
{
    pid_t childpid ;
    fflush(stdout);
    childpid=fork();
    if( childpid==0 ) {
        char execdir[512] ;
        char * d ;
        strncpy( execdir, execfile, 511);
        execdir[511]=0;
        d=strrchr( execdir, '/' );
        if( d ) {
            *d='\0' ;
            chdir( execdir );
        }
        execl( execfile, execfile, NULL );
        http_error( 403, "Forbidden" );
        exit(1) ;
    }
    else if( childpid>0) {
        waitpid(childpid, NULL, 0);
    }
    else {
        http_error( 403, "Forbidden" );
        exit(1) ;
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
    int c = fgetc(fp);
    if( c==EOF ) return 0;
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
                        while( r<bufsize-2 ) {
                            r++;
                            token[r]=fgetc(fp) ;
                            if( token[r]=='>' && token[r-1]=='-' && token[r-2]=='-' ) {
                                break;
                            }
                        }
                    }
                }}}}
    token[r+1]=0;
    return r+1 ;
}

void smallssi_include_file( char * ifilename);

// ex: <!--#include file="includefile" -->
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

// ex: <!--#exec cmd="ls" -->
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
    fp = fopen( ifilename, "r" ) ;
    if( fp==NULL ) {
        printf("Page file not available!" );
        return ;
    }
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
}


void transfer( char * newpage )
{
    printf("Location: /%s\r\n", newpage);
    printf("\r\n<html><body>Transfer to %s</body></html>\n", newpage);
}

char serfile[]="/tmp/wwwserialnofile" ;
char * makeserialno( char * buf, int bufsize ) 
{
    time_t t ;
    FILE * sfile ;
    int i ;
    unsigned int c ;
    // generate random serialno
    sfile=fopen("/dev/urandom", "r");
    for(i=0;i<(bufsize-1);i++) {
        c=(((unsigned int)fgetc(sfile))*256+(unsigned int)fgetc(sfile))%62 ;
        if( c<10 )
            buf[i]= '0'+ c;
        else if( c<36 ) 
            buf[i]= 'a' + (c-10) ;
        else if( c<62 )
            buf[i]= 'A' + (c-36) ;
        else 
            buf[i]='A' ;
    }
    buf[i]=0;
    fclose(sfile);
    
    sfile=fopen(serfile, "w");
    time(&t);
    if(sfile) {
        fprintf(sfile, "%u %s", (unsigned int)t, buf);
        fclose(sfile);
    }
    return buf ;
}

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
}

extern char *crypt (__const char *__key, __const char *__salt) __THROW;

int checkloginpassword()
{
    int res=0;
    char * p ;
    char username[100], password[100] ;
    char passwdline[200] ;    
    char salt[20] ;
    char * key ;
    int l;
    FILE * fpasswd;
    username[0]=0;
    password[0]=0;
    p = getquery( "login_username" );
    if( p )strcpy( username, p );
    p = getquery( "login_password" );
    if( p )strcpy( password, p );
    
    fpasswd=fopen("/etc/passwd", "r");
    if( fpasswd ) {
        while( fgets(passwdline, sizeof(passwdline), fpasswd) ) {
            l=strlen(username);
            if( strncmp(username, passwdline, l)==0 && 
               passwdline[l]==':' ) {                    // username matched
                   strncpy(salt, passwdline+l+1, 13);
                   salt[12]=0;
                   key = crypt(password, salt);
                   if( key ) {
                       if( strncmp( key, passwdline+l+1, strlen(key) )==0 ) {
                           setenv("loginname", username, 1);
                           res=1 ;
                       }
                   }
                   break;
            }
        }
        fclose(fpasswd);
    }
    return res ;
}

void savepostfile(char * postfiledir )
{
    char postfilename[1024] ;
    FILE * postfile ;
    char * boundary ;
    char * p ;
    char * p2 ;
    char linebuf[4096] ;
    int  i ;
    int  content_length ;
    int  d ;
    int filesize=0 ;
    int lbdy ;          // boundary length ;
    int datatype = 1 ;      // 0: dataend, 1: data, 2: databoundary

    p = getenv("CONTENT_TYPE");
    if( p ) {
        boundary = strstr(p, "boundary=" ) ;
        if( boundary ) {
            boundary += 9 ;
            lbdy = strlen(boundary);
        }
        else {
            return;
        }
    }
    else {
        return ;
    }

    content_length = 0 ;
    p = getenv("CONTENT_LENGTH" );
    if( p ) {
        sscanf(p,"%d", &content_length);
    }
    if( content_length <= 2*lbdy ) {
        return ;
    }

    // get first boundary
    if( fgets( linebuf, sizeof(linebuf), stdin)== NULL )
        return ;
    if( linebuf[0]!='-' ||
        linebuf[1]!='-' ||
        strncmp( &linebuf[2], boundary, lbdy )!=0 ) 
        return ;

    mkdir(postfiledir, 0755);

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
        
        sprintf(postfilename, "%s/post_file_%s", postfiledir, part_name );
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
            else {
                break;
            }
        }
    }
}
        

void smallssi_run( char * ssipath )
{
    int r ;
    char * p ;          // generic pointer
    FILE * fp ;
    char * request_method = NULL ;
    int    content_length = 0 ;
    char * content_type = NULL ;
    char * post_string = NULL ;
    char ssidir[512] ;
    char * postfiledir = NULL ;
    char * ssid ;
    strncpy( ssidir, ssipath, 511);
    ssidir[511]=0 ;
    ssid=strrchr(ssidir, '/' );
    if( ssid ) {
        *ssid=0;
        chdir(ssidir);
    }
    else {
        chdir(document_root);
    }
    
    // check input
    request_method = getenv("REQUEST_METHOD") ;
    if( getenv("CONTENT_LENGTH") ) {
        content_length=atoi( getenv("CONTENT_LENGTH") );
    }
    else {
        content_length=0;
    }
    content_type = getenv("CONTENT_TYPE");
    if( content_length>0 && content_length< 20000000 ) {         // maximum content 20 MB 
        if( strcmp( request_method, "POST" )==0 &&
               content_type &&
               content_length<5000 &&
               strncasecmp( content_type, "application/x-www-form-urlencoded", 32)==0 ) {
            post_string = malloc( content_length + 1 );
            if( post_string ) {
                r = fread( post_string, 1, content_length, stdin );
                if( r>0 ) {
                    post_string[r]=0;
                    setenv( "POST_STRING", post_string, 1 );
                }
                free( post_string );
            }
            else {
                http_error( 500, "Internal Server Error.");
                return ;
            }
        }
        else if( strcmp( request_method, "POST" )==0 &&
                   content_length > 0 &&
                   content_type &&
                   strncasecmp( content_type, "multipart/form-data", 19 )==0 ) {
            postfiledir="post_file_001" ;
            savepostfile( postfiledir );
        }
    }

    fp = fopen( ssipath, "r" );
    if ( fp == NULL ) {
        http_error( 403, "Forbidden" );
        return ;
    }
    fclose( fp );

    http_header( 200, "OK", "text/html; charset=UTF-8", -1 );

//  dvr specific extension
    p = getquery( "page" );
    if( p && strcmp(p, "login")==0 ) {          // login page 
        if( checkloginpassword() ) {
               char serno[30] ;
               // setup configure data
               runapp("./getsetup", NULL);
               // make serial number
               makeserialno( serno, sizeof(serno));
               setcookie("ser",serno);
               strncpy( ssidir, ssipath, sizeof(ssidir) );
           }
        else {                                  // password error
            strcat( ssidir, "/login.html");
        }
    }
    else {
        p = getquery("ser") ;
        if( p ) {
            setcookie("ser",p);
        }
        else {
            p = getcookie("ser");
        }
            
        if( p && checkserialno (p)) {
            // excute pre-page 
            runapp("./eagle_setup", NULL );
            updateserialno(p);
            strncpy( ssidir, ssipath, sizeof(ssidir) );
            http_cache(0, (time_t)0);
        }
        else {
            strcat( ssidir, "/login.html");
        }
    }
    
    // generic ssi
    printf("\r\n");                                 // empty line.
    smallssi_include_file( ssidir );

    // remove all posted files
    if( postfiledir ) {
        sprintf( ssidir, "rm -r %s", postfiledir );
        system( ssidir );
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
// return 0 : no more HTTP_* environment, 1: 1 HTTP_* environment removed
int unsethttpenv()
{
    int i ;
    char envname[105] ;
    extern char **environ ;
    for(i=0; i<200; i++) {
        if( environ[i]==NULL || environ[i][0]==0 ) {
            break;
        }
        if( strncmp( environ[i], "HTTP_", 5 )==0 ) {
            char * eq = strchr( environ[i], '=' ) ;
            if( eq ) {
                int l = eq-environ[i] ;
                strncpy( envname, environ[i], l);
                envname[l]=0 ;
                unsetenv( envname );
                return 1 ;
            }
        }
    }
    return 0 ;
}

int http()
{
    int res=0 ;
    char * method ;	// request method
    char * uri ;	// request URI
    char * query ;	// request query string
    char * protocol ;	// request protocol

    char * ext ;
    char * pc ;
    size_t len;
    int ch;
    FILE* fp;
    int i;
    struct stat sb;
    
    //    setvbuf( stdin, NULL, _IONBF, 0);	// unbuffered mode
    linebuf[0]=0 ;
    if ( fgets( linebuf, sizeof(linebuf), stdin ) == NULL )
    {
//        http_error(400, "Bad Request" );
        goto http_done ;
    }
    
    method = cleanstring(linebuf);
    uri = strchr(method, ' ');
    if( uri==NULL ) {
//        http_error(400, "Bad Request" );
        goto http_done ;
    }
    *uri=0;
    uri=cleanstring(uri+1);
    
    // remove all HTTP_* environment
    while( unsethttpenv() ) ;
    
    // set request method, support POST and GET only
    if( strcasecmp(method, "POST")==0 ) {
        setenv( "REQUEST_METHOD", "POST", 1 );
    }
    else if( strcasecmp(method, "GET")==0 ) {
        setenv( "REQUEST_METHOD", "GET", 1 );
    }
    else {
        http_error(300, "Bad Query");
        goto http_done ;
    }
    
    // check uri
    if( uri[0] != '/' ||
       uri[1] == '/' ||
       strstr( uri, "/../" ) ) {
        http_error(400, "Bad Request" );
        goto http_done ;
    }
    
    protocol=strchr(uri, ' ');
    if( protocol==NULL ) {
        http_error(400, "Bad Request" );
        goto http_done ;
    }

    *protocol=0;
    protocol=cleanstring(protocol+1);
    // check protocol
    if( strncasecmp(protocol, "HTTP/1.", 7)!=0 ) {
        http_error( 300, "Bad Request");
        goto http_done ;
    }
    setenv("SERVER_PROTOCOL", protocol, 1 );

    // get query string
    query=strchr(uri, '?');
    if( query ){
        *query=0;
        query++;
        setenv( "QUERY_STRING", query, 1);
    }
    else {
        unsetenv( "QUERY_STRING" );
    }

    if( uri[1]=='\0' ) {        // default page ?
        setenv( "REQUEST_URI", "/system.html", 1 );
    }
    else {
        setenv( "REQUEST_URI", uri, 1 );
    }
    setenv( "DOCUMENT_ROOT", document_root, 1);
    chdir(document_root);

    // Not using these variables, but put them there for compatible.
    setenv("PATH_INFO", "", 1 );
    setenv("PATH_TRANSLATED", "", 1);
    
    // analyz request header
    while ( fgets( linebuf, sizeof(linebuf), stdin )  )
    {
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
        else if( strncmp(linebuf, "Content-Type:", 13)==0 ) {
            setenv("CONTENT_TYPE", cleanstring(linebuf+13), 1 );
        }
        else if( strncmp(linebuf, "Content-Length:", 15)==0 ) {
            setenv("CONTENT_LENGTH", cleanstring(linebuf+15), 1 );
        }
        else {
            // all other header lines. set var as HTTP_???
            sethttpenv(linebuf);
        }
    }
    
    // use linebuf to store document name.
    sprintf(linebuf, "%s%s", document_root, getenv("REQUEST_URI") );
    // check if we need to run a cgi
    ext=strrchr( linebuf, '.' ) ;
    if( ext ) {
        if(strcasecmp(ext, ".html")==0 ||   // to run small ssi
           strcasecmp(ext, ".shtml")==0 ||
           strcasecmp(ext, ".shtm")==0 ) {  
            smallssi_run(linebuf);
            goto http_done ;
        }
        else if( strcasecmp(ext, ".cgi")==0 ) {
            // execute cgi.
            cgi_run(linebuf);
            return 0 ;
        }
    }

    // if we are here, no cgi is executed, dump out requested file.
    if ( stat( linebuf, &sb ) < 0 ) {
        http_error( 404, "Not Found");
        goto http_done ;
    }
    if ( S_ISDIR( sb.st_mode ) )    // not supporting directory list
    {
        http_error( 403, "Forbidden" );
        goto http_done ;
    }
    
    fp = fopen( linebuf, "r" );	    
    
    if ( fp == NULL ) {
        http_error( 403, "Forbidden" );
        goto http_done ;
    }
    
    fseek( fp, 0, SEEK_END );
    len = (size_t) ftell( fp );
    fseek( fp, 0, SEEK_SET );
    
    if( len<=0 ) {
        http_error( 404, "Not Found" );
        fclose( fp );
        goto http_done ;
    }
    
    http_header( 200, "Ok", get_mime_type( linebuf ), len );
    http_cache( 432000, sb.st_mtime );
    
    printf("\r\n");                                 // empty line.
   
    // output whole file 
    while ( ( ch = getc( fp ) ) != EOF ) {
        putchar( ch );    
    }
    
    fclose( fp );
    res=0 ;
    
http_done:
    printf("\r\n"); 
    return res ;
}

int main(int argc, char * argv[])
{
    if( argc>1 ) {
        document_root=argv[1] ;
    }
    while( http() ) ;
    return 0 ;
}

 
