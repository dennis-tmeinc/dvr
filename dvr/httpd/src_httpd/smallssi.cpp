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
#include <stdarg.h>

#include "eaglehttpd.h"

int smallssi_gettoken(FILE * in, char * token, int bufsize)
{
	const char start_mark[]="<!--#" ;
	const char end_mark[]="-->" ;
	int c;
    int i;
    for( i=0;  i<bufsize-1; i++ ) {
		c = fgetc(in);
		if( c==EOF ) return 0 ;
		
		token[i] = c ;
		if( i<5 && token[i] == start_mark[i] ) continue ;
		else if( i>=5 ) {
			// read until end mark
			if( token[i-2]==end_mark[0] && token[i-1]==end_mark[1] && token[i]==end_mark[2] ) {
				return i+1 ;
			}
		}
		else {
			return i+1 ;
		}
	}
	
	// oops, out of buffer. read until the end and discard it
	token[0] = token[i-2] ;
	token[1] = token[i-1] ;
	while( 1 ) {
		c = fgetc(in);
		if( c==EOF ) return 0 ;
		
		token[2] = c ;
		if( token[0]==end_mark[0] && token[1]==end_mark[1] && token[2]==end_mark[2] ) {
			break ;
		}
		token[0]=token[1] ;
		token[1]=token[2] ;
	}
	// faked empty charactor
	token[0]=' ' ;
	return 1 ;
}

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
                smallssi_include_file( trim(ifile) ) ;
            }
        }
    }
}

// ex: <!--#echo var="envname" -->
void smallssi_echo( char * ssicmd )
{
    char * eq ;
    char * ifile ;

	// no cache
    http_nocache();

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
					http_buffer_write( eq, strlen(eq) );
                }
            }
        }
    }
}


void smallssi_cmd( char * cmd )
{
	FILE * fin ;
	char tbuf[260] ;
	
	char * param = strchr( cmd, ' ' );
	if( param != NULL ) {
		*param = 0 ;
	}
	// check alt path for cgi
	int r = document_stat( cmd, NULL, tbuf ) ;
	if( param != NULL ) {
		*param = ' ' ;		// restore space
	}
	
	if( r == 2 ) {		// return 2, cgi file located in alt document root
		if( param != NULL ) {
			strcat( tbuf, param );
		}
		fin = popen( tbuf, "r" );
	}
	else {
		fin = popen( cmd, "r" );
	}
	
	if( fin!=NULL ) {
		http_buffer_writefile(fin);
		pclose( fin );
	}
}
    
// ex: <!--#exec cmd="ls" -->
void smallssi_exec( char * ssicmd )
{
    char * eq ;
    char * ifile ;

	// no cache
    http_nocache();

    eq = strchr( ssicmd, '=' );
    if( eq ) {
        ifile=strchr(eq, '\"');
        if( ifile ) {
            ifile++;
            eq=strchr(ifile, '\"');
            if( eq ) {
                *eq=0;
                smallssi_cmd( trim(ifile) ) ;
            }
        }
    }
}

void smallssi_include_file( char * ifilename )
{
    FILE * fp ;
    int r ;
    char * token ;
    
    r = document_stat( ifilename ) ;
    if( r<=0 || r>2 )	{		// support plain files only
		return ;
	}

    fp = document_open( ifilename ) ;
    if( fp==NULL ) {
        return ;
    }
    
	http_cacheupdate(ifilename);

    while( (token=http_buffer_request(HTTP_LINESIZE)) != NULL ) {
		r = smallssi_gettoken(fp, token, http_buffer_remain() );
		if( r<=0 ) {
			break ;
		}
        else if( r<8 ) {
			// http_buffer_write( token, r );
			http_buffer_advance( r );
        }
        else if( strncasecmp(token+5, "include ", 8) == 0 ) {	// found ssi include command
            smallssi_include(token+13);
        }
        else if( strncasecmp(token+5, "echo ", 5) == 0 ) {		// found ssi echo command
            smallssi_echo(token+10);
        }
        else if( strncasecmp(token+5, "exec ", 5) == 0 ) {		// found ssi exec command
            smallssi_exec(token+10);
        }
        //else {
			// discard inputs
        //}
    }

    fclose( fp );
}

