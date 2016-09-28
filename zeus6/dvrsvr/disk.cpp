
#include "dvr.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "dir.h"
#include "archive.h"
#include "../ioprocess/diomap.h"

// tvs fix, minimum disk size in Megabytes (default=1G)
#define MIN_DISK_SIZE (1024) 

#define MIN_HD_SIZE 150000

#define DVR_NETWORK     (0x10)

#ifdef TVS_APP_X

// log disk support
int    log_disk_timeout ;	// how long to wait 
string log_disk_id ;
string g_log_disk ;
#endif

char  g_diskwarning[256] ;		// diskwarning
int g_nodiskcheck ;

int disk_scanreq = 1 ;			// request for video file scanning

static float disk_mindiskspace_percent; 	// minimum free space percentage.  
static int disk_allow_recycle_lfile ;		// allow recycle _L_ files

int copy_files(char *from_root, 
	       char  *dest_root,
	       char *dest_dir,
	       char *servername);
int copy_file_to_path(char *from_fullname, char *to_path,
		      int forcewrite, int check_stop);

static string disk_base;                // recording disk base dir
static string disk_curdiskfile;         // record current disk

int   turndiskon_power=0;

char *basefilename(const char *fullpath)
{
    char *basename;
    basename = strrchr( fullpath, '/' ) ;
    if( basename!=NULL ) {
		return basename+1 ;
	}
	else {
		return (char *)fullpath ;
	}
}

#define F264TIME( filename )	(basefilename( filename ) + 5 )
#define F264CHANNEL( filename ) (basefilename( filename ) + 2 )

int f264time(const char *filename, struct dvrtime *dvrt)
{
    char * basename=NULL ;
    int n ;
    time_dvrtime_init(dvrt,2000);
    basename = basefilename( filename );
    n=sscanf( basename+5, "%04d%02d%02d%02d%02d%02d", 
             &(dvrt->year),
             &(dvrt->month),
             &(dvrt->day),
             &(dvrt->hour),
             &(dvrt->minute),
             &(dvrt->second) );
    if (n!=6) {
        dvr_log("error on filename");
        time_dvrtime_init(dvrt, 2000);
        return 0 ;
    }
    else {
        dvrt->milliseconds=0 ;
        return 1 ;
    }
}

int f264channel(const char *filename)
{
	int ch ;
    char * basename ;
    basename = basefilename( filename );
    if( strlen( basename )<21 ) {
        return 0 ;
    }
    if( sscanf( basename+2, "%2d", &ch )==1 ) {
        return ch ;
    }
    else 
        return 0;
}

int f264length(const char *filename)
{
    int length ;
    char lock ;
    char * basename ;
    basename = basefilename( filename );
    if( strlen( basename )<21 ) {
        return 0 ;
    }
    if( sscanf( basename+19, "_%d_%c", &length, &lock )==2 ) {
        return length ;
    }
    else 
        return 0;
}

// get lock length of the file
int f264locklength(const char *filename)
{
    int length ;
    int locklength ;
    char * basename ;
    char host1 ;
    char lock ;
    int n ;
    basename = basefilename( filename );
    if( strlen( basename )<21 ) {
           return 0 ;
    }
    n=sscanf( basename+19, "_%d_%c_%d_%c", &length, &lock, &locklength, &host1 ) ;
    if( n>=2 && lock=='L' ) {
        if( n==4 && (host1 == g_hostname[0]) ) {        // partial locked file
            return locklength ;
        }
        else {
            return length ;
        }
    }
    return 0 ;
}

// return free disk space in Megabytes
int disk_freespace(char *filename)
{
    struct statfs stfs;
    
    if (statfs(filename, &stfs) == 0) {
        return stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
    }
    return 0;
}

// return free disk space in percentage
float disk_freeratio(char *filename)
{
    struct statfs stfs;
    if (statfs(filename, &stfs) == 0) {
        return ((float)stfs.f_bavail) * 100.0 / ((float)stfs.f_blocks) ;
    }
    return 0.0;
}

// return disk size in Megabytes
int disk_size(char *filename)
{
    struct statfs stfs;
    
    if (statfs(filename, &stfs) == 0) {
        return stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
    }
    return 0;
}

// remove all files and directory
static void disk_rmdir(const char * tdir )
{
	dir d( tdir ) ;
	while( d.find() ) {
		if( d.isdir() ) {
			disk_rmdir(d.pathname() );
		}
		else {
			remove(d.pathname());
		}
	}
	d.close();
	rmdir( tdir );
}

// remove directory with no files
int disk_rmemptydir(char *tdir)
{
	int f = 0 ;
	dir d( tdir ) ;
	while( d.find(NULL, DIR_FINDDIR) ) {
		f += disk_rmemptydir( d.pathname() ) ;
	}
	if( f==0 ) {
		d.rewind();
		f = d.find(NULL, DIR_FINDFILE );
	}
	d.close();
	if( f==0 ) {
		rmdir( tdir );
	} 
	return f;
}

// remove .264 files and related .k, .idx, and all parent folder if it is empty
static int disk_removefile( const char * file264 ) 
{
    string f264 ;
    char * fn ;
    char * extension ;
    f264 = file264 ;
    fn = (char *)f264 ;
    if( remove( fn ) != 0 ) {
    	dvr_log( "Delete file failed: %s", fn );
    	return -1 ;
    }
    int l = strlen( fn );
    extension = fn+l-4 ;
    if( strcmp(extension, ".266")==0 ) {
        strcpy( extension, ".k");
        if( remove( fn )!= 0 ) {
            dvr_log( "Delete file failed: %s", fn );
		}
    }
    
    // try remove all parent folder
    char * r ;
    while( (r = strrchr(fn, '/'))!=NULL ) {
		*r = 0 ;
		if( rmdir( fn )!=0 ) {
			break;
		}
	}
    
    return 0 ;
}

// return 0, no changes
// return 1, renamed
// return 2, deleted
static int repairfile(const char *filename)
{
    dvrfile vfile;
    
    if (vfile.open(filename, "r+") == 0) {	// can't open it
        dvr_log("Repair file open error: %s is deleted", filename);
        disk_removefile( filename );		// try delete it.
        return 0;
    }

    if( vfile.repair() ) {// success?
        vfile.close();
        dvr_log("File repaired. <%s>", filename);		
        return 1;
    }
    else {
        // can't repair, delete it
        vfile.close();
        disk_removefile( filename );
        dvr_log("Corrupt file deleted. <%s>", filename);
        return 2;
    }
}

// try to repair partial lock files
static int  repairepartiallock(const char * filename) 
{
    dvrfile vfile ;
    int length, locklength ;
    length = f264length (filename) ;
    locklength = f264locklength (filename);
    if( locklength>0 && locklength<length ) {       // partial locked file ?
        if (vfile.open(filename, "r+") ) {	// can't open it
            vfile.repairpartiallock() ;
        }
    }
    return 0;
}

// remove directories has no .264 files
// dirlevel: 0:disk, 1:host, 2:date, 3: channel
// return 0: empty, 1: .266 files found
static int disk_rmempty264dir(char *tdir, int dirlevel)
{
	int f = 0 ;
	int r ;
	dir d( tdir ) ;
	while( d.find() ) {
		if( dirlevel <= 0 ) {
			if( d.isdir() ) {
				if( fnmatch( "_*_", d.filename(), 0 )==0 ) {
					f+=disk_rmempty264dir( d.pathname(), 1 );
				}
				else if( fnmatch( "FOUND.*", d.filename(), 0 )==0 ) {
					// clear disk check files
					disk_rmdir( d.pathname() );	
				}
				else {
					f+=disk_rmempty264dir( d.pathname(), 0 );
				}
			}
		}
		else if( dirlevel==1 || dirlevel==2 ) {
			if( d.isdir() ) {
				r = disk_rmempty264dir( d.pathname(), dirlevel+1 );
				if( r== 0 ) {
					disk_rmdir( d.pathname() );					
				}
				f+=r ;
			}
		}
		else if( dirlevel>=3 ) {
			if( d.isfile() && fnmatch( "CH*.266", d.filename(), 0 )==0 ) {
				return 1 ;
			}
		}
	}

	return f;
}

// repaire all .264 files under dir
static void disk_repair264(char *tdir)
{
    char * pathname ;
    char * filename ;
    dir dfind(tdir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        filename = dfind.filename();
        if( dfind.isdir() ) {
            disk_repair264( pathname );
        }
        else if( dfind.isfile() && fnmatch( "CH*.266", filename, 0 )==0 ) 
        {
            if( strstr(filename, "_0_") ) {
                repairfile(pathname);
            }
            else if( strstr(filename, "_L_") ) {
                repairepartiallock(pathname) ;
            }
        }
    }
}

// fix all "_0_" files & delete non-local video files
// disk: disk mount point
static void disk_diskfix(char *disk)
{
    char pattern[512];
    char * pathname ;
    char * filename ;
    // disable watchdog, so system don't get reset while repairing files
    dio_disablewatchdog();  
    
    disk_repair264(disk);
	disk_rmempty264dir(disk, 0);    
    
    dio_enablewatchdog();
}

// get a list of .264 files, for play back list use
// dirlevel: 0: disk, 1: _HOST_, 2: YYYYMMDD, 3: CH00
// return files found
static int disk_list264files(const char *tdir, array <f264name> & flist, char * day, int channel, int dirlevel )
{
	int f = 0;
	int l ;
    char * dname ;

    dir d(tdir);
    while( d.find() ) {
		if( dirlevel <= 0 ) {
			if( d.isdir() ) {
				dname = d.filename();
				l=strlen(dname);
				if( l>2 && *dname == '_' && *(dname+l-1) == '_' ) {
					// found _HOST_ pattern
					f+=disk_list264files( d.pathname(), flist, day, channel, 1 );
				}
				else {
					f+=disk_list264files( d.pathname(), flist, day, channel, 0 );
				}
			}
		}
		else if( dirlevel == 1 ) {		// _HOST_
			if( d.isdir()  ) {
				// check day (YYYYMMDD)
				dname = d.filename();
				if( *dname == '2' && (day==NULL || strcmp( d.filename(), day )==0 ) ) {
					f += disk_list264files( d.pathname(), flist, day, channel, 2 );
				}
			}
		}
		else if( dirlevel == 2 ) {		// YYYYMMDD
			if( d.isdir() ) {
				// check channel
				dname = d.filename();
				if( strncmp( dname, "CH0", 3 )==0 
					&& (channel<0 || dname[3] == ('0'+channel ) ) )
				{
					f += disk_list264files( d.pathname(), flist, day, channel, 3 );
				}
			}
		}
		else {		// level == 3,  video files
			if( d.isfile() && fnmatch( "CH*.266", d.filename(), 0 )==0 ) {
				f264name fn ;
				fn = d.pathname() ;
				flist.add(fn);
				f++ ;
			}
		}
	}
	return f;
}

// get .264 file list by day and channel, for playback
void disk_listday(array <f264name> & flist, struct dvrtime * day, int channel)
{
	int pos ;
	char * fn ;
    long long ptime, ctime ;
	char date[10] ;
	sprintf(date, "%04d%02d%02d", 
                    day->year,
                    day->month,
                    day->day );
	flist.setsize(0) ;
    disk_list264files( "/var/dvr/disks",  flist, date, channel, 0 );
    flist.sort();
    
    // to remove repeated files
    ptime = 0 ;
    for( pos=0; pos<flist.size();  ) {
		fn = F264TIME( (char *)(flist[pos]) );
		sscanf( fn, "%14Ld", &ctime );
		if( ctime == ptime ) {			// match same file time
			if( strstr( fn+14, "_L_" ) != NULL ) {
				flist.remove( pos-1 ) ;
			}
			else {
				flist.remove( pos ) ;
			}
		}
		else {
			ptime = ctime ;
			pos++ ;
		}
	}
}

// get a list of .264 files, for play back list use
// dirlevel: 0 before _HOST_, 1: YYYYMMDD
static void disk_listday_help(const char *tdir, array <int> & daylist, int channel, int dirlevel )
{
    char * dname ;
    int l ;
    dir d(tdir);
    while( d.find() ) {
		if( dirlevel <= 0 ) {
			if( d.isdir() ) {
				dname = d.filename() ;
				l = strlen(dname);
				if( l>2 && *dname == '_' && *(dname+l-1)=='_' ) {
					// found _HOST_ pattern
					disk_listday_help( d.pathname(), daylist, channel, 1 );
				}
				else {
					disk_listday_help( d.pathname(), daylist, channel, 0 );
				}
			}
		}
		else if( dirlevel == 1 ) {		// YYYYMMDD
			if( d.isdir() ) {
				// check day (YYYYMMDD)
				dname = d.filename();
				if( strlen( dname ) == 8 && *dname == '2' ) {
					int day = atoi(dname);
					if( day>20100000 && day<21000000 ) {
						if( channel>=0 && channel<16 ) {
							string chname ;
							struct stat st ;
							sprintf( chname.setbufsize(512), "%s/CH%02d", (char *)d.pathname(), channel );
							if( stat( (char *)chname, &st ) != 0 ) {
								continue ;
							}
						}

						// add to list if not already there
						int found = 0 ;
						for( int i = 0; i<daylist.size(); i++  ) {
							if( daylist[i] == day ) {
								found=1 ;
								break;
							}
						}						
						if( !found )
							daylist.add( day );
					}
				}
			}
		}
	}
}

void disk_getdaylist_bydisk(const char *diskbase, array <int> & daylist, int channel)
{
	int pday ;
	int pos ;
	disk_listday_help( diskbase, daylist, channel, 0 ) ;
    daylist.sort();
}

// get full day list for playback
void disk_getdaylist(array <int> & daylist, int channel)
{
	disk_getdaylist_bydisk( "/var/dvr/disks", daylist, channel);
}

// unlock locked file overlaped with specified time range
int disk_unlockfile( dvrtime * begin, dvrtime * end )
{
    return 1;
}

struct t_oldfile {
	int oldestdate ;		// bcd date of oldest video file (may be smartlog file)
	int oldesttime ;		// bcd time of oldest video file
	string filename ;		// oldest file name
	int  lock ;				// to search lock file only ?
} ;	

// find oldest .264 files 
// parameter:
//      dir:   	directory to start search,
//      oldfile:to return oldest file result
//      dirlevel: search level  0=disk, 1=_HOST_, 2=YYYYMMDD, 3=CH00 (video files), 4=smartlog 
static int disk_olddestfile( char * tdir,  t_oldfile * oldfile, int dirlevel )
{
    char * pathname ;
    char * filename ;
    int l, d, t ;
    int res = 0 ;
    
    dir dfind(tdir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dirlevel <= 0 ) {
			if( dfind.isdir() ) {
				filename = dfind.filename();
				l = strlen( filename );
				if( *filename == '_' && *(filename+l-1) == '_' ) {
					res+=disk_olddestfile( dfind.pathname(), oldfile, 1 );
				}
				else if( strcasecmp( filename, "smartlog" )==0 ) {
					res+=disk_olddestfile( dfind.pathname(), oldfile, 4 );
				}
				else {
					res+=disk_olddestfile( dfind.pathname(), oldfile, 0 );
				}
			}
		}
		else if( dirlevel == 1 ) {
			if( dfind.isdir() ) {
				filename = dfind.filename();
				if( *filename == '2' ) {
					if( sscanf(filename, "%d", &d) == 1 ) {
						if( d>20000000 && d<oldfile->oldestdate ) {
							res += disk_olddestfile( dfind.pathname(), oldfile, 2 );
						}
					}
				}
			}
		}
		else if( dirlevel == 2 ) {
			if( dfind.isdir() ) {
				filename = dfind.filename();
				l = strlen( filename );
				if( l==4 && *filename == 'C' && *(filename+1) == 'H' ) {
					res+=disk_olddestfile( dfind.pathname(), oldfile, 3 );
				}
			}			
		}
		else if( dirlevel == 3 ) {
			if( dfind.isfile() ) {
				filename = dfind.filename();
				l = strlen(filename) ;
				if( l>24 && *filename=='C' && strcmp(&filename[l-4], ".266")==0 ) {    // is it a .264 file ?
					if( oldfile->lock || strstr(filename+20, "_L_")==NULL ) {
						if( sscanf( filename+5, "%8d%6d", &d, &t )==2 ) {
							if( d < oldfile->oldestdate || 
								( d == oldfile->oldestdate && t<oldfile->oldesttime ) ) 
							{
								oldfile->oldestdate = d ;
								oldfile->oldesttime = t ;
								oldfile->filename = dfind.pathname();
								res = 1 ;
							}
						}
					}
				}
			}
		}
		else if( dirlevel == 4 ) {
			if( dfind.isfile() ) {
				filename = dfind.filename();
				l = strlen(filename) ;
				if( l>14 && fnmatch("*_*_?.0??", filename,0)==0 ) {    // is it a smartlog file
					if( oldfile->lock || strstr(filename+9, "_L.")==NULL ) {
						filename = strstr( filename, "_2" ) ;
						if( filename ) {
							if( sscanf( filename+1, "%d", &d )==1 ) {
								if( d>20000000 && d<oldfile->oldestdate  )
								{
									oldfile->oldestdate = d ;
									oldfile->oldesttime = 500000 ;		// something big
									oldfile->filename = dfind.pathname();
									res = 1 ;
								}
							}
						}
					}
				} 
			}			
		}
	}
	return res ;
}

void disk_scan( char * disk, int *nlen, int *llen )
{
	dir dfind(disk);
    while( dfind.find() ) {
		if( dfind.isdir() ) {
			disk_scan( dfind.pathname(), nlen, llen );
		}
		else if( dfind.isfile() && fnmatch( "CH*.266", dfind.filename(), 0 )==0 ) {			// might be video file
			int flen = f264length( dfind.filename() );
			if( strstr( dfind.filename(), "_L_" )!=NULL ) {
				*llen+= flen ;
			}
			else {
				*nlen+= flen ;
			}
		}
	}
}

int disk_testwritable( char * dir ) 
{
    char testfilename[512] ;
    int t1 = 12345 ;
    int t2 = 54321 ;

    sprintf( testfilename, "%s/dvrdisktestfile", dir );

    FILE * ftest = fopen( testfilename, "w" );
    if( ftest!=NULL ) {
        fwrite( &t1, 1, sizeof(t1), ftest );
        fclose( ftest ) ;
    }
    //don't abuse this,system can hang:sync();
    ftest = fopen( testfilename, "r" );
    if( ftest!=NULL ) {
        fread( &t2, 1, sizeof(t2), ftest );
        fclose( ftest );
    }
    unlink( testfilename );
    return t1==t2 ;
}

int get_base_dir(char *base, int size, char *busname_dir)
{	
  strncpy(base, busname_dir, size);
  char *ptr = strrchr(base, '/');
  if (!ptr) return 1;
  *ptr = 0;

  return 0;
}


void formatflash(char *path)
{
	char base[256];
    char devname[60];
    char syscommand[80];  
    char filename[256];
    int ret;
    char* ptemp;   
    if(get_base_dir(base, sizeof(base), path))
		return;   
    ret=umount2(base,MNT_FORCE);   
    if(ret<0)
      return;
    ptemp=&base[17];
    sprintf(devname,"/dev/%s",ptemp);        
    sprintf(syscommand,"/mnt/nand/mkdosfs -F 32 %s",devname);
    dvr_log("syscommand:%s",syscommand);
    ret=system(syscommand); 
    if(ret>=0){
		sleep(5);
		mount(devname,base,"vfat",MS_MGC_VAL,"");
		sprintf(filename,"%s/diskid",base);
		FILE *fp=fopen(filename,"w");
		if(fp){
			fprintf(fp,"FLASH");
			fclose(fp);
		}
		mkdir(path,0777);	
    }    
    return;
}

int disk_capacitycheck(char *path)
{
	int disksize=0;  	
	int i;	
	for(i=0;i<3;++i){
	    disksize=disk_size(path);
	    if(disksize>=MIN_DISK_SIZE)
	      break;
//            sleep(2);
	}  
	if(disksize<MIN_DISK_SIZE){
	   return -1;
	}
	return 0;
}

void FormatFlashToFAT32(char* diskname)
{  
    char devname[60];
    int ret=0;
    pid_t format_id;
    char* ptemp;
    FILE *fp;    
    
    ret=umount2(diskname,MNT_FORCE);
    if(ret<0)
      return;    
    dvr_log("umounted:%s",diskname);
    
    ptemp=&diskname[17];
    sprintf(devname,"/dev/%s",ptemp);      
    format_id=fork();
    if(format_id==0){     
       execlp("/mnt/nand/mkdosfs", "mkdosfs", "-F","32", devname, NULL );  
       exit(0);
    } else if(format_id>0){    
        int status=-1;
        waitpid( format_id, &status, 0 ) ;   
        if( WIFEXITED(status) && WEXITSTATUS(status)==0 ) {
            ret=1 ;
        }	
    }    
    if(ret>=0){
	mount(devname,diskname,"vfat",MS_MGC_VAL,"");
	dvr_log("%s is formated to FAT32",devname);	 
    } else {
        dvr_log("Format Flash failed"); 
    }
}

#ifdef APP_PWZ5

struct pw_diskinfo pw_disk[3] ;

// backup disk support, ( if no primary disk being used, mount backup disk )
static int disk_pw_inittime ;

/*

 There are three methods of system recording on system page setup "One disk recording"
"Just in case recording"
"Auto back up"

"One disk recording":
System record in according to the setup for each cameras One the disk with signature file name "disk one"
If the "disk one" is not found for 5 minutes after power up, find disk with signature "disk two" to record onto, 
if "disk two" not found in 5 minutes then declare "no disk found"

In this recording mode, system always has a disk to record on unless both disk are dead

"Just In Case Recording"
If "disk two" is not found, record as "one disk recording" 

if "disk two is found" 
 
All cameras that are enable are recorded to "disk two"regardless the trigger recording 
setup on the camera as long as ignition signal is on When this mode is set up in system page, all enable camera 
are treat as continue recording and the trigger sources are EM trigger to create locked files

Copy locked files to "disk one" in background then change L files on "disk two" to N files so when "disk two" is full, 
oldest files are erased to give room to new files

If "disk one" is not found then continue until the "disk two is full of lock files

"Auto back up"
This mode is enable only when there are "disk one", "disk two" and  "disk three" in the system 
Disk three typical a 512G HDD or bigger

When ignition is off, after delay shut down timer time out, power on " disk three" and move N files from "disk two" to "disk three"
Then format "disk two"

*/ 
int disk_pw_recordmethod ;	// 0: "One disk Recording", 1: "Just in case recording", 2: "Auto back up"
static int disk_pw_curlogdisk = -1 ;
static int disk_pw_autobackup_run = 0 ;

// no: disk number,  DISK1 :0, DISK2: 1 ...
static int  disk_pw_diskdev( int no, char * dev )
{
	int l = 0 ;
	FILE * fdev ;
	char devfile[]="/var/dvr/disk1_device" ;
	
	devfile[13]= '1' + no ;
	fdev = fopen( devfile, "r");
	if( fdev ) {
		if( fscanf( fdev, " %99s", (dev) ) > 0 ) {
			l = strlen(dev);
		}
		fclose( fdev );
	}
	return l ;
}	

// disknum :  0=DISK1, 1=DISK2
// return 1 success
int  disk_pw_checkdiskspace( int disknum ) 
{
	int mounted = 0 ;
    struct statfs rstfs;
    struct statfs stfs;

    statfs("/var/dvr", &rstfs) ;
    
    if (statfs((char*)(pw_disk[disknum].disk), &stfs) == 0  && 
		memcmp( &(rstfs.f_fsid), &(stfs.f_fsid), sizeof(stfs.f_fsid) ) != 0 )
	{
		pw_disk[disknum].freespace = stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize); 
		pw_disk[disknum].totalspace = stfs.f_blocks/ ((1024 * 1024) / stfs.f_bsize); 
		pw_disk[disknum].reserved = (int) ( disk_mindiskspace_percent * pw_disk[disknum].totalspace / 100 ) ;
		if( pw_disk[disknum].totalspace > 500 ) {
			mounted = 1 ;
		}
	}
	pw_disk[disknum].mounted = mounted ;	
	return mounted ;
}

// disknum: disk number,  DISK1=0, DISK2=1 ...
// return 1 if mounted !
int disk_pw_mount( int disknum )
{
	string dev ;
	string diskpath ;

	if( disknum>2 || disk_pw_diskdev(disknum, dev.setbufsize(200) )<3 ) {
		// disk not ready
		return 0 ;
	}

	sprintf( diskpath.setbufsize(300), "/var/dvr/disks" );
	mkdir( diskpath, 0777 );

	sprintf( diskpath.setbufsize(300), "/var/dvr/disks/%s", (char *)dev );
	mkdir( diskpath, 0777);
	
	// sprintf(mountcmd.setbufsize(500), "mount -t vfat %s %s -o noatime,shortname=winnt", (char *)dev, (char *)diskpath );
	// system( mountcmd );

	string devpath ;
	sprintf( devpath.setbufsize(100), "/dev/%s", (char*)dev );
	if( mount( devpath, diskpath, "vfat", MS_NOATIME, "shortname=winnt" ) == 0 ) {
		dvr_log( "Mount %s to %s", (char *)dev, (char *)diskpath );
	}
	
	pw_disk[disknum].disk =  (char *)diskpath ;
	pw_disk[disknum].mounted =  0 ;
	pw_disk[disknum].totalspace =  1 ;
	pw_disk[disknum].freespace =  0 ;
	pw_disk[disknum].reserved =  0 ;
	pw_disk[disknum].full =  0 ;
	pw_disk[disknum].l_len =  1 ;
	pw_disk[disknum].n_len =  0 ;
	
	if( disk_pw_checkdiskspace( disknum ) ) {
		disk_diskfix( pw_disk[disknum].disk ) ;
		
		// try reopen recording files, since new disk mounted!
		rec_break();

		// try rescan disk N/L space
		disk_scanreq = 1 ;
		
		return 1 ;
	}
	return 0 ;
}

// mount disks
void disk_pw_checkmode()
{
	if( pw_disk[0].mounted == 0 ) {
		if( disk_pw_mount(0) ) {
			dvr_log("Disk1 mounted.");
		}
	}
	if( pw_disk[1].mounted == 0 ) {
		if( disk_pw_mount(1) ) {
			dvr_log("Disk2 mounted.");
		}
	}
	if( dio_curmode() == APPMODE_STANDBY && pw_disk[2].mounted == 0 ) {
		if( disk_pw_mount(2) ) {
			dvr_log("Disk3 mounted, start auto-backup!" );
		}
	}
}

/*
static char *diskmodename[] = {
	"One disk",
	"JIC",
	"Auto backup" 
};

void disk_pw_checkmode()
{
	string cmd ;
	string dev ;
	int t = time_gettick() ;
	if( disk_pw_recordmethod == 1  || disk_pw_recordmethod ==2 ) {		// Just In Case recording / Autobackup
		if( pw_disk[0].mounted == 0 ) {
			if( disk_pw_mount(0) ) {
				dvr_log("%s mode, disk1 mounted.", diskmodename[disk_pw_recordmethod] );
			}
		}
		if( pw_disk[1].mounted == 0 ) {
			if( disk_pw_mount(1) ) {
				dvr_log("%s mode, disk2 mounted.", diskmodename[disk_pw_recordmethod] );
			}
		}
		if( disk_pw_recordmethod == 2 ) { 		// Auto Backup
			if( pw_disk[2].mounted == 0 ) {
				if( disk_pw_mount(2) ) {
					dvr_log("%s mode, disk3 mounted.", diskmodename[disk_pw_recordmethod] );
				}
			}
		}
	}
	else {		// 0 or other: One disk recording method
		if( t-disk_pw_inittime <= 180000 ) {	// < 5 minutes, changed to 3 minutes as meeting with Quang, Nov 26, 2015
			// Try mount disk1 only
			if( pw_disk[0].mounted == 0 ) {
				if( disk_pw_mount(0) ) {
					dvr_log("One disk mode, disk1 mounted.");
					strcpy( g_diskwarning, "" );
				}
			}
		}
		else if( pw_disk[0].mounted == 0 && pw_disk[1].mounted == 0 ) {
			// Try mount disk2
			if( disk_pw_mount(1) ) {
				dvr_log("One disk mode, disk2 mounted.");
				strcpy( g_diskwarning, "" );
			}
		}
	}
}
*/

// return 1 success
int  disk_pw_deleteoldfile( char * disk, int lockfile ) 
{
    struct t_oldfile oldfile ;
    oldfile.oldestdate = 30000000 ;
    oldfile.oldesttime = 0;
    oldfile.lock = lockfile ;

	if( disk_olddestfile( disk, &oldfile, 0 ) ) {
		if( oldfile.filename.length()>10 ) {
			disk_removefile( (char *) oldfile.filename ) ;
			return 1 ;
		}
	}
	return 0 ;
}

void disk_pw_checkspace()
{
	int disk ;
	int diskfull ;

	for( disk=0; disk<3; disk++ ) {
		if( pw_disk[disk].mounted ) {
			if( pw_disk[disk].full == 0 || disk_scanreq ) {
				diskfull = 1 ;
				for( int retry=0; retry<20; retry++ ) {
					if( disk_pw_checkdiskspace( disk ) ) {
						if( pw_disk[disk].freespace < pw_disk[disk].reserved  ) {
							if( disk_pw_deleteoldfile( pw_disk[disk].disk, 0 )==0 ) {
								if( disk_allow_recycle_lfile || disk==2 ) {				// disk 2 , auto backup disk, always allow recycle _L_ files
									disk_pw_deleteoldfile( pw_disk[disk].disk, 1 );
								}
							}
							disk_scanreq = 1 ;
						}
						else {
							diskfull = 0 ;
							break ;
						}
					}
				}

				if( diskfull && pw_disk[disk].full == 0 ) {
					dvr_log("Disk %d space full!", disk+1) ;
					pw_disk[disk].full = 1 ;
				}

				// rescan for total L/N file length
				if( disk_scanreq ) {
					pw_disk[disk].l_len=0;
					pw_disk[disk].n_len=0;
					disk_scan( pw_disk[disk].disk, &(pw_disk[disk].n_len), &(pw_disk[disk].l_len) );
				}
				
			}
		}
	}
	
	disk_scanreq = 0 ;

}

// get .264 file list by day and disk number (for pwcontroller)
void disk_pw_listdaybydisk(array <f264name> & flist, struct dvrtime * day, int disk)
{
    char * rootfolder = (char *)"/var/dvr/disks" ;
	char date[10] ;
	if( disk>=0 && disk<3 ) {
		
printf("Req Disk: %d\n", disk );
		
		if( pw_disk[disk].mounted ) {
			rootfolder = (char *)pw_disk[disk].disk ;
			
printf("Req Root: %s\n", 		rootfolder );	
		}
		else {
printf("Req Not Mounted\n" );
			return ;
		}
	}
	sprintf(date, "%04d%02d%02d", 
                    day->year,
                    day->month,
                    day->day );
                    
printf("ReqDate: %s\n", date );
                    
    disk_list264files( rootfolder, flist, date, -1, 0 );
    flist.sort();
}

void disk_pw_init( config &dvrconfig )
{
	disk_pw_recordmethod=dvrconfig.getvalueint("system", "pw_recordmethod");
	disk_pw_inittime = time_gettick() ;
	
	// reset current logging disk
	disk_pw_curlogdisk = -1 ;
	
	archive_start();
}

void disk_pw_uninit()
{
	archive_stop();
}

#endif


// to rescan disk N/L file ration
void disk_rescan()
{
	disk_scanreq = 1 ;
}


// retrive base dir for recording
char * disk_getbasedisk( int locked )
{
	if( locked ) {
		if( pw_disk[0].mounted && !pw_disk[0].full ) {
			return (char *) (pw_disk[0].disk );
		}
		else if( pw_disk[1].mounted && !pw_disk[1].full ) {
			return (char *) (pw_disk[1].disk );
		}
	}
	else {
		if( pw_disk[1].mounted && !pw_disk[1].full ) {
			return (char *) (pw_disk[1].disk );
		}
		else if( pw_disk[0].mounted && !pw_disk[0].full ) {
			return (char *) (pw_disk[0].disk );
		}
	}
	
	return NULL ;
}

char * disk_getlogdisk()
{
	
	if( disk_pw_curlogdisk < 0 ) {
		int t = time_gettick() ;		
		if( pw_disk[0].mounted ) {
			disk_pw_curlogdisk = 0 ;		// use DISK1 as logging disk
		}
		else if( t-disk_pw_inittime > 120000  && pw_disk[1].mounted ) {		// use DISK2 as logging disk after 2 minutes
			disk_pw_curlogdisk = 1 ;		
		}
		if( disk_pw_curlogdisk>=0 ) {
			// write disk content to curdisk file, for glog to work
			if( strlen( (char *)disk_curdiskfile )>5 ) {
				FILE * fcurdiskfile = fopen((char *)disk_curdiskfile, "w") ;
				if( fcurdiskfile ) {
					fprintf( fcurdiskfile, "%s", (char *)(pw_disk[disk_pw_curlogdisk].disk) ) ;
					fclose( fcurdiskfile );
				}
			}
		}
	}
	
	if( disk_pw_curlogdisk < 0 ) {
		return NULL ;
	}
	else {
		return (char *)(pw_disk[disk_pw_curlogdisk].disk );
	}
}


// retrive base dir for recording
char * disk_getdisk( int disknum )
{
	if( disknum>=0 && disknum<=2 ) {
		if( pw_disk[disknum].mounted ) {
			return (char *) ( pw_disk[disknum].disk );
		}
	}
	return NULL ;
}

void disk_check()
{
#ifdef APP_PWZ5
	disk_pw_checkmode();
	disk_pw_checkspace();
#endif 
}

void disk_init(int check_only)
{
    string pcfg;
    int l;

    config dvrconfig(dvrconfigfile);
    
   
#ifdef TVS_APP_X

    // first time init
    if( check_only == 0 ) {	
		log_disk_timeout = dvrconfig.getvalueint("system", "logdisktimeout");
		if( log_disk_timeout==0 ) log_disk_timeout=100 ;
		log_disk_id = dvrconfig.getvalue("system", "logdiskid");
		g_log_disk="";
	}

#endif

    disk_base = dvrconfig.getvalue("system", "mountdir");
    if (disk_base.length() == 0) {
        disk_base = "/var/dvr/disks";
    }
    
    disk_mindiskspace_percent = 5.0 ;
    pcfg = dvrconfig.getvalue("system", "mindiskspace_percent");
    l = pcfg.length();
    if( l>0 ) {
		if (sscanf(pcfg.getstring(), "%f", &disk_mindiskspace_percent)!=1 ) {
			disk_mindiskspace_percent = 5.0 ;
		}
	}
    if( disk_mindiskspace_percent<1.0 ) disk_mindiskspace_percent = 1.0 ;
    else if( disk_mindiskspace_percent>20.0 ) disk_mindiskspace_percent = 20.0 ;
    
    disk_allow_recycle_lfile = dvrconfig.getvalueint("system", "recycle_l");

    disk_curdiskfile = dvrconfig.getvalue("system", "currentdisk");
    if( disk_curdiskfile.length()<2) {
        disk_curdiskfile="/var/dvr/dvrcurdisk" ;
    }
    
	// init pw disk info
	for( int x=0; x<3; x++ ) {
		pw_disk[x].freespace = 0 ;
		pw_disk[x].totalspace = 1 ;
		pw_disk[x].mounted = 0 ;
		pw_disk[x].disk = "" ;
		pw_disk[x].full = 0;
	}
	
#ifdef APP_PWZ5
	disk_pw_init( dvrconfig );
#endif 	
	
    if (!check_only) {
      // check disk
      disk_check();
      
      dvr_log("Disk initialized.");
    }
    
}

void disk_uninit(int check_only)
{
    disk_base="";
    
    if( strlen((char *)disk_curdiskfile )>0 ) {
		remove( (char *)disk_curdiskfile );
	}
    
#ifdef APP_PWZ5
	disk_pw_uninit();
#endif 

    if (!check_only)
      dvr_log("Disk uninitialized.");
    
}
