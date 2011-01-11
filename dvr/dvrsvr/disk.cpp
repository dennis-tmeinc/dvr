#include "dvr.h"

#include <sys/mount.h>

#define MINDATE     (19800000)
#define MAXDATE     (21000000)

int disk_busy ;

static int disk_minfreespace;              // minimum free space for current disk, in Magabytes
static int disk_lockfile_percentage ;      // how many percentage locked file can occupy.
static int disk_removeprerecordfile ;       // if pre-recorded file be removed
static int disk_tlen ;
static int disk_llen ;

struct disk_info {
    dev_t   dev ;                        // disk device id
    int     mark ;
    string  basedir ;
//    int     tlen ;                       // total video len
//    int     llen ;                       // total lock file len
} ;

static array <disk_info> disk_disklist ;   // recording disk list
static string disk_base;                // recording disk base dir
static string disk_play;                // playback disk base dir. ("/var/dvr")
static string disk_arch;                // archiving disk base dir, ("/var/dvr/arch")
static string disk_curdiskfile;         // record current disk
static string disk_archdiskfile ;       // current archieving disk

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
            int r=0 ;
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
                    r=1 ;
                    break ;
                }
            }
            return r ;
        }

        int find(char * pattern) {
            int r=0 ;
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
                        r=1 ;
                        break ;
                    }
                }
            }
            return r ;
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

// get channel number of .264 file
int f264channel(const char *filename)
{
    int ch ;
    char * basename = basefilename( filename );
    if( basename[0]=='C' && basename[1]=='H' && sscanf( basename+2, "%02d", &ch )==1 ) {
        return ch;
    }
    else {
        return -1 ;
    }
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

// remove empty directories
void disk_rmemptytree(const char *path)
{
    char pa[256] ;
    char * p;
    strncpy(pa, path, 255);

    while( (p=strrchr(pa,'/'))!=NULL ) {
        *p=0;
        if( strlen(pa)>0 ) {
            if( rmdir(pa)!=0 ) {
                break;
            }
        }
    }
}

// remove .264 files and related .k, .idx
// return 0 for success
static int disk_removefile( const char * file264 ) 
{
    string f264 ;
    char * extension ;
    f264 = file264 ;
    if( unlink(file264 )!=0 ) {
        dvr_log( "Delete file failed : %s", basename(file264) );
        return -1;
    }
    extension=strstr( f264.getstring(), g_264ext );
    if( extension ) {
        strcpy( extension, ".k");
        unlink( f264.getstring() );
        strcpy( extension, ".idx" );
        unlink( f264.getstring() );
    }
    disk_rmemptytree(file264);
    return 0 ;
}

// return 0, no changes
// return 1, renamed
// return 2, deleted
static int repairfile(const char *filename)
{
    dvrfile vfile;
    
    if (vfile.open(filename, "r+b") == 0) {	// can't open it
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
        if (vfile.open(filename, "r+b") ) {	// can't open it
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
            if ( l > 20 && strcmp(&filename[l - 4], g_264ext) == 0 ) {
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
            char * lp ;
            char * filename = dfind.filename() ;
            int l = strlen(filename);
            if ( l > 20 && strcmp(&filename[l - 4], g_264ext ) == 0 ) {
                if( (lp=strstr( filename, "_P_" ))!=NULL ) {
                    if( disk_removeprerecordfile ) {
                        dvrfile::remove(dfind.pathname());                      // remove pre-recording file
                        continue ;
                    }
                    else {
                        string cpfname(dfind.pathname());
                        lp[1]='N' ;
                        dvrfile::rename(cpfname.getstring(), dfind.pathname());
                    }
                }
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
    int channel ;
    dir_find dfind(dir);
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if( fnmatch( "_*_", dfind.filename(), 0 )==0 ){		// a root direct for DVR files
                char chpath[512] ;
                f264name p ;
                for( channel=0; channel<16; channel++ ) {
                    if( ch>=0 && channel!=ch ) {
                        continue ;
                    }
                    sprintf(chpath, "%s/%08d/CH%02d", 
                        dfind.pathname(),
                        day,
                        channel ) ;
                    dir_find find264 ;
                    find264.open(chpath);
                    while( find264.find() ) {
                        if( find264.isfile() ) {
                            char * filename = find264.filename() ;
                            int l = strlen(filename) ;
                            if( l>8 && 
                                strcmp(&filename[l-4], g_264ext)==0 && 
								( strstr(filename, "_L_")!=NULL || strstr(filename, "_N_")!=NULL ) &&
                                strstr(filename, "_0_")==NULL )          // is it a .264 file?
                            {	
                                p=find264.pathname() ;
                                flist.add(p);
                            }
                        }
                    }
                    find264.close();
                }
            }
            else {
                disk_listday_help( dfind.pathname(), flist, day, ch );
            }
        }
    }
}

// get .264 file list by day and channel (for playback)
int disk_listday(array <f264name> & flist, struct dvrtime * day, int channel)
{
    flist.empty();
    disk_listday_help(disk_play.getstring(), flist, day->year * 10000 + day->month*100 + day->day, channel );
    flist.sort();
    if( flist.size()<2 ) {
        return flist.size();
    }
    
    // remove duplicated files
    int i ;
    struct dvrtime t1, t2 ;
    int l1, l2 ;
    int ch1, ch2 ;
    f264time(  flist[0].getstring(), &t1 );
    l1 = f264length(  flist[0].getstring() );
    ch1 = f264channel( flist[0].getstring() );
    for( i=1; i<flist.size();  ) {
        f264time(  flist[i].getstring(), &t2 );
        l2 = f264length(  flist[i].getstring() );
        ch2 = f264channel( flist[i].getstring() );
        if( t1==t2 && l1==l2 && ch1==ch2 ) {
            flist.remove(i);
        }
        else {
            t1 = t2 ;
            l1 = l2 ;
            ch1 = ch2 ;
            i++ ;
        }
    }
    flist.compact();
    return flist.size();
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
            if( l>20 && strcmp(&filename[l-4], g_264ext)==0 ) {    // is it a .264 file ?
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

// delete old video file,
//   lock: 1=include locked file (any video file), 0=non locked file only
int disk_deloldfile( int lock )
{
    int day ;
    int thismonth ;
    int i, j ;

    static array <f264name> oldfilelist ;
    static array <f264name> oldnfilelist ;

    if( lock ) {
        if( oldfilelist.size()>0 ) {
            if( disk_removefile( oldfilelist[0].getstring() )==0 ) {
                disk_renew( oldfilelist[0].getstring(), 0 );
            }
            oldfilelist.remove(0);
            return 1 ;
        }
    }
    else {  // only delete _N_ files
        for( i=0; i<oldfilelist.size(); i++ ) {
            if( strstr( basename(oldfilelist[i].getstring()), "_N_" ) ) {
                if( disk_removefile( oldfilelist[i].getstring() )==0 ) {
                    disk_renew( oldfilelist[i].getstring(), 0 );
                }
                oldfilelist.remove(i);
                return 1 ;
            }
        }
        if( oldnfilelist.size()>0 ) {
            if( disk_removefile( oldnfilelist[0].getstring() )==0 ) {
                disk_renew( oldnfilelist[0].getstring(), 0 );
            }
            oldnfilelist.remove(0);
            return 1 ;
        }
    }

    oldfilelist.empty();
    oldnfilelist.empty();

    array <int> daylist ;
    disk_getdaylist_help( disk_base.getstring(), daylist, -1);
    daylist.sort();

    array <f264name> flist ;
    
    struct dvrtime dvrt ;
    time_now(&dvrt);
    thismonth = dvrt.year*12+dvrt.month ;

    // fill oldfilelist
    for( j=0; j<daylist.size(); j++) {
        day = daylist[j] ;
        flist.empty();
        disk_listday_help(disk_base.getstring(), flist, daylist[j], -1 );
        for( i=0; i<flist.size(); i++ ) {
            oldfilelist.add(flist[i]);
        }
        if( oldfilelist.size()>0 ) {
            oldfilelist.sort();
            break ;
        }
    }

    // fill oldnfilelist
    for( j=0; j<daylist.size(); j++) {
        day = daylist[j] ;
        flist.empty();
        disk_listday_help(disk_base.getstring(), flist, daylist[j], -1 );
        if( flist.size()<=0 ) 
            continue ;
        if( (thismonth-day/10000*12-day%10000/100)>6 ) {
            for( i=0; i<flist.size(); i++ ) {
                oldnfilelist.add( flist[i] ) ;
            }
        }
        else {
            for( i=0; i<flist.size(); i++ ) {
                if( strstr( basename(flist[i].getstring()), "_N_" ) ) {
                    oldnfilelist.add( flist[i] );
                }
            }
        }
        if( oldnfilelist.size()>0 ) {
            oldnfilelist.sort();
            break;
        }
    }

    return 0 ;
}


// To renew a file list on current disk
int disk_renew( char * filename, int add )
{
//    int disk ;
//    int l ;
//    char * base ;

    if( add ) {
        disk_tlen += f264length(filename);
        disk_llen += f264locklength(filename);
    }
    else {
        disk_tlen -= f264length(filename);
        if( disk_tlen<0 ) {
            disk_tlen=0;
        }
        disk_llen -= f264locklength(filename);
        if( disk_llen<0 ) {
            disk_llen=0;
        }
    }

    if( disk_llen>disk_tlen ) {
        disk_llen=disk_tlen ;
    }

/*    
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
*/     
    return 0;
}

// test if this directory writable
int disk_testwritable( char * dir ) 
{
    char testfilename[512] ;
    int t1 = 12345 ;
    int t2 = 54321 ;

    sprintf( testfilename, "%s/dvrdisktestfile", dir );

    FILE * ftest = file_open( testfilename, "w" );
    if( ftest!=NULL ) {
        file_write( &t1, sizeof(t1), ftest );
        if( file_close( ftest )!= 0 ) {
			return 0 ;
		}
    }
	else {
		return 0 ;
	}

    ftest = file_open( testfilename, "r" );
    if( ftest!=NULL ) {
        file_read( &t2, sizeof(t2), ftest );
		if( file_close( ftest )!= 0 ) {
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
//    int total_t, total_l ;
    
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
        if( disk_tlen<=0 && disk_llen<=0 )        // no video files at all !!!
            return -1 ;

        if( disk_llen >= disk_tlen*disk_lockfile_percentage/100 ) //  locked lengh > 30%
        {
            disk_deloldfile(1);
        }
        else {
            disk_deloldfile(0);
        }
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
            if( l>20 && strcmp( &filename[l-4], g_264ext)==0 ) {
                disk_renew(dfind.pathname());
            }
        }
    }
}

/*
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
//    disk_disklist[diskindex].tlen=0;
//    disk_disklist[diskindex].llen=0;
    disk_renewdisk(basedir);
    
    return 1 ;
}
*/

// scanning all disks, update disk list, check and repair video files
int disk_scanalldisk()
{
    struct stat basestat;
    struct stat diskstat;
    char * diskname;
    int i;
    
    if (stat(disk_base.getstring(), &basestat) != 0) {
        dvr_log("Disk base error, not able to record!");
        return 0;
    }
    
    for( i=0; i<disk_disklist.size(); i++ ) {
        disk_disklist[i].mark=0 ;
    }

    dir_find disks(disk_base.getstring());
    while( disks.find() ) {
        if( disks.isdir() ) {
            diskname = disks.pathname();
            if( stat(diskname, &diskstat)==0 ) {
                if( diskstat.st_dev != basestat.st_dev) {	// found a mount point
                    for( i=0; i<disk_disklist.size(); i++ ) {
                        if( disk_disklist[i].dev == diskstat.st_dev ) {     // already in list
                            disk_disklist[i].mark=1 ;
                            break;
                        }
                    }
                    if( i<disk_disklist.size() ) 
                        continue ;

                    // find a new disk
                    if( disk_testwritable( diskname ) ) {
                        dio_disablewatchdog();                     // disable watchdog, so system don't get reset while repairing files
                        disk_repair264(diskname);
                        dio_enablewatchdog();                     // disable watchdog, so system don't get reset while repairing files
                        disk_renewdisk(diskname);
                        i = disk_disklist.size() ;
                        disk_disklist[i].dev = diskstat.st_dev ;
                        disk_disklist[i].mark=1 ;
                        disk_disklist[i].basedir = diskname ;
                        dvr_log("Detected hard drive: %s", disks.filename() );
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

    return disk_disklist.size();
}

int disk_stat(int * recordtime, int * lockfiletime, int * remaintime)
{
    int totalsp=0 ;
    int freesp= 0 ;

    if( rec_basedir.length()>0 ) {
        struct statfs stfs;
        if (statfs(rec_basedir.getstring(), &stfs) == 0) {
            totalsp =  stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
            freesp = stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
        }
    }

    if( totalsp == 0 ) {
        *recordtime = 0 ;
        *lockfiletime = 0 ;
        *remaintime = 0 ;
        return 0 ;
    }
    
    if( freesp <= disk_minfreespace ) {
        *recordtime = disk_tlen ;
        *lockfiletime = disk_llen ;
        *remaintime = 0 ;
        return 1 ;
    }

    if( disk_tlen<=0 ) {
        *recordtime=0 ;
        *lockfiletime=0 ;
        *remaintime= (freesp-disk_minfreespace) * 10 ;     // estimated recording time
        return 1 ;
    }

    *recordtime = disk_tlen ;
    *lockfiletime = disk_llen ;
    if( totalsp>freesp ) {
        *remaintime = (int)((float)(freesp-disk_minfreespace)* (float)disk_tlen / (float)(totalsp-freesp)) ;
    }
    else {
        *remaintime = (freesp-disk_minfreespace) * 10 ;
    }
    return 1 ;
}

int disk_maxdirty ;

void disk_sync()
{
    if( g_memdirty > disk_maxdirty ) {
        disk_busy = 1 ;
        sync();
        disk_busy = 0 ;
    }
    return ;
}

// 0: not archiving, 1: running, -1: to stop archiving
static int disk_archive_run = 0 ;

// delete oldest archive video file,
int disk_archive_deloldfile(char * archdir)
{
    int day ;
    int i, j ;

    static string s_archdir ;
    static array <f264name> oldfilelist ;

    if( strcmp(archdir, s_archdir.getstring())!=0 ) {
        oldfilelist.empty();
        s_archdir = archdir ;
    }

    if( oldfilelist.size()>0 ) {
        disk_removefile( oldfilelist[0].getstring() );
        oldfilelist.remove(0);
        return 1 ;
    }
    
    oldfilelist.empty();

    array <int> daylist ;
    disk_getdaylist_help( archdir, daylist, -1);
    daylist.sort();

    array <f264name> flist ;

    // fill oldfilelist
    for( j=0; j<daylist.size(); j++) {
        day = daylist[j] ;
        flist.empty();
        disk_listday_help(disk_arch.getstring(), flist, day, -1 );
        for( i=0; i<flist.size(); i++ ) {
            oldfilelist.add(flist[i]);
        }
        if( oldfilelist.size()>0 ) {
            oldfilelist.sort();
            break ;
        }
    }
    return 0 ;
}

// return 1 success
int disk_archive_mkfreespace( char * diskdir, int minfreespace )
{

    int retry ;

    if( disk_freespace( diskdir ) > disk_minfreespace ) {
        return 1 ;
    }
    
    for(retry=0; retry<100; retry++) {
        if( disk_freespace( diskdir ) <= disk_minfreespace ) {
            disk_archive_deloldfile(diskdir);
        }
        else {
            return 1;
        }
    }
    return 0 ;
}


static char disk_archive_tmpfile[]=".DVRARCH" ;
static int  disk_archive_unlock = 0 ;           // by default not to unlock archived file

// return 1: success, 0: fail, -1: to quit
int disk_archive_copyfile( char * srcfile, char * destfile )
{
    FILE * fsrc ;
    FILE * fdest ;
    char * filebuf ;
    int r ;
    int res ;

#define ARCH_BUFSIZE (0x40000)    

    filebuf=(char *)malloc(ARCH_BUFSIZE) ;
    if( filebuf==NULL ) {
        return 0 ;
    }
    
    fsrc = file_open( srcfile, "rb" );
    fdest = file_open( destfile, "wb" );
    if( fsrc==NULL || fdest==NULL ) {
        if( fsrc ) file_close( fsrc );
        if( fdest ) file_close( fdest );
        free(filebuf);
        return 0 ;
    }

    res = 1 ;
    while( (r=file_read( filebuf, ARCH_BUFSIZE, fsrc ))>0 ) {
        file_write( filebuf, r, fdest ) ;
        while( rec_busy || disk_busy || g_cpu_usage>0.6 ) {
            usleep( 10000 );
            if( disk_archive_run != 1 ) {
                break;
            }
        }
        if( disk_archive_run != 1 ) {
            res = 0 ;
            break;
        }
    }
    file_close( fsrc ) ;
    file_close( fdest ) ;
    free(filebuf);
    return res ;

}

// archiving files
int disk_archive_arch( char * filename, char * srcdir, char * destdir )
{
    struct stat sfst ;
    struct stat fst ;
    char * p ;
    int l1, l ;
    int res=0 ;

    char arch_filename[256] ;
    char arch_tmpfile[256] ;

    if( stat( filename, &sfst )!=0 ) {
        return 0;
    }

    if( disk_archive_mkfreespace( destdir, disk_minfreespace )==0 ) {
        // can't free more disk space
        return 0;
    }

    l1 = strlen(filename);
    l = strlen(srcdir) ;
    if( l1<=l ) {               // filename error!
        return 0;
    }

    if( strncmp( filename, srcdir, l )!=0 ) {      // source file name error!
        return 0;
    }

    sprintf( arch_filename, "%s%s", destdir, &filename[l] );

    if( stat( arch_filename, &fst )==0 ) {
        if( S_ISREG( fst.st_mode ) && fst.st_size == sfst.st_size ) {
            // destination file already exist
            if( disk_archive_unlock ) {
                dvrfile::unlock( filename );
            }
            return 1;
        }
    }

    l1=strlen( destdir ) ;
    p = strstr( &arch_filename[l1], "_L_" ) ;
    if( p ) {
        p[1]='N' ;  
        if( stat( arch_filename, &fst )==0 ) {
            if( S_ISREG( fst.st_mode ) && fst.st_size == sfst.st_size ) {      
                // unlocked arch file already exist
                if( disk_archive_unlock ) {
                    dvrfile::unlock( filename );
                }
                return 1;
            }
        }
        p[1] = 'L' ;
    }
        
    // create dirs
    p = &arch_filename[l1] ;
    while( (p=strchr( p+1, '/' ))!=NULL ) {
        *p = 0 ;
        mkdir( arch_filename, 0777 );
        *p = '/' ;
    }

    // copy file to a temperary file in arching dir
    sprintf(arch_tmpfile, "%s/%s%s", destdir, disk_archive_tmpfile, g_264ext );
    res = disk_archive_copyfile( filename, arch_tmpfile );
    if( res ) {
        // move file to final archive file
        rename( arch_tmpfile, arch_filename );

        // copy .k file if available
        strcpy( arch_tmpfile, filename ) ;
        l = strlen( arch_tmpfile ) ;
        if( strcmp( &arch_tmpfile[l-4], g_264ext)==0 ) {
            arch_tmpfile[l-3] = 'k' ;
            arch_tmpfile[l-2] = 0 ;
            l = strlen( arch_filename );
            arch_filename[l-3] = 'k' ;
            arch_filename[l-2] = 0 ;
            res = disk_archive_copyfile( arch_tmpfile, arch_filename );
        }
        if( disk_archive_unlock ) {
            dvrfile::unlock( filename );
        }
    }
    return res ;
}

// copy log files, smartlog files
static int disk_archive_cplogfile( char * srcdir, char * destdir)
{
    int l ;
    char srcfile[256] , destfile[256] ;
    dir_find dfind ;

    extern string logfile ;
// copy dvrlog file
    if( logfile.length()>0 ) {
        sprintf(srcfile, "%s/_%s_/%s", srcdir, g_hostname, logfile.getstring());
        sprintf(destfile, "%s/_%s_/%s", destdir, g_hostname, logfile.getstring());
        disk_archive_copyfile(srcfile, destfile ) ;
    }

    // copy smartlog
    sprintf( srcfile, "%s/smartlog", srcdir );
    dfind.open( srcfile );
    while( disk_archive_run == 1 && dfind.find() ) {
        if( dfind.isfile() ) {
            if( strstr( dfind.filename(), "_L." ) )
            {
                sprintf( destfile, "%s/smartlog", destdir );
                mkdir( destfile, 0755 );
                sprintf( destfile, "%s/smartlog/%s", destdir, dfind.filename() );
                if( disk_archive_copyfile( dfind.pathname(), destfile ) ) {
                    if( disk_archive_unlock ) {
                        strcpy( destfile, dfind.pathname() );
                        l=strlen( destfile );
                        if( l>6 && destfile[l-5]== 'L' ) {
                            destfile[l-5] = 'N' ;
                            rename( dfind.pathname(), destfile );
                        }
                    }
                }
            }
        }
    }
    return 1 ;
}

static void disk_archive_round(char * srcdir, char * destdir)
{
    int i, j, day ;
    array <int> daylist ;
    array <f264name> flist ;
    FILE * archupdfile ;
    char   archupdfilename[128] ;
    int    upd_date ;
    int    upd_time ;

    // get last archived file time
    upd_date=upd_time=0;
    sprintf( archupdfilename, "%s/.archupd", srcdir);
    archupdfile = file_open( archupdfilename, "r" );
    if( archupdfile ) {
        if( fscanf(archupdfile, "%d %d", 
             &upd_date,
             &upd_time )<2 ) 
        {
            upd_date=upd_time=0 ;
        }
        file_close( archupdfile );
    }

    disk_getdaylist_help(srcdir, daylist, -1);
    daylist.sort();

    for( j=0; j<daylist.size(); j++ ) {
        day = daylist[j] ;
        if( day<upd_date ) 
            continue ;
        flist.empty();
        disk_listday_help(srcdir, flist, day, -1 );
        flist.sort();
        for( i=0; i<flist.size(); i++ ) {
			int l, lockl; 
			l = f264length(flist[i].getstring());
			lockl = f264locklength( flist[i].getstring());
			if( lockl>0 && lockl==l ) {
                int ch, fday, ftime ;
                struct dvrtime ft ;
                ch = f264channel( flist[i].getstring() );
                if( ch<0 || ch>= cap_channels ) {
                    continue ;
                }
                f264time(flist[i].getstring(), &ft);
                fday = ft.year*10000+ft.month*100+ft.day ;
                ftime = ft.hour*10000+ft.minute*100+ft.second ;
                if( fday>upd_date || ftime>=upd_time ) {
                    if( disk_archive_arch( flist[i].getstring(), srcdir, destdir ) ) {
                        // successfully archived. update latest archive file date & time
                        upd_date=fday ;
                        upd_time=ftime ;
                    }
                }
			}
            if( disk_archive_run != 1 ) break;
        }
        if( disk_archive_run != 1 ) break;
    }

    archupdfile = file_open( archupdfilename, "w" );
    if( archupdfile ) {
        fprintf( archupdfile, "%d %d", upd_date, upd_time) ;
        file_close( archupdfile );
    }

    if( disk_archive_run == 1 ) {
        // copy log files, smartlog files
        disk_archive_cplogfile(srcdir, destdir);
    }
    return ;
}

int disk_archive_basedisk(string & archbase)
{
    struct stat basestat;
    struct stat diskstat;
    char * diskname;

    if( disk_arch.length() <= 0 ) {
        return 0 ;
    }
    
    if (stat(disk_arch.getstring(), &basestat) != 0) {      // no archive root dir
        return 0;
    }

    dir_find disks(disk_arch.getstring());
    while( disks.find() ) {
        if( disks.isdir() ) {
            diskname = disks.pathname();
            if( stat(diskname, &diskstat)==0 ) {
                if( diskstat.st_dev != basestat.st_dev) {	// found a mount point
                    if( disk_testwritable( diskname ) ) {
                        archbase = diskname ;
                        if( disk_archdiskfile.length()>0 ) {
                            FILE * f = fopen(disk_archdiskfile.getstring(), "w"); 
                            if( f ) {
                                fputs(diskname, f);
                                fclose(f);
                            }
                        }
                        return 1 ;
                    }
                }
            }
        }
    }
    return 0 ;
}

void * disk_archive_thread(void * param)
{
    string archbase ;
    if( rec_basedir.length()>0 && disk_archive_basedisk( archbase ) ) {
        disk_archive_round(rec_basedir.getstring(), archbase.getstring() );
    }
    disk_archive_run = 0 ;
    return NULL ;
}

void disk_archive_start()
{
    // archiving thread
    pthread_t archive_threadid ;    
    if( disk_archive_run == 0 ) {
        disk_archive_run = 1 ;
        if( pthread_create(&archive_threadid, NULL, disk_archive_thread, NULL )==0 ) {
            pthread_detach( archive_threadid ) ;        // detach archiving thread
        }
        else {
            disk_archive_run = 0 ;
        }
    }
}

void disk_archive_stop()
{
    disk_archive_run = -1 ;
}

int disk_archive_runstate()
{
    return disk_archive_run!=0 ;   // arch is actively run
}

// regular disk check, every 3 seconds
void disk_check()
{
    int i;

    // check disks
    if( disk_scanalldisk()<=0 ) {
        if( rec_basedir.length()>0 ) {
            dvr_log("Abnormal disk removal.");
        }
        rec_break();
        rec_basedir="";
        disk_llen = 0 ;
        disk_tlen = 0 ;
        goto disk_check_finish ;
    }

    disk_sync();
    
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
    char * pcfg;
    int l;

    config dvrconfig(dvrconfigfile);

    rec_basedir = "";
    disk_base = dvrconfig.getvalue("system", "mountdir");
    if (disk_base.length() == 0) {
        disk_base = "/var/dvr/disks";
    }
    disk_play = dvrconfig.getvalue("system", "playbackdir");
    if (disk_play.length() == 0) {
        disk_play = disk_base ;
    }
    disk_arch = dvrconfig.getvalue("system", "archivedir");

    pcfg = dvrconfig.getvalue("system", "mindiskspace");
    l = strlen( pcfg );
    if (sscanf(pcfg, "%d", &disk_minfreespace)) {
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

    disk_removeprerecordfile=dvrconfig.getvalueint("system", "removeprerecordfile");

    disk_maxdirty=dvrconfig.getvalueint("system", "maxdirty");
    if( disk_maxdirty < 50 || disk_maxdirty > 10000 ) {
        disk_maxdirty = 500 ;
    }

    // empty disk list
    disk_disklist.empty();
    disk_llen = 0 ;
    disk_tlen = 0 ;
    disk_busy = 0 ;

    disk_archdiskfile = dvrconfig.getvalue("system", "archdisk");
    if( disk_archdiskfile.length()<2) {
        disk_archdiskfile="/var/dvr/dvrarchdisk" ;
    }

    disk_archive_unlock = dvrconfig.getvalueint("system", "arch_unlock");
        
    // init archiving variable
    disk_archive_run = 0 ;
    
    dvr_log("Disk initialized.");

}

void disk_uninit()
{
    // to stop archiving thread 
    disk_archive_run = 0 ;

    if( rec_basedir.length()>0 ) {
        ;
    }
    rec_basedir="";
    disk_base="";
    disk_disklist.empty();
    disk_busy=0 ;

// make these file persistant    
    // un-mark current recording disk
//    unlink( disk_curdiskfile.getstring() );

    // un-mark archieving disk
//    unlink( disk_archdiskfile.getstring() );

    sync();

    dvr_log("Disk uninitialized.");

}
