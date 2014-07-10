/* --- Changes ---
 * 09/11/2009 by Harrison
 *   1. Added nodiskcheck
 *      to support temporary disk unmount for tab102
 *
 * 10/27/2009 by Harrison
 *   1. Added Hybrid disk support
 *
 * 11/11/2009 by Harrison
 *   1. Added Multidisk upport
 *
 * 11/18/2009 by Harrison
 *   1. Bug fix: delete dvrlog file when diskname changes
 *
 */

#include "dvr.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define MIN_HD_SIZE 150000
enum {HD_UNKNOWN, HD_SIMPLE, HD_HYBRID, HD_MULTIPLE};
enum {DT_SATA, DT_FLASH,DT_UNGET};
#define MAX_DISK 16

#define DEBUG_HBD 0
#define DVR_NETWORK     (0x10)

#define MIN_PARTITION_SIZE 2048

#define MAX_FORMAT_NUM   3
#define MAX_SCAN_FAIL    120
#define MAX_UNWRITABLE_NUM  60
int g_nodiskcheck ;

static pthread_mutex_t disk_mutex;
static int disk_minfreespace;    // minimum free space for current disk, in Magabytes

extern void writeDebug(char *fmt, ...);
int copy_files(char *from_root, 
	       char  *dest_root,
	       char *dest_dir,
	       char *servername);
void sync_dvrlog_for_hybrid(char *disk_a, char *disk_b);
void sync_smartlog_for_hybrid(char *disk_a, char *disk_b, int check_stop);
int copy_file_to_path(char *from_fullname, char *to_path,
		      int forcewrite, int check_stop);
void copydvrlogfrommemory(char* pathname);
void copydvrlogtomemory(char* pathname);

/* for hybrid playback support */
int sata_diskid = -1;
int flash_diskid = -1;
static int isHybridCopying=0;
static int mount_nodisk=0;
static int isUsingHbd=0;
static int firstTimeHbd=1;
int gHBD_Recording=0;
int recordingDisk = -1;
int lastturnontime=0;
int diskwritableNum=0;
int partitionperdisk=4;
int numberofdiskused=2;
int numberoftotalpartitions=0;
int partitioncheckdone=0;
clock_t diskcheckstartclock;
long ticks_per_sec;
long max_clock_t;
struct tms tms;

void disk_scanalldisk(int partial);
void formatflash(char *path);
struct disk_day_info {
    int bcdday ;                    // available day in BCD. ex 20090302
    int nfiletime ;                 // earliest _N_ file time in BCD. 133222
    int nfilelen ;                  // total length of _N_ files
    int lfiletime ;                 // earliest lfile time in BCD. 133222
    int lfilelen ;                  // total length of _L_ files
    int nfiletime_l ;               // latest _N_ file time in BCD. 133222
    int lfiletime_l ;               // latest lfile time in BCD. 133222
} ;

struct day_time {
  int day;
  int time;
};

class disk_info {
    public:    
        dev_t  dev ;                        // disk device id
        int    mark ;
        int    type;
        int    id;
	int    ndisk;
	//adding for multiple partitions
	int    status;   //status= 0 for good  1 for bad
	int    scanfail;   //if scan failed, it will be increased by 1
	int    notwritable; //not writable times
	int    formated;   //the number of which had been formatted
	int    lastrecorded;
	
	//adding ends
        string basedir ;
	

        array  <disk_day_info>  daylist ;
        
        disk_info() {
            dev=0;
            basedir="" ;
            mark=0;
	    type = 0;
	    id = 0;
	    ndisk = 0;
            daylist.empty();
        }
        
        disk_info & operator = ( disk_info & di ) {
            dev     = di.dev ;
            mark    = di.mark ;
            basedir = di.basedir ;
            daylist = di.daylist ;
	    type = di.type;
	    id = di.id;
	    ndisk = di.ndisk;
            return *this ;
        }
} ;

static array <disk_info> disk_disklist ;   // recording disk list
static string disk_base;                // recording disk base dir
static string disk_curdiskfile;         // record current disk
int   turndiskon_power=0;

class dir_find {
    protected:
        DIR * m_pdir ;
        struct dirent * m_pent ;
        struct dirent m_entry ;
        char m_dirname[PATH_MAX] ;
        char m_pathname[PATH_MAX] ;
        struct stat m_statbuf ;
    public:
        dir_find() {
            m_pdir=NULL ;
        }
        // close dir handle
        void close() {
            if( m_pdir ) {
                closedir( m_pdir );
                m_pdir=NULL ;
            }
        }
        // open an dir for reading
        void open( const char * path ) {
            int l ;
            close();
            m_pent=NULL ;
            strncpy( m_dirname, path, sizeof(m_dirname) );
            l=strlen( m_dirname ) ;
            if( l>0 ) {
                if( m_dirname[l-1]!='/' ) {
                    m_dirname[l]='/' ;
                    m_dirname[l+1]='\0';
                }
            }
            m_pdir = opendir(m_dirname);
        }
        ~dir_find() {
            close();
        }
        dir_find( const char * path ) {
            m_pdir=NULL;
            open( path );
        }
        int isopen(){
            return m_pdir!=NULL ;
        }
        // find directory.
        // return 1: success
        //        0: end of file. (or error)
        int find() {
            struct stat findstat ;
            if( m_pdir ) {
                while( readdir_r( m_pdir, &m_entry, &m_pent)==0  ) {
                    if( m_pent==NULL ) {
                        break;
                    }
                    if( (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='\0') || 
                       (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='.' && m_pent->d_name[2]=='\0') ) {
                           continue ;
                       }
                    if( m_pent->d_type == DT_UNKNOWN ) { // d_type not available
                        strcpy( m_pathname, m_dirname );
                        strcat( m_pathname, m_pent->d_name );
                        if( stat( m_pathname, &findstat )==0 ) {
                            if( S_ISDIR(findstat.st_mode) ) {
                                m_pent->d_type = DT_DIR ;
                            }
                            else if( S_ISREG(findstat.st_mode) ) {
                                m_pent->d_type = DT_REG ;
                            }
                        }
                    }
                    return 1 ;
                }
            }
            m_pent=NULL ;
            return 0 ;
        }
        char * pathname()  {
            if(m_pent) {
                strcpy( m_pathname, m_dirname );
                strcat( m_pathname, m_pent->d_name );
                return (m_pathname) ;
            }
            return NULL ;
        }
        char * filename() {
            if(m_pent) {
                return (m_pent->d_name) ;
            }
            return NULL ;
        }
        
        // check if found a dir
        int    isdir() {
            if( m_pent ) {
                return (m_pent->d_type == DT_DIR) ;
            }
            else {
                return 0;
            }
        }
        
        // check if found a regular file
        int    isfile(){
            if( m_pent ) {
                return (m_pent->d_type == DT_REG) ;
            }
            else {
                return 0;
            }
        }
        
        // return file stat
        struct stat * filestat(){
            char * path = pathname();
            if( path ) {
                if( stat( path, &m_statbuf )==0 ) {
                    return &m_statbuf ;
                }
            }
            return NULL ;
        }
};


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
    char *base1;
    
    basename = (char *)fullpath;
    while ((base1 = strchr(basename, '/')) != NULL) {
        basename = base1 + 1;
    }
    return basename;
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
    return -1;
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

/*
 void disk_build264filelist(char *dir, strarray * flist)
  {
      char * pathname ;
      int l ;
      dir_find dir264(dir) ;
      while( dir264.find() ) {
          pathname = dir264.pathname();
          if( dir264.isdir() ) {
              disk_build264filelist( pathname, flist );
          }
          if( dir264.isfile() ) {
              l = strlen(pathname) ;
              if( l>4 && 
                 strcmp(&pathname[l-4], ".264")==0 &&	// is it a .264 file?
                 strstr(dir264.filename(), "_0_")==NULL ) {
                     flist->add(new string(pathname));
                 }
          }
      }
      flist->sort();
  }
 */

/*
 void disk_builddeletefilelist(char *dir, strarray * flist)
  {
      char * pathname ;
      char * filename ;
      int l ;
      struct dvrtime now ;
      struct dvrtime filetime ;
      dir_find dfind(dir);
      time_dvrtime_now(&now);
      while( dfind.find() ) {
          pathname = dfind.pathname();
          filename = dfind.filename();
          if( dfind.isdir() ) {
              disk_builddeletefilelist( pathname, flist );
          }
          if( dfind.isfile() ) {
              l = strlen(filename) ;
              if( l>4 && 
                 strcmp(&filename[l-4], ".264")==0 ) {	// is it a .264 file?
                     if( strstr( filename, "_L_")!=NULL ) {		// for locked files
                         f264time(filename, &filetime);
                         if( time_dvrtime_diff( &now, &filetime)>(60*60*24*100) ) {		// 100 days old locked file
                             flist->add(new string(pathname));
                         }
                     }
                     else {
                         flist->add(new string(pathname));
                     }
                 }
          }
      }
  }
 */

// remove all files and directory
void disk_rmdir(const char *dir, int delete_dvrlog)
{
    char * pathname ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
	  disk_rmdir( pathname, delete_dvrlog );
        }
        else {
	  char *filename = dfind.filename();
	  if (delete_dvrlog ||
	      (filename && strcmp(filename, "dvrlog.txt"))) {
	    remove(pathname);
	  }
        }
    }
    rmdir(dir);
}

// remove directory with no files
int disk_rmemptydir(char *dir)
{
    int files = 0 ;
    char * pathname ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
            if( disk_rmemptydir( pathname ) ) {
                files=1;
                break;
            }
        }
        else {
            files=1;
            break;
        }
    }
    if( files==0 ) {
        rmdir(dir);
    }
    return files ;
}

// remove .264 files and related .k, .idx

static void disk_removefile( const char * file264 ) 
{
    string f264 ;
    char * extension ;
    f264 = file264 ;
    remove( file264 );
    extension=strstr( f264.getstring(), ".266" );
    if( extension ) {
        strcpy( extension, ".k");
        if(remove( f264.getstring() )<0){
	    char* strp=strstr(f264.getstring(),"_N_");
	    if(strp){
	        *(strp+1)='L';
	    } else {
	       strp=strstr(f264.getstring(),"_L_");   
	       if(strp){
		 *(strp+1)='N'; 
	       }
	    }
	    if(strp){
	       remove(f264.getstring());
	     //  dvr_log("file:%s is deleted",f264.getstring());
	    }	  	  
	}
    }
}

// return 0, no changes
// return 1, renamed
// return 2, deleted
static int repairfile(const char *filename)
{
    dvrfile vfile;
    
   // printf("%s will be repaired\n",filename);
    if (vfile.open(filename, "r+") == 0) {	// can't open it
        dvr_log("Repair file open error: %s is deleted", filename);
        disk_removefile( filename );		// try delete it.
        return 0;
    }
   // printf("%s is opened\n",filename);
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
int disk_find264(char *dir)
{
    int res=0 ;
    int l ;
    char * pathname ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
            if( disk_find264( pathname ) ) {
                res=1 ;
                break;
            }
        }
        else if( dfind.isfile() ) {
            l = strlen(pathname);
            if (strcmp(&pathname[l - 4], ".266") == 0) {
                res=1 ;
                break;
            }
        }
    }
    return res ;
}

// remove directories has no .264 files
void disk_rmempty264dir(char *dir)
{
    if( disk_find264(dir)==0 ) {
      disk_rmdir(dir, 0);
    }
}

// repaire all .264 files under dir
void disk_repair264(char *dir)
{
    char * pathname ;
    char * filename ;
    dir_find dfind(dir);
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
	      //  dvr_log("repair file:%s",pathname);
                repairfile(pathname);
            }
            else if( strstr(filename, "_L_") ) {
	      //  dvr_log("repair partiallock:%s",pathname);
                repairepartiallock(pathname) ;
            }
        }
    }
    disk_rmempty264dir(dir);
}


// fix all "_0_" files & delete non-local video files
void disk_diskfix(char *dir)
{
    char pattern[512];
    char * pathname ;
    char * filename ;
    dir_find dfind(dir);
    // disable watchdog, so system don't get reset while repairing files
    dio_disablewatchdog();  

    while( dfind.find() ) {
        pathname = dfind.pathname();
        filename = dfind.filename();
        if( dfind.isdir() ) {
            if( fnmatch( "_*_", filename, 0 )==0 ){ // a root direct for DVR files
                sprintf( pattern, "_%s_", g_hostname);
                if( strcasecmp( pattern, filename )!=0 ) {
                  dvr_log("%s is delteted",pathname);
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

// get a list of .264 files, for play back use
static void disk_list264files(char *dir, array <f264name> & flist)
{
    char * pathname ;
    int l ;
    int i;
    int mfind=-1;
    dvrtime mfiletime;
    dvrtime mLtime;
    f264name p;
    dir_find dfind(dir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
            disk_list264files( pathname, flist );
        }
        if( dfind.isfile() ) {
            l = strlen(pathname) ;
            if( l>4 && 
               strcmp(&pathname[l-4], ".266")==0 && 
               strstr(pathname, "_0_")==NULL ) {	// is it a .264 file?
                   p=pathname ;
		   f264time(pathname,&mfiletime);
		   mfind=-1;
		   for(i=0;i<flist.size();++i){
		      f264time(flist[i].getstring(),&mLtime);
		      if(time_dvrtime_diff(&mfiletime,&mLtime)==0){
			mfind=1;
			break;
		      }
		   }
		   if(mfind<0)
                     flist.add(p);
               }
        }
    }
    
}

// get .264 file list by day and channel
void disk_listday(array <f264name> & flist, struct dvrtime * day, int channel)
{
    int disk;
    char root264[512];
    int i, f ;
    int bcdday ;
    
    bcdday = day->year * 10000 + day->month*100 + day->day ;

    flist.expand(200);
    for( disk=0; disk<disk_disklist.size(); disk++) {
      /* In case of hybrid disk,
       * don't play from Flash disk if Sata disk is available
       */
#if 0      
        if ((disk == flash_diskid) && (sata_diskid >= 0)) {
	   continue;
        }
#endif

        f=0; 
        for( i=0; i<disk_disklist[disk].daylist.size(); i++ ) {
            if( bcdday == disk_disklist[disk].daylist[i].bcdday ) {
                f=1;
                break;
            }
        }
        if( f ) {
            sprintf(root264, "%s/%04d%02d%02d/CH%02d", 
                    disk_disklist[disk].basedir.getstring(),// disk root264
                    day->year,
                    day->month,
                    day->day,
                    (int) channel );
            disk_list264files(root264, flist);
        }
    }
    flist.sort();
}

// get list of day with available .264 files
void disk_getdaylist(array <int> & daylist)
{
    int disk;
    int di;
    int i;
    int day ;
    int dup ;
    struct timeval time0;
    struct timeval time1;	  
    int diff=0;
    int mFind=0;
  ///////////////////////////////////////////////////////////////////////     
  //  dvr_log("disk size=%d",disk_disklist.size());  
    if(disk_disklist.size()==1){
      FILE *fp;
      fp=fopen("/var/dvr/backupdisk","r");
      if(fp){
	  fclose(fp);
	  gettimeofday(&time0, NULL);
	  diff=time0.tv_sec-lastturnontime;
          if(diff>130){
	      lastturnontime=time0.tv_sec;
	      mFind=-1;
	      turndiskon_power=1;
	      powerNum=380;	 	  
	      sleep(5);
	      dio_hdpower(1);	  
	      gettimeofday(&time0, NULL);
	      while(1){	  
		sleep(3);
		gettimeofday(&time1, NULL);
		if(time1.tv_sec-time0.tv_sec>10)
		  break;
	      }
	      while(1){
		  sleep(3); 
		  disk_scanalldisk(0);
		  if(disk_disklist.size()==2){
		    mFind=1;
		    break;
		  }	   
		  gettimeofday(&time1, NULL);
		  if(time1.tv_sec-time0.tv_sec>60)
		    break; 	      
	      }
    #if 0	  
	      if(mFind<0){
		  dio_hdpower(0);
		  gettimeofday(&time0, NULL);
		  while(1){	  
		    sleep(3);
		    gettimeofday(&time1, NULL);
		    if(time1.tv_sec-time0.tv_sec>6)
		      break;
		  }	      
		  dio_hdpower(1);	
		  gettimeofday(&time0, NULL);
		  while(1){	  
		    sleep(3);
		    gettimeofday(&time1, NULL);
		    if(time1.tv_sec-time0.tv_sec>10)
		      break;
		  }
		  while(1){
		      sleep(3); 
		      disk_scanalldisk(0);
		      if(disk_disklist.size()==2){
			break;
		      }	   
		      gettimeofday(&time1, NULL);
		      if(time1.tv_sec-time0.tv_sec>60)
			break;             
		  }  	            
	      }
    #endif	  
	  }
      }
    }    
    dio_kickwatchdog();
   // printf("disklist size1:%d\n",disk_disklist.size());
    ////////////////////////////////////////////////////////////////////////
    for( disk=0; disk<disk_disklist.size(); disk++) {
        for( di=0; di<disk_disklist[disk].daylist.size(); di++ ) {
            day = disk_disklist[disk].daylist[di].bcdday ;
            // see if this day already in the list
            dup=0 ;
            for( i=0; i<daylist.size(); i++ ) {
                if( daylist[i] == day ) {
                    dup=1;
                    break;
                }
            }
            if( !dup ) {
                daylist.add(day);
            }
        }
    }
    daylist.sort();
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
int disk_olddestfile( char * dir,  struct dvrtime *oldesttime, string * oldestfilename, int lock )
{
    char * pathname ;
    char * filename ;
    int l ;
    struct dvrtime dvrt ;
    dir_find dfind(dir);
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
    
    return res ;
}

// scaning day files
void disk_scandayinfo( char * daydir, disk_day_info * ddi )
{
    char * filename ;
    int l ;
    struct dvrtime dvrt ;
    int bcdt ;
    dir_find dfind(daydir);
    while( dfind.find() ) 
    {
        if( dfind.isdir() ) {
            disk_scandayinfo( dfind.pathname(), ddi ) ;
        }
        if( dfind.isfile() ) {
            filename = dfind.filename();
            l = strlen(filename) ;
            if( l>4 && strcmp(&filename[l-4], ".266")==0 ) {    // it is a .264 file
                f264time(filename, &dvrt);
                bcdt = dvrt.hour*10000 + dvrt.minute*100 + dvrt.second ;
                l = f264length (filename);
                if( l>0 ) {
                    if( strstr(filename,"_L_")==NULL ) {    // locked file ?
                        ddi->nfilelen+=l ;
                        if( bcdt<ddi->nfiletime ) {
                            ddi->nfiletime=bcdt ;
                        }
                        if( bcdt>ddi->nfiletime_l ) {
                            ddi->nfiletime_l=bcdt ;
                        }
                    }
                    else {
                        ddi->lfilelen+=l ;
                        if( bcdt<ddi->lfiletime ) {
                            ddi->lfiletime=bcdt ;
                        }
                        if( bcdt>ddi->lfiletime_l ) {
                            ddi->lfiletime_l=bcdt ;
                        }
                    }
                }
            }
        }
    }
}

int disk_renewdiskdayinfo( int diskindex, int day )
{
    int i ;
    char diskdaypath[512] ;
   // disk_lock();
    
    if( diskindex<disk_disklist.size() ){
        for( i=0; i<disk_disklist[diskindex].daylist.size(); i++ ) {
            if( day == disk_disklist[diskindex].daylist[i].bcdday ) {
                break;
            }
        }
        sprintf(diskdaypath, "%s/%08d", disk_disklist[diskindex].basedir.getstring(), day );
        disk_disklist[diskindex].daylist[i].bcdday = day ;
        disk_disklist[diskindex].daylist[i].nfilelen = 0 ;
        disk_disklist[diskindex].daylist[i].nfiletime = 240000 ;
        disk_disklist[diskindex].daylist[i].nfiletime_l = 0 ;
        disk_disklist[diskindex].daylist[i].lfilelen = 0 ;
        disk_disklist[diskindex].daylist[i].lfiletime = 240000 ;
        disk_disklist[diskindex].daylist[i].lfiletime_l = 0 ;
        disk_scandayinfo( diskdaypath, &(disk_disklist[diskindex].daylist[i]) );
        if( disk_disklist[diskindex].daylist[i].nfilelen == 0 && disk_disklist[diskindex].daylist[i].lfilelen == 0 )
        {
            disk_disklist[diskindex].daylist.remove(i);
        }
    }
    
    //disk_unlock();
    return 0;
}


// To renew a file list on current disk
int disk_renew( char * filename )
{
    int disk ;
    int l ;
    int day ;
    char * base ;
    disk_lock();
    
    for( disk=0; disk<disk_disklist.size(); disk++ ) {
        base = disk_disklist[disk].basedir.getstring() ;
        l=disk_disklist[disk].basedir.length() ;
        if( strncmp(base, filename, l )==0 ) {
            day=0 ;
            sscanf( &filename[l], "/%08d", &day );
            if( day>19990000 && day<20990000 ) {
                disk_renewdiskdayinfo( disk, day );
            }
            break;
        }
    }

    disk_unlock();
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

int isflashcopydone(char *path)
{
  char base[256];
  char filename[256];
  FILE* fp;
  if (get_base_dir(base, sizeof(base), path))
    return 0;
  sprintf(filename,"%s/flashcopydone",base);
  fp=fopen(filename,"r");
  if(fp){
    fclose(fp);
    unlink(filename);
    sleep(3);
    return 1;
  }
  return 0;
}

void formatflash(char *path)
{
    char base[256];
    char devname[60];
    char* ptemp;  
    int ret=0;
    pid_t format_id;
    struct timeval time1, time2 ;
    
    if (get_base_dir(base, sizeof(base), path))
    return;   
    ret=umount2(base,MNT_FORCE);   
    if(ret<0)
      return;
    gettimeofday(&time1, NULL);
    ptemp=&base[17];
    sprintf(devname,"/dev/%s",ptemp);        
    format_id=fork();
    if(format_id==0){    
       nice(10);
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
	mount(devname,base,"vfat",MS_MGC_VAL,"");
	mkdir(path,0777);
	dvr_log("%s is formated to FAT32",devname);	 
    } else {
        dvr_log("Format Flash failed"); 
    }    
    gettimeofday(&time2, NULL);
    dvr_log("format takes:%d seconds",time2.tv_sec-time1.tv_sec);
    return;
}

void writeBackupDiskInfo(char *path)
{
  char base[256];

  if (get_base_dir(base, sizeof(base), path))
    return;

  FILE *fp = fopen("/var/dvr/backupdisk", "w");
  if (fp != NULL) {
    fputs(base, fp);
    fclose(fp);
  }
}

void writeMultipleDiskInfo(int diskid, int totalMulti)
{
  FILE *fp = fopen("/var/dvr/multidisk", "w");
  if (fp != NULL) {
    fprintf(fp, "%d/%d", diskid, totalMulti);
    fclose(fp);
  }
}

void unmountBackupDisk(char *path)
{
  char base[256], cmd[512], dev[32];

  if (get_base_dir(base, sizeof(base), path))
    return;

  /* delete /var/dvr/xdisk/mount_devname */
  char *ptr = strrchr(base, '_');
  if (!ptr) return;
  strncpy(dev, ptr + 1, sizeof(dev));
  snprintf(cmd, sizeof(cmd),
	  "/var/dvr/xdisk/mount_%s",
	   dev);
  unlink(cmd);

  snprintf(cmd, sizeof(cmd),
	  "umount %s",base);
  fprintf(stderr, "%s\n", cmd);
  system(cmd);
  snprintf(cmd, sizeof(cmd),
	  "rmdir %s",base);
  system(cmd);
}

/* check if we have Hybrid disk
 * and return disk index for sata/flash
 */
int isHybridDisk(int *sata, int *flash)
{
  int i, s, f, total = 0;

  s = f = -1;

  for( i=0; i<disk_disklist.size(); i++ ) {
    if (disk_disklist[i].type == HD_HYBRID) {
      total++;
      if (disk_disklist[i].id == DT_SATA) {
	s = i;
      } else if (disk_disklist[i].id == DT_FLASH) {
	f = i;
      }
    }
  }

  if ((total >= 2) && (s >= 0) && (f >= 0)) {
   // fprintf(stderr, "Hybrid disk detected\n\tF:%s\n\tS:%s\n",
//	    disk_disklist[f].basedir.getstring(),
//	    disk_disklist[s].basedir.getstring());
   // dvr_log("Hybrid disk detected:");
   // dvr_log("flash basedir:%s",disk_disklist[f].basedir.getstring());
   // dvr_log("hard disk basedir:%s",disk_disklist[s].basedir.getstring());
    if (sata) *sata = s;
    if (flash) *flash = f;
    /* for playback(disk_listday) */
    sata_diskid = s;
    flash_diskid = f;
    return 1;
  } else if(total>0){
     *sata=-1;
     *flash=-1;
     sata_diskid = -1;
     flash_diskid = -1;
     return 1;
  }

 /* for playback(disk_listday) */
  sata_diskid = s;
  flash_diskid = f;

  return 0;
}

/*
 * returns index in disklist for the given disk_id
 */
int getDisklistIdfromMultipleDiskId(int id)
{
  int i;

  for (i = 0; i < disk_disklist.size(); i++) {
    if ((disk_disklist[i].type == HD_MULTIPLE) &&
	(disk_disklist[i].id == id)) {
      return i;
    }
  }

  return -1;
}

/*
 * returns multiple disk id for the given disklist idx
 */
int getMultipleDiskIdfromDisklistId(int idx)
{
  if ((idx < 0) || (idx >= disk_disklist.size()) ||
      (disk_disklist[idx].type != HD_MULTIPLE))
    return -1;

  return disk_disklist[idx].id;
}

/*
 * idx: disk list id (NOT:multiple disk id (e.g 0 for disk 1/3))
 * returns index in disklist for the next disk_id
 */
int getNextMultipleDiskId(int idx)
{
  int i, cur_id, next_id, total;

  /* sanity check */
  if ((idx < 0) || (idx >= disk_disklist.size()) ||
      (disk_disklist[idx].type != HD_MULTIPLE))
    return -1;
 
  cur_id = disk_disklist[idx].id;
  total = disk_disklist[idx].ndisk;
  next_id = cur_id + 1;
  if (next_id >= total)
    next_id = 0;

  for (i = 0; i < disk_disklist.size(); i++) {
    if ((disk_disklist[i].type == HD_MULTIPLE) &&
	(disk_disklist[i].id == next_id)) {
      return i;
    }
  }

  return -1;
}

/* check if we have Multiple disk
 * and return total disk numbers
 */
int isMultipleDisk(int *t)
{
  int i, total = 0;
  char found[MAX_DISK];

  memset(found, 0, MAX_DISK);

  for( i=0; i<disk_disklist.size(); i++ ) {
    if (disk_disklist[i].type == HD_MULTIPLE) {
      total++;
      found[disk_disklist[i].id] = disk_disklist[i].ndisk;
    }
  }

  if (total < 2)
    return 0;

  /* all disks found? */
  for (i = 0; i < total; i++) {
    if (found[i] == 0) {
      return 0;
    }
  }

  /* sanity check: ndisk */
  for (i = 0; i < total; i++) {
    if (found[i] > total) {
      return 0;
    }
  }

  fprintf(stderr, "Multiple disk detected(%d)\n", total);
  if (t) *t = total;

  return 1;
}

void mark_current_disk(char *curdisk_with_busname)
{
  char basedir[512];
  int i;

  FILE * f = fopen(disk_curdiskfile.getstring(), "w"); 
  if( f ) {
    strncpy( basedir, curdisk_with_busname, sizeof(basedir)-1);
    basedir[sizeof(basedir)-1]=0 ;
    for( i=strlen(basedir)-1; i>0 ; i--) {
      if( basedir[i]=='/' ) {
	basedir[i]=0 ;
	break;
      }
    }
    fputs(basedir, f);
    fclose(f);
  }
}

/*
 * DEPRECATED: use sync_logfiles_for_hybrid
 */
void copyLogFiles(char *from, char *to)
{
  char baseFrom[256], baseTo[256], cmd[512];

  if (get_base_dir(baseFrom, sizeof(baseFrom), from))
    return;
  strcat(baseFrom, "/smartlog");

  if (get_base_dir(baseTo, sizeof(baseTo), to))
    return;
  strcat(baseTo, "/smartlog");

  /* copy dvrlog file */
  snprintf(cmd, sizeof(cmd),
	   "ls %s", from);
  return;
  snprintf(cmd, sizeof(cmd),
	  "ls %s && cp -f %s/dvrlog.txt %s",
	   from, from, to);
  fprintf(stderr, "%s\n", cmd);
  system(cmd);

  struct tm tm;
  time_t t = time(NULL);
  localtime_r(&t, &tm);

  /* copy today's smartlog file */
  snprintf(cmd, sizeof(cmd),
	   "mkdir -p %s && cp -f %s/%s_%04d%02d%02d_?.001 %s",
	   baseTo, baseFrom, g_hostname,
	   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	   baseTo);
  fprintf(stderr, "%s\n", cmd);
  system(cmd);
}

int copyPendingFiles(char *from, char *to)
{
  char baseFrom[256], baseTo[256];
  int ret;
  FILE *fw;
  if (get_base_dir(baseFrom, sizeof(baseFrom), from))
    return -1;

  if (get_base_dir(baseTo, sizeof(baseTo), to))
    return -1;

  /* copy video files */
#if DEBUG_HBD
  dvr_log("copy video files: from %s to %s",baseFrom,baseTo);
  dvr_log("to:%s,hostname:%s",to,g_hostname);
#endif
  ret=copy_files(baseFrom, baseTo, to, g_hostname);
  if(ret<0)
    return -1;
  fw=fopen("/var/dvr/flashcopydone","w");
  if(fw){
      fprintf(fw,"done"); 
      fclose(fw);
  }
  return 0;
}

/* 
 * sync dvrlog/smartlog files between 2 given disks.
 * Note: if /var/dvr/dvrcurdisk is already set, it'll be changed to diskb
 */
void sync_logfiles_for_hybrid(int diska_id, int diskb_id, int check_stop)
{
  int remark_curdisk = 0;
  char *from, *to;

  from = disk_disklist[diska_id].basedir.getstring();
  to = disk_disklist[diskb_id].basedir.getstring();
  if (!from[0] || !to[0])
    return;

  /* before copying dvrlog file,
   * make sure the logfile is not overwritten by ioprocess
   */
  if (rec_basedir.length() > 0) {
    rec_basedir = "";
    unlink(disk_curdiskfile.getstring());
  }
  //remark_curdisk = 1;
  /* sync dvrlog */
//  dvr_log("sync dvrlog");
  sync_dvrlog_for_hybrid(from, to);
  
  /* now, dvrlog can be written to SATA */
 // if (remark_curdisk) {
    rec_basedir = to;
   // dvr_log("mark current disk");
    mark_current_disk(rec_basedir.getstring());
//  }
 // dvr_log("sync_smartlog");
  /* copy smartlog files */
  sync_smartlog_for_hybrid(from, to, check_stop);
 // dvr_log("sync_smartlog done");
}

int get_month_from_string(char *mon)
{
  if (!strcmp(mon, "Jan"))
    return 1;
  else if (!strcmp(mon, "Feb"))
    return 2;
  else if (!strcmp(mon, "Mar"))
    return 3;
  else if (!strcmp(mon, "Apr"))
    return 4;
  else if (!strcmp(mon, "May"))
    return 5;
  else if (!strcmp(mon, "Jun"))
    return 6;
  else if (!strcmp(mon, "Jul"))
    return 7;
  else if (!strcmp(mon, "Aug"))
    return 8;
  else if (!strcmp(mon, "Sep"))
    return 9;
  else if (!strcmp(mon, "Oct"))
    return 10;
  else if (!strcmp(mon, "Nov"))
    return 11;
  else if (!strcmp(mon, "Dec"))
    return 12;

  return 0;
}

time_t parse_logtime(char *line)
{
  char mon[4];
  int month, day, hour, min, sec, year;
  struct tm tm;
  time_t t;

  t = 0;
  if (sscanf(line, "%*3s %3s %d %02d:%02d:%02d %d:",
	     mon, &day, &hour, &min, &sec, &year) == 6) {

    month = get_month_from_string(mon);
    if (month) {
      tm.tm_isdst = -1;
      tm.tm_year = year - 1900;
      tm.tm_mon = month - 1;
      tm.tm_mday = day;
      tm.tm_hour = hour;
      tm.tm_min = min;
      tm.tm_sec = sec;
      t = mktime(&tm);
      if (t < 0)
	t = 0;
    }
  }
  return t;
}

#if 0
void sync_dvrlog_for_hybrid(char *disk_a, char *disk_b)
{
  int i;
  char filename[512];
  time_t ct[2], latest_ts, latest_disk;
  FILE *fp;
  char line[256];

  dvr_log("inside sync_dvrlog_for_hybrid");
  /* check the timestamp in each dvrlog.txt */
  latest_ts = 0;
  latest_disk = -1;
  for (i = 0; i < 2; i++) {
    ct[i] = 0;
    snprintf(filename, sizeof(filename),
	     "%s/dvrlog.txt",
	     i ? disk_b : disk_a);
    fp = fopen(filename, "r");
    if (fp) {
      int size;
      if (!fseek(fp, 0, SEEK_END)) {
	size = ftell(fp);
	if (size != -1) {
	  if (size > 300) {
	    fseek(fp, size - 300, SEEK_SET);
	  } else {
	    fseek(fp, 0, SEEK_SET);
	  }
	  time_t lt = 0, t;
	  /* get the latest time */
	  while (fgets(line, sizeof(line), fp)) {
	    t = parse_logtime(line);
	    if (t > lt) lt = t;
	  }
	  ct[i] = lt;
	}
      }
      fclose(fp);
    }
 
    fprintf(stderr, "%d:%s,%s\n", ct[i],ctime(&ct[i]),filename);
    if (ct[i] > latest_ts) {
      latest_ts = ct[i];
      latest_disk = i; /* i:0 means disk_a, 1:disk_b */
      fprintf(stderr, "latest_disk:%d,%s\n", i, i ? disk_b : disk_a);
    }
  }

  if (latest_disk == -1)
    return;

  dvr_log("copy latest to others");
  /* copy latest to others */
  for (i = 0; i < 2; i++) {
    
    if (ct[i] != ct[latest_disk]) {
      /* i==0: copy disk_b --> a */
      snprintf(filename, sizeof(filename),
	       "%s/dvrlog.txt",
	       i ? disk_a : disk_b);
      fprintf(stderr, "cp:%s,%s\n", filename,i ? disk_b : disk_a, 1);
      copy_file_to_path(filename, i ? disk_b : disk_a, 1, 0);
    }
  }
}
#else
void sync_dvrlog_for_hybrid(char *disk_flash, char *disk_sata)
{
  char filename[512];
  FILE *fp_sata,*fp_flash;
  char line[512];
  //check the timestamp in sata_disk
  snprintf(filename, sizeof(filename),
	     "%s/dvrlog.txt", disk_sata);
  dvr_cleanlog(filename);    
  fp_sata = fopen(filename, "a");
  if(!fp_sata){
      dvr_log("dvr_log.txt in sata can't be created");
     return; 
  }
  
  //copy the lines from flash to sata according to timestamp
  snprintf(filename, sizeof(filename),
	     "%s/dvrlog.txt", disk_flash);
  fp_flash = fopen(filename, "r");
  if(!fp_flash){
     fclose(fp_sata);
     dvr_log("dvrlog.txt doesn't exist in FLASH");
     return;
  }
  while (fgets(line, sizeof(line), fp_flash)) {
      if(parse_logtime(line)>0){
	 fputs(line,fp_sata);
	 break;
      }
  }
  while (fgets(line, sizeof(line), fp_flash)) {
 	 fputs(line,fp_sata);	   
  }  
  fclose(fp_flash);
  fclose(fp_sata);
  remove(filename);
}
#endif

struct logtime {
    time_t  t;
    int index;
};

void merge_dvrlog()
{
   int i;
   int index=0;
   int j;
   FILE* fr;
   FILE* fw;
   char filename[512];
   char filename1[512];
   char line[512];
   char curdisk[256];
   string rec_basedir_o;
   time_t  t;
   array<logtime>  logarray;
   logarray.empty();
   dvr_log("start to merge");
   fr = fopen("/var/dvr/dvrcurdisk", "r");
   if(!fr)
     return;
   i = fread(curdisk, 1, sizeof(curdisk), fr);
   fclose(fr);
   if(i<10)
     return;
   curdisk[i]='\0'; 

   //search start time of dvrlog in different partitions 
   for(i=0;i<disk_disklist.size();++i){
      snprintf(filename, sizeof(filename),
	       "%s/dvrlog.txt",
	       disk_disklist[i].basedir.getstring());    
      fr=fopen(filename,"r");
      if(!fr)
	continue;
      while (fgets(line, sizeof(line), fr)) {
	    t=parse_logtime(line);
	    if(t>0){	      
	      break;
	    }
      }
      fclose(fr);
      if(t>0){
	index=logarray.size();
	logarray[index].t=t;
	logarray[index].index=i;
      }
   }
   dvr_log("logarray size:%d",logarray.size());
   if(logarray.size()>1){
        //disable writing to dvrlog file
	rec_basedir_o=rec_basedir.getstring();
	rec_basedir = "";
	rename("/var/dvr/dvrcurdisk","/var/dvr/currecorddisk");
	snprintf(filename1,sizeof(filename1),"%s/_%s_/dvrlog.back",curdisk,g_hostname);  
	fw=fopen(filename1,"w");
	if(!fw){
	      dvr_log("%s can't be opened",filename1);
	      return;
	}
	dvr_log("%s is opened to write",filename1);  
	while(logarray.size()>0){
	      index=0;
	      t=logarray[0].t;
	      for(i=0;i<logarray.size();++i){
		if(logarray[i].t<t){
		    t=logarray[i].t;
		    index=i;
		}
	      }
	      j=logarray[index].index;
	      logarray.remove(index); 
	      snprintf(filename, sizeof(filename),
		"%s/dvrlog.txt",
		disk_disklist[j].basedir.getstring()); 
	      fr=fopen(filename,"r");
	      if(!fr){
		continue;	    
	      }
	      while (fgets(line, sizeof(line), fr)) {
		      t=parse_logtime(line);
		      if(t>0){
			fputs(line,fw);
			break;
		      }
	      }
	      while (fgets(line, sizeof(line),fr)) {
		  fputs(line,fw);	   
	      }  
	      fclose(fr);
	}
	fclose(fw);
	dvr_log("dvr_back is created");   
	snprintf(filename,sizeof(filename),"%s/_%s_/dvrlog1.txt",curdisk,g_hostname); 
	rename(filename1,filename);    
	for(i=0;i<disk_disklist.size();++i){
	      snprintf(filename, sizeof(filename),
		"%s/dvrlog.txt",
		disk_disklist[i].basedir.getstring());  
	      remove(filename);
	}
	snprintf(filename1,sizeof(filename1),"%s/_%s_/dvrlog1.txt",curdisk,g_hostname); 
	snprintf(filename,sizeof(filename),"%s/_%s_/dvrlog.txt",curdisk,g_hostname); 
	rename(filename1,filename); 
	rec_basedir=rec_basedir_o.getstring();
	rename("/var/dvr/currecorddisk","/var/dvr/dvrcurdisk");	
   }
 
   
}

void removedir(const char *dir)
{
    char * pathname ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
	   removedir( pathname);
        }
        else {
	    remove(pathname);
        }
    }
    rmdir(dir);
}

struct gpslogfile{
    string filename;
    int starttime;
    int year;
    int month;
    int day;
    int fileno;
    int date;
    char lock;
};

void merge_smartlog()
{
   FILE* fr;
   FILE* fw;
   int i,j;
   int size;
   int ret;
   int year, fileno;
   int month, day;
   int hh, mm, ss;
   char lock;   
   char line[256];
   char cmd[512], ext[16];
   char curdisk[256];    
   char basedir[256];
   char smartfolder[256];
   char desfolder[256];
   char name[256], name1[256];   
   int servernameLen;
   array<struct gpslogfile> filearray;
   array<struct gpslogfile> gpslogarray;
   
   struct stat sb, sb1;
   DIR *dir;
   struct dirent *de;
   int mBcdday;
   int tempday;
   
   fr = fopen("/var/dvr/dvrcurdisk", "r");
   if(!fr)
     return;
   size = fread(curdisk, 1, sizeof(curdisk), fr);
   fclose(fr);
   if(size<10)
     return;
   curdisk[size]='\0';
   //create the destination folder
   snprintf(desfolder,sizeof(desfolder),"%s/smartlog",curdisk);
   mkdir(desfolder, 0777);
   servernameLen = strlen(g_hostname);  
   
  // dvr_log("destination folder:%s",desfolder);
   mBcdday=21000101;
   //search oldest bcdday code	      
   for( i=0; i<disk_disklist.size(); i++ ) {
	array <disk_day_info> * ddi = & (disk_disklist[i].daylist) ;
	int j;
	for(j=0; j<ddi->size(); j++) {
	    if( (*ddi)[j].nfilelen > 0|| (*ddi)[j].lfilelen > 0) {
		if((*ddi)[j].bcdday < mBcdday)
		{
		   mBcdday=(*ddi)[j].bcdday;
		}
	    }
	}
   }
  // dvr_log("bcd day:%8d",mBcdday);

   //start to copy the smart files to current recording partition
   for(i=0;i<disk_disklist.size();++i){
	ret=strncmp(disk_disklist[i].basedir.getstring(),curdisk,strlen(curdisk));
	if(!ret){	    
	    continue;
	}
	if (get_base_dir(basedir, sizeof(basedir), disk_disklist[i].basedir.getstring()))
	    continue;   
	//create source folder name
	snprintf(smartfolder,sizeof(smartfolder),"%s/smartlog",basedir);
	//dvr_log("source folder:%s",smartfolder);
        dir = opendir(smartfolder);
	if (dir == NULL) {
            continue;
	}
	while ((de = readdir(dir)) != NULL) {

	      if (!strncmp(g_hostname, de->d_name, servernameLen) &&
		  ((int)strlen(de->d_name) > servernameLen)) {
		      ret = sscanf(de->d_name + servernameLen,
			    "_%04d%02d%02d_%c.%03d",
			     &year, &month, &day, &lock, &fileno);
		      if (ret == 5) {
			    //gps log
			    snprintf(name, sizeof(name), "%s/%s",smartfolder,de->d_name);			    

			    tempday=year*10000+month*100+day;
			    if(tempday<mBcdday){
			       remove(name);
			      // dvr_log("%s is deleted",name);
			    } else {			    
				if (!stat(name, &sb)){

				      if(lock=='L'||lock=='l'){	
					  snprintf(name1, sizeof(name1), "%s/%s",desfolder,de->d_name);		  
					  if (!stat(name1, &sb1)){
					      //there is file with the same name
					      for(j=0;j<numberoftotalpartitions;++j){
						  snprintf(name1,sizeof(name),"%s/%s_%d",desfolder,de->d_name,j);
						  if (!stat(name1, &sb1)){
						    continue;
						  }
						  break;
					      }
					  }				    				  
				      } else {
					  snprintf(name1, sizeof(name1),
						"%s/%s_%04d%02d%02d_L.%03d",
					  desfolder, g_hostname, year, month, day,fileno);	
					  if (!stat(name1, &sb1)){
					      for(j=0;j<numberoftotalpartitions;++j){
						
						    snprintf(name1, sizeof(name1),
							"%s/%s_%04d%02d%02d_L.%03d_%d",
						    desfolder, g_hostname, year, month, day,fileno,j);						
						    if (!stat(name1, &sb1)){
						      continue;
						    }
						    break;
					      }
					  } else {
					      snprintf(name1, sizeof(name1),
						    "%s/%s_%04d%02d%02d_N.%03d",
					      desfolder, g_hostname, year, month, day,fileno);					  
					  }
				      }
				      snprintf(cmd,sizeof(cmd),"cp %s %s",name,name1);
				      system(cmd);
				  //    dvr_log("%s is copied to %s",name,name1);
				      remove(name);
				   //   dvr_log("%s is deleted",name);
				} 
			    }
			    
		      } else {
			  ret = sscanf(de->d_name + servernameLen,
			  "_%04d%02d%02d%02d%02d%02d_TAB102log_%c.%s",
			  &year, &month, &day, &hh, &mm, &ss, &lock, ext);
			  if (ret == 8) {
			      //tab101 file
			      snprintf(name, sizeof(name), "%s/%s",smartfolder,de->d_name);
			      tempday=year*10000+month*100+day;
			      if(tempday<mBcdday){
				  remove( name);
				 // dvr_log("%s is deleted",name);
			      } else   {	
				    if (!stat(name, &sb)){
					  snprintf(name1,sizeof(name),"%s/%s",desfolder,de->d_name);			
					  if (!stat(name1, &sb1)){
					      if (sb.st_size != sb1.st_size){
						unlink(name1);
						snprintf(cmd,sizeof(cmd),"cp %s %s",name,name1);
						system(cmd);	
						//dvr_log("%s is copied to %s",name,name1);
					      }
					  } else {
					      snprintf(cmd,sizeof(cmd),"cp %s %s",name,name1);
					      system(cmd);
					    //  dvr_log("%s is copied to %s",name,name1);
					  }
					  remove(name);
					 // dvr_log("%s is deleted",name); 
				    }	
			      }
			  }//if (ret == 8) {						
		      }		
	      }	  
	}  //while
	
    }//for i<disk_disklist.size();
   //////////////////////////////////////////////////////////////////////////

    dir = opendir(desfolder);
    if(dir==NULL)
      return;
    //search the gps log file in current partition
  //  dvr_log("start search the gps data file in current partitions");
    filearray.empty();
    while ((de = readdir(dir)) != NULL) {
	if (!strncmp(g_hostname, de->d_name, servernameLen) &&
	    ((int)strlen(de->d_name) > servernameLen)) {
		ret = sscanf(de->d_name + servernameLen,
		      "_%04d%02d%02d_%c.%03d",
			&year, &month, &day, &lock, &fileno);
		if (ret == 5) { 
		    snprintf(name, sizeof(name), "%s/%s",desfolder,de->d_name);
		    if (!stat(name, &sb)){
		        int tempday=(int)year*10000+month*100+day;
			if(tempday<mBcdday){
			    remove(name);
			 //   dvr_log("%s is deleted",name);
			} else {
			    size=filearray.size();
			    filearray[size].filename=name;
			    filearray[size].date=tempday;
			    filearray[size].year=year;
			    filearray[size].month=month;			    
			    filearray[size].day=day;
			    filearray[size].fileno=fileno;
			    filearray[size].lock=lock;
			  //  dvr_log("%s is found,tempday:%8d year:%04d month:%02d day:%02d fileno:%03d",\
			   //         name,tempday,year,month,day,fileno);
			}		      		      
		    } else {
			remove(name);
			//dvr_log("%s is deleted",name);
		    }
		} 
	}
      
    }//while
      
   ///////////////////////////////////////////////
    if(filearray.size()==0)
     return;

   //handling the gps log;
    
    while(filearray.size()>0){     
	  gpslogarray.empty();
	  year=filearray[0].year;
	  month=filearray[0].month;
	  day=filearray[0].day;
	  fileno=filearray[0].fileno;
	  lock=filearray[0].lock;
	    
	//  dvr_log("year:%d month:%d day:%d fileno:%d",year,month,day,fileno);
	  fr=fopen(filearray[0].filename.getstring(),"r");
	  if(!fr)
	    return;
	  while (fgets(line, sizeof(line), fr)){
	      if((line[0]=='9')&&(line[1]=='9')){
		  break;
	      }
	  }
	  fgets(line, sizeof(line), fr);
	  ret=sscanf(&line[3],"%02d:%02d:%02d",&hh,&mm,&ss);
	  fclose(fr);  
	  if(ret!=3){
	    remove(filearray[0].filename.getstring());
	 //   dvr_log("%s is deleted",filearray[0].filename.getstring());
	    filearray.remove(0);
	    continue;
	  }
	  gpslogarray[0].filename=filearray[0].filename.getstring();
	  gpslogarray[0].year=year;
	  gpslogarray[0].month=month;
	  gpslogarray[0].day=day;
	  gpslogarray[0].date=filearray[0].date;
	  gpslogarray[0].fileno=fileno;
	  gpslogarray[0].lock=lock;
	  gpslogarray[0].starttime=hh*10000+mm*100+ss;
	//  dvr_log("%s hh:%d mm:%d ss:%d :%d",filearray[0].filename.getstring(),hh,mm,ss,gpslogarray[0].starttime);	  
	  
	  for(i=1;i<filearray.size();++i){
		if(filearray[i].date!=gpslogarray[0].date){
		    continue;		 
		}
		//search the start time
		fr=fopen(filearray[i].filename.getstring(),"r");
		if(!fr){
		    continue; 
		}
		while (fgets(line, sizeof(line), fr)){
		    if((line[0]=='9')&&(line[1]=='9')){
			break;
		    }
		}
		fgets(line, sizeof(line), fr);
		ret=sscanf(&line[3],"%02d:%02d:%02d",&hh,&mm,&ss);
		fclose(fr);  
		if(ret!=3){
		  continue;
		}
		size=gpslogarray.size();
		gpslogarray[size].filename=filearray[i].filename.getstring();
		gpslogarray[size].year=year;
		gpslogarray[size].month=month;
		gpslogarray[size].day=day;
		gpslogarray[size].date=filearray[i].date;
		gpslogarray[size].lock=filearray[i].lock;
		gpslogarray[size].fileno=fileno;
		gpslogarray[size].starttime=hh*10000+mm*100+ss;	  
		//dvr_log("%s hh:%d mm:%d ss:%d :%d",filearray[i].filename.getstring(),hh,mm,ss,gpslogarray[size].starttime);
	  }
	  for(i=0;i<filearray.size();){
	      if(filearray[i].date==gpslogarray[0].date){
		 filearray.remove(i);
	      } else {
		  ++i;
	      }
	  }
	  if(gpslogarray.size()==1){
	      snprintf(name,sizeof(name),".%03d_t",fileno);
	      int len=strlen(name);
	      if(!strncmp(gpslogarray[0].filename.getstring(),name,len)){
		   snprintf(name, sizeof(name),
	           "%s/%s_%04d%02d%02d_L.%03d",
	           desfolder, g_hostname, year, month, day,fileno);
		   rename(gpslogarray[0].filename.getstring(),name);		   
	      }
	      continue;	    
	  }	  
	  //create the temporary file name
	  snprintf(name, sizeof(name),
	      "%s/%s_%04d%02d%02d_L.%03d_t",
	      desfolder, g_hostname, year, month, day,fileno);

	  int index=-1;	  
	  //check whether the temporary file exist
	  for(i=0;i<gpslogarray.size();++i){
	      if(!strncmp(gpslogarray[i].filename.getstring(),name,strlen(name))){
		  dvr_log("%s exists",name);
		  gpslogarray.remove(i);
		  index=i;
		  break;
	      }	    
	  }
	  if(index<0){	  
	      int starttime=1000000;
	     //find the with oldest data
	      for(i=0;i<gpslogarray.size();++i){
		  if(gpslogarray[i].starttime<starttime){
		      index=i; 
		      starttime=gpslogarray[i].starttime;
		  }
	      }

	      rename(gpslogarray[index].filename.getstring(),name) ;
	   //   dvr_log("%s is renamed to %s",gpslogarray[index].filename.getstring(),name);
	      gpslogarray.remove(index);
	  }  
	  fw=fopen(name,"a");
	  if(!fw){
	    dvr_log("%s can't be openned");
	    break;
	  }
	//  dvr_log("%s is opened for append",name);
	  while(gpslogarray.size()>0){
	      int starttime=1000000;
	      int index=-1;
	      for(i=0;i<gpslogarray.size();++i){
		  if(gpslogarray[i].starttime<starttime){
		      index=i; 
		      starttime=gpslogarray[i].starttime;
		  }
	      }
	      if(index<0)
		break;
	      
	      fr=fopen(gpslogarray[index].filename.getstring(),"r");
	      if(fr){
		   while (fgets(line, sizeof(line), fr)){
		      if((line[0]=='9')&&(line[1]=='9')){
			 break;
		      }
		   }		
	           while (fgets(line, sizeof(line), fr)) {
		      fputs(line,fw);	   
		   } 				  
		  fclose(fr);
	      }
	      remove(gpslogarray[index].filename.getstring());
	   //   dvr_log("%s is deleted",gpslogarray[index].filename.getstring());
	      gpslogarray.remove(index);
	      
	  } // while(gpslogarray.size()>0){
	  fclose(fw);
	  snprintf(name1, sizeof(name1),
	      "%s/%s_%04d%02d%02d_L.%03d",
	      desfolder, g_hostname, year, month, day,fileno);	  
	  rename(name,name1);   
	//  dvr_log("%s is renamed to %s",name,name1);
    }
    return;
     
};

void sync_dvrlog(int totalMulti)
{
  int i, mid;
  char filename[512];
  time_t ct[MAX_DISK], latest_ts, latest_disk;
  FILE *fp;
  char line[256];
  int idsrc, iddst;


  /* check the timestamp in each dvrlog.txt */
  latest_ts = 0;
  latest_disk = -1;
  for (i = 0; i < totalMulti; i++) {
    ct[i] = 0;
    mid = getDisklistIdfromMultipleDiskId(i);
    if (mid >= 0) {
      snprintf(filename, sizeof(filename),
	       "%s/dvrlog.txt",
	       disk_disklist[mid].basedir.getstring());
      fp = fopen(filename, "r");
      if (fp) {
	int size;
	if (!fseek(fp, 0, SEEK_END)) {
	  size = ftell(fp);
	  if (size != -1) {
	    if (size > 300) {
	      fseek(fp, size - 300, SEEK_SET);
	    } else {
	      fseek(fp, 0, SEEK_SET);
	    }
	    time_t lt = 0, t;
	    /* get the latest time */
	    while (fgets(line, sizeof(line), fp)) {
	      t = parse_logtime(line);
	      if (t > lt) lt = t;
	    }
	    ct[i] = lt;
	  }
	}
	fclose(fp);
      }
    }
    if (ct[i] > latest_ts) {
      latest_ts = ct[i];
      latest_disk = i; /* multiple disk id, not disklist id */
    }
  }

  if (latest_disk == -1)
    return;

  /* copy latest to others */
  for (i = 0; i < totalMulti; i++) {
    if (ct[i] != ct[latest_disk]) {
      char cmd[512];
      idsrc = getDisklistIdfromMultipleDiskId(latest_disk);
      iddst = getDisklistIdfromMultipleDiskId(i);
      if ((idsrc >= 0) && (iddst >= 0)) {
	snprintf(cmd, sizeof(cmd),
		 "cp %s/dvrlog.txt %s",
		 disk_disklist[idsrc].basedir.getstring(),
		 disk_disklist[iddst].basedir.getstring());
	system(cmd);
      }
    }
  }
}


/*
 * N to N: copy from bigger file to smaller file only
 * L to L: copy from bigger file to smaller file only
 * N to L: copy if diffent size, just rename if same size
 * L to N: never copy
 * N,L to none: copy
 */
#if 0
int copy_smartlog(int src, int dst, int check_stop)
{
  DIR *dir;
  struct dirent *de;
  char name[256], name2[256];
  char srcdir[128], dstdir[128];
  unsigned short year, fileno;
  unsigned char month, day;
  unsigned char hh, mm, ss;
  char lock;
  struct stat sb, sb2;
  int copyIt;
  char cmd[512], ext[16];
	  
  if (get_base_dir(srcdir, sizeof(srcdir),
		   disk_disklist[src].basedir.getstring())) {
    return 1;
  }
  if (get_base_dir(dstdir, sizeof(dstdir),
		   disk_disklist[dst].basedir.getstring())) {
    return 1;
  }

  snprintf(name, sizeof(name), "%s/smartlog", srcdir);

  dir = opendir(name);
  if (dir == NULL) {
    if (errno != ENOENT)
      fprintf(stderr, "opendir:%s(%s)\n", strerror(errno), name);
    return 1;
  }

  while ((de = readdir(dir)) != NULL) {
    /* asked to stop by ioprocess? */
    dio_get_nodiskcheck();
    if (check_stop && !g_nodiskcheck)
      break;

    int servernameLen = strlen(g_hostname);
    if (!strncmp(g_hostname, de->d_name, servernameLen) &&
	((int)strlen(de->d_name) > servernameLen)) {
      int ret = sscanf(de->d_name + servernameLen,
		       "_%04hd%02hhd%02hhd_%c.%03hd",
		       &year, &month, &day, &lock, &fileno);
      if (ret == 5) {
	snprintf(name, sizeof(name), "%s/smartlog/%s",srcdir,de->d_name);
	copyIt = 0;
	/* get file size */
	if (!stat(name, &sb)) {
	  //fprintf(stderr, "%s:%lu\n", name, sb.st_size);
	  /* get file size of the same file on destination */
	  snprintf(name2, sizeof(name2),
		   "%s/smartlog/%s_%04hd%02hhd%02hhd_%c.%03d",
		   dstdir, g_hostname, year, month, day, lock, fileno);
	  if (!stat(name2, &sb2)) {
	    //fprintf(stderr, "name2:%s:%lu\n", name2, sb2.st_size);
	    if (sb.st_size > sb2.st_size) {
	      copyIt = 1;
	    }
	  } else {
	    /* Can't find the same name, try different lock type */
	    snprintf(name2, sizeof(name2),
		     "%s/smartlog/%s_%04hd%02hhd%02hhd_%c.%03d",
		     dstdir, g_hostname, year, month, day,
		     ((lock == 'N') || (lock == 'n')) ? 'L' : 'N', fileno);
	    if (!stat(name2, &sb2)) {
	      if (sb.st_size == sb2.st_size) {
		if ((lock == 'N') || (lock == 'n')) {
		  /* N --> L, same size: rename it (L-->N) */
		  char newname[256];
		  snprintf(newname, sizeof(newname),
			   "%s/smartlog/%s_%04hd%02hhd%02hhd_N.%03d",
			   dstdir, g_hostname, year, month, day, fileno);
		  rename(name2, newname);
		} else {
		  /* L --> N, same size: do nothing */
		}
	      } else {
		if ((lock == 'N') || (lock == 'n')) {
		  /* N --> L, different size */
		  copyIt = 1;
		} else {
		  /* L --> N, different size: do nothing */
		}
	      }
	    } else { /* no file found */
	      /* N,L --> none */
	      copyIt = 1;
	    }
	  }
	}
	
	
	if (copyIt) {
	  /* call rm just in case(e.g. N --> L)	*/ 
	  snprintf(cmd, sizeof(cmd),
		   "rm %s/smartlog/%s_%04hd%02hhd%02hhd_?.%03d; "
		   "mkdir -p %s/smartlog && "
		   "cp %s %s/smartlog",
		   dstdir, g_hostname, year, month, day, fileno,
		   dstdir, name, dstdir);
	  fprintf(stderr, "%s\n", cmd);
	  system(cmd);
	}
      } else { /* ret == 5 */
	ret = sscanf(de->d_name + servernameLen,
		     "_%04hd%02hhd%02hhd%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		     &year, &month, &day, &hh, &mm, &ss, &lock, ext);
	if (ret == 8) {
	  snprintf(name, sizeof(name), "%s/smartlog/%s", srcdir, de->d_name);
	  copyIt = 0;
	  /* get file size */
	  if (!stat(name, &sb)) {
	    //fprintf(stderr, "%s:%lu\n", name, sb.st_size);
	    /* get file size of the same file on destination */
	    snprintf(name2, sizeof(name2),
		     "%s/smartlog/%s_%04hd%02hhd%02hhd"
		     "%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		     dstdir, g_hostname,
		     year, month, day, hh, mm, ss, lock, ext);
	    if (!stat(name2, &sb2)) {
	      //fprintf(stderr, "name2:%s:%lu\n", name2, sb2.st_size);
	      if (sb.st_size > sb2.st_size) {
		copyIt = 1;
	      }
	    } else {
	      /* Can't find the same name, try different lock type */
	      snprintf(name2, sizeof(name2),
		       "%s/smartlog/%s_%04hd%02hhd%02hhd"
		       "%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		       dstdir, g_hostname,
		       year, month, day, hh, mm, ss,
		       ((lock == 'N') || (lock == 'n')) ? 'L' : 'N', ext);
	      if (!stat(name2, &sb2)) {
		if (sb.st_size == sb2.st_size) {
		  if ((lock == 'N') || (lock == 'n')) {
		    /* N --> L, same size: rename it (L-->N) */
		    char newname[256];
		    snprintf(newname, sizeof(newname),
			     "%s/smartlog/%s_%04hd%02hhd%02hhd"
			     "%02hhd%02hhd%02hhd_TAB102log_N.%s",
			     dstdir, g_hostname,
			     year, month, day, hh, mm, ss, ext);
		    rename(name2, newname);
		  } else {
		    /* L --> N, same size: do nothing */
		  }
		} else {
		  if ((lock == 'N') || (lock == 'n')) {
		    /* N --> L, different size */
		    copyIt = 1;
		  } else {
		    /* L --> N, different size: do nothing */
		  }
		}
	      } else { /* no file found */
		/* N,L --> none */
		copyIt = 1;
	      }
	    }
	  }
	  
	  if (copyIt) {
	    /* call rm just in case(e.g. N --> L)	*/ 
	    snprintf(cmd, sizeof(cmd),
		     "rm %s/smartlog/%s_%04hd%02hhd%02hhd"
		     "%02hhd%02hhd%02hhd_TAB102log_?.%s; "
		     "mkdir -p %s/smartlog && "
		     "cp %s %s/smartlog",
		     dstdir, g_hostname,
		     year, month, day, hh, mm, ss, ext,
		     dstdir, name, dstdir);
	    fprintf(stderr, "%s\n", cmd);
	    system(cmd);
	  }
	}
      }
    }
  }
  closedir(dir);
  
  return 0;
}
#else
int copy_smartlog(int src, int dst, int check_stop)
{
  DIR *dir;
  struct dirent *de;
  char name[256], name2[256];
  char tempname[256];
  char srcdir[128], dstdir[128];
  char line[512];
  unsigned short year, fileno;
  unsigned char month, day;
  unsigned char hh, mm, ss;
  char lock;
  struct stat sb, sb2;
  int copyIt;
  char cmd[512], ext[16];
  FILE* fr=NULL;
  FILE* fw=NULL;  
	  
  if (get_base_dir(srcdir, sizeof(srcdir),
		   disk_disklist[src].basedir.getstring())) {
    return 1;
  }
  if (get_base_dir(dstdir, sizeof(dstdir),
		   disk_disklist[dst].basedir.getstring())) {
    return 1;
  }

  snprintf(name, sizeof(name), "%s/smartlog", srcdir);

  dir = opendir(name);
  if (dir == NULL) {
    if (errno != ENOENT)
      fprintf(stderr, "opendir:%s(%s)\n", strerror(errno), name);
    return 1;
  }
  snprintf(name2,sizeof(name2),"%s/smartlog",dstdir);
  mkdir(name2, 0777);
  
  while ((de = readdir(dir)) != NULL) {
    /* asked to stop by ioprocess? */
    dio_get_nodiskcheck();
    if (check_stop && !g_nodiskcheck)
      break;
    
    int servernameLen = strlen(g_hostname);
    if (!strncmp(g_hostname, de->d_name, servernameLen) &&
	((int)strlen(de->d_name) > servernameLen)) {
      int ret = sscanf(de->d_name + servernameLen,
		       "_%04hd%02hhd%02hhd_%c.%03hd",
		       &year, &month, &day, &lock, &fileno);
      if (ret == 5) {
	//handle gps log file
	snprintf(name, sizeof(name), "%s/smartlog/%s",srcdir,de->d_name);	
	snprintf(name2, sizeof(name2),
		     "%s/smartlog/%s_%04hd%02hhd%02hhd_N.%03d",
		     dstdir, g_hostname, year, month, day,fileno);
        if (!stat(name2, &sb2)){
	    if(lock=='L'|| lock=='l'){
	        snprintf(tempname, sizeof(tempname),
		     "%s/smartlog/%s_%04hd%02hhd%02hhd_L.%03d",
		     dstdir, g_hostname, year, month, day,fileno);	 
		rename(name2, tempname);
	    }	 
	    snprintf(name2, sizeof(name2), "%s/smartlog/%s",dstdir,de->d_name);				    
	} else {
	    snprintf(name2, sizeof(name2),
		     "%s/smartlog/%s_%04hd%02hhd%02hhd_L.%03d",
		     dstdir, g_hostname, year, month, day,fileno);
	    if (stat(name2, &sb2)){
                snprintf(name2, sizeof(name2), "%s/smartlog/%s",dstdir,de->d_name);	      	      
	    }	  
	}	
	fr=fopen(name,"r");
	if(fr){
	   fw=fopen(name2,"a"); 
	   if(fw){
	        if(ftell(fw)>10){
		  while (fgets(line, sizeof(line), fr)){
		      if((line[0]=='9')&&(line[1]=='9')){
			 break;
		      }
		  }
		}
	        while (fgets(line, sizeof(line), fr)) {
		      fputs(line,fw);	   
		} 
	        fclose(fr);
		fclose(fw);
		remove(name);
	   } else {
	     fclose(fr); 
	   }
	}	
      } else { /* ret == 5 */
	ret = sscanf(de->d_name + servernameLen,
		     "_%04hd%02hhd%02hhd%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		     &year, &month, &day, &hh, &mm, &ss, &lock, ext);
	if (ret == 8) {
	  snprintf(name, sizeof(name), "%s/smartlog/%s", srcdir, de->d_name);
	  copyIt = 0;
	  /* get file size */
	  if (!stat(name, &sb)) {
	    //fprintf(stderr, "%s:%lu\n", name, sb.st_size);
	    /* get file size of the same file on destination */
	    snprintf(name2, sizeof(name2),
		     "%s/smartlog/%s_%04hd%02hhd%02hhd"
		     "%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		     dstdir, g_hostname,
		     year, month, day, hh, mm, ss, lock, ext);
	    if (!stat(name2, &sb2)) {
	      //fprintf(stderr, "name2:%s:%lu\n", name2, sb2.st_size);
	      if (sb.st_size > sb2.st_size) {
		copyIt = 1;
	      }
	    } else {
	      /* Can't find the same name, try different lock type */
	      snprintf(name2, sizeof(name2),
		       "%s/smartlog/%s_%04hd%02hhd%02hhd"
		       "%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		       dstdir, g_hostname,
		       year, month, day, hh, mm, ss,
		       ((lock == 'N') || (lock == 'n')) ? 'L' : 'N', ext);
	      if (!stat(name2, &sb2)) {
		if (sb.st_size == sb2.st_size) {
		  if ((lock == 'N') || (lock == 'n')) {
		    /* N --> L, same size: rename it (L-->N) */
		    char newname[256];
		    snprintf(newname, sizeof(newname),
			     "%s/smartlog/%s_%04hd%02hhd%02hhd"
			     "%02hhd%02hhd%02hhd_TAB102log_N.%s",
			     dstdir, g_hostname,
			     year, month, day, hh, mm, ss, ext);
		    rename(name2, newname);
		  } else {
		    /* L --> N, same size: do nothing */
		  }
		} else {
		  if ((lock == 'N') || (lock == 'n')) {
		    /* N --> L, different size */
		    copyIt = 1;
		  } else {
		    /* L --> N, different size: do nothing */
		  }
		}
	      } else { /* no file found */
		/* N,L --> none */
		copyIt = 1;
	      }
	    }
	  }
	  
	  if (copyIt) {
	   // dvr_log("copyIT1");
	    /* call rm just in case(e.g. N --> L)	*/ 
	    /*
	    snprintf(cmd, sizeof(cmd),
		     "rm %s/smartlog/%s_%04hd%02hhd%02hhd"
		     "%02hhd%02hhd%02hhd_TAB102log_?.%s; "
		     "mkdir -p %s/smartlog && "
		     "cp %s %s/smartlog",
		     dstdir, g_hostname,
		     year, month, day, hh, mm, ss, ext,
		     dstdir, name, dstdir);
	    
	    fprintf(stderr, "%s\n", cmd);
	    dvr_log("cmd:%s",cmd);
	    system(cmd);*/
	    snprintf(cmd,sizeof(cmd),"rm %s/smartlog/%s_%04hd%02hhd%02hhd"
		     "%02hhd%02hhd%02hhd_TAB102log_?.%s",dstdir, g_hostname,
		     year, month, day, hh, mm, ss, ext); 
          //  dvr_log("cmd:%s",cmd);		     
            system(cmd);
	    sleep(1);
	    memset(cmd,0,sizeof(cmd));
	    snprintf(cmd,sizeof(cmd),"cp %s %s/smartlog",name, dstdir);
	  //  dvr_log("cmd:%s",cmd);
	    system(cmd);
	  }
	}
      }
    }
  }
  closedir(dir);
  
  return 0;
}
#endif
int getDisklistIdfromBasedir(char *base_dir_with_busname)
{
  int i;

  for (i = 0; i < disk_disklist.size(); i++) {
    if (!strcmp(disk_disklist[i].basedir.getstring(),
		base_dir_with_busname)) {
      return i;
    }
  }

  return -1;
}      

/*
 * copy smartlog file.
 * ex) 1->2, 1->3, 2->1, 2->3, 3->1, 3->2
 */
void sync_smartlog(int totalMulti)
{
  int i, j, idsrc, iddst;

  for (i = 0; i < totalMulti; i++) {
    for (j = 0; j < totalMulti; j++) {
      if (i == j)
	continue;
      idsrc = getDisklistIdfromMultipleDiskId(i);
      iddst = getDisklistIdfromMultipleDiskId(j);
      if ((idsrc >= 0) && (iddst >= 0)) {
	/* stop glog? */
	copy_smartlog(idsrc, iddst, 0);
      }
    }
  }
}

/*
 * copy smartlog file.
 * ex) sata->flash, flash->sata
 */
void sync_smartlog_for_hybrid(char *disk_a, char *disk_b, int check_stop)
{
  int idsrc, iddst;

  idsrc = getDisklistIdfromBasedir(disk_a);
  iddst = getDisklistIdfromBasedir(disk_b);
  if ((idsrc >= 0) && (iddst >= 0)) {
    /* disk_a --> disk_b */
//    dvr_log("start copy smartlog:idsrc:%d iddst:%d",idsrc, iddst);
    copy_smartlog(idsrc, iddst, check_stop);
    /* disk_b --> disk_a */
  //  copy_smartlog(iddst, idsrc, check_stop);
  }
}

int is_free_and_writable(int idx)
{
  int space = disk_freespace(disk_disklist[idx].basedir.getstring());
  if (space > disk_minfreespace) {
    if (disk_testwritable(disk_disklist[idx].basedir.getstring())) {
      return 1;
    }    
  }

  return 0;
}

int find_first_filetime(int idx, struct day_time *dt)
{
  int j;
  int firstfiletime, firstday;
  
  if (!dt)
    return 1;

  dt->day = firstday = 21000101;
  dt->time = firstfiletime = 240000;

  /* sanity check */
  if ((idx < 0) || (idx >= disk_disklist.size())) {
    return 1;
  }

  array <disk_day_info> * ddi = & (disk_disklist[idx].daylist) ;
  for (j = 0; j < ddi->size(); j++) {
    if ((*ddi)[j].nfilelen > 0) {
      if ((*ddi)[j].bcdday < firstday ||
	  ((*ddi)[j].bcdday == firstday &&
	   (*ddi)[j].nfiletime < firstfiletime)) {
	firstday = (*ddi)[j].bcdday;
	firstfiletime = (*ddi)[j].nfiletime;
      }
    }
    
    if ((*ddi)[j].lfilelen > 0) {
      if ((*ddi)[j].bcdday < firstday ||
	  ((*ddi)[j].bcdday == firstday &&
	   (*ddi)[j].lfiletime < firstfiletime)) {
	firstday = (*ddi)[j].bcdday;
	firstfiletime = (*ddi)[j].lfiletime;
      }
    }
  }

  dt->day = firstday;
  dt->time = firstfiletime;

  return 0;
}

int find_last_filetime(int idx, struct day_time *dt)
{
  int j;
  int lastfiletime, lastday;
  
  if (!dt)
    return 1;

  dt->day = lastday = 0;
  dt->time = lastfiletime = 0;

  /* sanity check */
  if ((idx < 0) || (idx >= disk_disklist.size())) {
    return 1;
  }

  array <disk_day_info> * ddi = & (disk_disklist[idx].daylist) ;
  for (j = 0; j < ddi->size(); j++) {
    if ((*ddi)[j].nfilelen > 0) {
      if ((*ddi)[j].bcdday > lastday ||
	  ((*ddi)[j].bcdday == lastday &&
	   (*ddi)[j].nfiletime_l > lastfiletime)) {
	lastday = (*ddi)[j].bcdday;
	lastfiletime = (*ddi)[j].nfiletime_l;
      }
    }
    
    if ((*ddi)[j].lfilelen > 0) {
      if ((*ddi)[j].bcdday > lastday ||
	  ((*ddi)[j].bcdday == lastday &&
	   (*ddi)[j].lfiletime_l > lastfiletime)) {
	lastday = (*ddi)[j].bcdday;
	lastfiletime = (*ddi)[j].lfiletime_l;
      }
    }
  }

  dt->day = lastday;
  dt->time = lastfiletime;

  return 0;
}

int compare_daytime(struct day_time *a, struct day_time *b)
{
  if (a->day != b->day)
    return a->day - b->day;

  return a->time - b->time;
}

/*
 * find the disk with the latest file,
 * and check if it can be the recording disk
 * 1. no files on any disk: disk 1 
 */
int findCurrentMultiDisk(int totalMulti)
{
  int id, idx;
  int lastdisk;
  int nextdisk = -1;
  struct day_time lastfile, dt, dt2;

  lastfile.day = 0;
  lastfile.time = 0;
  lastdisk = -1;

  for (id = 0; id < totalMulti; id++) {
    idx = getDisklistIdfromMultipleDiskId(id);

    if (idx < 0)
      continue;

    find_last_filetime(idx, &dt);

    if (compare_daytime(&dt, &lastfile) > 0) {
      lastfile.day = dt.day;
      lastfile.time = dt.time;
      lastdisk = idx;
    }
  }

  if (lastdisk == -1) {
    lastdisk = getDisklistIdfromMultipleDiskId(0);
  }

  /* check free space */
  if (is_free_and_writable(lastdisk)) {
    return lastdisk;
  }

  /* disk is full, check if can delete files on the current.
   * for this, we need to compare:
   * 1. oldest file on the current disk
   * 2. oldest file on the next disk
   */
  nextdisk = getNextMultipleDiskId(lastdisk);
  if (nextdisk < 0) { /* should not happen */
    return lastdisk; 
  }

  find_first_filetime(lastdisk, &dt);
  find_first_filetime(nextdisk, &dt2);

  if (dt2.day == 21000101) {
    /* no files found on the next disk, switch to next */
    return nextdisk;
  }

  if (compare_daytime(&dt, &dt2) < 0) {
    /* older files found, delete files on the last disk */
    return lastdisk;
  }

  return nextdisk;
}

void checkrecordablepartition()
{
    int i;
    int recordablepartition=-1;
    int size;
    int freesize;
    int total_n, total_l ;
    int firstndisk, firstldisk ;
    int loop;
    int numofgooddisk=0;
    disk_day_info firstl, firstn ;  
    for( i=0; i<disk_disklist.size(); i++ ) {
	if(disk_disklist[i].status)
	  continue;
	numofgooddisk++;
    }
    if(numofgooddisk<2){
        return; 
    }
    for( i=0; i<disk_disklist.size(); i++ ) {
	if(disk_disklist[i].status)
	  continue;
	freesize=disk_freespace( disk_disklist[i].basedir.getstring());
	size=disk_size(disk_disklist[i].basedir.getstring());
	if(freesize>disk_minfreespace) {		
	    if( disk_testwritable( disk_disklist[i].basedir.getstring() ) ) {
                 recordablepartition=i;
		 break;
	    }
	}
    }//for   
    if(recordablepartition<0){
	// first, let see what kind of file to delete, _L_ or _N_
	for(loop=0;loop<50;++loop){
	      firstl.bcdday=21000101 ;
	      firstn.bcdday=21000101 ;
	      total_n=0 ;
	      total_l=0 ;
	      firstndisk=-1;
	      firstldisk=-1;
	      dio_kickwatchdog ();
	      for( i=0; i<disk_disklist.size(); i++ ) {
		    if(disk_disklist[i].status)
		      continue;
		    array <disk_day_info> * ddi = & (disk_disklist[i].daylist) ;
		    int j;
		    for(j=0; j<ddi->size(); j++) {
			if( (*ddi)[j].nfilelen > 0 ) {
			    if( (*ddi)[j].bcdday < firstn.bcdday ||
			      ((*ddi)[j].bcdday == firstn.bcdday && (*ddi)[j].nfiletime < firstn.nfiletime ) )
			    {
				firstn = (*ddi)[j] ;
				firstndisk = i ;
			    }
			    total_n+= (*ddi)[j].nfilelen ;
			}
			if( (*ddi)[j].lfilelen > 0 ) {
			    if( (*ddi)[j].bcdday < firstl.bcdday ||
			      ((*ddi)[j].bcdday == firstl.bcdday && (*ddi)[j].lfiletime < firstl.lfiletime ) )
			    {
				firstl = (*ddi)[j] ;
				firstldisk = i ;
			    }
			    total_l+= (*ddi)[j].lfilelen ;
			}
		    }
	      }
	      
	      if( total_n<=0 && total_l<=0 ) {       // on video files at all !!!?
		  dvr_log("no video files to be used to deleted");
		  return;
	      }
	      if( total_l>0 )  {
		  if( firstl.bcdday<firstn.bcdday ) {
		      firstn = firstl ;
		      firstndisk = firstldisk ;
		  }
	      }
	      if(firstndisk>=0){
		  formatflash(disk_disklist[firstndisk].basedir.getstring());
		  disk_disklist[firstndisk].formated++;
		  disk_disklist[firstndisk].daylist.empty();	
		  if(disk_disklist[firstndisk].formated>MAX_FORMAT_NUM){
		      disk_disklist[firstndisk].status=1;
		  } else {
		      if(disk_testwritable(disk_disklist[firstndisk].basedir.getstring())){
			  break;
		      } 
		      disk_disklist[firstndisk].status=1;
		  }
	      } else {
		 break;
	      }
	}
    }
    return;  
}

void checkfreepartition()
{
    int i;
    int freepartition=-1;
    int size;
    int freesize;
    int total_n, total_l ;
    int firstndisk, firstldisk ;
    int loop;
    disk_day_info firstl, firstn ;  
    int numofgooddisk=0; 
    for( i=0; i<disk_disklist.size(); i++ ) {
	if(disk_disklist[i].status)
	  continue;
	numofgooddisk++;
    }
    if(numofgooddisk<2){
        return; 
    }
    
    for( i=0; i<disk_disklist.size(); i++ ) {
	if(disk_disklist[i].status)
	  continue;
	freesize=disk_freespace( disk_disklist[i].basedir.getstring());
	size=disk_size(disk_disklist[i].basedir.getstring());
	if(freesize>size/2) {		
	    if( disk_testwritable( disk_disklist[i].basedir.getstring() ) ) {
                 freepartition=i;
		 break;
	    }
	}
    }//for   
    if(freepartition<0){
	// first, let see what kind of file to delete, _L_ or _N_
	for(loop=0;loop<50;++loop){
	      firstl.bcdday=21000101 ;
	      firstn.bcdday=21000101 ;
	      total_n=0 ;
	      total_l=0 ;
	      firstndisk=-1;
	      firstldisk=-1;
	      dio_kickwatchdog ();
	      for( i=0; i<disk_disklist.size(); i++ ) {
		    if(disk_disklist[i].status)
		      continue;
		    array <disk_day_info> * ddi = & (disk_disklist[i].daylist) ;
		    int j;
		    for(j=0; j<ddi->size(); j++) {
			if( (*ddi)[j].nfilelen > 0 ) {
			    if( (*ddi)[j].bcdday < firstn.bcdday ||
			      ((*ddi)[j].bcdday == firstn.bcdday && (*ddi)[j].nfiletime < firstn.nfiletime ) )
			    {
				firstn = (*ddi)[j] ;
				firstndisk = i ;
			    }
			    total_n+= (*ddi)[j].nfilelen ;
			}
			if( (*ddi)[j].lfilelen > 0 ) {
			    if( (*ddi)[j].bcdday < firstl.bcdday ||
			      ((*ddi)[j].bcdday == firstl.bcdday && (*ddi)[j].lfiletime < firstl.lfiletime ) )
			    {
				firstl = (*ddi)[j] ;
				firstldisk = i ;
			    }
			    total_l+= (*ddi)[j].lfilelen ;
			}
		    }
	      }
	      
	      if( total_n<=0 && total_l<=0 ) {       // on video files at all !!!?
		  dvr_log("no video files to be used to deleted");
		  return;
	      }
	      if( total_l>0 )  {
		  if( firstl.bcdday<firstn.bcdday ) {
		      firstn = firstl ;
		      firstndisk = firstldisk ;
		  }
	      }
	      if(firstndisk>=0){
		  if(strcmp(rec_basedir.getstring(), disk_disklist[firstndisk].basedir.getstring())!=0 ){
		      formatflash(disk_disklist[firstndisk].basedir.getstring());
		      disk_disklist[firstndisk].formated++;
		      disk_disklist[firstndisk].daylist.empty();	
		      if(disk_disklist[firstndisk].formated>MAX_FORMAT_NUM){
			 disk_disklist[firstndisk].status=1;
		      } else {
			  if(disk_testwritable(disk_disklist[firstndisk].basedir.getstring())){
			      break;
			  } 
			  disk_disklist[firstndisk].status=1;
		      }
		  } else {
		      break;
		  }
	      } else {
		 break;
	      }
	}
    }
    return;
}

/* find a recordable disk
 *  1. find disk with disk space available
 *  2. delete old video file and return disk with space available
 * return index on disk_disklist, -1 for error
 */
int disk_findrecdisk(int prerun)
{
    int i;
    int loop ;
    int dlock ;
    int total_n, total_l ;
    int firstndisk, firstldisk ;
    disk_day_info firstl, firstn ;
    struct dvrtime oldesttime ;
    char diskday[512] ;
    string oldestfilename ;
    int n;
    int mFree;

    n = disk_disklist.size();
    if (n <= 0) {
      dvr_log("disk size=%d",n);
      return -1;
    }

    if (recordingDisk < 0) { /* current disk not set yet */
	int index=-1;  
        for(i=0;i<disk_disklist.size();++i){
	   if(disk_disklist[i].status)
	     continue;
	   if(disk_disklist[i].lastrecorded){
	      index=i;
	      break;
	   }	  
	}
        if(index>=0){
	      if( disk_freespace( disk_disklist[index].basedir.getstring()) > disk_minfreespace ) {		
		  if( disk_testwritable( disk_disklist[index].basedir.getstring() ) ) {
		    recordingDisk = index;
		   // dvr_log("Recording on latest recording partition");
		    return index ;              // writable disk!
		  }
	      }	   	  
	}
	for( i=0; i<disk_disklist.size(); i++ ) {
	    if(disk_disklist[i].status)
	      continue;
	    if( disk_freespace( disk_disklist[i].basedir.getstring()) > disk_minfreespace ) {		
		if( disk_testwritable( disk_disklist[i].basedir.getstring() ) ) {
		//  dvr_log("Recording on partition with enough free space");
		  recordingDisk = i;
		  return i ;              // writable disk!
		}
	    }
	}//for
    } else {       
 	for( i=0; i<disk_disklist.size(); i++ ) {
	    if(disk_disklist[i].status)
	       continue;
	    if( disk_freespace( disk_disklist[i].basedir.getstring()) > disk_minfreespace ) {		
		if( disk_testwritable( disk_disklist[i].basedir.getstring() ) ) {
		//  dvr_log("Recording on partition with enough free space when full");
		  recordingDisk = i;
		  return i ;              // writable disk!
		}
	    }
	}//for
    }//else
      
#if 1      
    for( loop=0; loop<50; loop++ ) {
            
 	// so, none of the disks has space
	dio_kickwatchdog ();
	
	// first, let see what kind of file to delete, _L_ or _N_
	
	firstl.bcdday=21000101 ;
	firstn.bcdday=21000101 ;
	total_n=0 ;
	total_l=0 ;
	firstndisk=0;
	firstldisk=0;
	//find the disk with oldest data
	for( i=0; i<disk_disklist.size(); i++ ) {
	    if(disk_disklist[i].status)
	      continue;
	    array <disk_day_info> * ddi = & (disk_disklist[i].daylist) ;
	    int j;
	    for(j=0; j<ddi->size(); j++) {
		if( (*ddi)[j].nfilelen > 0 ) {
		    if( (*ddi)[j].bcdday < firstn.bcdday ||
		      ((*ddi)[j].bcdday == firstn.bcdday && (*ddi)[j].nfiletime < firstn.nfiletime ) )
		    {
			firstn = (*ddi)[j] ;
			firstndisk = i ;
		    }
		    total_n+= (*ddi)[j].nfilelen ;
		}
		if( (*ddi)[j].lfilelen > 0 ) {
		    if( (*ddi)[j].bcdday < firstl.bcdday ||
		      ((*ddi)[j].bcdday == firstl.bcdday && (*ddi)[j].lfiletime < firstl.lfiletime ) )
		    {
			firstl = (*ddi)[j] ;
			firstldisk = i ;
		    }
		    total_l+= (*ddi)[j].lfilelen ;
		}
	    }
	}
	
	if( total_n<=0 && total_l<=0 ) {       // on video files at all !!!?
	    dvr_log("no video files to be used to deleted");
	    return -1 ;
	}
	dlock=0 ;
	if( total_l>0 )  {
	    struct dvrtime dvrt ;
	    time_now(&dvrt);
	    if( total_n<=0 ||
	      ((dvrt.year*12+dvrt.month)-((firstl.bcdday/10000)*12+(firstl.bcdday/100%100)))>6 ||       // 6 month old locked file?
	      ( total_l>(total_n)/2 && firstl.bcdday < firstn.bcdday ) )           // locked lengh > 30%
	    {
		dlock=1 ;
	    }
	}

	if( dlock )             // allow to delete locked file
	{
	    if( firstl.bcdday<firstn.bcdday ) {
		firstn = firstl ;
		firstndisk = firstldisk ;
	    }
	}
	
	// delete files on <firstn>
	sprintf( diskday, "%s/%08d", disk_disklist[firstndisk].basedir.getstring(), firstn.bcdday );
	
	time_dvrtime_init(&oldesttime, 2099);
	
	if( disk_olddestfile( diskday, &oldesttime, &oldestfilename, dlock) ) {
	  //  dvr_log("Deleting: %s", oldestfilename.getstring());
	    disk_removefile( oldestfilename.getstring() );
	    disk_rmemptydir(diskday);
	  //  disk_rmempty264dir( diskday );
	    disk_renewdiskdayinfo( firstndisk, firstn.bcdday ) ;
	}
	recordingDisk=firstndisk;           
	if(disk_size(disk_disklist[recordingDisk].basedir.getstring())<MIN_PARTITION_SIZE)
	{
	    dvr_log("disk size is wrong");
	    return -1;
	}
	
	mFree=disk_freespace( disk_disklist[recordingDisk].basedir.getstring());
	if( mFree > disk_minfreespace ) {
	      if( disk_testwritable( disk_disklist[recordingDisk].basedir.getstring() ) ) {
		  disk_disklist[recordingDisk].notwritable=0;
		//  dvr_log("Recording on the partition with oldest recording files");
		  return recordingDisk ;              // writable disk!
	      } else {
		  disk_disklist[recordingDisk].notwritable++;
		  dvr_log("%s is not writable",disk_disklist[recordingDisk].basedir.getstring());
		  if(disk_disklist[recordingDisk].notwritable>MAX_UNWRITABLE_NUM){
			copydvrlogtomemory(disk_disklist[recordingDisk].basedir.getstring()); 
			formatflash(disk_disklist[recordingDisk].basedir.getstring());
			disk_disklist[recordingDisk].formated++;
			if(disk_disklist[recordingDisk].formated>MAX_FORMAT_NUM){
			    disk_disklist[recordingDisk].status=1;
			    dvr_log("%s becomes bad partition",disk_disklist[recordingDisk].basedir.getstring());
			} else {
			    if( disk_testwritable( disk_disklist[recordingDisk].basedir.getstring() ) ) {
				disk_disklist[recordingDisk].notwritable=0;
				copydvrlogfrommemory(disk_disklist[recordingDisk].basedir.getstring());
				return recordingDisk;
			    }
			    disk_disklist[recordingDisk].status=1;
			    dvr_log("%s becomes bad partition",disk_disklist[recordingDisk].basedir.getstring());
			}
		  }		      
	      }  	  
	} else if(mFree<0){
	    dvr_log("disk status is wrong");
	    return -1;
	}
    }  
#endif    
    dvr_log("can't find writable disk");
    return -1;    
}

/* Read NMEMB elements of SIZE bytes into PTR from STREAM.  Returns the
 * number of elements read, and a short count if an eof or non-interrupt
 * error is encountered.  */
static size_t safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret = 0;

	do {
		clearerr(stream);
		ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
	} while (ret < nmemb && ferror(stream) && errno == EINTR);

	return ret;
}

/* Write NMEMB elements of SIZE bytes from PTR to STREAM.  Returns the
 * number of elements written, and a short count if an eof or non-interrupt
 * error is encountered.  */
size_t safe_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret = 0;
  
  do {
    clearerr(stream);
    ret += fwrite((char *)ptr + (ret * size), size, nmemb - ret, stream);
  } while (ret < nmemb && ferror(stream) && errno == EINTR);
  
  return ret;
}

/* Read a line or SIZE - 1 bytes into S, whichever is less, from STREAM.
 * Returns S, or NULL if an eof or non-interrupt error is encountered.  */
static char *safe_fgets(char *s, int size, FILE *stream)
{
	char *ret;

	do {
		clearerr(stream);
		ret = fgets(s, size, stream);
	} while (ret == NULL && ferror(stream) && errno == EINTR);

	return ret;
}

int disk_capacitycheck(char *path)
{
	int disksize=0;  	
	int i;	
	for(i=0;i<3;++i){
	    disksize=disk_size(path);
	    if(disksize>MIN_PARTITION_SIZE)
	      break;
            sleep(2);
	}  
	if(disksize<MIN_PARTITION_SIZE){
	   return -1;
	}
	return 0;
}

void get_disk_id(char *path, int *type, int *id, int *ndisk)
{
  *type = HD_SIMPLE;
  *id = DT_FLASH;
  *ndisk = 1; 
}

#if 0
void get_disk_id(char *path, int *type, int *id, int *ndisk)
{
  char buf[256];
  FILE *fp;

  *type = HD_SIMPLE;
  *id = 0;
  *ndisk = 1;

  snprintf(buf, sizeof(buf),
	   "%s/diskid", path);
  //dvr_log("open diskid file %s",buf);
  fp = fopen(buf, "r");
  if (fp) {
    if (safe_fgets(buf, sizeof(buf), fp)) {
     // dvr_log("type=%c%c%c%c",buf[0],buf[1],buf[2],buf[3]);
      if (!strncasecmp(buf, "SATA", 4)) {
	*type = HD_HYBRID;
	*id = DT_SATA;
	*ndisk = 2;
      } else if (!strncasecmp(buf, "FLASH", 5)) {
	*type = HD_HYBRID;
	*id = DT_FLASH;
	*ndisk = 2;
      } else {
	if (sscanf(buf, "%d/%d", id, ndisk) == 2) {
	  if (id > 0) {
	    /* change id to index from 0: 1,2,3-->0,1,2 */
	    (*id)--;
	  }
	  *type = HD_MULTIPLE;
	} else {
	  *id = 0;
	  *ndisk = 1;
	}
      }
    } else{
       printf("can't get ID\n");
    }
    fclose(fp);
  } 
}

void get_disk_id(char *path, int *type, int *id, int *ndisk)
{
  char buf[256];
  FILE *fp;

  *type = HD_SIMPLE;
  *id = 0;
  *ndisk = 1;
  if(gHBD_Recording){
        struct statfs stfs; 
	int disk_size=0;
	*type = HD_HYBRID;
	*ndisk = 2;
	if (statfs(path, &stfs) == 0){
	    disk_size=stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
	}
	int i;
	for(i=0;i<3;++i){
	    if(disk_size<5*1024){	   
		sleep(1);
		if (statfs(path, &stfs) == 0){
		    disk_size=stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
		}	   
	    } else {
	       break; 
	    }
	}
	if(disk_size>128*1024){
	    *id = DT_SATA;
	} else if(disk_size>5*1024){
	  *id = DT_FLASH;
	} else {  	  
	  *id=DT_UNGET;
	}	
  } else {
        struct statfs stfs; 
	int disk_size=0;
	if (statfs(path, &stfs) == 0){
	    disk_size=stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
	}
	int i;
	for(i=0;i<3;++i){
	    if(disk_size<5*1024){	   
		sleep(1);
		if (statfs(path, &stfs) == 0){
		    disk_size=stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
		}	   
	    } else {
	       break; 
	    }
	}
	if(disk_size>128*1024){
	    *id = DT_SATA;
	} else if(disk_size>5*1024){
	  *id = DT_FLASH;
	} else {  	  
	  *id=DT_UNGET;
	}	    
  }
}
#endif
// return 1: success, 0:failed
int disk_scandisk( int diskindex )
{
    int day ;
    
    if( diskindex<0 || diskindex>=disk_disklist.size() ){
        return 0 ;
    }

    mkdir(disk_disklist[diskindex].basedir.getstring(), S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);	
    if( !disk_testwritable( disk_disklist[diskindex].basedir.getstring() ) ) {
        return 0 ;              // no writable disk, return failed
    }

    // updating day lists
    disk_disklist[diskindex].daylist.empty();

    dir_find findbase(disk_disklist[diskindex].basedir.getstring());
    if( !findbase.isopen() ) {
        return 0;               // can't open recording root directory? I just create it
    }
    
    while( findbase.find() ) {
        if( findbase.isdir() ) {
            if( sscanf( findbase.filename( ), "%08d", &day )== 1 ) {
                if( day>19990000 && day<20990000 ) { 		// found a day
                    disk_renewdiskdayinfo( diskindex, day );
                }
            }
        }
    }
    findbase.close();
    return 1 ;
}

void FormatFlashToFAT32(char* diskname)
{  
    char devname[60];
    int ret=0;
    pid_t format_id;
    char* ptemp;
    
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
int checklastrecord(char* diskname)
{  	
      char filename[256];
      FILE* fr;
      snprintf(filename,sizeof(filename),"%s/record",diskname);
      fr=fopen(filename,"r");
      if(fr){
	 fclose(fr);
	 return 1;
      }
      return 0;
}
void checkdvrlogfile(char* pathname)
{
    char filename[256];
    char filename1[256];
    FILE* fr;
    snprintf(filename,sizeof(filename),"%s/dvrlog1.txt",pathname);
    fr=fopen(filename,"r");
    if(fr){      
	fclose(fr);
	snprintf(filename1,sizeof(filename1),"%s/dvrlog.txt",pathname);
	rename(filename,filename1);
	dvr_log("%s was renamed to %s",filename,filename1);
    }
    return;
}

void copydvrlogtomemory(char* pathname)
{
    char filename[256];
    char line[512];
    FILE* fr;
    FILE* fw;
    snprintf(filename,sizeof(filename),"%s/dvrlog.txt",pathname);
    fr=fopen(filename,"r");
    if(fr){
	fw=fopen("/var/dvr/dvrlog_back","w");
	if(!fw){
	    fclose(fr);
	    return;
	}
        while (fgets(line, sizeof(line), fr)) {
	      fputs(line,fw);	   
	} 
        fclose(fr);
	fclose(fw);
    }
    return;
}

void copydvrlogfrommemory(char* pathname)
{
    char filename[256];
    char line[512];
    FILE* fr;
    FILE* fw;
    fr=fopen("/var/dvr/dvrlog_back","r");   
    if(fr){
        snprintf(filename,sizeof(filename),"%s/dvrlog.txt",pathname);
	fw=fopen(filename,"w");	
	if(!fw){
	    fclose(fr);
	    unlink("/var/dvr/dvrlog_back");
	    return;
	}
        while (fgets(line, sizeof(line), fr)) {
	      fputs(line,fw);	   
	} 
        fclose(fr);
	fclose(fw);
	unlink("/var/dvr/dvrlog_back");
    }
    return;  
}

// scanning all disks, update disk list, check and repair _0_ files
void disk_scanalldisk(int partial)
{
    struct stat basestat;
    struct stat diskstat;
    static int diskfailedNum=0;    
    char * diskname;
    char diskbase[512] ;
    int  disk_check ;
    int i;
    int firstscan=0;
    
    if (stat(disk_base.getstring(), &basestat) != 0) {
	if (!partial)
	  dvr_log("Disk base error, not able to record!");
	  return;
    }
    
    disk_lock(); 
    for( i=0; i<disk_disklist.size(); i++ ) {
        disk_disklist[i].mark=0 ;
    }
    //dvr_log("disk scanalldisk");
    dir_find disks(disk_base.getstring());
    while( disks.find() ) {
        if( disks.isdir() ) {
            diskname = disks.pathname();
            if( stat(diskname, &diskstat)==0 ) {
                if( diskstat.st_dev != basestat.st_dev) {	// found a mount point
                    disk_check = 0 ;
                    for( i=0; i<disk_disklist.size(); i++ ) {
                        if( disk_disklist[i].dev == diskstat.st_dev ) {     // already in list
                            if(disk_disklist[i].status){
				disk_check=1;
			    } else if(disk_disklist[i].scanfail==0){
			        disk_check=1;
			    }
			    disk_disklist[i].mark=1 ;
                            break;
                        }
                    }
                    if( disk_check ) 
                        continue ;
                    if(disk_capacitycheck(diskname)<0){
		       continue;
		    }      
		    firstscan=0;
		    if(i==disk_disklist.size()){
		     // dvr_log("%s is first time scanned",diskname);
		      firstscan=1;
		    }
		  //  dvr_log("i=%d array size=%d",i,disk_disklist.size());
                    // do disk fix here ?
		    //dvr_log("fix:%s",diskname);
                    disk_diskfix(diskname);
                   // dvr_log("fix done");
                    disk_disklist[i].dev = diskstat.st_dev ;
                    disk_disklist[i].mark=1 ;
                    sprintf( diskbase, "%s/_%s_", diskname, g_hostname );
                    disk_disklist[i].basedir = diskbase ;
                    disk_disklist[i].daylist.empty();
		    get_disk_id(diskname,
				&disk_disklist[i].type,
				&disk_disklist[i].id,
				&disk_disklist[i].ndisk);
		    fprintf(stderr, "Detected:%s(type:%d,%d/%d),listsize:%d\n",
			    diskname,
			    disk_disklist[i].type,
			    disk_disklist[i].id + 1,
			    disk_disklist[i].ndisk,
			    i + 1);
		    if(firstscan){
		       disk_disklist[i].status=0;
		       disk_disklist[i].notwritable=0;
		       disk_disklist[i].scanfail=0;
		       disk_disklist[i].formated=0;
		       disk_disklist[i].lastrecorded=checklastrecord(disk_disklist[i].basedir.getstring());
		    }
		    
                    if( disk_scandisk( i )<=0 ) {  // failed ?
		        dvr_log("Scan failed(%d): %s",partial,diskname );
                        disk_disklist[i].scanfail++;
			if(disk_disklist[i].scanfail>MAX_SCAN_FAIL){
				dvr_log("Disk is corrupted, and start to format Disk");
				copydvrlogtomemory(disk_disklist[i].basedir.getstring());
				FormatFlashToFAT32(diskname);					
				disk_disklist[i].formated++;
				if(disk_disklist[i].formated>=MAX_FORMAT_NUM){
				    disk_disklist[i].status=1;
				    disk_disklist[i].scanfail=0;				  
				    dvr_log("Partition:%s become bad",diskname);
				} else {
				    if( disk_scandisk( i )<=0 ){
					disk_disklist[i].status=1;
					disk_disklist[i].scanfail=0;				  
					dvr_log("Partition:%s become bad",diskname);				      
				    } else {
					  disk_disklist[i].scanfail=0;
					  copydvrlogfrommemory(disk_disklist[i].basedir.getstring());
					  dvr_log("Detected hard drive(%d): %s",partial,diskname );				     				     
				    }				  
				}
			}					
                    }
                    else {
		      disk_disklist[i].scanfail=0;
		      dvr_log("Detected hard drive(%d): %s",partial,diskname );
                    }
                }
            }
        }
    }

    for( i=0; i<disk_disklist.size(); ) {
        if( disk_disklist[i].mark==0 ) {
            disk_disklist.remove(i) ;
        }
        else {
            i++ ;
        }
    }
    //dvr_log("disk_scanalldisk done");
    disk_unlock();
}

int all_disks_detected()
{
  int i;
  char found[MAX_DISK];
  int hybrid_detected = 0, multiple_detected = 0;

  memset(found, 0, MAX_DISK);

  for (i = 0; i < disk_disklist.size(); i++) {
    if (disk_disklist[i].type == HD_HYBRID) {
      if (disk_disklist[i].ndisk > hybrid_detected) 
	hybrid_detected =  disk_disklist[i].ndisk;
      found[disk_disklist[i].id] = disk_disklist[i].ndisk;
    } else if (disk_disklist[i].type == HD_MULTIPLE) {
      if (disk_disklist[i].ndisk > multiple_detected) 
	multiple_detected =  disk_disklist[i].ndisk;
      found[disk_disklist[i].id] = disk_disklist[i].ndisk;
    }
  }

  fprintf(stderr, "hybrid_detected:%d, multiple_detected:%d, disks:%d\n",
	  hybrid_detected, multiple_detected, disk_disklist.size());

  /* hard disk not inserted? */
  if (!hybrid_detected &&
      !multiple_detected && 
      (disk_disklist.size() == 0))
    return 0;

  for (i = 0; i < hybrid_detected; i++) {
    if (found[i] == 0) {
      fprintf(stderr, "Hybrid %d/%d missing\n", i + 1, hybrid_detected);
      return 0;
    }
  }

  for (i = 0; i < multiple_detected; i++) {
    if (found[i] == 0) {
      fprintf(stderr, "Multiple %d/%d missing\n", i + 1, multiple_detected);
      return 0;
    }
  }

  return 1;
}
#if 0
int copy_tab102_data(char *mount_dir)
{
  char path[256];
  int ret = 1, copy = 0;

  snprintf(path, sizeof(path), "%s/smartlog", mount_dir);
  if(mkdir(path, 0777) == -1 ) {
    if(errno == EEXIST) {
      copy = 1; // directory exists
    }
  } else {
    copy = 1; // directory created
  }
  
  if (copy) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
	     "mv -f /var/dvr/*.log /var/dvr/*.peak %s",
	     path);
    system(cmd);
    ret = 0;
  }
  
  return ret;
}

void check_tab102_data(char *path)
{
  char line[64];
  FILE *fp;   
  char *str = NULL;
  char base[256];

  if (get_base_dir(base, sizeof(base), path))
    return;

  fp = fopen("/var/dvr/tab102", "r");
  if (fp) {
    str = safe_fgets(line, sizeof(line), fp);
    fclose(fp);
  }

  if (str && !strncmp(str, "data", 4)) {
    if(!copy_tab102_data(base)) {
      //writeTab102Status("copy");
    }
  }
}
#endif

static int isHdDetected()
{
  int fd, bytes;
  char buf[256];
  char *ptr, *line;

  fd = open("/proc/partitions", O_RDONLY);
  if (fd == -1)
    return 0;

  bytes = read(fd, buf, sizeof(buf));

  line = buf;
  while (1) {
    ptr = strchr(line, 0x0a);
    if (!ptr)
      break;
    *ptr = 0;

    if (strstr(line, " sd")) { /* look for sda, sdb, etc */
      close(fd);
      return 1;
    }

    if (line < buf + bytes)
      line = ptr + 1;
    else
      break;
  }

  close(fd);
  return 0;
}

void detect_all_disks()
{
  int ticks = -1;
  struct tms tms;
  clock_t t, ot, diff;
  unsigned long long elapsed; /* in millisec */
  long ticks_per_sec = sysconf(_SC_CLK_TCK);
  /* sizeof(clock_t):4, max_clock_t:0x7fffffff */
  long max_clock_t = (1 << (sizeof(clock_t) * 8 - 1)) - 1;

  elapsed = 0;
  ot = times(&tms);

  while (1) {
    ticks++;
    disk_scanalldisk(1);

    /* wait until at least one disk is detected (timeout:15sec) */
    if(!isHdDetected()) {
      if (ticks > 15) {
	fprintf(stderr, "no hd detected:%d\n", ticks);
	break; /* no disk inserted */
      }
      sleep(1);
      continue;
    }

    /* make sure we have all disks(HDB/MDB) detected */
    if (all_disks_detected()) {
      break;
    }
    
    t = times(&tms);
    diff = t - ot;
    ot = t;
    if (diff < 0) { /* overflow */
      diff = diff + max_clock_t + 1;
    }
    if (ot == (clock_t)-1) {
      elapsed += 1000LL;
    } else {
      elapsed += diff * 1000LL / ticks_per_sec;
    }
    fprintf(stderr, "elapsed:%llu(%08x)\n", elapsed,ot);

    /* timeout after 3 min */
    /* times has a bug, so use ticks as backup */
    if (((ot != (clock_t)-1) && (elapsed > 120 * 1000)) ||
	(ticks > 120)) {
      fprintf(stderr, "disk check timeout:%d\n",ticks);
      break;
    }

    sleep(1);
  }

  return;
}

void create_copyack()
{
  FILE *fp = fopen("/var/dvr/copyack", "w");
  if (!fp) {
    fprintf(fp, "1");
    fclose(fp);
  }
}

int check_logfiles()
{
   int i;
   FILE *fp=NULL;   
   if(disk_disklist.size()<2)
     return -1;
   for(i=0;i<60;++i){
        sleep(2);
	fp=fopen("/var/dvr/logcopy","r");
	if(fp){
	   break;  
	}
   }
   if(!fp){
      dvr_log("There is no /var/dvr/logcopy");
      return -1;
   }
   fclose(fp);
   create_copyack();
   unlink("/var/dvr/logcopy");
   merge_dvrlog();
   merge_smartlog();
   
   unlink("/var/dvr/copyack");
}

int check_hybridcopy()
{
  int i=0;
  int ret=-1;
  FILE *fp;
 // dvr_log("inside check_hybridcopy");
  /* check if we are told to copy files for hybrid disk */
  fp=fopen("/var/dvr/backupdisk","r");
  if(!fp)
    return -1;
  fclose(fp);
  for(i=0;i<60;++i){
        sleep(2);
	fp = fopen("/var/dvr/hybridcopy", "r");
	if (fp) {
           break;
	}
  }
  if (!fp) {
      dvr_log("no /var/dvr/hybridcopy file");
      return -1;
  }
  fclose(fp);
 // dvr_log("start check hybridcopy");
  /* give ack to tab102.sh */
  create_copyack();

 /* if so, turn on the hard disk */
  int sata = -1, flash = -1;  
  for(i=0;i<10;++i){
	dio_hdpower(1);
	int mfind=-1;
	struct timeval time0;
	struct timeval time1;
	gettimeofday(&time0, NULL);
	while(1){
	    sleep(3);  
	    gettimeofday(&time1, NULL);
	    if(time1.tv_sec-time0.tv_sec>10)
	      break;	  
	}		
	while(1){
	    sleep(5);
	    /* wait until SATA is dectected */
	    detect_all_disks();
	    
	    /* copy files */
	    if (isHybridDisk(&sata, &flash)) {
		    if(sata<0){
			 //   dvr_log("can't find HD of HBD");
			 mfind=-1;   
		    } else {
			mfind=1;
			break;
		    }
	    }
	    gettimeofday(&time1, NULL);
	    if(time1.tv_sec-time0.tv_sec>60) //45)
	      break;
	}
	if(mfind>0)
	   break;
        dvr_log("can't find HD of HBD");
	dio_hdpower(0);
	gettimeofday(&time0, NULL);
	while(1){
	   sleep(4);
	   gettimeofday(&time1, NULL);
	   if(time1.tv_sec-time0.tv_sec>10) //2)
	      break;
        }
        dio_kickwatchdog();
  }  

  if(sata>=0){
      rename(disk_curdiskfile.getstring(),"/var/dvr/flashdiskdir");
      dio_disablewatchdog();
      sync_logfiles_for_hybrid(flash, sata, 1);
      #if DEBUG_HBD		
      dvr_log("start copy pending files");
      #endif		
      ret=copyPendingFiles(disk_disklist[flash].basedir.getstring(),
		      disk_disklist[sata].basedir.getstring());
      dio_enablewatchdog();
  }
#if DEBUG_HBD	
  dvr_log("remove copyack");
#endif
  /* let tab102.sh know that we are done */
  unlink("/var/dvr/copyack");
  unlink("/var/dvr/hybridcopy");
  return ret;
}

void check_multicopy()
{
  /* check if we are told to copy files for hybrid disk */
  FILE *fp = fopen("/var/dvr/multicopy", "r");
  if (!fp) {
    return;
  }
  fclose(fp);

  if (recordingDisk < 0)
    return;

  /* give ack to tab102.sh */
  create_copyack();

  /* sync log files */
  int totalMulti;
  if (isMultipleDisk(&totalMulti)) {
    dio_disablewatchdog();
    sync_dvrlog(totalMulti);
    sync_smartlog(totalMulti);
    dio_enablewatchdog();
  }

  /* let tab102.sh know that we are done */
  unlink("/var/dvr/copyack");
  unlink("/var/dvr/multicopy");
}

/*
 * 1. wait until all disks are detected
 * 2. Hybrid: copy pending files from flash to sata
 * 3. copy tab102 files. In case of Hybrid, they are copied to flash,
 *    and they will be copied to SATA again when dvrsvr will be run as daemon
 */
int do_disk_check()
{
  int ret = 1, recdisk;

  fprintf(stderr, "do disk check\n");
  disk_init(1);

  detect_all_disks();
  recdisk = disk_findrecdisk(1);
  if (recdisk >= 0) {
    fprintf(stderr, "Found recording disk:%d\n", recdisk);
    /* copy tab102 files if necessary */
    //check_tab102_data(disk_disklist[recdisk].basedir.getstring());
  } else {
    fprintf(stderr, "No disk found\n");
  }

  disk_uninit(1);

  return ret;
}

int allpartitionchecked()
{
    int i;
    int checked=1;
    for(i=0;i<disk_disklist.size();++i){
        if(disk_disklist[i].scanfail){
	    checked=0;
	    break;
	}
      
    }
    return checked;
 }
  
void mark_latest_disk(int index)
{
    int i;
    char filename[256];
    FILE* fw;
   //remove all record files 
    for(i=0;i<disk_disklist.size();++i){
	disk_disklist[i].lastrecorded=0;
	snprintf(filename,sizeof(filename),"%s/record",disk_disklist[i].basedir.getstring());
	remove(filename);
    }      
    snprintf(filename,sizeof(filename),"%s/record",disk_disklist[index].basedir.getstring());
    fw=fopen(filename,"w");
    if(fw){
        dvrtime tnow;;
        time_now(&tnow);        
	fprintf(fw,"%04d%02d%02d%02d%02d%02d",
		tnow.year,
		tnow.month,
		tnow.day,
		tnow.hour,
		tnow.minute,
		tnow.second
		);
	fclose(fw);
    }
    disk_disklist[index].lastrecorded=1;
    return;
}
  
void sync_latest_partition()  
{
    int i;
    int mNum=0;
    int mIndex=0;
    int mday=0;
    int msecond=0;
    int mindex=-1;
    int mbcdday=0;
    int mbcdsec=0;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int  mbytes=0;
    char filename[256];
    char buf[32];
    FILE* fr;
    struct dvrtime* lastrectime;
    for(i=0;i<disk_disklist.size();++i){
        if(disk_disklist[i].status){	   
	    continue;
	}
	if(disk_disklist[i].lastrecorded)
	  mNum++;
    }
    if(mNum<2)
      return;
    for(i=0;i<disk_disklist.size();++i){
	if(disk_disklist[i].status)
	   continue;
	if(!disk_disklist[i].lastrecorded)
	  continue;
	snprintf(filename,sizeof(filename),"%s/record",disk_disklist[i].basedir.getstring());
	fr=fopen(filename,"r");
	if(fr){
	    mbytes=fread(buf,1,sizeof(buf),fr);
	    if(mbytes>8){
		  buf[mbytes]='\n';
		  mbytes=sscanf(buf,"%04d%02d%02d%02d%02d%02d",
      		   &year,
                   &month,
                   &day,
                   &hour,
                   &minute,
                   &second
                  );	
                  dvr_log("last record time:%d/%d/%d %d:%d:%d",month,day,year,hour,minute,second);
                  if(mbytes==6){
		      mbcdday=year*10000+month*100+day;
		      mbcdsec=hour*10000+minute*100+second;
		      if(mbcdday>mday){
			  mIndex=i;
			  mday=mbcdday;
			  msecond=mbcdsec;
		      } else if(msecond<mbcdsec){
			  mIndex=i;
			  mday=mbcdday;
			  msecond=mbcdsec;						
		      }
		  }	      
	    }
	    fclose(fr);
	}
    }
    if(mIndex>=0){
        dvr_log("%s is really latest recorded partition",disk_disklist[mIndex].basedir.getstring());
	for(i=0;i<disk_disklist.size();++i){
	    if(mIndex!=i){
		disk_disklist[i].lastrecorded=0;
		snprintf(filename,sizeof(filename),"%s/record",disk_disklist[i].basedir.getstring());
		remove(filename);
                dvr_log("%s was deleted",filename);		
	    }
	}
    }
}

void sync_latest_dvrlog()
{
    int i;
    int dvrlogIndex=-1;
    FILE* fr;
    char filename[256];
    char filename1[256];
    for(i=0;i<disk_disklist.size();++i){
	    snprintf(filename,sizeof(filename),"%s/dvrlog1.txt",disk_disklist[i].basedir.getstring());
	    fr=fopen(filename,"r");
	    if(fr){ 
	       fclose(fr);
	       dvrlogIndex=i;
	       break;
	    }
    }
    if(dvrlogIndex<0)
      return;
    for(i=0;i<disk_disklist.size();++i){
	snprintf(filename,sizeof(filename),"%s/dvrlog.txt",disk_disklist[i].basedir.getstring());
        unlink(filename);
    }
    snprintf(filename,sizeof(filename),"%s/dvrlog1.txt",disk_disklist[dvrlogIndex].basedir.getstring());
    snprintf(filename1,sizeof(filename1),"%s/dvrlog.txt",disk_disklist[dvrlogIndex].basedir.getstring());
    rename(filename,filename1);
    dvr_log("%s was renamed to %s",filename,filename1);
}

void check_totaldisksize()
{
   int i;
   int total=0;
   int disksize=0;
   FILE* fw=NULL;
   for(i=0;i<disk_disklist.size();++i){
       disksize=disk_size(disk_disklist[i].basedir.getstring());
       total+=disksize;
   }
   fw=fopen("/var/dvr/disksize","w");
   if(fw){
      fprintf(fw,"%d",total);
      fclose(fw);
   }
   return;
}
  
// regular disk check, every 1 seconds
void disk_check()
{
    int i;
    static int freepartitionchecked=0;
    /* check if all disks are temporarily unmounted for tab102 download */
    dio_get_nodiskcheck();
    if (g_nodiskcheck) {
      if(isHybridCopying==0){
	  dvr_log("DVR starts to process dvr log file");
	  dio_hybridcopy(1);
	  isHybridCopying=1;
	//  if(gHBD_Recording){	  
	  //    check_hybridcopy();
	 // }
	  check_logfiles();
	  dio_hybridcopy(0);
	  dvr_log("DVR log processing ends");
      }
      return;
    }
    disk_scanalldisk(0);
    if( disk_disklist.size()<=0 ) {
        if( rec_basedir.length()>0 ) {
            dvr_log("Abnormal disk removal.");
        }
        rec_break();
        rec_basedir="";
	recordingDisk=-1;
        return;
    }
    
    if(!partitioncheckdone){
	if(disk_disklist.size()!=numberoftotalpartitions){
	      clock_t t,diff;
	      unsigned long long elapsed;
	      t = times(&tms);
	      diff=t-diskcheckstartclock;
	      if(diff<0){
		  diff = diff + max_clock_t + 1;
	      }
	      elapsed = diff/ ticks_per_sec;
	      if(elapsed<300){
		//  dvr_log("disk_check time elapse:%d",elapsed);
		  return;
	      }
	      else {
		  partitioncheckdone=1;		   
	      }	  	  
	} else {	    
	    if(allpartitionchecked()){
	        dvr_log("total %d partitions are founded",disk_disklist.size());
	        partitioncheckdone=1;	
	    } else {
	       return;  
	    }
	}
	if(partitioncheckdone){
	    sync_latest_partition();
	    sync_latest_dvrlog();
	    check_totaldisksize();
	}
    }
    
    dio_hybridcopy(0);
    isHybridCopying=0;
#if 0    
    if(rec_basedir.length()>0){
	  if(disk_freespace(rec_basedir.getstring()) <= disk_minfreespace+400){
	      if(!freepartitionchecked){
		  checkfreepartition();
		  freepartitionchecked=1;
	      }
	  }
    } else {
	 checkrecordablepartition();          
    }
#endif

    if (rec_basedir.length()==0 || disk_freespace(rec_basedir.getstring()) <= disk_minfreespace ) {
        freepartitionchecked=0;
      	i = disk_findrecdisk(0);
        if( i<0 ) {
            rec_break();
            rec_basedir="";
	    recordingDisk=-1;
	    if(gHBD_Recording){
	      disk_disklist.empty();
	    }	    
            dvr_log("Can not find recordable disk.");        
            return;
        }
        
        if( strcmp(rec_basedir.getstring(), disk_disklist[i].basedir.getstring())!=0 ) {	// different disk
            if(rec_basedir.length()>0){
		mark_latest_disk(i);
		rec_basedir=disk_disklist[i].basedir.getstring() ;
		dvr_log("Start recording on disk : %s.", rec_basedir.getstring()) ;
		// mark current disk
		mark_current_disk(rec_basedir.getstring());
		rec_break();
	    } else {
		mark_latest_disk(i);
		rec_basedir=disk_disklist[i].basedir.getstring() ;
		dvr_log("Start recording on disk : %s.", rec_basedir.getstring()) ;
		// mark current disk
		mark_current_disk(rec_basedir.getstring());	      
	    }
        }
    } 
    return;
}

/*
void disk_logdir(char * logfilename)
{
    int i ;
    char logpath[512] ;
    struct stat logstat ;
    for( i=0; i<disk_disklist.size(); i++ ) {
        sprintf( logpath, "%s/_%s_/%s", 
                disk_disklist[i].basedir.getstring(),
                g_hostname,
                logfilename );
        if( stat( logpath, &logstat )==0 ) {
            if( S_ISREG( logstat.st_mode ) ) {      // find existing log file
                disk_disklist[i].basedir.getstring()
            }
            return i;
        }
    }
    return "" ;
}
*/

void disk_init(int check_only)
{
    string pcfg;
    int l;

    config dvrconfig(dvrconfigfile);

    // initial mutex
    disk_mutex = mutex_init ;
    
    rec_basedir = "";
    disk_base = dvrconfig.getvalue("system", "mountdir");
    if (disk_base.length() == 0) {
        disk_base = "/var/dvr/disks";
    }
    pcfg = dvrconfig.getvalue("system", "mindiskspace");
    l = pcfg.length();
    if (sscanf(pcfg.getstring(), "%d", &disk_minfreespace)) {
        l = pcfg.length();
        if (pcfg[l - 1] == 'G' || pcfg[l - 1] == 'g') {
            disk_minfreespace *= 1024;
        } else if (pcfg[l - 1] == 'K' || pcfg[l - 1] == 'k') {
            disk_minfreespace /= 1024;
        } else if (pcfg[l - 1] == 'B' || pcfg[l - 1] == 'b') {
            disk_minfreespace /= 1024 * 1024;
        }
        if (disk_minfreespace < 20)
            disk_minfreespace = 20;
    } else {
        disk_minfreespace = 100;
    }
    
    //partitionperdisk= dvrconfig.getvalueint("system", "partitionperdisk");
    partitionperdisk=1;
    /*
    if(partitionperdisk<2)
      partitionperdisk=2;
    if(partitionperdisk>4)
      partitionperdisk=4;
    */
    
    numberofdiskused= dvrconfig.getvalueint("system","numberofdiskused");
    if(numberofdiskused<1)
       numberofdiskused=1;
    
    numberoftotalpartitions=partitionperdisk*numberofdiskused;
    
    disk_curdiskfile = dvrconfig.getvalue("system", "currentdisk");
    if( disk_curdiskfile.length()<2) {
        disk_curdiskfile="/var/dvr/dvrcurdisk" ;
    }
    
    gHBD_Recording=dvrconfig.getvalueint("system", "hbdrecording");
    
    if(gHBD_Recording){
       dvr_log("HBD recording enabled");  
    } else {
       remove("/var/dvr/backupdisk");
    }
    ticks_per_sec = sysconf(_SC_CLK_TCK);  
   /* sizeof(clock_t):4, max_clock_t:0x7fffffff */
    max_clock_t = (1 << (sizeof(clock_t) * 8 - 1)) - 1;
    diskcheckstartclock = times(&tms); 
    
    recordingDisk=-1;
    // empty disk list
    disk_disklist.empty();

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
    
    if( rec_basedir.length()>0 ) {
        ;
    }
    rec_basedir="";
    disk_base="";
    disk_disklist.empty();
    dvr_log("%s is unlinked",disk_curdiskfile.getstring());
    // un-mark current disk
    unlink( disk_curdiskfile.getstring() );
    
    disk_unlock();
    
    pthread_mutex_destroy(&disk_mutex);

    if (!check_only)
      dvr_log("Disk uninitialized.");
    
}

enum {TYPE_SMART, TYPE_IMPACT, TYPE_PEAK};
enum {DVR_TYPE_DM500, DVR_TYPE_DM510};

struct fileInfo {
  char ch, month, day, hour, min, sec, type, ext;
  unsigned short year, millisec;
  unsigned int len;
  unsigned int size; /* file size */
  int pathId;
};

struct fileEntry {
  struct fileEntry *next, *prev;
  struct fileInfo fi;
};

struct sfileInfo {
  /* type: tab102 or smartlog, lock: L/N */
  char month, day, hour, min, sec, type, lock, reserved;
  unsigned short year, fileno;
  unsigned int size; /* file size */
  int pathId;
};

struct sfileEntry {
  struct sfileEntry *next, *prev;
  struct sfileInfo fi;
};

struct dirPath {
  int pathId;
  char pathStr[128];
};

struct dirEntry {
  struct dirEntry *next, *prev;
  struct dirPath path;
};

int dvr_type = DVR_TYPE_DM510;
struct fileEntry *file_list;
struct sfileEntry *sfile_list;
struct dirEntry *dir_list;

int cmpFileEntry(struct fileEntry *a, struct fileEntry *b)
{
  int ret;
  struct tm tma, tmb;
  time_t timea, timeb;
  
  tma.tm_year = a->fi.year - 1900;
  tma.tm_mon = a->fi.month - 1;
  tma.tm_mday = a->fi.day;
  tma.tm_hour = a->fi.hour;
  tma.tm_min = a->fi.min;
  tma.tm_sec = a->fi.sec;
  tma.tm_isdst = -1; // let OS decide
  
  tmb.tm_year = b->fi.year - 1900;
  tmb.tm_mon = b->fi.month - 1;
  tmb.tm_mday = b->fi.day;
  tmb.tm_hour = b->fi.hour;
  tmb.tm_min = b->fi.min;
  tmb.tm_sec = b->fi.sec;
  tmb.tm_isdst = -1; // let OS decide
  
  timea = mktime(&tma);
  if (timea == -1) {
	return 0; // what to do?
  }
  timeb = mktime(&tmb);
  if (timeb == -1) {
	return 0; // what to do?
  }
  ret = timea - timeb;
  
  if (!ret) {
    ret = a->fi.millisec - b->fi.millisec;
    if (!ret) {
      ret = a->fi.len - b->fi.len;
      if (!ret) {
	ret = a->fi.type - b->fi.type;
	if (!ret) {
	  ret = a->fi.ch - b->fi.ch; // check ch 1st before ext
	  if (!ret) {
	    ret = a->fi.ext - b->fi.ext;
	  }
	}
      }
    }
  }
  
  return ret;
}

struct fileEntry *listsort(struct fileEntry *list) {
  struct fileEntry *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;
  
  if (!list)
	return NULL;
  
  insize = 1;
  
  while (1) {
	p = list;
	list = NULL;
	tail = NULL;
	
	nmerges = 0;  /* count number of merges we do in this pass */
	
	while (p) {
	  nmerges++;  /* there exists a merge to be done */
	  /* step `insize' places along from p */
	  q = p;
	  psize = 0;
	  for (i = 0; i < insize; i++) {
		psize++;
		q = q->next;
		if (!q) break;
	  }
	  
	  /* if q hasn't fallen off end, we have two lists to merge */
	  qsize = insize;
	  
	  /* now we have two lists; merge them */
	  while (psize > 0 || (qsize > 0 && q)) {
		
		/* decide whether next element of merge comes from p or q */
		if (psize == 0) {
		  /* p is empty; e must come from q. */
		  e = q; q = q->next; qsize--;
		} else if (qsize == 0 || !q) {
		  /* q is empty; e must come from p. */
		  e = p; p = p->next; psize--;
		} else if (cmpFileEntry(p,q) <= 0) { /* oldest first */
		  //} else if (cmpFileEntry(p,q) >= 0) { /* latest first */
		  /* First element of p is lower (or same);
		   * e must come from p. */
		  e = p; p = p->next; psize--;
		} else {
		  /* First element of q is lower; e must come from q. */
		  e = q; q = q->next; qsize--;
		}
		
		/* add the next element to the merged list */
		if (tail) {
		  tail->next = e;
		} else {
		  list = e;
		}
		tail = e;
	  }
	  
	  /* now p has stepped `insize' places along, and q has too */
	  p = q;
	}
	tail->next = NULL;
	
	/* If we have done only one merge, we're finished. */
	if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
	  return list;
	
	/* Otherwise repeat, merging lists twice the size */
	insize *= 2;
  }
}

void mp4ToK(char *filename) {
  filename[strlen(filename) - 3] = '\0';
  strcat(filename, "k");
}

void kTo264(char *filename) {
  filename[strlen(filename) - 1] = '\0';
  strcat(filename, "266");
}

/* returns path in the list(NULL if not found),
 */
static char *get_path_for_list_id(int id)
{
  struct dirEntry *node;

  node = dir_list;
  while (node) {
    if (node->path.pathId == id) {
      return node->path.pathStr;
    }

    node = node->next;
  }

  return NULL;
}

int get_full_path_for_fileinfo(char *path, int size,
			       struct fileInfo *fi, char *servername)
{
  if (!servername || !path || size < 0 || !fi)
    return 1;
  
  char *dirname = get_path_for_list_id(fi->pathId);
  if (!dirname)
    return 1;
  
  snprintf(path, size,
	   "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
	   "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	   dirname, servername,
	   fi->year, fi->month, fi->day,
	   fi->ch, fi->ch,
	   fi->year, fi->month, fi->day,
	   fi->hour, fi->min, fi->sec,
	   fi->len, fi->type, servername,
	   (fi->ext == 'k') ? "k" : 
	   (dvr_type == DVR_TYPE_DM510) ? "266" : "mp4");
  
  return 0;
}

int get_full_path_for_smartfileinfo(char *path, int size,
				    struct sfileInfo *fi, char *servername)
{
  if (!servername || !path || size < 0 || !fi)
    return 1;
  
  char *dirname = get_path_for_list_id(fi->pathId);
  if (!dirname)
    return 1;
  
  if (fi->type == TYPE_SMART) {      
    snprintf(path, size,
	     "%s/smartlog/%s_%04hd%02hhd%02hhd_L.%03hd",
	     dirname,
	     servername,
	     fi->year, fi->month, fi->day,
	     fi->fileno);
  } else {
    snprintf(path, size,
	     "%s/smartlog/%s_%04hd%02hhd%02hhd"
	     "%02hhd%02hhd%02hhd_TAB102log_L.%s",
	     dirname,
	     servername,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec,
	     (fi->type == TYPE_IMPACT) ? "log" : "peak");
  }
  
  return 0;
}

int create_dest_directory(char *dest_root,
			  char *path, int size,
			  struct fileInfo *fi, char *servername)
{
  if (!servername || !path || size < 0 || !fi)
    return 1;

  // create _busname_ directory
  snprintf(path, size, "%s/_%s_",
	   dest_root,
	   servername);
  if (mkdir(path, 0777) == -1) {
    if(errno != EEXIST) {
#if DEBUG_HBD      
      dvr_log("mkdir %s error(%s)", path, strerror(errno));
#endif      
      return 1;
    }
  }
  
  // create YYYYMMDD directory
  snprintf(path, size, "%s/_%s_/%04d%02d%02d",
	   dest_root,
	   servername,
	   fi->year, fi->month, fi->day);
  if (mkdir(path, 0777) == -1) {
    if(errno != EEXIST) {
#if DEBUG_HBD      
      dvr_log("mkdir %s error(%s)", path, strerror(errno));
#endif      
      return 1;
    }
  }

  // create CHxx directory
  snprintf(path, size, "%s/_%s_/%04d%02d%02d/CH%02d",
	   dest_root,
	   servername,
	   fi->year, fi->month, fi->day,
	   fi->ch);
  if (mkdir(path, 0777) == -1) {
    if(errno != EEXIST) {
#if DEBUG_HBD      
      dvr_log("mkdir %s error(%s)", path, strerror(errno));
#endif      
      return 1;
    }
  }

  return 0;
}

int is_disk_full(char *dest_root)
{
  /* check disk space */
  struct statfs fsInfo;
  long long int freeSpace;
      
  if (!dest_root) return 1;

  if (statfs(dest_root, &fsInfo) == -1) {
    return 1;
  }
      
  long long size = fsInfo.f_bsize;
  freeSpace = fsInfo.f_bavail * size;
  
  if (freeSpace < 1024LL * 1024LL) {
    //writeDebug("disk full: %lld", freeSpace);
    return 1; /* disk full */
  }

  return 0;
}

static int check_servername(char *filename, char *servername,
			    char type)
{
  char *ptr, *afterType, str[128];
  int len;

  sprintf(str, "_%c_", type);

  /* look for _L_ or _N_, etc */
  ptr = strstr(filename, str);
  if (!ptr)
    return 1; /* shouldn't happen */

  /* check for servername */
  len = strlen(servername);
  afterType = ptr + 3;
  if (!strncmp(afterType, servername, len)) {
    /* servername right after TYPE */
    return 0;
  }

  return 1;
}

/* returns path_id in the list(-1 on error),
 * add item if not found
 */
static int get_path_list_id(char *path)
{
  int highestId = -1;
  struct dirEntry *node;

  node = dir_list;
  while (node) {
    if (!strncmp(node->path.pathStr, path, sizeof(node->path.pathStr))) {
      return node->path.pathId;
    }

    if (node->path.pathId > highestId) {
      highestId = node->path.pathId;
    }

    node = node->next;
  }

  /* not found in the list, add one */
  node = (struct dirEntry *)malloc(sizeof(struct dirEntry));
  if (node) {
    node->path.pathId = highestId + 1;
    strncpy(node->path.pathStr, path, sizeof(node->path.pathStr));
     /* add to the list */
    node->next = dir_list;
    dir_list = node;
    return node->path.pathId;
  }

  return -1;
}

static int 
add_file_to_list(char *path, char ch,
		 unsigned short year, char month, char day,
		 char hour, char min, char sec, unsigned short millisec,
		 unsigned int len, char type, char ext,
		 off_t size)
{
  struct fileEntry *node;

  node = (struct fileEntry *)malloc(sizeof(struct fileEntry));
  if (node) {
    node->fi.ch = ch;
    node->fi.year = year;
    node->fi.month = month;
    node->fi.day = day;
    node->fi.hour = hour;
    node->fi.min = min;
    node->fi.sec = sec;
    node->fi.millisec = millisec;
    node->fi.type = type;
    node->fi.len = len;
    node->fi.ext = ext;
    node->fi.size = size;
    node->fi.pathId = get_path_list_id(path);

#if 0
    if (dvr_type == DVR_TYPE_DM500) {
      writeDebug("adding CH%02d_%04d%02d%02d%02d%02d%02d%03d_%u_%c.%c",
	      ch,year,month,day,hour,min,sec,millisec,len,type,ext);
    } else {
      writeDebug("adding CH%02d_%04d%02d%02d%02d%02d%02d_%u_%c.%c",
	      ch,year,month,day,hour,min,sec,len,type,ext);
    }
#endif

     /* add to the list */
    node->next = file_list;
    file_list = node;
    //fprintf(stderr, "file_list:%p,next:%p\n", file_list, file_list->next);
  }

  return 0;
}

static int 
add_smartfile_to_list(char *path,
		      unsigned short year, char month, char day, 
		      char hour, char min, char sec,
		      char type, char lock, 
		      unsigned short fileno,
		      off_t size)
{
  struct sfileEntry *node;

  node = (struct sfileEntry *)malloc(sizeof(struct sfileEntry));
  if (node) {
    node->fi.year = year;
    node->fi.month = month;
    node->fi.day = day;
    node->fi.hour = hour;
    node->fi.min = min;
    node->fi.sec = sec;
    node->fi.type = type;
    node->fi.lock = lock;
    node->fi.fileno = fileno;
    node->fi.size = size;
    node->fi.pathId = get_path_list_id(path);

    /* add to the list */
    node->next = sfile_list;
    sfile_list = node;
  }

  return 0;
}

static int scanSmartFiles(char *dir_path, char *servername)
{
  DIR *dir;
  struct dirent *de;
  char dirpath[128], dname[128], fullname[256];
  int ret;
  int servernameLen = 0;
  char month = 0, day = 0, hour = 0, min = 0, sec = 0, lock = 0;
  unsigned short year = 0, fileno = 0;
  struct stat st;


  fprintf(stderr, "scanSmartFiles\n");

  strncpy(dirpath, dir_path, sizeof(dirpath));

  sprintf(dname, "%s/smartlog", dirpath);
  dir = opendir(dname);
  if (dir == NULL) {
    perror("scanSmartFiles:opendir");
    return 1;
  }

  if (servername) 
    servernameLen = strlen(servername);

  while ((de = readdir(dir)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    
  //  if (de->d_type != DT_REG)
   //   continue;

    if (((int)strlen(de->d_name) > servernameLen) &&
	!strncmp(servername, de->d_name, servernameLen)) {
      ret = sscanf(de->d_name + servernameLen,
		   "_%04hd%02hhd%02hhd_L.%03hd",
		   &year, &month, &day, &fileno);
      if ((ret == 4) && (fileno == 1)) {
	snprintf(fullname, sizeof(fullname),
		 "%s/%s",
		 dname, de->d_name);
	fprintf(stderr, "adding file %s\n", fullname);
	if (stat(fullname, &st)) {
	  perror("scanSmartFiles");
	  continue;
	}
	add_smartfile_to_list(dirpath,
			      year, month, day, 
			      0, 0, 0,
			      TYPE_SMART, 'L', 
			      fileno, st.st_size);
      } else {
	char extension[128];

	ret = sscanf(de->d_name + servernameLen,
		     "_%04hd%02hhd%02hhd%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		     &year, &month, &day,
		     &hour, &min, &sec,
		     &lock, extension);
	if ((ret == 8) && (lock == 'L')) {
	  int type = -1;
	  if (!strcmp(extension, "log")) {
	    type = TYPE_IMPACT;
	  } else if (!strcmp(extension, "peak")) {
	    type = TYPE_PEAK;
	  }

	  if ((type == TYPE_IMPACT) || (type == TYPE_PEAK)) {
	    snprintf(fullname, sizeof(fullname),
		     "%s/%s",
		     dname, de->d_name);
	    fprintf(stderr, "adding file %s\n", de->d_name);
	    if (stat(fullname, &st)) {
	      perror("scanSmartFiles");
	      continue;
	    }
	    add_smartfile_to_list(dirpath,
				  year, month, day, 
				  hour, min, sec,
				  type, lock, 
				  0, st.st_size);
	  }
	}
      }
    }
  }
  
  closedir(dir);

  return 0;
}

static int 
create_data_file_list(char *mount_dir, char *servername,
	     char *path, char *filename, void *arg)
{
  int ret;
  unsigned short year, millisec;
  unsigned int len;
  char month, day, hour, min, sec, ch, type, ext;
  struct stat st;
  char fullname[256];
	
  /* get the file extension */
  char *ptr = strrchr(filename, '.');
  if (!ptr)
    return 1;
  ext = *(ptr + 1); /* 1st character of the extension */

  millisec = 0;
  if (dvr_type == DVR_TYPE_DM500) {
    ret = sscanf(filename,
		 "CH%02hhd_%04hd%02hhd%02hhd"
		 "%02hhd%02hhd%02hhd%03hd_%u_%c_",
		 &ch,
		 &year,
		 &month,
		 &day,
		 &hour,
		 &min,
		 &sec,
		 &millisec,
		 &len,
		 &type);
  } else {
    ret = sscanf(filename,
		 "CH%02hhd_%04hd%02hhd%02hhd"
		 "%02hhd%02hhd%02hhd_%u_%c_",
		 &ch,
		 &year,
		 &month,
		 &day,
		 &hour,
		 &min,
		 &sec,
		 &len,
		 &type);
  }
  snprintf(fullname, sizeof(fullname), "%s/%s",
	     path, filename);
#if DEBUG_HBD
  dvr_log("filename:%s",fullname);
#endif
  if (((dvr_type == DVR_TYPE_DM500) && (ret == 10)) ||
      ((dvr_type == DVR_TYPE_DM510) && (ret == 9))) {
    if (check_servername(filename, servername, type))
      return 1;
    if ((type == 'L') || (type == 'l') ||
	(type == 'N') || (type == 'n')) {
      if (stat(fullname, &st)) {
	//writeDebug("create_data_file_list:%s", strerror(errno));
	return 1;
      }
#if DEBUG_HBD
      dvr_log("add file:%s to list",fullname);
#endif
      add_file_to_list(mount_dir, ch, year, month, day,
		       hour, min, sec, millisec,
		       len, type, ext, st.st_size);
    } /* if (type) */	  
  } /* if (dvr_type) */
  
  return 0;
}

static int scan_files(char *mount_dir, char *servername,
		      void *arg,
		      int (*doit)(char*, char*, char*, char*, void*))
{
  DIR *dir, *dirCh, *dirFile;
  struct dirent *de, *deCh, *deFile;
  char dname[128];
  struct stat findstat ;

  sprintf(dname, "%s/_%s_", mount_dir, servername);
#if DEBUG_HBD
  dvr_log("inside scan_files:%s",dname);
#endif
  dir = opendir(dname);
  if (dir == NULL) {
    //writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
    return 1;
  }
#if DEBUG_HBD
  dvr_log("dir:%s is opened",dname);
#endif
  while ((de = readdir(dir)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
     sprintf(dname, "%s/_%s_/%s", mount_dir, servername, de->d_name);
     if(de->d_type==DT_UNKNOWN){
             if( stat(dname, &findstat )==0 ){
                 if( S_ISDIR(findstat.st_mode) ) {
                       de->d_type = DT_DIR ;
                 }
                 else if( S_ISREG(findstat.st_mode) ) {
                     de->d_type = DT_REG ;
                 }
             }
     }
     if (de->d_type != DT_DIR)
      continue;
    
#if DEBUG_HBD
    dvr_log("start to open dir:%s",dname);
#endif
    dirCh = opendir(dname);
    if (dirCh == NULL) {
      //writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
      closedir(dir);
      return 1;
    }
#if DEBUG_HBD
    dvr_log("dir:%s is opened",dname);
#endif
    while ((deCh = readdir(dirCh)) != NULL) {
      if (deCh->d_name[0] == '.')
	continue;
      
     // if (deCh->d_type != DT_DIR)
	//continue;
      
      sprintf(dname, "%s/_%s_/%s/%s",
	      mount_dir, servername,
	      de->d_name, deCh->d_name);
#if DEBUG_HBD
      dvr_log("open dir:%s",dname);
#endif
      dirFile = opendir(dname);
      if (dirFile == NULL) {
	//writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
	closedir(dirCh);
	closedir(dir);
	return 1;
      }
      while ((deFile = readdir(dirFile)) != NULL) {
	if (deFile->d_name[0] == '.')
	  continue;
	//if (deFile->d_type != DT_REG)
	//  continue;
	(*doit)(mount_dir, servername, dname, deFile->d_name, arg);
      }
      
      closedir(dirFile);
    }
    
    closedir(dirCh);
  }
  
  closedir(dir);
  
  return 0;
}

int delete_oldest_file(char *base_dir_with_busname)
{
  int i, j;
  int total_n, total_l ;
  //int firstndisk, firstldisk ;
  disk_day_info firstl, firstn ;
  struct dvrtime oldesttime ;
  string oldestfilename ;
  char diskday[512] ;

  firstl.bcdday=21000101 ;
  firstn.bcdday=21000101 ;
  total_n=0 ;
  total_l=0 ;

  for( i=0; i<disk_disklist.size(); i++ ) {
    if (!strcmp(disk_disklist[i].basedir.getstring(),
		base_dir_with_busname)) {
      break;
    }
  }

  if (i >= disk_disklist.size())
    return -1;
        
  array <disk_day_info> * ddi = & (disk_disklist[i].daylist) ;
  for(j=0; j<ddi->size(); j++) {
    if( (*ddi)[j].nfilelen > 0 ) {
      if( (*ddi)[j].bcdday < firstn.bcdday ||
	  ((*ddi)[j].bcdday == firstn.bcdday && (*ddi)[j].nfiletime < firstn.nfiletime ) )
	{
	  firstn = (*ddi)[j] ;
	}
      total_n+= (*ddi)[j].nfilelen ;
    }
    if( (*ddi)[j].lfilelen > 0 ) {
      if( (*ddi)[j].bcdday < firstl.bcdday ||
	  ((*ddi)[j].bcdday == firstl.bcdday && (*ddi)[j].lfiletime < firstl.lfiletime ) )
	{
	  firstl = (*ddi)[j] ;
	}
      total_l+= (*ddi)[j].lfilelen ;
    }
  }
        
  if( total_n<=0 && total_l<=0 ){        // on video files at all !!!?
     dvr_log("there is no video file on HD to be deleted");
     return -1 ;
  }      
  int dlock=0 ;
  if( total_l>0 )  {
    struct dvrtime dvrt ;
    time_now(&dvrt);
    if( total_n<=0 ||
	((dvrt.year*12+dvrt.month)-((firstl.bcdday/10000)*12+(firstl.bcdday/100%100)))>6 ||       // 6 month old locked file?
	( total_l>(total_n)/2 && firstl.bcdday < firstn.bcdday ) )           // locked lengh > 30%
      {
	dlock=1 ;
      }
  }

  if( dlock )             // allow to delete locked file
    {
      if( firstl.bcdday<firstn.bcdday ) {
	firstn=firstl;
	//firstndisk = firstldisk ;
      }
    }
  
  // delete files on <firstn>
  sprintf( diskday, "%s/%08d", base_dir_with_busname, firstn.bcdday );
  
  time_dvrtime_init(&oldesttime, 2099);
  
  if( disk_olddestfile( diskday, &oldesttime, &oldestfilename, dlock) ) {
    disk_removefile( oldestfilename.getstring() );
    disk_rmempty264dir( diskday );
    disk_renewdiskdayinfo( i, firstn.bcdday ) ;
  }

  return 0;
}

/* better than strstr("_L_"), works only with normal video files.
 *
 */
static int is_lock_file(char *filename)
{
  char *ptr;

  ptr = strrchr(filename, '/');
  if (ptr) {
    /* look for 1st underbar after CHxx */
    ptr = strchr(ptr + 1, '_');
    if (ptr) {
      /* look for 2nd under bar after YYYYMMDDhhmmss(mmm) */
      ptr = strchr(ptr + 1, '_');
      if (ptr) {
	/* look for 3rd under bar after Length */
	ptr = strchr(ptr + 1, '_');
	if (ptr) {
	  if (*(ptr + 1) == 'L')
	    return 1;
	}
      }
    }
  }
  
  return 0;
}

/* change file type
 * param: filename - filename with or without path
 *        newname - buffer for the new name (only the filename portion)
 *        size    - buffer size
 *        type    - 'L' or 'N'
 */
static int get_new_file_type_name(char *filename,
				  char *newname, int size, char type)
{
  char *ptr, *fname; 
  int ret = 1;

  /* check if filename has path portion */
  fname = filename;
  ptr = strrchr(filename, '/');
  if (ptr) {
    fname = ptr + 1;
  }

  /* insure dest buffer size */
  if (size < (int)(strlen(fname) + 1))
    return 1;

  strcpy(newname, fname);

  /* look for 1st underbar after CHxx */
  ptr = strchr(newname, '_');
  if (ptr) {
    /* look for 2nd under bar after YYYYMMDDhhmmss(mmm) */
    ptr = strchr(ptr + 1, '_');
    if (ptr) {
      /* look for 3rd under bar after Length */
      ptr = strchr(ptr + 1, '_');
      if (ptr) {
	*(ptr + 1) = type;
	ret = 0;
      }
    }
  }
  
  return ret;
}

/* rename the recording file(full path) to newtype */
static int rename_file(char *filename, char type)
{
  char *ptr, *newname;
  int ret = 1;

  fprintf(stderr, "rename: %s\n", filename);
  newname = strdup(filename);
  if (newname) {
    ptr = strrchr(newname, '/');
    if (ptr) {
      /* look for 1st underbar after CHxx */
      ptr = strchr(ptr + 1, '_');
      if (ptr) {
	/* look for 2nd under bar after YYYYMMDDhhmmss(mmm) */
	ptr = strchr(ptr + 1, '_');
	if (ptr) {
	  /* look for 3rd under bar after Length */
	  ptr = strchr(ptr + 1, '_');
	  if (ptr) {
	    *(ptr + 1) = type;
	    fprintf(stderr, "%s rename to %s\n", filename, newname);
	    if (rename(filename, newname) == -1) {
	      perror("rename_file");
	    } else {
	      ret = 0;
	    }
	  }
	}
      }
    }
    free(newname);
  }

  return ret;
}

int copy_file_to_path(char *from_fullname, char *to_path,
		      int forcewrite, int check_stop)
{
  char filepath[512], filepath2[512], filename[512], *ptr;
  FILE *fpsrc, *fpdst;
  int n, ret = 0;
  char buf[64*1024];
  struct stat sb, sb2;
  
  ptr = strrchr(from_fullname, '/');
  if (!ptr) 
    return 1;

  snprintf(filepath, sizeof(filepath),
	   "%s/%s", to_path, ptr + 1);

  if (stat(from_fullname, &sb))
    return 1;

  if (!forcewrite) {
    /* file with same name ? */
    if (!stat(filepath, &sb2)) {
      if (sb.st_size <= sb2.st_size) {
	return 0; /* file exists, don't copy */
      }
    } else {
      /* no match found on the destination */

      /* check for L --> N case */
      if (is_lock_file(from_fullname)) {
	/* check for N file */
	if (!get_new_file_type_name(from_fullname,
				    filename, sizeof(filename), 'N')) {
	  snprintf(filepath2, sizeof(filepath2),
		   "%s/%s", to_path, filename);
	  if (!stat(filepath2, &sb2)) {
	    /* yes: L --> N case */
	    if (sb.st_size == sb2.st_size) {
	      /* same size, just rename it (source:L to N) */
	      rename_file(from_fullname, 'N');
	      return 0;
	    } else {
	      /* different size, remove the dest file and copy again */
	      unlink(filepath2);
	    }
	  }
	}
      }
    }
  }

  fpsrc = fopen(from_fullname, "r");
  if (!fpsrc)
    return 1;

  fpdst = fopen(filepath, "w");
  if (!fpdst) {
    fclose(fpsrc);
    return 1;
  }
#if DEBUG_HBD
  dvr_log("file:%s is copied",from_fullname);
#endif
#if DEBUG_HBD
  dvr_log("file:%s copy to:%s",from_fullname,filepath);
#endif

  while ((n = safe_fread(buf, 1, sizeof(buf), fpsrc)) > 0) {
    /* asked to stop by ioprocess? */
    dio_get_nodiskcheck();
    if (check_stop && !g_nodiskcheck)
      break;
    if(net_liveview>0){	   
	   net_liveview--;
           dio_setstate (DVR_NETWORK);	  
           sleep(1);	   
	   continue;
    }   
    if(net_playarc>0){	   
	   net_playarc--;  
           sleep(1);	   
	   continue;
    } 
   if (safe_fwrite(buf, 1, n, fpdst) != (size_t)n) {
      ret = 1;
      break;
    }
   // dvr_log("copying:%d",n);
  }
#if DEBUG_HBD  
  dvr_log("copy ends");
#endif  
  fclose(fpdst);
  fclose(fpsrc);

  return ret;
}

static void free_file_list()
{
  struct fileEntry *node;

  while (file_list) {
    node = file_list;
    file_list = node->next;
    free(node);
  }
}

static void free_smartfile_list()
{
  struct sfileEntry *node;

  while (sfile_list) {
    node = sfile_list;
    sfile_list = node->next;
    free(node);
  }
}

static void free_dir_list()
{
  struct dirEntry *node;

  while (dir_list) {
    node = dir_list;
    dir_list = node->next;
    free(node);
  }
}

/* TODO: copy _L_ file first.
 * dest_dir: /mount_point/_busname_
 * dest_root:/mount_point
 */
#define ARRAY_DELETE
static array <string> mDayFileList;
static int mbcdday=-1;
int  deletefilefromarray(){
  int i=0;
  int index=0;
  int res=0;
  struct dvrtime dvrt ;
  struct dvrtime old_time ;
  if(mDayFileList.size()<2){
    mDayFileList.empty();
    return -1;
  }
  f264time(mDayFileList[1].getstring(), &old_time);
  index=1;
  for(i=2;i<mDayFileList.size();++i){
       f264time(mDayFileList[i].getstring(), &dvrt);  
       if(dvrt<old_time){
	 index=i;
	 old_time=dvrt;
       }
  }
  disk_removefile(mDayFileList[index].getstring());
//  dvr_log("array:%d",mDayFileList.size());
//  dvr_log("file:%s is deleted",mDayFileList[index].getstring());
  
  mDayFileList.remove(index);
  if(mDayFileList.size()==1){
       disk_rmempty264dir(mDayFileList[0].getstring());
     //  disk_renewdiskdayinfo(recordingDisk,mbcdday);
       mDayFileList.empty();       
       return res;
  }  
  return res;  
}
int check_dayfile(char *dir) {
    int res=0;
    int i=0;
    char * pathname ;
    char * filename ;
    string mfilepath;
    int l ; 
    mfilepath=dir;
  //  dvr_log("check dayfile:%s",dir);
    dir_find dfind(dir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
            res+=check_dayfile( pathname);
        }
        if( dfind.isfile() ) {
            filename = dfind.filename();
            l = strlen(filename) ;
            if( l>4 && strcmp(&filename[l-4], ".266")==0 ) {    // is it a .264 file ?
                if(strstr(filename, "_N_")) {
		   mfilepath=pathname;
                   mDayFileList.add(mfilepath);
                }
            }
        }
    }
    return res ;   
}

void removedayinfo(int diskindex, int day)
{
    int i ;  
    if( diskindex<disk_disklist.size() ){
        for( i=0; i<disk_disklist[diskindex].daylist.size(); i++ ) {
            if( day == disk_disklist[diskindex].daylist[i].bcdday ) {	      
	        disk_disklist[diskindex].daylist.remove(i);
                return;
            }
        }           
    }  
    return;
}
int createfilearray(char *base_dir_with_busname)
{
  int i, j;
  int total_n, total_l ;
//  int firstndisk, firstldisk ;
  disk_day_info firstl, firstn ;
  struct dvrtime oldesttime ;
  string oldestfilename ;
  char diskday[512] ;

  firstl.bcdday=21000101 ;
  firstn.bcdday=21000101 ;
  total_n=0 ;
  total_l=0 ;

  for( i=0; i<disk_disklist.size(); i++ ) {
    if (!strcmp(disk_disklist[i].basedir.getstring(),
		base_dir_with_busname)) {
      break;
    }
  }

  if (i >= disk_disklist.size())
    return -1;
        
  array <disk_day_info> * ddi = & (disk_disklist[i].daylist) ;
  for(j=0; j<ddi->size(); j++) {
    if( (*ddi)[j].nfilelen > 0 ) {
      if( (*ddi)[j].bcdday < firstn.bcdday  )
	{
	  firstn = (*ddi)[j] ;
	}
      total_n+= (*ddi)[j].nfilelen ;
    }
    if( (*ddi)[j].lfilelen > 0 ) {
      if( (*ddi)[j].bcdday < firstl.bcdday)
	{
	  firstl = (*ddi)[j] ;
	}
      total_l+= (*ddi)[j].lfilelen ;
    }
  }
        
  if( total_n<=0 && total_l<=0 )        // on video files at all !!!?
    return -1 ;
        
  int dlock=0 ;
  if( total_l>0 )  {
    struct dvrtime dvrt ;
    time_now(&dvrt);
    if( total_n<=0 ||
	((dvrt.year*12+dvrt.month)-((firstl.bcdday/10000)*12+(firstl.bcdday/100%100)))>6 ||       // 6 month old locked file?
	( total_l>(total_n)/2 && firstl.bcdday < firstn.bcdday ) )           // locked lengh > 30%
      {
	dlock=1 ;
      }
  }

  if( dlock )             // allow to delete locked file
    {
      if( firstl.bcdday<firstn.bcdday ) {
	firstn=firstl;
	//firstndisk = firstldisk ;
      }
    }
  
  // delete files on <firstn>
  sprintf( diskday, "%s/%08d", base_dir_with_busname, firstn.bcdday );
  mbcdday=firstn.bcdday;
  if(dlock){
      time_dvrtime_init(&oldesttime, 2099);      
      if( disk_olddestfile( diskday, &oldesttime, &oldestfilename, dlock) ) {
	disk_removefile( oldestfilename.getstring() );
	disk_rmempty264dir( diskday );
	disk_renewdiskdayinfo( i, firstn.bcdday ) ;
      }
  } else {
	char *pchar=diskday;
	string p(pchar);
	mDayFileList.add(p);
	check_dayfile(diskday); 
	deletefilefromarray();   
        removedayinfo(i,firstn.bcdday);	
  }
  return 0;  
  
}

int copy_files(char *from_root, 
	       char  *dest_root,
	       char *dest_dir,
	       char *servername)
{
  struct fileEntry *node;
  char path[256], path2[256];
  int ret=0;
#if DEBUG_HBD
  dvr_log("inside copy_files");
#endif
#ifdef ARRAY_DELETE
  mDayFileList.empty();
#endif
  
  scan_files(from_root, servername, NULL, &create_data_file_list);

  //dvr_log("scan_files done");
  file_list = listsort(file_list);

  node = file_list;
  while (node) {
    /* asked to stop by ioprocess? */
    dio_get_nodiskcheck();
    if (!g_nodiskcheck){
      ret=-1;
      break;
     }
    if(!dio_poweroff()){
      ret=-1;
      break;
     }
    if(net_active>0)
       net_active--;
     
    if(net_liveview>0){	   
	   net_liveview--;
           dio_setstate (DVR_NETWORK);	  
           sleep(1);	   
	   continue;
    } 
    if(net_playarc>0){	   
	   net_playarc--;  
           sleep(1);	   
	   continue;
    }     
    if(isInUSBretrieve()){
       sleep(1);
       continue;
    }
      
    if(get_full_path_for_fileinfo(path, sizeof(path),
				  &node->fi, servername)) {
      node = node->next;
      continue;
    }

    /* make sure there is enough space */
    while (1) {
      int space = disk_freespace(dest_dir);
      if ((space < 0) || /* error */
	  (space >= (int)(disk_minfreespace + node->fi.size / 1024 / 1024))) {
	break;
      }
#ifdef ARRAY_DELETE
      if(mDayFileList.size()>0){
	  deletefilefromarray();
      } else {
	  createfilearray(dest_dir);
      }
#else
      delete_oldest_file(dest_dir);
#endif      
    }

    if (create_dest_directory(dest_root,
			      path2, sizeof(path2),
			      &node->fi, servername)) {
#if DEBUG_HBD				
      dvr_log("create_dest_directory failed");	
#endif      
      ret=-1;
      break;
    }
#if DEBUG_HBD
    dvr_log("copy file from %s",path);
    dvr_log("to %s",path2);
#endif
    copy_file_to_path(path, path2, 0, 1);
//    remove(path);

    node = node->next;
  }

  free_file_list();
  free_dir_list();
  
#if DEBUG_HBD
  dvr_log("Copy files to SATA %s", g_nodiskcheck ? "done" : "canceled");
#endif

  return ret;
}

/*
 * DEPRECATED : USE sync_smartlog_for_hybrid
 * dest_dir: /mount_point/_busname_
 * dest_root:/mount_point
 */
int copy_smartlog_files(char *from_root, 
			char  *dest_root,
			char *dest_dir,
			char *servername)
{
  struct sfileEntry *node;
  char path[256], path2[256];

  scanSmartFiles(from_root, servername);

  node = sfile_list;
  while (node) {
    if(get_full_path_for_smartfileinfo(path, sizeof(path),
				       &node->fi, servername)) {
      node = node->next;
      continue;
    }
    
    /* make sure there is enough space */
    while (1) {
      int space = disk_freespace(dest_dir);
      if ((space < 0) || /* error */
	  (space >= (int)(disk_minfreespace + node->fi.size / 1024 / 1024))) {
	break;
      }
      delete_oldest_file(dest_dir);
    }

    /* create smartlog directory */
    snprintf(path2, sizeof(path2), "%s/smartlog", dest_root);
    if (mkdir(path2, 0777) == -1) {
      if(errno != EEXIST) {
	break;
      }
    }

    copy_file_to_path(path, path2, 0, 0);

    node = node->next;
  }

  free_smartfile_list();
  free_dir_list();

  return 0;
}
