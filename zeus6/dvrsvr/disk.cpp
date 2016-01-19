
#include "dvr.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "dir.h"

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

static pthread_mutex_t disk_mutex;
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


inline void disk_lock()
{
    pthread_mutex_lock(&disk_mutex);
}

inline void disk_unlock()
{
    pthread_mutex_unlock(&disk_mutex);
}

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
void disk_rmdir(const char * tdir, int delete_dvrlog)
{
	dir d( tdir ) ;
	while( d.find() ) {
		if( d.isdir() ) {
			disk_rmdir(d.pathname(), delete_dvrlog );
		}
		else {
			if ( delete_dvrlog ||
				( strcmp(d.filename(), "dvrlog.txt")!=0) ) {
				remove(d.pathname());
			}
		}
	}
	d.close();
	rmdir( tdir );
}

// remove directory with no files
int disk_rmemptydir(char *tdir)
{
	int files = 0 ;
	dir d( tdir ) ;
	while( d.find() ) {
		if( d.isdir() ) {
			if( disk_rmemptydir(d.pathname()) == 0 ) {
				rmdir(d.pathname());
			}
			else {
				files++ ;
			}
		}
		else if( d.isfile() ) {
			files++ ;
		}
	}
	return files ;
}

// remove .264 files and related .k, .idx
static int disk_removefile( const char * file264 ) 
{
	int res = -1;
    string f264 ;
    char * fn ;
    char * extension ;
    f264 = file264 ;
    fn = (char *)f264 ;
    res = remove( fn ) ;
    if( res != 0 ) {
    	dvr_log( "Delete file failed: %s", file264 );
    	return res ;
    }
    int l = strlen( fn );
    extension = fn+l-4 ;
    if( strcmp(extension, ".266")==0 ) {
        strcpy( extension, ".k");
        remove( fn );
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
int  repairepartiallock(const char * filename) 
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

// find any 264 files
// return 1: found at least one .264 file
//        0: can't find any .264 file
int disk_find264(char *tdir)
{
	char *filename ;
	int l ;
	int files = 0 ;
	dir d( tdir ) ;
	while( d.find() ) {
		if( d.isfile() ) {
			filename = d.filename();
			l = strlen( filename );
            if (strcmp( filename+l-4, ".266") == 0) {
				return 1 ;
            }
		}
	}
	d.rewind();
	while( d.find() ) {
		if( d.isdir() ) {
			if( disk_find264( d.pathname() ) ) {
				return 1 ;
			}
		}
	}
	return 0 ;
}

// remove directories has no .264 files
void disk_rmempty264dir(char *tdir)
{
    if( disk_find264(tdir)==0 ) {
		disk_rmdir(tdir, 0);
    }
}

// repaire all .264 files under dir
void disk_repair264(char *tdir)
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
        else if( dfind.isfile() &&
                strstr(filename, ".266") ) 
        {
            if( strstr(filename, "_0_") ) {
                repairfile(pathname);
            }
            else if( strstr(filename, "_L_") ) {
                repairepartiallock(pathname) ;
            }
        }
    }
    disk_rmempty264dir(tdir);
}


// fix all "_0_" files & delete non-local video files
void disk_diskfix(char *tdir)
{
    char pattern[512];
    char * pathname ;
    char * filename ;
    dir dfind(tdir);
    // disable watchdog, so system don't get reset while repairing files
    dio_disablewatchdog();  

    while( dfind.find() ) {
        pathname = dfind.pathname();
        filename = dfind.filename();
        if( dfind.isdir() ) {
            if( fnmatch( "_*_", filename, 0 )==0 ){ // a root direct for DVR files
                sprintf( pattern, "_%s_", g_hostname);
                if( strcasecmp( pattern, filename )!=0 ) {
					dvr_log("%s is deleted",pathname);
					disk_rmdir( pathname, 1 );
                }
                else {
					// dvr_log("repair:%s",pathname);
                    disk_repair264(pathname);
                }
            }
        }
    }
    
    dio_enablewatchdog();
}

// get a list of .264 files, for play back list use
// dirlevel: 0 before _HOST_, 1: YYYYMMDD, 2: CH00, 3: video files
static void disk_list264files(const char *tdir, array <f264name> & flist, char * day, int channel, int dirlevel )
{
    char * dname ;
    int l ;

    dir d(tdir);
    while( d.find() ) {
		if( dirlevel <= 0 ) {
			if( d.isdir() ) {
				if( *(d.filename()) == '_' ) {
					// found _HOST_ pattern
					disk_list264files( d.pathname(), flist, day, channel, 1 );
				}
				else {
					disk_list264files( d.pathname(), flist, day, channel, 0 );
				}
			}
		}
		else if( dirlevel == 1 ) {		// YYYYMMDD
			if( d.isdir()  ) {
				// check day (YYYYMMDD)
				dname = d.filename();
				if( *dname == '2' && (day==NULL || strcmp( d.filename(), day )==0 ) ) {
					disk_list264files( d.pathname(), flist, day, channel, 2 );
				}
			}
		}
		else if( dirlevel == 2 ) {		// CH?? 
			if( d.isdir() ) {
				// check channel
				dname = d.filename();
				if( strncmp( dname, "CH0", 3 )==0 
					&& (channel<0 || dname[3] == ('0'+channel ) ) )
				{
					disk_list264files( d.pathname(), flist, day, channel, 3 );
				}
			}
		}
		else {		// level == 3
			if( d.isfile() ) {
				// check channel
				dname = d.filename();
				l = strlen(dname);
				if( strncmp( dname, "CH0", 3 )==0  
					&& l>4 
					&& strcmp( dname + l - 4, ".266")==0 ) 
				{
					f264name fn ;
					fn = d.pathname() ;
					flist.add(fn);
				}
			}
		}
	}
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
static void disk_listday_help(const char *tdir, array <int> & daylist, int dirlevel )
{
    char * dname ;
    int day;

    dir d(tdir);
    while( d.find() ) {
		if( dirlevel <= 0 ) {
			if( d.isdir() ) {
				if( *(d.filename()) == '_' ) {
					// found _HOST_ pattern
					disk_listday_help( d.pathname(), daylist, 1 );
				}
				else {
					disk_listday_help( d.pathname(), daylist, 0 );
				}
			}
		}
		else if( dirlevel == 1 ) {		// YYYYMMDD
			if( d.isdir() ) {
				// check day (YYYYMMDD)
				dname = d.filename();
				if( *dname == '2' ) {
					int day = atoi(dname);
					daylist.add( day );
				}
			}
		}
	}
}

void disk_getdaylist_bydisk(const char *diskbase, array <int> & daylist)
{
	int pday ;
	int pos ;
	disk_listday_help( diskbase, daylist, 0 ) ;
	
    daylist.sort();
    
    // remove repeated days
    pday = 0 ;
    for( pos=0; pos<daylist.size();  ) {
		if( daylist[pos] == pday ) {
			daylist.remove(pos);
		}
		else {
			pos++ ;
		}
	}
}

// get full day list for playback
void disk_getdaylist(array <int> & daylist)
{
	disk_getdaylist_bydisk( "/var/dvr/disks", daylist);
}

// unlock locked file overlaped with specified time range
int disk_unlockfile( dvrtime * begin, dvrtime * end )
{
    return 1;
}

// find oldest .264 files 
// parameter:
//      dir:   directory to start search,
//   oldesttime: keep oldest file time searched
//   oldestfilename:  oldest file name searched
//     lock:   locked file searching allowed. ( if(lock==0) don't search locked file)
int disk_olddestfile( char * tdir,  struct dvrtime *oldesttime, string * oldestfilename, int lock )
{
    char * pathname ;
    char * filename ;
    int l ;
    struct dvrtime dvrt ;
    dir dfind(tdir);
    int res = 0 ;
    time_dvrtime_init(&dvrt, 2000);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
            res+=disk_olddestfile( pathname, oldesttime, oldestfilename, lock );
        }
        if( dfind.isfile() ) {
            filename = dfind.filename();
            l = strlen(filename) ;
            if( l>4 && strcmp(&filename[l-4], ".266")==0 ) {    // is it a .264 file ?
				if( f264length( filename ) > 0 ) {				// only for closed file!
					if( lock || strstr(filename, "_L_")==NULL ) {
						f264time(filename, &dvrt);
						if( dvrt < *oldesttime ) {
							*oldesttime = dvrt ;
							*oldestfilename = pathname ;
							res=1 ;
						}
					}
				}
            } 
        }
    }
    
    return res ;
}


// To renew a file list on current disk
int disk_renew( char * filename )
{
    return 0;
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
static int disk_pw_recordmethod ;	// 0: "One disk Recording", 1: "Just in case recording", 2: "Auto back up"

/*
static float disk_pw_disk1_usage ;
static int disk_pw_disk1_freespace ;
static int disk_pw_disk1_full = 0 ;
static int disk_pw_disk1_mounted = 0 ;
static string disk_pw_disk1 ;
static string disk_pw_disk1_base ;

static float disk_pw_disk2_usage ;
static int disk_pw_disk2_freespace ;
static int disk_pw_disk2_full = 0 ;
static int disk_pw_disk2_mounted = 0 ;
static string disk_pw_disk2 ;
static string disk_pw_disk2_base ;

static float disk_pw_disk3_usage ;
static int disk_pw_disk3_freespace ;
static int disk_pw_disk3_mounted = 0 ;
static int disk_pw_disk3_full = 0 ;
static string disk_pw_disk3 ;
static string disk_pw_disk3_base ;
*/

struct pw_diskinfo {
	int freespace ;		// free space in MB
	int totalspace ;	// total space in MB
	float freeratio ;	// freespace ratio
	int lowspace ;		// 
	int mounted ;		// boolean
	string disk ;		// disk mount point
	string rec_base ;	// video recording base dir   (disk + _HOST_)
} pw_disk[3] ;

static int disk_pw_curlogdisk = -1 ;
static int disk_pw_autobackup_run = 0 ;
static pid_t disk_arch_process1 = (pid_t)0 ;
static pid_t disk_arch_process2 = (pid_t)0 ;

// no: disk number,  DISK1 :0, DISK2: 1 ...
static int  disk_pw_diskdev( int no, char * dev )
{
	int l = 0 ;
	string devfile ;
	FILE * fdev ;
	sprintf( devfile.setbufsize(200), "/var/dvr/disk%d_device", no+1 );
	fdev = fopen( devfile, "r");
	if( fdev ) {
		if( fscanf( fdev, " %99s", dev )>0 ) {
			l = strlen(dev);
		}
		fclose( fdev );
	}
	return l ;
}	

pid_t disk_pw_runbackup(char * src, char * dest, int type, int mode )
{
	pid_t child ;
	child = fork();
	if(child==0){
		static char * bgarch = "/davinci/dvr/bgarch" ;
		// child?
		char atype[20] , amode[20] ;
		sprintf( atype, "%d", type );
		sprintf( amode, "%d", mode );
		execl( bgarch, bgarch, src, dest, atype, amode, NULL ) ;
	}
	else if( child>0 ) {
		return child ;
	}
	return 0 ;
}

// disknum :  0=DISK1, 1=DISK2
// return 0 success
int  disk_pw_checkdiskspace( int disknum ) 
{
    struct statfs stfs;
    if (statfs((char*)(pw_disk[disknum].disk), &stfs) == 0) {
		pw_disk[disknum].freespace = stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize); 
		pw_disk[disknum].totalspace = stfs.f_blocks/ ((1024 * 1024) / stfs.f_bsize); 
		pw_disk[disknum].freeratio = ((float)stfs.f_bavail) * 100.0 / ((float)stfs.f_blocks) ;
		pw_disk[disknum].mounted = 1 ;
		return 1 ;
	}
	else {
		pw_disk[disknum].freespace = 0 ; 
		pw_disk[disknum].totalspace = 1 ;
		pw_disk[disknum].freeratio = 0.0 ;
		pw_disk[disknum].mounted = 0 ;
		return 0 ;
	}
}

// disknum: disk number,  DISK1=0, DISK2=1 ...
// return 1 if mounted !
int disk_pw_mount( int disknum )
{
	string dev ;
	string diskpath ;
	string diskbasepath ;
	string mountcmd ;

	if( disk_pw_diskdev(disknum, dev.setbufsize(200) )<5 ) {
		// disk not ready
		return 0 ;
	}

	sprintf( diskpath.setbufsize(300), "/var/dvr/disks" );
	mkdir( diskpath, 0777 );

	sprintf( diskpath.setbufsize(300), "/var/dvr/disks/%s", (char *)dev );
	mkdir( diskpath, 0777);
	
	sprintf(mountcmd.setbufsize(500), "mount /dev/%s %s -o shortname=winnt", (char *)dev, (char *)diskpath );
	system( mountcmd );
	
	sprintf(diskbasepath.setbufsize(500), "%s/_%s_", (char *)diskpath, g_hostname );
	mkdir( diskbasepath, 0777);

	pw_disk[disknum].disk =  (char *)diskpath ;
	pw_disk[disknum].rec_base = (char *)diskbasepath ;
	
	if( disk_pw_checkdiskspace( disknum ) ) {
		disk_diskfix(diskbasepath) ;
		return 1 ;
	}
	return 0 ;
}

void disk_pw_checkmode()
{
	string cmd ;
	string dev ;
	int t = time_gettick() ;
	if( disk_pw_recordmethod == 1 ) {		// Just In Case recording
		if( pw_disk[0].mounted == 0 ) {
			if( disk_pw_mount(0) ) {
				dvr_log("JIC mode, disk1 mounted.");
				strcpy( g_diskwarning, "" );
			}
		}
		if( pw_disk[1].mounted == 0 ) {
			if( disk_pw_mount(1) ) {
				dvr_log("JIC mode, disk2 mounted.");
				strcpy( g_diskwarning, "" );
			}
		}
		if( disk_arch_process1 == 0 && pw_disk[0].mounted  && pw_disk[1].mounted ) {
			// start archive process
			// ...    run bgarch disk2 disk1 2 1    ( lock only, unlock _L_ )
			disk_arch_process1 = disk_pw_runbackup( pw_disk[1].disk, pw_disk[0].disk, 2, 1 );

			// force continue recording
			extern int rec_trigger ;
			rec_trigger = 1;
			dvr_log("JIC mode, continue recording all video.");
		}		
		
		if( disk_pw_curlogdisk < 0 ) {
			if( pw_disk[0].mounted ) {
				disk_pw_curlogdisk = 0 ;		// use DISK1 as logging disk
			}
			else if( t-disk_pw_inittime > 180000  && pw_disk[1].mounted ) {		// use DISK2 as logging disk after 3 minutes
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
	}
	else if( disk_pw_recordmethod == 2 ) { 		// Auto Backup
		if( dio_runmode() ) {
			// would run as Just In Case recording
			if( pw_disk[0].mounted == 0 ) {
				if( disk_pw_mount(0) ) {
					dvr_log("Auto Backup mode, disk1 mounted.");
					strcpy( g_diskwarning, "" );
				}
			}
			if( pw_disk[1].mounted == 0 ) {
				if( disk_pw_mount(1) ) {
					dvr_log("Auto Backup mode, disk2 mounted.");
					strcpy( g_diskwarning, "" );
				}
			}
			if( disk_arch_process1 == 0 && pw_disk[0].mounted  && pw_disk[1].mounted ) {
				// start archive process
				// ...    run bgarch disk2 disk1 2 1    ( lock only, unlock _L_ )
				disk_arch_process1 = disk_pw_runbackup( pw_disk[1].disk, pw_disk[0].disk, 2, 1 );

				// force continue recording
				extern int rec_trigger ;
				rec_trigger = 1;
				dvr_log("Auto Backup mode, continue recording all video.");
			}	
		}
		else {
			if( disk_arch_process2 == 0 && pw_disk[1].mounted ) {		
				// do copy only when disk 2 available
				// disk2 mounted, so mount disk3 as auto back disk
				if( pw_disk[2].mounted == 0 ) {
					if( disk_pw_mount(2) ) {
						dvr_log("Auto Backup mode, disk3 mounted for auto backup.");
						// start auto backup process
						// ...    run bgarch disk2 disk3 1 2    ( N file only , delete after copy )
						disk_arch_process2 = disk_pw_runbackup( pw_disk[1].disk, pw_disk[2].disk, 1, 2 );
					}
				}
			}
		}

		if( disk_pw_curlogdisk < 0 ) {
			if( pw_disk[0].mounted ) {
				disk_pw_curlogdisk = 0 ;		// use DISK1 as logging disk
			}
			else if( t-disk_pw_inittime > 180000  && pw_disk[1].mounted ) {		// use DISK2 as logging disk after 3 minutes
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
			// Try mount disk1 or disk2
			if( pw_disk[0].mounted == 0 ) {
				if( disk_pw_mount(0) ) {
					dvr_log("One disk mode, disk1 mounted.");
					strcpy( g_diskwarning, "" );
				}
			}
			else if( pw_disk[1].mounted == 0 ) {
				if( disk_pw_mount(1) ) {
					dvr_log("One disk mode, disk2 mounted.");
					strcpy( g_diskwarning, "" );
				}
			}
		}
		
		if( disk_pw_curlogdisk < 0 ) {
			if( pw_disk[0].mounted ) {
				disk_pw_curlogdisk = 0 ;		// use DISK1 as logging disk
			}
			else if( pw_disk[1].mounted ) {		// use DISK2 as logging disk 
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

	}

}

// return 1 success
int  disk_pw_deleteoldfile( char * disk, int lockfile ) 
{
	dvrtime oldesttime ;
	time_dvrtime_init(&oldesttime, 2099);
    
	// build video file list
	string oldestfilename ;
	if( disk_olddestfile( disk, &oldesttime, &oldestfilename, lockfile ) ) {
		if( oldestfilename.length()>1 ) {
			int res = (disk_removefile( (char *) oldestfilename ) == 0) ; 
			if( res ) {
				dvr_log( "Recycled file: %s", (char *) oldestfilename  );
				disk_rmempty264dir( disk );
			}
			else {
				dvr_log( "Recycling file failed: %s",  (char *) oldestfilename  );
			}
			return res ;
		}
	}
	return 0 ;
}

void disk_pw_checkspace()
{
	int res ;
	int disk ;
	int retry ;
	int lowspace ;
	int diskav = 0 ;
	int allgood = 1 ;
	for( disk=0; disk<3; disk++ ) {
		if( pw_disk[disk].mounted ) {
			diskav = 1 ;
			lowspace = 1 ;
			for( retry=0; retry<10; retry++ ) {		
				if( disk_pw_checkdiskspace( disk ) ) {
					if( pw_disk[disk].freeratio < disk_mindiskspace_percent ) {
						if( disk_pw_deleteoldfile( pw_disk[disk].disk, 0 )==0 ) {
							if( disk_allow_recycle_lfile ) {
								disk_pw_deleteoldfile( pw_disk[disk].disk, 1 );
							}
						}
					}
					else {
						lowspace = 0 ;
					}
				}
			}

			if( lowspace != pw_disk[disk].lowspace ) {
				pw_disk[disk].lowspace = lowspace ;
				if( lowspace ) {
					dvr_log("Disk %d space full!", disk+1) ;
					sprintf( g_diskwarning, "Not enough space on disk %d !", disk+1 ); 
				}
			}
			if( lowspace ) {
				allgood = 0 ;
			}

		}
	}
	if( diskav && allgood ) {
		// remove disk warning message
		g_diskwarning[0]=0 ;
	}
}

void disk_pw_init( config &dvrconfig )
{
	disk_pw_recordmethod=dvrconfig.getvalueint("system", "pw_recordmethod");
	disk_pw_inittime = time_gettick() ;
	
	strcpy( g_diskwarning, "No video storage!" );
	
	// reset current logging disk
	disk_pw_curlogdisk = -1 ;

	if( disk_arch_process1 ) {
		kill( disk_arch_process1, SIGTERM ) ;
		disk_arch_process1 = (pid_t)0 ;
	}
	if( disk_arch_process2 ) {
		kill( disk_arch_process2, SIGTERM ) ;
		disk_arch_process2 = (pid_t)0 ;
	}
}

void disk_pw_uninit()
{
	if( disk_arch_process1 ) {
		kill( disk_arch_process1, SIGTERM ) ;
		disk_arch_process1 = (pid_t)0 ;
	}
	if( disk_arch_process2 ) {
		kill( disk_arch_process2, SIGTERM ) ;
		disk_arch_process2 = (pid_t)0 ;
	}
}

#endif

// retrive base dir for recording
char * disk_getbasedir( int locked )
{
	if( locked ) {
		if( pw_disk[0].mounted ) {
			if( !pw_disk[0].lowspace ) {
				return (char *) (pw_disk[0].rec_base );
			}
		}
		else if( pw_disk[1].mounted ) {
			if( !pw_disk[1].lowspace ) {
				return (char *) (pw_disk[1].rec_base );
			}
		}
	}
	else {
		if( pw_disk[1].mounted ) {
			if( !pw_disk[1].lowspace ) {
				return (char *) (pw_disk[1].rec_base );
			}
		}
		else if( pw_disk[0].mounted ) {
			if( !pw_disk[1].lowspace ) {
				return (char *) (pw_disk[0].rec_base );
			}
		}
	}
	
	return NULL ;
}

char * disk_getlogdir()
{
	printf("Curlogdisk %d\n", disk_pw_curlogdisk );
	if( disk_pw_curlogdisk < 0 ) {
		return NULL ;
	}
	else {
		return (char *)(pw_disk[disk_pw_curlogdisk].rec_base );
	}
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
    
#ifdef APP_PWZ5
	disk_pw_init( dvrconfig );
#endif 
    
#ifdef TVS_APP_X

    // first time init
    if( check_only == 0 ) {	
		log_disk_timeout = dvrconfig.getvalueint("system", "logdisktimeout");
		if( log_disk_timeout==0 ) log_disk_timeout=100 ;
		log_disk_id = dvrconfig.getvalue("system", "logdiskid");
		g_log_disk="";
	}

#endif

    // initial mutex
    disk_mutex = mutex_init ;
    
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
		pw_disk[x].totalspace = 0 ;
		pw_disk[x].mounted = 0 ;
		pw_disk[x].disk = "" ;
		pw_disk[x].rec_base = "" ;
		pw_disk[x].lowspace = 0;
	}
	
    if (!check_only) {
      // check disk
      disk_check();
      
      dvr_log("Disk initialized.");
    }
    
}

void disk_uninit(int check_only)
{
	//sync();
    
    disk_lock();
    
    disk_base="";
    
    if( strlen((char *)disk_curdiskfile )>0 ) {
		remove( (char *)disk_curdiskfile );
	}
    
    disk_unlock();
    
#ifdef APP_PWZ5
	disk_pw_uninit();
#endif 
    
    pthread_mutex_destroy(&disk_mutex);

    if (!check_only)
      dvr_log("Disk uninitialized.");
    
}
