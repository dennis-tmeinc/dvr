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

#include "../../cfg.h"

// fucntion from getquery.cpp
int decode(const char * in, char * out, int osize );
char * getquery( const char * qname );

char serfile[]=WWWSERIALFILE;
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

int cleanserialno()
{
    remove( serfile );
    return 0 ;
}

extern char *crypt (__const char *__key, __const char *__salt);

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

    if( strcmp(username, "root") != 0 ) {
		fpasswd = fopen( APP_DIR "/adminpasswd", "r");
		if( fpasswd ) {
		    if( fgets(passwdline, sizeof(passwdline), fpasswd) ) {
				strncpy(salt, passwdline, 13);
				salt[12]=0;
				key = crypt(password, salt);
				if( key && strncmp( key, passwdline, strlen(key) )==0 ) {
					setenv("loginname", username, 1);
					res=1 ;
				}
			}
			fclose( fpasswd );
		}
		return res ;
	}

    int useshadow = 0 ;

    fpasswd=fopen("/etc/passwd", "r");
    if( fpasswd ) {
        while( fgets(passwdline, sizeof(passwdline), fpasswd) ) {
            l=strlen(username);
            if( strncmp(username, passwdline, l)==0 &&
               passwdline[l]==':' ) {                    // username matched
                if( passwdline[l+1]=='x' && passwdline[l+2]==':') {
                    // to use shadowed password
                    useshadow = 1 ;
                }
                else {
                    strncpy(salt, passwdline+l+1, 13);
                    salt[12]=0;
                    key = crypt(password, salt);
                    if( key ) {
                        if( strncmp( key, passwdline+l+1, strlen(key) )==0 ) {
                            setenv("loginname", username, 1);
                            res=1 ;
                        }
                    }
                }
                break;
            }
        }
        fclose(fpasswd);
    }

    fpasswd=NULL ;
    if( useshadow ) {
        fpasswd=fopen("/etc/shadow", "r");
    }
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

void set_cookie( char * cname, char * cvalue )
{
    char * ex_header_filename ;
    FILE * ex_header_file ;
    ex_header_filename=getenv("POST_FILE_EXHEADER");
    if( ex_header_filename ) {
        ex_header_file=fopen(ex_header_filename, "a");
        if( ex_header_file ) {
            fprintf( ex_header_file, "Set-Cookie: %s=%s\r\n", cname, cvalue );
            fclose( ex_header_file );
        }
    }
}

int main()
{
	// print header
    printf("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    if( checkloginpassword() ) {
        char serno[40] ;
        // setup configure data
        system("cgi/getsetup");
        // make serial number
        makeserialno( serno, sizeof(serno));
        // set cookie
        set_cookie( "ser", serno );
        printf("{\"res\":\"1\",\"ser\":\"%s\"}", serno );
    }
    else {
        printf("{\"res\":\"0\"}" );
	}
	return 0;
}
