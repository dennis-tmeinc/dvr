#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
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

#include "dvrsvr/dir.h"

#include "eaglehttpd.h"

extern char **environ ;

void mod_env()
{
	int i;
	http_set_contenttype( "text/plain" );
	for(i=0; i<200; ) {
        if( environ[i]==NULL || environ[i][0]==0 ) {
            break;
        }
        http_buffer_print("%s\r\n", environ[i] );
        i++ ;
    }
}

void mod_test()
{
	http_set_contenttype( "text/plain" );
    http_buffer_print("MOD TEST");
}

// ls directorys on /var/dvr/disks
void mod_ls()
{
	char * xd ;
	int  type = 0 ;			// 0: all, 1: files, 2: directoies
	char ldir[256] ;
	dir mdir ;
	http_set_contenttype( "text/plain" );
	xd = getRequest("n");
	if(xd) {
		sprintf(ldir, "/var/dvr/disks/%s", xd );
	}
	else {
		strcpy(ldir, "/var/dvr/disks" );
	}
	
	if( (xd = getRequest("t"))!=NULL ) {
		if( *xd=='f' ) type=1 ;
		else if( *xd=='d' ) type=2 ;
	}
	
	mdir.open( ldir );
	while( mdir.find() ) {
		if( type==1 ) {
			if( !mdir.isfile() ) continue ;
		}
		else if( type==2 ) {
			if( !mdir.isdir() ) continue ;
		}
		http_buffer_print( mdir.filename() );
		http_buffer_print( "\r\n" );
	}
}

// read file
void mod_cat()
{
	char * xd ;
	int  pos = 0 ;
	int  len = -1 ;
	char lpath[256] ;
	http_set_contenttype( "stream/octem" );
	
	xd = getRequest("n");
	if(xd) {
		sprintf(lpath, "/var/dvr/disks/%s", xd );
	}
	else {
		return ;
	}

	xd = getRequest("o");		// offset
	if(xd) {
		pos = atoi(xd);
	}

	xd = getRequest("l");		// length
	if(xd) {
		len = atoi(xd);
	}

	FILE * f = fopen(lpath, "r" );
	if( f ) {
		if( pos>0 )
			fseek(f, pos, SEEK_SET);
			
		if( len<0 ) {
			http_buffer_writefile( f );
		}
		else {
			while( len>0 && (xd=http_buffer_request(4096))!=NULL ) {
				int l = http_buffer_remain();
				if( l>len ) l=len ;
				
				int n = fread( xd, 1, l, f );
				if( n > 0 ) {
					http_buffer_advance( n );
					len-=n ;
				}
				else {
					break ;
				}
			}			
		
		}
		 fclose(f);
	 }
 }

void mod_cmd()
{
	http_set_contenttype( "text/plain" );
	FILE * p = popen( getRequest("n"), "r" );
	if( p ) {
		http_buffer_writefile( p );
		pclose(p);
	}

}

void mod_run()
{
	char * uri ;
    uri = getenv("REQUEST_URI");
    char * module = uri+5 ;
    
    if( strcmp( module, "env" )==0 ) {
		mod_env();
	}
	else if( strcmp( module, "ls" )==0 ) {
		mod_ls();
	}
	else if( strcmp( module, "cat" )==0 ) {
		mod_cat();
	}
	else if( strcmp( module, "cmd" )==0 ) {
		mod_cmd();
	}
	else if( strcmp( module, "test" )==0 ) {
		mod_test();
	}
	else {
		// unknown mod name
		http_set_contenttype( "text/plain" );
		http_buffer_print( "What?" );
	}

}
