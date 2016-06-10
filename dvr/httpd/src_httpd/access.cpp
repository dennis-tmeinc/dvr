/* access.cpp
 *   access check for eagle httpd
 */

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

char serfile[]=WWWSERIALFILE ;

int checkserialno( char * serialno )
{
	int res = 0 ;
    time_t t ;
    unsigned int ui ;
    FILE * sfile ;
    char buf[1024];
    sfile = document_open(serfile);
    if( sfile ) {
		int n ;
		char line[1024] ;	
		char *l ;
		while( res==0 && (l=fgets(line, 1020, sfile))!=NULL ) {
			char usbid[32] ;
			n = sscanf(line, "%u %s %s",&ui, buf, usbid);
			if( n<2 ) break;
			if( strcmp(serialno, buf)==0 ) {
				if( n>=3 ) {
					// usbid available
					FILE * fconnid = document_open( "/var/dvr/connectid" );
					if( fconnid ) {
						char connid[32] ;
						if( fscanf( fconnid, "%s", connid )>0 ) {
							if( strcmp( connid, usbid )==0 ) {
								res = 1 ;
							}
						}
						fclose( fconnid );
					}
				}
				else {
					time(&t);
					if( ((unsigned int)t)-ui<600 ) {
						res = 1 ;
					}
				}
			}
		}
		fclose( sfile );
    }
    return res ;
}

// update valid serialno time
int updateserialno(char * serialno)
{
	int res = 0 ;
    time_t t ;
    unsigned int ui ;
    char buf[1024];
	char nserialfile[256] ;
    FILE * sfile ;
    sfile = fopen(serfile, "r");
	sprintf( nserialfile, "%s.n", serfile);
	FILE * nfile ;
	nfile = fopen( nserialfile, "w");
    if( sfile && nfile ) {
		int n ;
		char line[1024] ;	
		char *l ;
		while( res==0 && (l=fgets(line, 1020, sfile))!=NULL ) {
			char usbid[32] ;
			n = sscanf(line, "%u %s %s",&ui, buf, usbid);
			if( n<2 ) break;
			if( strcmp(serialno, buf)==0 ) {
		        time(&t);
				if( n>=3 ) {
					sprintf( line, "%u %s %s\n", (unsigned int)t, serialno, usbid );
				}
				else {
					sprintf( line, "%u %s\n", (unsigned int)t, serialno );
				}
			}
			fputs(line, nfile );
		}
		fclose( sfile );
		fclose( nfile );
		unlink( serfile );
		rename( nserialfile, serfile );
    }
    return 0 ;
}

int access_check()
{
    char * p ;
    char * uri ;
    int res=1 ;
    char access[128] ;
    
    FILE * accessfile = document_open( "access" );
    
    if( accessfile ) {
        uri = getenv("REQUEST_URI");
        while( fgets( access, 128, accessfile ) ) {
            if( strcmp( trim(access), uri )==0 ) {
                // access check required
                p = getRequest( "ser", "GP" ) ;
                if( p ) {
                    http_setcookie("ser",p);                    
				}
				else {
					// get from cookie
					p = getRequest( "ser", "C" ) ;
				}

                if( p && checkserialno (p)) {
                    updateserialno(p);
                }
                else {
                    res=0 ;
                }
                break;
            }
        }
        fclose( accessfile );
    }
    return res ;
}

