/* bgarch.cpp
 *     disk archiving support for PWZ5
 */

#include <errno.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/vfs.h>

#include "dvr.h"
#include "dir.h"
#include "genclass.h"
#include "../ioprocess/diomap.h"

struct archive_t {
	int src; 		// source disk
	int dst;		// destination disk
	int srctype;	// archive source file type, bit0: N file, bit1: L file
	int srcmode; 	// srcmode: 0: just copy, 1: unlock _L_ file, 2: move mode (delete source file )
	int when;		// when to copy, 0: normal period, 1: when standby mode
};

struct archive_t archive_param[3] = {
	{ 1, 0, 2, 1, 0 },		// move _L_ file from DISK2 to DISK1, don overwrite DISK1
	{ 0, 2, 3, 1, 1 },		// copy all file from DISK1 to DISK3, overwrite DISK3
	{ 1, 2, 3, 1, 1 }		// copy all file from DISK2 to DISK3, overwrite DISK3
};

static int archive_run = 0 ;			// 0: stopped, 1: running, -1 signal to stop
pthread_t  archive_thread = NULL ;		// task thread id
static int archive_curtask = 0 ;

static string archive_target_tmpfile ;


static int checkrunmode()
{
	// check copy time period
	int runmode = dio_curmode();
	if( archive_param[archive_curtask].when == 0 ) {		// normal mode
		return  (runmode == APPMODE_RUN || runmode == APPMODE_SHUTDOWNDELAY ) ;
	}
	else {										// during standby 
		return ( runmode == APPMODE_STANDBY ) ;
	}
}

static int checkdisks()
{
	return  ( pw_disk[ archive_param[archive_curtask].src ].mounted &&
				pw_disk[ archive_param[archive_curtask].dst ].mounted &&
				! pw_disk[ archive_param[archive_curtask].dst ].full );
}

// return 1: success, 0: fail, -1: to quit
static int archive_copyfile( char * srcfile, char * destfile )
{
    FILE * fsrc ;
    FILE * fdest ;
    char   *filebuf ;
    int r ;
    int complete ;

    struct stat sfst ;		// source file stat
    struct stat dfst ;		// destfile stat
    
    if( stat( srcfile, &sfst )!=0 ) {
        return 0;
    }
    
	if( stat( destfile, &dfst )==0 ) {
		// destination file already exist
		if( S_ISREG( dfst.st_mode ) && dfst.st_size == sfst.st_size ) {
			return 1 ;
		}
		remove(destfile);
	}

printf("archive: %s -> %s\n", srcfile, destfile );

	// copy source file to temperary file
    fsrc = fopen( srcfile, "r" );
    fdest = fopen( archive_target_tmpfile, "w" );
    if( fsrc==NULL || fdest==NULL ) {
        if( fsrc ) fclose( fsrc );
        if( fdest ) fclose( fdest );
        return 0 ;
    }

    filebuf=(char *)malloc(128*1024);
    complete = 0 ;
	// scan files
	while( archive_run==1 &&
		  checkrunmode() && 
		  checkdisks() ) {
		r=fread( filebuf, 1, 128*1024, fsrc );
		if( r>0 ) {
			fwrite( filebuf, 1, r, fdest ) ;
		}
		else {
			complete = 1 ;       // success
			break ;
		}
		if( g_cpu_usage > 90 && g_memdirty>1000 ) {
			usleep(10000);			// make it nicer
		}
    }
    free(filebuf);
    fclose( fsrc ) ;
    if( fclose( fdest ) != 0 ) {
		complete=0 ;
	}
    
    if( complete ) {				// copy completed
		rename( archive_target_tmpfile, destfile );
		
		int srcmode = archive_param[archive_curtask].srcmode ;
		
		if( srcmode == 2 ) {		// move mode
			remove( srcfile );		// delete source file
		}
		else if( srcmode == 1 ) {	// unlock mode
			string tmpfile ;
			tmpfile = srcfile ;
			char * l = strstr( (char *)tmpfile, "_L" ) ;
			if( l != NULL ) {		// locked file?
				l[1] = 'N' ;
				rename( srcfile, tmpfile );		// rename to _N_ file
			}
		}
	
	}
    
    return complete ;
}

// copy directory			
static int archive_copydir( char * srcdir, char * destdir ) 
{
	char * filename ;
	string destfile ;
	destfile.setbufsize(1024);
	dir sdir( srcdir ) ;
	
	// scan sub dir
	while( archive_run==1 &&
		  checkrunmode() && 
		  checkdisks() && 
		  sdir.find() ) {
		if( sdir.isdir() ) {
			sprintf( destfile, "%s/%s", destdir, sdir.filename() );
			mkdir( destfile, 0777 );
			archive_copydir( sdir.pathname(), destfile );
		}
    }
	
	sdir.rewind();
	
	// scan files
	while( archive_run==1 &&
		  checkrunmode() && 
		  checkdisks() && 
		  sdir.find() ) {
		if( sdir.isfile() ) {
			filename = sdir.filename() ;
			sprintf( destfile, "%s/%s", destdir, filename );				
			
			if( fnmatch( "CH0?_2*", filename, 0 )==0 ) {			// might be video file or .k file
				if( strstr( filename, "_0_" ) ) {					// currently recording video file, skip it
					continue ;
				}
				int stype = archive_param[archive_curtask].srctype ;
				if( strstr( filename, "_L_" )  && (stype&2) != 0 ) {					// locked file
					archive_copyfile( sdir.pathname(), destfile );
				}
				else if( strstr( filename, "_N_" ) && (stype&1) != 0 ) {				// nonlocked file
					archive_copyfile( sdir.pathname(), destfile );
				}
			}
			else if( fnmatch( "*_L.0??", filename, 0 )==0 ) {			// smartlog files
				archive_copyfile( sdir.pathname(), destfile );
			}
		}
    }

    return 1 ;
}

static void * archive_thread_proc( void * )
{
	nice(18);
	// run each task in turn
	archive_curtask = 0;
	while( archive_run==1 ) {
		// check copy time period
		if( checkrunmode() && checkdisks() ) { 
			sprintf( archive_target_tmpfile.setbufsize(512), "%s/.ARF", (char *)( pw_disk[ archive_param[archive_curtask].dst ].disk ) );			
			archive_copydir( pw_disk[ archive_param[archive_curtask].src ].disk, pw_disk[ archive_param[archive_curtask].dst ].disk ) ; 
		}
		archive_curtask=(archive_curtask+1)%3 ;
		// sleep
		for( int x=0; x<1000 && archive_run==1 ; x++ )
			usleep(10000);
			
	}
	archive_run = 0 ;
}

void archive_start()
{
	archive_run = 1 ;
	// setup signal
	
	if( pthread_create(&archive_thread, NULL, archive_thread_proc, (void *)NULL ) != 0 ) {
		archive_thread = NULL ;
	}
}

void archive_stop()
{
	archive_run = -1 ;
	if( archive_thread != (pthread_t) NULL ) {
		// pthread_kill( archive_thread, SIGTERM );
		pthread_join(archive_thread, NULL);
		archive_thread = NULL ;
	}
}

