#include "dvr.h"

#include <sys/mount.h>

#define MINDATE     (19000000)
#define MAXDATE     (21000000)

int disk_busy ;

static pthread_mutex_t disk_mutex;
static int disk_minfreespace;              // minimum free space for current disk, in Magabytes
static int disk_lockfile_percentage ;      // how many percentage locked file can occupy.

static int disk_nfileday ;

struct disk_info {
    dev_t   dev ;                        // disk device id
    int     mark ;
    string  basedir ;
    int     tlen ;                       // total video len
    int     llen ;                       // total lock file len
} ;

static array <disk_info> disk_disklist ;   // recording disk list
static string disk_base;                // recording disk base dir
static string disk_play;                // playback disk base dir. ("/var/dvr")
static string disk_curdiskfile;         // record current disk

class dir_find {
    protected:
        DIR * m_pdir ;
        struct dirent * m_pent ;
        int  m_dirlen ;
        char m_pathname[PATH_MAX] ;

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
            close();
            strncpy( m_pathname, path, sizeof(m_pathname) );
            m_dirlen=strlen( m_pathname ) ;
            if( m_dirlen>0 ) {
                if( m_pathname[m_dirlen-1]!='/' ) {
                    m_pathname[m_dirlen]='/' ;
                    m_dirlen++;
                    m_pathname[m_dirlen]='\0';
                }
            }
            m_pdir = opendir(m_pathname);
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
            if( m_pdir ) {
                while( (m_pent=readdir(m_pdir))!=NULL  ) {
                    // skip . and .. directory
                    if( (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='\0') || 
                       (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='.' && m_pent->d_name[2]=='\0') ) 
                    {
                        continue ;
                    }
                    strcpy( &m_pathname[m_dirlen], m_pent->d_name );
                    if( m_pent->d_type == DT_UNKNOWN ) {                   // d_type not available
                        struct stat findstat ;
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
            return 0 ;
        }

        int find(char * pattern) {
            if( m_pdir ) {
                while( (m_pent=readdir(m_pdir))!=NULL  ) {
                    if( fnmatch(pattern, m_pent->d_name, 0 )==0 ) {
                        strcpy( &m_pathname[m_dirlen], m_pent->d_name );
                        if( m_pent->d_type == DT_UNKNOWN ) {                   // d_type not available
                            struct stat findstat ;
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
            }
            return 0 ;
        }
        
        char * pathname()  {
            return m_pathname ;
        }
        
        char * filename() {
            return &m_pathname[m_dirlen] ;
        }
        
        // check if found a dir
        int    isdir() {
            if(m_pdir && m_pent) {
                return (m_pent->d_type == DT_DIR) ;
            }
            else {
                return 0;
            }
        }
        
        // check if found a regular file
        int    isfile(){
            if(m_pdir && m_pent) {
                return (m_pent->d_type == DT_REG) ;
            }
            else {
                return 0;
            }
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
    basename = strrchr(fullpath, '/');
    if( basename ) {
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
    int n ;
    time_dvrtime_init(dvrt,2000);
    n=sscanf( F264TIME( filename ), "%04d%02d%02d%02d%02d%02d", 
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
        if( n==4 && locklength >= 0 && locklength<length ) {
            return locklength ;
        }
        else {
            return length ;
        }
    }
    return 0 ;
}

// return free disk space in Megabytes
int disk_freespace(char *path)
{
    struct statfs stfs;
    
    if (statfs(path, &stfs) == 0) {
        return stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
    }
    return 0;
}

// remove all files and directory
void disk_rmdir(const char *dir)
{
    char * pathname ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        pathname = dfind.pathname();
        if( dfind.isdir() ) {
            disk_rmdir( pathname );
        }
        else {
            unlink(pathname);
        }
    }
    dfind.close();
    rmdir(dir);
}

// remove empty directories
int disk_rmemptydir(char *dir)
{
    int files = 0 ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if( disk_rmemptydir( dfind.pathname() ) ) {
                // dir not empty!
                files++;
            }
        }
        else {
            files++;
        }
    }
    dfind.close();
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
    if( unlink(file264 )!=0 ) {
        dvr_log( "Delete file failed : %s", basename(file264) );
        return ;
    }
    extension=strstr( f264.getstring(), ".264" );
    if( extension ) {
        strcpy( extension, ".k");
        unlink( f264.getstring() );
        strcpy( extension, ".idx" );
        unlink( f264.getstring() );
    }
}

// return 0, no changes
// return 1, renamed
// return 2, deleted
static int repairfile(const char *filename)
{
    dvrfile vfile;
    
    if (vfile.open(filename, "r+") == 0) {	// can't open it
        disk_removefile( filename );		// try delete it.
        return 0;
    }
    
    if( vfile.repair() ) {					// success?
        vfile.close();
        dvr_log("File repaired. %s", basename(filename));		
        return 1;
    }
    else {
        // can't repair, delete it
        vfile.close();
        disk_removefile( filename );
        dvr_log("Corrupt file deleted. %s", basename(filename));
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
            return vfile.repairpartiallock() ;
        }
    }
    return 0;
}

// check if there are 264 files
// return 1: found at least one .264 file
//        0: can't find any .264 file
static int disk_find264(char *dir)
{
    dir_find dfind(dir);
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if(disk_find264( dfind.pathname() )) {
                return 1;
            }
        }
        else if( dfind.isfile() ) {
            char * filename = dfind.filename() ;
            int l = strlen(filename);
            if ( l > 20 && strcmp(&filename[l - 4], ".264") == 0 ) {
                return 1 ;
            }
        }
    }
    return 0 ;
}

// remove directories has no .264 files
void disk_rmempty264dir(char *dir)
{
    if( disk_find264(dir)==0 ) {
        disk_rmdir(dir);
    }
}

// repaire all .264 files under dir
void disk_repair264(char *dir)
{
    int r264 = 0 ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            disk_repair264(  dfind.pathname() );
        }
        else if( dfind.isfile() ) {
            char * filename = dfind.filename() ;
            int l = strlen(filename);
            if ( l > 20 && strcmp(&filename[l - 4], ".264") == 0 ) {
		int fl = f264length( filename );
		int ll = f264locklength(filename);
		if( fl == 0 ) {
                    r264+=repairfile(dfind.pathname());
                }
		if( ll>0 && ll<fl ) {
                    r264+=repairepartiallock(dfind.pathname()) ;
                }
            }
        }
    }
    dfind.close();
    if( r264>0 ) {
        disk_rmempty264dir(dir);
    }
}

// play back supporting routine.

// get a list of .264 files, for play back use
static void disk_listday_help(char *dir, array <f264name> & flist, int day, int ch )
{
    dir_find dfind(dir);
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if( fnmatch( "_*_", dfind.filename(), 0 )==0 ){		// a root direct for DVR files
                char chpath[512] ;
                f264name p ;
                sprintf(chpath, "%s/%08d/CH%02d", 
                        dfind.pathname(),
                        day,
                        ch ) ;
                dir_find find264 ;
                find264.open(chpath);
                while( find264.find() ) {
                    if( find264.isfile() ) {
                        char * filename = find264.filename() ;
                        int l = strlen(filename) ;
                        if( l>8 && 
                           strcmp(&filename[l-4], ".264")==0 && 
                           strstr(filename, "_0_")==NULL )          // is it a .264 file?
                        {	
                            p=find264.pathname() ;
                            flist.add(p);
                        }
                    }
                }
                find264.close();
            }
            else {
                disk_listday_help( dfind.pathname(), flist, day, ch );
            }
        }
    }
}

// get .264 file list by day and channel
void disk_listday(array <f264name> & flist, struct dvrtime * day, int channel)
{
    flist.empty();
    disk_listday_help(disk_play.getstring(), flist, day->year * 10000 + day->month*100 + day->day, channel );
    flist.sort();
}

static void disk_getdaylist_help( char * dir, array <int> & daylist, int ch )
{
    int i;
    int day ;
    int dup ;

    dir_find dfind(dir);

    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if( sscanf(dfind.filename(), "%d", &day) == 1 ) {
                if( day>MINDATE && day<MAXDATE ) {
                    // see if this day already in the list
                    dup=0 ;
                    for( i=0; i<daylist.size(); i++ ) {
                        if( daylist[i] == day ) {
                            dup=1;
                            break;
                        }
                    }
                    if( dup==0 ) {
                        if( ch>=0 ) {
                            struct stat dstat ;
                            char chpath[512] ;
                            sprintf( chpath, "%s/CH%02d", dfind.pathname(), ch );
                            if( stat( chpath, &dstat )==0 ) {
                                if( S_ISDIR(dstat.st_mode) ) {
                                    daylist.add(day);
                                }
                            }
                        }
                        else {
                            daylist.add(day);
                        }
                    }
                }
            }
            else {
                disk_getdaylist_help(dfind.pathname(), daylist, ch );
            }
        }
    }
}

// get list of day with available .264 files
void disk_getdaylist(array <int> & daylist, int ch)
{
    daylist.empty();
    disk_getdaylist_help( disk_play.getstring(), daylist, ch);
    daylist.sort();                  // sort day list
}

// unlock locked file overlaped with specified time range
int disk_unlockfile( dvrtime * begin, dvrtime * end )
{
    return 1;
}

// find oldest .264 files 
// parameter:
//      dir: directory to start search,
//      day: only to search this day
//      timeofday:  earliest time in 'day'
//      lock:   locked file searching allowed. ( if(lock==0) don't search locked file)
static int disk_oldestfile_hlp( char *dir, int day, int & timeofday, string & filepath, int lock )
{
    char * filename ;
    struct dvrtime dvrt ;
    dir_find dfind(dir);
    int res = 0 ;
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            int nday ;
            if( sscanf( dfind.filename(), "%d", &nday)==1 ) {
                if( nday==day ) {
                    res+=disk_oldestfile_hlp( dfind.pathname(), day, timeofday, filepath, lock );    // pass onto sub dir
                }
            }
            else {
                res+=disk_oldestfile_hlp( dfind.pathname(), day, timeofday, filepath, lock );    // pass onto sub dir
            }
        }
        else if( dfind.isfile() ) {
            filename = dfind.filename();
            int l = strlen(filename) ;
            if( l>20 && strcmp(&filename[l-4], ".264")==0 ) {    // is it a .264 file ?
                if( lock || strstr(filename, "_L_")==NULL ) {
                    f264time(filename, &dvrt);
                    int tod = dvrt.hour*3600+dvrt.minute*60+dvrt.second ;
                    if( tod<timeofday ) {
                        timeofday = tod ;
                        filepath = dfind.pathname();
                        res=1;
                    }
                }
            }
        }
    }
    
    return res ;
}

// delete oldest video file,
//   lock: 1=include locked file (any video file), 0=non locked file only
int disk_deloldfile( int lock )
{
    int day ;
    int thismonth ;
    int i, j ;
    array <int> daylist ;
    
    for( i=0; i<disk_disklist.size(); i++ ) {
        disk_getdaylist_help( disk_disklist[i].basedir.getstring(), daylist, -1);
    }
    daylist.sort();

    struct dvrtime dvrt ;
    time_now(&dvrt);
    thismonth = dvrt.year*12+dvrt.month ;
    
    for( j=0; j<daylist.size(); j++) {
        day = daylist[j] ;
        string filepath ;
        int old=0 ;
        int oldtime=25*3600 ;
        if( lock || (thismonth-day/10000*12-day%10000/100)>6 ) {
            for( i=0; i<disk_disklist.size();i++) {
                old+=disk_oldestfile_hlp( disk_disklist[i].basedir.getstring(), day, oldtime, filepath, 1 );
            }
            if( old > 0 ) {
                disk_removefile( filepath.getstring() ) ;
                disk_renew( filepath.getstring(), 0 );
                break ;
            }
        }
        else {
            if( day < disk_nfileday ) {
                continue ;
            }
            else {
                for( i=0; i<disk_disklist.size();i++) {
                    old+=disk_oldestfile_hlp( disk_disklist[i].basedir.getstring(), day, oldtime, filepath, 0 );
                }
                if( old > 0 ) {
                    disk_removefile( filepath.getstring() );
                    disk_renew( filepath.getstring(), 0 );
                    disk_nfileday = day ;
                    break ;
                }
            }
        }
    }

    return 1;
}

// To renew a file list on current disk
int disk_renew( char * filename, int add )
{
    int disk ;
    int l ;
    char * base ;
    
    disk_lock();
    
    for( disk=0; disk<disk_disklist.size(); disk++ ) {
        base = disk_disklist[disk].basedir.getstring() ;
        l=disk_disklist[disk].basedir.length() ;
        if( strncmp(base, filename, l )==0 ) {
            if( add ) {
                disk_disklist[disk].tlen += f264length(filename);
                disk_disklist[disk].llen += f264locklength(filename);
            }
            else {
                disk_disklist[disk].tlen -= f264length(filename);
                if( disk_disklist[disk].tlen<0 ) {
                    disk_disklist[disk].tlen=0;
                }
                disk_disklist[disk].llen -= f264locklength(filename);
                if( disk_disklist[disk].llen<0 ) {
                    disk_disklist[disk].llen=0;
                }
            }
            break;
        }
    }

    disk_unlock();
    return 0;
}

// test if this directory writable
int disk_testwritable( char * dir ) 
{
    char testfilename[512] ;
    int t1 = 12345 ;
    int t2 = 54321 ;

    sprintf( testfilename, "%s/dvrdisktestfile", dir );

    FILE * ftest = fopen( testfilename, "w" );
    if( ftest!=NULL ) {
        fwrite( &t1, 1, sizeof(t1), ftest );
        if( fclose( ftest )!= 0 ) {
			return 0 ;
		}
    }
	else {
		return 0 ;
	}

    ftest = fopen( testfilename, "r" );
    if( ftest!=NULL ) {
        fread( &t2, 1, sizeof(t2), ftest );
		if( fclose( ftest )!= 0 ) {
			return 0 ;
		}
    }
	else {
		return 0 ;
	}
	if( unlink( testfilename )!=0 ) {
		return 0 ;
	}
    return t1==t2 ;
}

// find a recordable disk
//  1. find disk with disk space available
//  2. delete old video file and return disk with space available
// return index on disk_disklist, -1 for error
int disk_findrecdisk()
{
    int i;
    int loop ;
    int total_t, total_l ;
    
    if( disk_disklist.size()<=0 ) {
        return -1;
    }
    
    for( loop=0; loop<50; loop++ ) {
        
        // find a disk with available space
        for( i=0; i<disk_disklist.size(); i++ ) {
            if( disk_freespace( disk_disklist[i].basedir.getstring()) > disk_minfreespace ) {
                if( disk_testwritable( disk_disklist[i].basedir.getstring() ) ) {
                    return i ;              // writable disk!
                }
            }
        }

        disk_lock();

        // ok, none of the disks has space
        
        // first, let see what kind of file to delete, _L_ or _N_
        total_t=0 ;
        total_l=0 ;

        for( i=0; i<disk_disklist.size(); i++ ) {
            total_t += disk_disklist[i].tlen ;
            total_l += disk_disklist[i].llen ;
        }

        disk_unlock();
        
        if( total_t<=0 && total_l<=0 )        // no video files at all !!!
            return -1 ;

        disk_lock();
        
        if( total_l > total_t*disk_lockfile_percentage/100 ) //  locked lengh > 30%
        {
            disk_deloldfile(1);
        }
        else {
            disk_deloldfile(0);
        }

        disk_unlock();
    }    
    return -1;    
}

// 
void disk_renewdisk(char * dir)
{
    dir_find dfind(dir);
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            disk_renewdisk( dfind.pathname());
            disk_rmemptydir(dfind.pathname());
        }
        else if( dfind.isfile() ) {
            char *filename=dfind.filename();
            int l = strlen(filename);
            if( l>20 && strcmp( &filename[l-4], ".264")==0 ) {
                disk_renew(dfind.pathname());
            }
        }
    }
}

// return 1: success, 0:failed
int disk_scandisk( int diskindex )
{
    char * basedir = disk_disklist[diskindex].basedir.getstring();
    
    if( !disk_testwritable( basedir ) ) {
        return 0 ;              // no writable disk, return failed
    }

    dio_disablewatchdog();                     // disable watchdog, so system don't get reset while repairing files
    disk_repair264(basedir);
    dio_enablewatchdog();                     // disable watchdog, so system don't get reset while repairing files

    // renew video file total length
    disk_disklist[diskindex].tlen=0;
    disk_disklist[diskindex].llen=0;
    disk_renewdisk(basedir);
    
    return 1 ;
}

// scanning all disks, update disk list, check and repair video files
void disk_scanalldisk()
{
    struct stat basestat;
    struct stat diskstat;
    char * diskname;
    int  check ;
    int i;
    
    if (stat(disk_base.getstring(), &basestat) != 0) {
        dvr_log("Disk base error, not able to record!");
        return;
    }
    
    disk_lock(); 
    for( i=0; i<disk_disklist.size(); i++ ) {
        disk_disklist[i].mark=0 ;
    }

    dir_find disks(disk_base.getstring());
    while( disks.find() ) {
        if( disks.isdir() ) {
            diskname = disks.pathname();
            if( stat(diskname, &diskstat)==0 ) {
                if( diskstat.st_dev != basestat.st_dev) {	// found a mount point
                    check = 0 ;
                    for( i=0; i<disk_disklist.size(); i++ ) {
                        if( disk_disklist[i].dev == diskstat.st_dev ) {     // already in list
                            check=1;
                            disk_disklist[i].mark=1 ;
                            break;
                        }
                    }
                    if( check ) 
                        continue ;
                
                    i = disk_disklist.size() ;
                    disk_disklist[i].dev = diskstat.st_dev ;
                    disk_disklist[i].mark=1 ;
                    disk_disklist[i].basedir = diskname ;
                    if( disk_scandisk( i )<=0 ) {  // failed ?
                        disk_disklist.remove( i );
                    }
                    else {
                        dvr_log("Detected hard drive: %s", disks.filename() );
                        disk_nfileday = 0 ;         // reset oldest nfile day
                    }
                }
            }
        }
    }

    for( i=0; i<disk_disklist.size(); ) {
        if( disk_disklist[i].mark==0 ) {
            dvr_log("Hard drive removed: %s", basename(disk_disklist[i].basedir.getstring()) );
            disk_disklist.remove(i) ;
        }
        else {
            i++ ;
        }
    }

    disk_unlock();
}

int disk_maxdirty ;
static int disk_sync_state = 0 ;    // 0: no sync thread, 1: sync running, 2: restart sync

void * disk_sync(void * param)
{
    while( disk_sync_state==1 ) {
        if( g_memdirty > disk_maxdirty ) {
            disk_busy = 1 ;
            sync();
            usleep(10000);
        }
        else {
            disk_busy = 0 ;
            sleep(1);
        }
    }
    disk_sync_state = 0 ;
    return NULL ;
}

// regular disk check, every 1 seconds
void disk_check()
{
    int i;

    // check sync()
    if( disk_sync_state == 0 ) {
       // start sync thread ;
        pthread_t thid ;
        disk_sync_state = 1 ;
        if( pthread_create(&thid, NULL, disk_sync, NULL)==0 ) {
            pthread_detach(thid) ;
        }
    }

    // check disks
    disk_scanalldisk();
    
    if( disk_disklist.size()<=0 ) {
        if( rec_basedir.length()>0 ) {
            dvr_log("Abnormal disk removal.");
        }
        rec_break();
        rec_basedir="";
        goto disk_check_finish ;
    }
    
    if (rec_basedir.length()==0 || disk_freespace(rec_basedir.getstring()) <= disk_minfreespace ) {

        i = disk_findrecdisk();
        if( i<0 ) {
            rec_break();
            rec_basedir="";
            dvr_log("Can not find recordable disk.");
            goto disk_check_finish ;
        }
        
        if( strcmp(rec_basedir.getstring(), disk_disklist[i].basedir.getstring())!=0 ) {	// different disk
            if( rec_basedir.length()>0 ) {
                rec_break();
            }
            rec_basedir=disk_disklist[i].basedir.getstring() ;
            dvr_log("Start recording on disk : %s.", basename(rec_basedir.getstring())) ;
            // mark current disk
            FILE * f = fopen(disk_curdiskfile.getstring(), "w"); 
            if( f ) {
                fputs(rec_basedir.getstring(), f);
                fclose(f);
            }
        }
    }
    disk_check_finish:
        if( rec_basedir.length()<1 ) {
            // un-mark current disk
            unlink( disk_curdiskfile.getstring() );
        }
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

void disk_init()
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
    disk_play = dvrconfig.getvalue("system", "playbackdir");
    if (disk_play.length() == 0) {
        disk_play = disk_base ;
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
    
    disk_curdiskfile = dvrconfig.getvalue("system", "currentdisk");
    if( disk_curdiskfile.length()<2) {
        disk_curdiskfile="/var/dvr/dvrcurdisk" ;
    }

    // percentage of minimum locked file can be kept
    disk_lockfile_percentage = dvrconfig.getvalueint("system", "lockfile_percentage");
    if( disk_lockfile_percentage < 1 || disk_lockfile_percentage > 100 ) {
        disk_lockfile_percentage = 30 ;
    }

    disk_maxdirty=dvrconfig.getvalueint("system", "maxdirty");
    if( disk_maxdirty < 50 || disk_maxdirty > 10000 ) {
        disk_maxdirty = 500 ;
    }

    // empty disk list
    disk_disklist.empty();

    dvr_log("Disk initialized.");
}


void disk_uninit()
{

    disk_sync_state = 2 ;
    
    disk_lock();
    
    if( rec_basedir.length()>0 ) {
        ;
    }
    rec_basedir="";
    disk_base="";
    disk_disklist.empty();
    
    // un-mark current disk
    unlink( disk_curdiskfile.getstring() );
    
    disk_unlock();
    
    pthread_mutex_destroy(&disk_mutex);

    dvr_log("Disk uninitialized.");

}
