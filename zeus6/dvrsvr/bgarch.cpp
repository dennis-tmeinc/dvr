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

#include "dir.h"
#include "genclass.h"

static char *basefilename(const char *fullpath)
{
    char *basename;
    char *base1;
    
    basename = (char *)fullpath;
    while ((base1 = strchr(basename, '/')) != NULL) {
        basename = base1 + 1;
    }
    return basename;
}

float cpu_usage()
{
	static float x_uptime = 0.0 ;
	static float x_idletime = 0.0 ;
	static float x_usage = 0.0 ;
	float uptime ;
	float idletime ;
		
	FILE * fuptime = fopen("/proc/uptime", "r" );
	if( fuptime ) {
		fscanf( fuptime, "%f %f", &uptime, &idletime ) ;
		fclose( fuptime );
	}
	if( uptime-x_uptime > 0.1 ) {
		x_usage = 1.0 - (idletime-x_idletime) / (uptime - x_uptime );
		x_uptime = uptime ;
		x_idletime = idletime ;
	}
	return x_usage ;
}

// 0: quit archiving, 1: archiving
static int * archive_run ;
static int archive_type ;
static int archive_mode ;
static int old_bcddate, old_bcdtime ;		// oldest file date/time been deleted
static string archive_target_disk ;

int archive_cleandir( char * tdir )
{
	int clean = 1 ;
	dir sdir( tdir ) ;
	while( sdir.find() ) {
		if( sdir.isdir() ) {
			if( archive_cleandir( sdir.pathname() ) ) {
				rmdir( sdir.pathname() ) ;
			}
			else {
				clean = 0 ;
			}
		}
		else {
			clean = 0 ;
		}
	}
	return clean ;
}

static void archive_findoldvideo( char * tdir, string & vfile )
{
	dir sdir(tdir) ;
	// find video files
	while( sdir.find( "CH*" ) ) {
		if( sdir.isfile() ) {
			int bd, bt ;
			bd=bt=0;
			sscanf( sdir.filename()+5, "%8d%6d", &bd, &bt );
			if( bd < old_bcddate || ( bd == old_bcddate && bt<old_bcdtime ) ) {
				vfile = sdir.pathname();
				old_bcddate = bd ;
				old_bcdtime = bt ;
			}
		}
	}
	sdir.rewind();
	// find video files
	while( sdir.find() ) {
		if( sdir.isdir() ) {
			archive_findoldvideo( sdir.pathname(), vfile );
		}
	}
	
}

// To remove oldest video file and make more free space on disk
int archive_makespace( char * basedir )
{
	string oldvideofile ;

	// a very big bcd date
	old_bcddate = 30000000 ;
	old_bcdtime = 0 ;

	archive_findoldvideo( basedir, oldvideofile ) ;
	if( strlen( oldvideofile )>0 ) {
		remove( oldvideofile );
		return 1 ;
	}
	return 0 ;
}

	
// check disk free space,
//    return true if more then 10% space and 500Mbytes is free
int archive_checkfreespace( char * disk )
{
	struct statfs stfs;
    if (statfs(disk, &stfs) == 0) {
		return ((stfs.f_bavail * 100)/stfs.f_blocks > 10 ) &&
				stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize) > 500 ;
    }
    return 0;
}
	
// 		
int archive_diskspace()
{
	int d ;
	int retry ;
    for(retry=0; retry<500; retry++) {
		if( archive_checkfreespace(archive_target_disk) ) 
			return 1 ;
		archive_makespace(archive_target_disk);
		sync();
    }
}


// return 1: success, 0: fail, -1: to quit
static int archive_copyfile( char * srcfile, char * destfile )
{
    FILE * fsrc ;
    FILE * fdest ;
    int    dest_exist = 0 ;
    char   *filebuf ;
    string tmpfile ;
    int r ;
    int res ;

    struct stat sfst ;		// source file stat
    struct stat dfst ;		// destfile stat
    
    if( stat( srcfile, &sfst )!=0 ) {
        return 0;
    }
    
	if( stat( destfile, &dfst )==0 ) {
		dest_exist = 1 ;
		if( S_ISREG( dfst.st_mode ) && dfst.st_size == sfst.st_size ) {
			// destination file already exist
			return 1 ;
		}
	}
	
	sprintf( tmpfile.setbufsize(512), "%s/.tarchf", (char *)archive_target_disk );
	
	// copy source file to temperary file
    fsrc = fopen( srcfile, "rb" );
    fdest = fopen( tmpfile, "wb" );
    if( fsrc==NULL || fdest==NULL ) {
        if( fsrc ) fclose( fsrc );
        if( fdest ) fclose( fdest );
        return 0 ;
    }

    filebuf=(char *)malloc(128*1024);
    res = 0 ;
    while( *archive_run > 0 ) {
        if( cpu_usage() < 0.8 ) {
            r=fread( filebuf, 1, 128*1024, fsrc );
            if( r>0 ) {
                fwrite( filebuf, 1, r, fdest ) ;
            }
            else {
                res = 1 ;       // success
                break ;
            }
        }
        else {
            usleep(100000);
        }
    }
    free(filebuf);
    fclose( fsrc ) ;
    fclose( fdest ) ;
    
    if( res == 1 ) {		// success
		if (dest_exist)
			remove(destfile);
		rename( tmpfile, destfile );
		
		if( archive_mode == 2 ) {		// move mode
			remove( srcfile );
		}
		else if( archive_mode == 1 ) {	// unlock mode
			tmpfile = srcfile ;
			char * l = strstr( (char *)tmpfile, "_L" ) ;
			if( l != NULL ) {		// locked file?
				l[1] = 'N' ;
				rename( srcfile, tmpfile );		// rename to _N_ file
			}
		}
	
	}
    
    return res ;
}

// copy directory			
int archive_copydir( char * srcdir, char * destdir ) 
{
	char * filename ;
	string destfile ;
	destfile.setbufsize(1024);
	dir sdir( srcdir ) ;
	while( *archive_run>0  && sdir.find() ) {
		if( sdir.isfile() ) {
			filename = sdir.filename() ;
			
			// don't copy DISK* file , temperary to fix bug "dis2 copy"
			if( fnmatch( "DISK?", filename, FNM_CASEFOLD )==0 ) {
				continue ;
			}
			
			if( filename[0]=='C' && filename[1]=='H' ) {			// might be video file or .k file
				
				int bcddate, bcdtime ;
				if( strstr( filename, "_0_" ) ) {	// currently recording video file, skip it
					continue ;
				}
				if( strstr( filename, "_L_" ) ) {					// locked file
					if( (archive_type & 2) == 0 ) {
						continue ;
					}
					sscanf( filename+5, "%8d%6d", &bcddate, &bcdtime );
					if( bcddate<old_bcddate || (bcddate == old_bcddate && bcdtime<=old_bcdtime ) ) {
						// do not copy older files
						continue ;		
					}
				}
				if( strstr( filename, "_N_" ) ) {					// nonlocked file
					if( (archive_type & 1) == 0 ) {
						continue ;
					}
					sscanf( filename+5, "%8d%6d", &bcddate, &bcdtime );
					if( bcddate<old_bcddate || (bcddate == old_bcddate && bcdtime<=old_bcdtime ) ) {
						// do not copy older files
						continue ;		
					}
				}
			}
			
			sprintf( destfile, "%s/%s", destdir, sdir.filename() );
			
			// to check target disk space
			archive_diskspace();
			archive_copyfile( sdir.pathname(), destfile );
		}
    }
    // scan sub dir
    sdir.rewind();
	while( *archive_run>0  && sdir.find() ) {
		if( sdir.isdir() ) {
			sprintf( destfile, "%s/%s", destdir, sdir.filename() );
			mkdir( destfile, 0777 );
			archive_copydir( sdir.pathname(), destfile );
		}
    }
    return 1 ;
}

int archive_multidisk( char * targetdir )
{
	int disks = 0 ;
	struct stat fstarget, fsdisk ;
	
    if (stat(targetdir, &fstarget) !=0 ) 
		return 0 ;
    
	dir sdir( targetdir );
	while( sdir.find() ) {
		if( sdir.isdir() ) {
			if( stat(sdir.pathname(), &fsdisk) ==0 ) {
				if( major(fstarget.st_dev) != major(fsdisk.st_dev) ||
					minor(fstarget.st_dev) != minor(fsdisk.st_dev) ) {
					disks++;
				} 
			}
		}
	}
	return disks ;
}

int archive_load_xdate()
{
	string xdatefile ;
	sprintf( xdatefile.setbufsize(500), "%s/.xarchdate", (char *)archive_target_disk );
	
	FILE * fxdate = fopen( (char *)xdatefile, "r" );
	if( fxdate ) {
		fscanf( fxdate, "%d %d", &old_bcddate, &old_bcdtime );
		fclose( fxdate );
	}
	else {
		old_bcddate = 20100000 ;
		old_bcdtime = 0 ;
	}
	return 0;
}

int archive_save_xdate()
{
	string xdatefile ;
	sprintf( xdatefile.setbufsize(500), "%s/.xarchdate", (char *)archive_target_disk );
	FILE * fxdate = fopen( (char *)xdatefile, "w" );
	if( fxdate ) {
		fprintf(fxdate, "%d %d", old_bcddate, old_bcdtime );
		fclose( fxdate );
	}
	else {
		old_bcddate = 20100000 ;
		old_bcdtime = 0 ;
	}
	return 0 ;
}

// archive hold directory, 
//   atype:  ORed bits for file types,  bit 0: _N_ files, bit 1: _L_ files
//   mode:  0, just copy
//              1, lock to unlock
//              2,  move mode (delete source file)
int archive( char * sourcedir, char * targetdir, int atype, int mode )
{
	archive_target_disk = targetdir ;
	archive_type = atype ;
	archive_mode = mode ;
	archive_load_xdate();
	archive_copydir( sourcedir, archive_target_disk ) ;
	archive_cleandir( targetdir );	
	archive_save_xdate();
	return 0 ;
}

void sig_handler(int signum)
{
	*archive_run = 0;
}

int main(int argc, char * argv[] )
{
	int atype = 3 ;
	int mode = 0 ;
	char * targetdir = (char *)"." ;
	if( argc>4 ) {
		mode = atoi( argv[4] );
	}
	if( argc>3 ) {
		atype = atoi( argv[3] );
	}
	if( argc>2 ) {
		targetdir = argv[2] ;
	}
	if( argc<2 ) {
		printf("No source dir specified!\n");
		return 1 ;
	}
	int run = 1 ;
	archive_run = &run ;
	
	// setup signal
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
      
    while( run ) {  
		archive( argv[1], targetdir, atype, mode );
		for( int delay=0; run && delay<30; delay++ ) {
			sleep(1);
		}
	}
		
	return run ;
}
