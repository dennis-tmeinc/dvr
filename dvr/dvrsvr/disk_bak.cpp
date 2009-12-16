#include "dvr.h"

#include <sys/mount.h>

int disk_busy ;

static pthread_mutex_t disk_mutex;
static int disk_minfreespace;              // minimum free space for current disk, in Magabytes
static int disk_lockfile_percentage ;      // how many percentage locked file can occupy.
static int disk_lockfile_keepdays ;        // how may days should locked file be kept. (180)

struct disk_day_info {
    int bcdday ;                    // available day in BCD. ex 20090302
    int nfiletime ;                 // earliest _N_ file time in BCD. 133222
    int nfilelen ;                  // total length of _N_ files
    int lfiletime ;                 // earliest lfile time in BCD. 133222
    int lfilelen ;                  // total length of _L_ files
} ;

class disk_info {
    public:    
        dev_t  dev ;                        // disk device id
        int    mark ;
        string basedir ;
        array  <disk_day_info>  daylist ;

        int    nfilelen ;
        int    lfilelen ;
        int    nfileday ;
        
        disk_info() {
            dev=0;
            basedir="" ;
            mark=0;
            daylist.empty();
        }
        
        disk_info & operator = ( disk_info & di ) {
            dev     = di.dev ;
            mark    = di.mark ;
            basedir = di.basedir ;
            daylist = di.daylist ;
            return *this ;
        }
} ;

static array <disk_info> disk_disklist ;   // recording disk list
static string disk_base;                // recording disk base dir
static string disk_arch;                // archive disk base dir
static string disk_play;                // playback disk base dir. ("/var/dvr")
static string disk_curdiskfile;         // record current disk

class dir_find {
    protected:
        DIR * m_pdir ;
        struct dirent * m_pent ;
        struct dirent m_entry ;
        char m_dirname[PATH_MAX] ;
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
            if( m_pdir ) {
                while( readdir_r( m_pdir, &m_entry, &m_pent)==0  ) {
                    if( m_pent==NULL ) {
                        break;
                    }
                    // skip . and .. directory
                    if( (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='\0') || 
                       (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='.' && m_pent->d_name[2]=='\0') ) {
                           continue ;
                       }
                    if( m_pent->d_type == DT_UNKNOWN ) {                   // d_type not available
                        struct stat findstat ;
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
            remove(pathname);
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
    remove( file264 );
    extension=strstr( f264.getstring(), ".264" );
    if( extension ) {
        strcpy( extension, ".k");
        remove( f264.getstring() );
        strcpy( extension, ".idx" );
        remove( f264.getstring() );
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
            if (strcmp(&pathname[l - 4], ".264") == 0) {
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
        disk_rmdir(dir);
    }
}

// repaire all .264 files under dir
void disk_repair264(char *dir)
{
    int r264 = 0 ;
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
                strstr(filename, ".264") ) 
        {
            if( strstr(filename, "_0_") ) {
                repairfile(pathname);
                r264++ ;
            }
            else if( strstr(filename, "_L_") ) {
                repairepartiallock(pathname) ;
                r264++ ;
            }
        }
    }
    dfind.close();
    if( r264>0 ) {
        disk_rmempty264dir(dir);
    }
}


// fix all "_0_" files & delete non-local video files
void disk_diskfix(char *dir)
{
//    char pattern[512];
    char * pathname ;
    char * filename ;
    dir_find dfind(dir);

    dio_disablewatchdog();                     // disable watchdog, so system don't get reset while repairing files

    while( dfind.find() ) {
        pathname = dfind.pathname();
        filename = dfind.filename();
        if( dfind.isdir() ) {
            if( fnmatch( "_*_", filename, 0 )==0 ){		// a root direct for DVR files
/*                
                sprintf( pattern, "_%s_", g_hostname);
                if( strcasecmp( pattern, filename )!=0 ) {
                    disk_rmdir( pathname );
                }
                else {
                    disk_repair264(pathname);
                }
*/            
                disk_repair264(pathname);
                disk_rmemptydir( pathname );
            }
        }
    }
    
    dio_enablewatchdog();                     // disable watchdog, so system don't get reset while repairing files
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
                if( day>20000100 && day<21000000 ) {
                    struct stat dstat ;
                    char chpath[512] ;
                    sprintf( chpath, "%s/CH%02d", dfind.pathname(), ch );
                    if( stat( chpath, &dstat )==0 ) {
                        if( S_ISDIR(dstat.st_mode) ) {
                            // see if this day already in the list
                            dup=0 ;
                            for( i=0; i<daylist.size(); i++ ) {
                                if( daylist[i] == day ) {
                                    dup=1;
                                    break;
                                }
                            }
                            if( dup==0 ) {
                                daylist.add(day);
                            }
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
            if( l>4 && strcmp(&filename[l-4], ".264")==0 ) {    // is it a .264 file ?
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
            if( l>4 && strcmp(&filename[l-4], ".264")==0 ) {    // it is a .264 file
                f264time(filename, &dvrt);
                bcdt = dvrt.hour*10000 + dvrt.minute*100 + dvrt.second ;
                l = f264length (filename);
                if( l>0 ) {
                    if( strstr(filename,"_L_")==NULL ) {    // locked file ?
                        ddi->nfilelen+=l ;
                        if( bcdt<ddi->nfiletime ) {
                            ddi->nfiletime=bcdt ;
                        }
                    }
                    else {
                        ddi->lfilelen+=l ;
                        if( bcdt<ddi->lfiletime ) {
                            ddi->lfiletime=bcdt ;
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
    disk_lock();
    
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
        disk_disklist[diskindex].daylist[i].lfilelen = 0 ;
        disk_disklist[diskindex].daylist[i].lfiletime = 240000 ;
        disk_scandayinfo( diskdaypath, &(disk_disklist[diskindex].daylist[i]) );
        if( disk_disklist[diskindex].daylist[i].nfilelen == 0 && disk_disklist[diskindex].daylist[i].lfilelen == 0 )
        {
            disk_disklist[diskindex].daylist.remove(i);
        }
    }
    
    disk_unlock();
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
        fclose( ftest ) ;
    }
    sync();
    ftest = fopen( testfilename, "r" );
    if( ftest!=NULL ) {
        fread( &t2, 1, sizeof(t2), ftest );
        fclose( ftest );
    }
    unlink( testfilename );
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
    int dlock ;
    int total_n, total_l ;
    int firstndisk, firstldisk ;
    disk_day_info firstl, firstn ;
    struct dvrtime oldesttime ;
    char diskday[512] ;
    string oldestfilename ;
    
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
        
        // ok, none of the disks has space
        
        // first, let see what kind of file to delete, _L_ or _N_
        
        firstl.bcdday=21000101 ;
        firstn.bcdday=21000101 ;
        total_n=0 ;
        total_l=0 ;
        firstndisk=0;
        firstldisk=0;
        
        disk_lock();
        
        for( i=0; i<disk_disklist.size(); i++ ) {
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
        
        if( total_n<=0 && total_l<=0 )        // on video files at all !!!?
            return -1 ;
        
        dlock=0 ;
        if( total_l>0 )  {
            struct dvrtime dvrt ;
            struct dvrtime firstlday ;
            time_now(&dvrt);
            firstlday.year = firstl.bcdday/10000 ;
            firstlday.month = firstl.bcdday/100%100 ;
            firstlday.day = firstl.bcdday%100 ;
            firstlday.hour = 0 ;
            firstlday.minute = 0 ;
            firstlday.second = 0 ;
            firstlday.milliseconds = 0 ;

            if(total_l > (total_n+total_l)*disk_lockfile_percentage/100 || //  locked lengh > 30%
               total_n <= 0 ||
               time_dvrtime_diff( &dvrt, &firstlday )/86400 > disk_lockfile_keepdays )   //  180 days by default
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
            disk_removefile( oldestfilename.getstring() );
            disk_rmempty264dir( diskday );
            disk_renewdiskdayinfo( firstndisk, firstn.bcdday ) ;
        }
        
        disk_unlock();
        
    }    
    return -1;    
}

/*
void disk_finddisk()
{
    struct stat basestat;
    struct stat diskstat;
    char * diskname;
    char hostname[128] ;
    char diskbase[512] ;
    
    // empty disk list
    disk_disklist.empty();
    
    if (stat(disk_base.getstring(), &basestat) != 0)
        return;
    dir_find disks(disk_base.getstring());
    
    while( disks.find() ) {
        if( disks.isdir() ) {
            diskname = disks.pathname();
            if( stat(diskname, &diskstat)==0 ) {
                if ( diskstat.st_dev != basestat.st_dev) {	// a mount point
                    gethostname(hostname, 128);
                    sprintf(diskbase, "%s/_%s_", diskname, hostname);
                    mkdir(diskbase, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);	
                    dir_find disktest(diskbase);
                    if( disktest.isopen() ) {				// Probably disk failed so the directory can't be opened.
                        string dn ;
                        string dntestfile ;
                        dn=diskname ;
                        dntestfile.setbufsize( dn.length()+20 ) ;
                        sprintf( dntestfile.getstring(), "%s/dvrdisktest", dn.getstring() );
                        FILE * ftest = fopen( dntestfile.getstring(), "w" );
                        if( ftest!=NULL ) {
                            if( fclose( ftest )==0 ) {
                                disk_disklist.add(dn);
                            }
                        }
                        unlink( dntestfile.getstring() );
                        disktest.close();
                    }
                }
            }
        }
    }
}
*/

/*
// disk: new plugin disk mount point
void disk_diskadd(char *disk)
{
    string dn ;
    dn = disk ;
    disk_disklist.add(dn);
}

// disk: removed plugin disk mount point
void disk_diskremove(char *disk)
{
    int len = strlen(disk);
    
    if (strncmp(disk, rec_basedir.getstring(), len) == 0) {	// current disk been removed!!!
        rec_basedir = "";
        rec_break();
        dvr_log("danger: opening disk been removed!");
    }
    for (len = 0; len < disk_disklist.size(); len++) {
        if (strcmp(disk, disk_disklist[len].getstring()) == 0) {
            disk_disklist.remove(len);
        }
    }
}
*/

// return 1: success, 0:failed
int disk_scandisk( int diskindex )
{
    int day ;
    
    if( diskindex<0 || diskindex>=disk_disklist.size() ){
        return 0 ;
    }

//    mkdir(disk_disklist[diskindex].basedir.getstring(), S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);	
    if( !disk_testwritable( disk_disklist[diskindex].basedir.getstring() ) ) {
        return 0 ;              // no writable disk, return failed
    }

    // do disk fix here ?
    disk_diskfix(disk_disklist[diskindex].basedir.getstring());
    
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

// scanning all disks, update disk list, check and repair video files
void disk_scanalldisk()
{
    struct stat basestat;
    struct stat diskstat;
    char * diskname;
//    char diskbase[512] ;
    int  disk_check ;
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
                    disk_check = 0 ;
                    for( i=0; i<disk_disklist.size(); i++ ) {
                        if( disk_disklist[i].dev == diskstat.st_dev ) {     // already in list
                            disk_check=1;
                            disk_disklist[i].mark=1 ;
                            break;
                        }
                    }
                    if( disk_check ) 
                        continue ;
                
                    i = disk_disklist.size() ;
                    disk_disklist[i].dev = diskstat.st_dev ;
                    disk_disklist[i].mark=1 ;
//                    sprintf( diskbase, "%s/_%s_", diskname, g_hostname );
//                    disk_disklist[i].basedir = diskbase ;
                    disk_disklist[i].basedir = diskname ;
                    disk_disklist[i].daylist.empty();
                    if( disk_scandisk( i )<=0 ) {  // failed ?
                        disk_disklist.remove( i );
                    }
                    else {
                        dvr_log("Detected hard drive: %s", disks.filename() );
                    }
                }
            }
        }
    }

    for( i=0; i<disk_disklist.size(); ) {
        if( disk_disklist[i].mark==0 ) {
            dvr_log("Hard drive removed: %s", disk_disklist[i].basedir.getstring() );
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
    char basedir[512];
    int i;

    if( disk_sync_state == 0 ) {
       // start sync thread ;
        pthread_t thid ;
        disk_sync_state = 1 ;
        if( pthread_create(&thid, NULL, disk_sync, NULL)==0 ) {
            pthread_detach(thid) ;
        }
    }

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
            dvr_log("Start recording on disk : %s.", rec_basedir.getstring()) ;
            // mark current disk
            FILE * f = fopen(disk_curdiskfile.getstring(), "w"); 
            if( f ) {
                strncpy( basedir, rec_basedir.getstring(), sizeof(basedir)-1);
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

    // how many days locked file can be kept
    disk_lockfile_keepdays = dvrconfig.getvalueint("system", "lockfile_keepdays");
    if( disk_lockfile_keepdays < 1 || disk_lockfile_keepdays > 800 ) {
        disk_lockfile_keepdays = 180 ;
    }

    disk_maxdirty=dvrconfig.getvalueint("system", "maxdirty");
    if( disk_maxdirty < 50 || disk_maxdirty > 10000 ) {
        disk_maxdirty = 500 ;
    }

    // empty disk list
    disk_disklist.empty();

    // check disk
//    disk_check();
    
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
