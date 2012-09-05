#include "dvr.h"

#include <sys/mount.h>

#include "dir.h"

#define MINDATE     (19800000)
#define MAXDATE     (21000000)

volatile int disk_busy ;
int disk_error ;

static int disk_minfreespace;              // minimum free space for current disk, in Magabytes
static int disk_minfreespace_percentage;   // minimum free space for current disk, in percentage
static int disk_lockfile_percentage ;      // how many percentage locked file can occupy.
static int disk_filekeepdays ;
static int disk_removeprerecordfile ;       // if pre-recorded file be removed
static int disk_tlen ;
static int disk_llen ;
static array <f264name> disk_oldfilelist ;
static array <f264name> disk_oldnfilelist ;

static pthread_t disk_cleanthreadid;
static int disk_clean_run = 0 ;           // 0: quit cleaning thread, 1: keep running

struct disk_info {
    dev_t   dev ;                        // disk device id
    int     mark ;
    int		readonly ;
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
static int    disk_arch_mincpuusage ;   // minimum cpu usage to do archiving

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

// return true for disk space check pass, false for failed
int disk_freespace_check(char *path)
{
    struct statfs stfs;

    if (statfs(path, &stfs) == 0) {
        return
            (int)(stfs.f_bavail * 100 / stfs.f_blocks) >= disk_minfreespace_percentage  &&
            (int)(stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize)) >= disk_minfreespace ;
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
    extension=strstr( f264, g_264ext );
    if( extension ) {
        strcpy( extension, ".k");
        unlink( f264 );
        strcpy( extension, ".idx" );
        unlink( f264 );
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
        if( disk_removefile( filename )==0 ) {
            dvr_log("Corrupt file deleted. %s", basename(filename));
        }
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
    struct dvrtime today, fileday ;
    time_now(&today);
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
                f264time( filename, &fileday );
                if( time_dvrtime_diff( &today, &fileday )> disk_filekeepdays * (24*60*60) ) {
                    dvrfile::remove(dfind.pathname());                      // remove pre-recording file
                    continue ;
                }
                if( (lp=strstr( filename, "_P_" ))!=NULL ) {
                    if( disk_removeprerecordfile ) {
                        dvrfile::remove(dfind.pathname());                      // remove pre-recording file
                        continue ;
                    }
                    else {
                        string cpfname(dfind.pathname());
                        lp[1]='N' ;
                        dvrfile::rename(cpfname, dfind.pathname());
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
                                strstr(filename, "_0_")==NULL )          // must be a propery closed file
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
    disk_listday_help(disk_play, flist, day->year * 10000 + day->month*100 + day->day, channel );
    flist.sort();
    if( flist.size()<2 ) {
        return flist.size();
    }

    // remove duplicated files
    int i ;
    struct dvrtime t1, t2 ;
    int l1, l2 ;
    int ch1, ch2 ;
    f264time(  flist[0], &t1 );
    l1 = f264length(  flist[0]);
    ch1 = f264channel( flist[0] );
    for( i=1; i<flist.size();  ) {
        f264time(  flist[i], &t2 );
        l2 = f264length(  flist[i] );
        ch2 = f264channel( flist[i] );
        if( t1==t2 && l1==l2 && ch1==ch2 ) {
            if( strstr( flist[i], disk_base )!=NULL ) {			// prefer to use (keep) recording disk
                flist.remove(i-1);
            }
            else {
                flist.remove(i);
            }
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
    disk_getdaylist_help( disk_play, daylist, ch);
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
//   return 0: failed, 1: success
int disk_deloldfile( int lock )
{
    int res = 0 ;
    int day ;
    int thismonth ;
    int i, j ;

    if( lock ) {
        if( disk_oldfilelist.size()>0 ) {
            if( disk_removefile( disk_oldfilelist[0] )==0 ) {
                disk_renew( disk_oldfilelist[0], 0 );
                res = 1 ;
            }
            disk_oldfilelist.remove(0);
            return res ;
        }
    }
    else {  // only delete _N_ files
        for( i=0; i<disk_oldfilelist.size(); i++ ) {
            if( strstr( basename(disk_oldfilelist[i]), "_N_" ) ) {
                if( disk_removefile( disk_oldfilelist[i] )==0 ) {
                    disk_renew( disk_oldfilelist[i], 0 );
                    res = 1 ;
                }
                disk_oldfilelist.remove(i);
                return res ;
            }
        }
        if( disk_oldnfilelist.size()>0 ) {
            if( disk_removefile( disk_oldnfilelist[0] )==0 ) {
                disk_renew( disk_oldnfilelist[0], 0 );
                res = 1 ;
            }
            disk_oldnfilelist.remove(0);
            return res ;
        }
    }

    // rebuild old file lists

    disk_oldfilelist.empty();
    disk_oldnfilelist.empty();

    array <int> daylist ;
    disk_getdaylist_help( disk_base, daylist, -1);
    daylist.sort();

    array <f264name> flist ;

    struct dvrtime dvrt ;
    time_now(&dvrt);
    thismonth = dvrt.year*12+dvrt.month ;

    // fill oldfilelist
    for( j=0; j<daylist.size(); j++) {
        day = daylist[j] ;
        flist.empty();
        disk_listday_help(disk_base, flist, daylist[j], -1 );
        for( i=0; i<flist.size(); i++ ) {
            disk_oldfilelist.add(flist[i]);
        }
        if( disk_oldfilelist.size()>0 ) {
            disk_oldfilelist.sort();
            break ;
        }
    }

    // fill oldnfilelist
    for( j=0; j<daylist.size(); j++) {
        day = daylist[j] ;
        flist.empty();
        disk_listday_help(disk_base, flist, daylist[j], -1 );
        if( flist.size()<=0 )
            continue ;
        if( (thismonth-day/10000*12-day%10000/100)>6 ) {
            for( i=0; i<flist.size(); i++ ) {
                disk_oldnfilelist.add( flist[i] ) ;
            }
        }
        else {
            for( i=0; i<flist.size(); i++ ) {
                if( strstr( basename(flist[i]), "_N_" ) ) {
                    disk_oldnfilelist.add( flist[i] );
                }
            }
        }
        if( disk_oldnfilelist.size()>0 ) {
            disk_oldnfilelist.sort();
            break;
        }
    }

    return 1 ; // return 1, so deleting may continue
}


// To renew a file list on current disk
// parameter:
//    add :   1, add a new file
//            0, removed a file
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
        base = disk_disklist[disk].basedir ;
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
    int writetest = 1 ;
    //    int total_t, total_l ;

    if( disk_disklist.size()<=0 ) {
        return -1 ;
    }

    for( loop=0; loop<20; loop++ ) {
        // find a disk with available space
        for( i=0; i<disk_disklist.size(); i++ ) {
            if( writetest && !disk_testwritable( disk_disklist[i].basedir ) ) {
                dvr_log( "Disk: %s not writable, removed from disk lists", (char *)disk_disklist[i].basedir );
                disk_error=1;
                return -1 ;
            }
            if( disk_freespace_check(disk_disklist[i].basedir) ) {
                return i ;
            }
        }
        writetest=0 ;

        // ok, none of the disks has space

        // first, let see what kind of file to delete, _L_ or _N_
        if( disk_tlen<=0 && disk_llen<=0 ){        // no video files at all !!!
            dvr_log("No video files found!");
            return -1 ;
        }

        if( disk_llen >= disk_tlen*disk_lockfile_percentage/100 ) //  locked lengh > 30%
        {
            if( disk_deloldfile(1)==0 ) {
                writetest = 1;
            }
        }
        else {
            if( disk_deloldfile(0)==0 ) {
                writetest = 1;
            }
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
    char * basedir = disk_disklist[diskindex].basedir;

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

    if (stat(disk_base, &basestat) != 0) {
        dvr_log("Disk base error, not able to record!");
        return 0;
    }

    if( disk_error || rec_basedir.length()<2 ) {
        if( disk_error ) {
            dvr_log("Disk writing error detect, rescan disks.");
            disk_error=0;
        }
        rec_basedir="";
        // empty disk list
        disk_disklist.empty();
        disk_llen = 0 ;
        disk_tlen = 0 ;
        disk_oldfilelist.empty() ;
        disk_oldnfilelist.empty() ;
    }

    for( i=0; i<disk_disklist.size(); i++ ) {
        disk_disklist[i].mark=0 ;
    }

    dir_find disks(disk_base);
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
            dvr_log("Hard drive removed: %s", basename(disk_disklist[i].basedir) );
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
        if (statfs(rec_basedir, &stfs) == 0) {
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

    if( strcmp(archdir, s_archdir)!=0 ) {
        oldfilelist.empty();
        s_archdir = archdir ;
    }

    if( oldfilelist.size()>0 ) {
        disk_removefile( oldfilelist[0] );
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
        disk_listday_help(disk_arch, flist, day, -1 );
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
int disk_archive_mkfreespace( char * diskdir )
{

    int retry ;

    for(retry=0; retry<100; retry++) {
        if( disk_freespace_check( diskdir ) ) {
            return 1 ;
        }
        else {
            disk_archive_deloldfile(diskdir);
        }
    }
    return 0 ;
}


static char disk_archive_tmpfile[]=".DVRARCH" ;
static int  disk_archive_unlock = 0 ;           // by default not to unlock archived file
static int  disk_archive_bufsize = (4*1024*1024) ;

// return 1: success, 0: fail, -1: to quit
static int disk_archive_copyfile( char * srcfile, char * destfile, int * arc )
{
    FILE * fsrc ;
    FILE * fdest ;
    char * filebuf ;
    int r ;
    int res ;

    fsrc = fopen( srcfile, "rb" );
    fdest = fopen( destfile, "wb" );
    if( fsrc==NULL || fdest==NULL ) {
        if( fsrc ) fclose( fsrc );
        if( fdest ) fclose( fdest );
        return 0 ;
    }

    res = 0 ;
    while( * arc > 0 ) {
        usleep(10000);
        if( (!rec_busy) && (!disk_busy) && g_cpu_usage < (float)disk_arch_mincpuusage/100.0 ) {
            filebuf=(char *)malloc(disk_archive_bufsize) ;
            if( filebuf ) {
                r=fread( filebuf, 1, disk_archive_bufsize, fsrc );
                if( r>0 ) {
                    fwrite( filebuf, 1, r, fdest ) ;
                    free( filebuf );
                }
                else {
                    res = 1 ;       // success
                    free( filebuf );
                    break ;
                }
            }
            else {
                res=0 ;
                break;
            }
        }
    }

    fclose( fsrc ) ;
    fclose( fdest ) ;
    return res ;
}

// archiving files
static int disk_archive_arch( char * filename, char * srcdir, char * destdir )
{
    struct stat sfst ;
    struct stat fst ;
    char * p ;
    int l1, l ;
    int res=0 ;

    string st_ar_filename ;
    string st_ar_tmpfile ;

    char * arch_filename = st_ar_filename.setbufsize(512) ;
    char * arch_tmpfile = st_ar_tmpfile.setbufsize(512) ;

    if( stat( filename, &sfst )!=0 ) {
        return 0;
    }

    if( disk_archive_mkfreespace( destdir )==0 ) {
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

    // copy file to a temperary file in archivng dir
    sprintf(arch_tmpfile, "%s/%s%s", destdir, disk_archive_tmpfile, g_264ext );
    res = disk_archive_copyfile( filename, arch_tmpfile, &disk_archive_run );
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
            res = 1 ;
            disk_archive_copyfile( arch_tmpfile, arch_filename, &res );
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
    string srcfile, destfile ;
    dir_find dfind ;
    char * df ;

    extern string logfile ;
// copy dvrlog file
    if( logfile.length()>0 ) {
        sprintf(srcfile.setbufsize(256), "%s/_%s_/%s", srcdir, (char *)g_servername, (char *)logfile);
        sprintf(destfile.setbufsize(256), "%s/_%s_/%s", destdir, (char *)g_servername, (char *)logfile);
        disk_archive_copyfile((char *)srcfile, (char *)destfile,  &disk_archive_run ) ;
    }

    // copy smartlog
    sprintf( srcfile.setbufsize(256), "%s/smartlog", srcdir );
    dfind.open( (char *)srcfile );
    while( disk_archive_run > 0 && dfind.find() ) {
        if( dfind.isfile() ) {
            if( strstr( dfind.filename(), "_L." ) )
            {
                sprintf( destfile.setbufsize(256), "%s/smartlog", destdir );
                mkdir( (char *)destfile, 0755 );
                sprintf( destfile.setbufsize(256), "%s/smartlog/%s", destdir, dfind.filename() );
                if( disk_archive_copyfile( dfind.pathname(), (char *)destfile,  &disk_archive_run ) ) {
                    if( disk_archive_unlock ) {
                        df = destfile.setbufsize(256);
                        strcpy( df, dfind.pathname() );
                        l=strlen( df );
                        if( l>6 && df[l-5]== 'L' ) {
                            df[l-5] = 'N' ;
                            rename( dfind.pathname(), df );
                        }
                    }
                }
            }
        }
    }
    return 1 ;
}

static int disk_archive_round(char * srcdir, char * destdir)
{
    int i, j, day ;
    array <int> daylist ;
    array <f264name> flist ;
    FILE * archupdfile ;
    char   archupdfilename[128] ;
    int    upd_date ;
    int    upd = 1 ;        // to update last date
    int    res = 0 ;

    // get last archived file time
    upd_date=0;
    sprintf( archupdfilename, "%s/.archupd", srcdir);
    archupdfile = fopen( archupdfilename, "r" );
    if( archupdfile ) {
        if( fscanf(archupdfile, "%d", &upd_date)<1 )
        {
            upd_date=0 ;
        }
        fclose( archupdfile );
    }

    disk_getdaylist_help(srcdir, daylist, -1);
    daylist.sort();

    for( j=0; disk_archive_run > 0 && j<daylist.size(); j++ ) {
        day = daylist[j] ;
        if( day<upd_date )
            continue ;
        flist.empty();
        disk_listday_help(srcdir, flist, day, -1 );
        flist.sort();
        for( i=0; disk_archive_run > 0 && i<flist.size(); i++ ) {
            int l, lockl;
            char * fname = (char *)flist[i] ;
            lockl = f264locklength(fname);
            if( lockl>0 ) {
                l = f264length(fname);
                if( lockl==l ) {
                    int ch ;
                    ch = f264channel( fname );
                    if( ch<0 || ch>= cap_channels ) {
                        continue ;
                    }
                    if( disk_archive_arch( flist[i], srcdir, destdir ) ) {
                        // successfully archived. update latest archive file date & time
                        res = 1 ;
                        if( upd ) {
                            upd_date=day ;
                        }
                    }
                }
                else {
                    upd = 0 ;       // stop update last archive date, this is unfinished (partial) locked file
                }
            }
        }
    }

    if( res ) {
        archupdfile = fopen( archupdfilename, "w" );
        if( archupdfile ) {
            fprintf( archupdfile, "%d", upd_date) ;
            fclose( archupdfile );
        }
    }

    if( disk_archive_run > 0 ) {
        // copy log files, smartlog files
        disk_archive_cplogfile(srcdir, destdir);
    }
    return res ;
}

int disk_archive_basedisk(string & archbase)
{
    struct stat basestat;
    struct stat diskstat;
    char * diskname;

    if( disk_arch.length() <= 0 ) {
        return 0 ;
    }

    if (stat(disk_arch, &basestat) != 0) {      // no archive root dir
        return 0;
    }

    dir_find disks(disk_arch);
    while( disks.find() ) {
        if( disks.isdir() ) {
            diskname = disks.pathname();
            if( stat(diskname, &diskstat)==0 ) {
                if( diskstat.st_dev != basestat.st_dev) {	// found a mount point
                    if( disk_testwritable( diskname ) ) {
                        archbase = diskname ;
                        if( disk_archdiskfile.length()>0 ) {
                            FILE * f = fopen(disk_archdiskfile, "w");
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

static pthread_t disk_archive_threadid ;
static int       disk_archive_round_wait ;

void * disk_archive_thread(void * )
{
    while( 1 ) {     // run all the time?
        if( disk_archive_run ) {
            disk_archive_round_wait = 30 ;          // wait 30 seconds after this round
            // run one round archiving
            string archbase ;
            if( dio_mode_archive() && rec_basedir.length()>0 && disk_archive_basedisk( archbase ) ) {
                disk_archive_round(rec_basedir, archbase ) ;
            }
            disk_archive_run = 0 ;
        }
        else {
            if ( --disk_archive_round_wait <= 0 ) {
                disk_archive_run = 1 ;
            }
            sleep(1);
        }
    }
    return NULL ;
}

void disk_archive_start()
{
    disk_archive_round_wait=0 ;
    disk_archive_run = 1 ;
    if( disk_archive_threadid == 0 ) {
        if( pthread_create(&disk_archive_threadid, NULL, disk_archive_thread, NULL )!=0 ) {
            disk_archive_threadid = 0 ;
            disk_archive_run = 0 ;
        }
    }
}

void disk_archive_stop()
{
    disk_archive_round_wait = 60 ;      // let it wait in non archiving state for 1 minute
    disk_archive_run = -1 ;
}

int disk_archive_runstate()
{
    return disk_archive_run > 0 ;   // arch is actively run
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

    if (rec_basedir.length()<2 ||
        (! disk_freespace_check( rec_basedir )) ) {
        i = disk_findrecdisk();
        if( i<0 ) {
            rec_break();
            rec_basedir="";
            dvr_log("Can not find recordable disk.");
            goto disk_check_finish ;
        }

        if( strcmp(rec_basedir, disk_disklist[i].basedir)!=0 ) {	// different disk
            if( rec_basedir.length()>0 ) {
                rec_break();
            }
            rec_basedir=disk_disklist[i].basedir ;

            // mark current disk
            FILE * f = fopen(disk_curdiskfile, "w");
            if( f ) {
                fputs(rec_basedir, f);
                fclose(f);
            }

#ifdef PWII_APP
        // To change hostname specified from media disk
        char * mediaidfile ;
        mediaidfile = new char [1024] ;
        sprintf( mediaidfile, "%s/pwid.conf", (char *)rec_basedir );
        config mediaidconfig( mediaidfile, 0 );		// don't merge defconf
        delete mediaidfile ;
        string mediaid ;
        mediaid = mediaidconfig.getvalue("system","id1");
        if( mediaid.length()>0 ) {
        sprintf(g_id1, "%s", (char *)mediaid );
        g_servername = mediaid ;
        sethostname( (char *)g_servername, g_servername.length()+1);
        dvr_log("Setup server name from media disk: %s", (char *)g_servername);
        }
#endif

            dvr_log("Start recording on disk : %s.", basename(rec_basedir)) ;

        }
    }

disk_check_finish:
    if( rec_basedir.length()<1 ) {
        // un-mark current disk
        unlink( disk_curdiskfile );
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
                (char *)(disk_disklist[i].basedir),
                (char *)g_servername,
                logfilename );
        if( stat( logpath, &logstat )==0 ) {
            if( S_ISREG( logstat.st_mode ) ) {      // find existing log file
                disk_disklist[i].basedir
            }
            return i;
        }
    }
    return "" ;
}
*/

void * disk_cleanthread(void *param)
{
    while (disk_clean_run) {
        disk_check();
        usleep(1000000);
    }
    return NULL;
}

void disk_init(config &dvrconfig)
{
    const char * pcfg;
    int l;

    rec_basedir = "";
    disk_base = dvrconfig.getvalue("system", "mountdir");
    if (disk_base.length() == 0) {
        disk_base = VAR_DIR"/disks";
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

    disk_minfreespace_percentage=dvrconfig.getvalueint("system", "mindiskspace_percent");
    if (disk_minfreespace_percentage<1 || disk_minfreespace_percentage>50 ) {
        disk_minfreespace_percentage=5 ;
    }

    disk_curdiskfile = dvrconfig.getvalue("system", "currentdisk");
    if( disk_curdiskfile.length()<2) {
        disk_curdiskfile=VAR_DIR"/dvrcurdisk" ;
    }

    // percentage of minimum locked file can be kept
    disk_lockfile_percentage = dvrconfig.getvalueint("system", "lockfile_percentage");
    if( disk_lockfile_percentage < 1 || disk_lockfile_percentage > 100 ) {
        disk_lockfile_percentage = 30 ;
    }

    disk_filekeepdays = dvrconfig.getvalueint("system", "filekeeydays");
    if(disk_filekeepdays<10 || disk_filekeepdays>2000 ) {
        disk_filekeepdays = 366 ;
    }

    disk_removeprerecordfile=dvrconfig.getvalueint("system", "removeprerecordfile");

    disk_maxdirty=dvrconfig.getvalueint("system", "maxdirty");
    if( disk_maxdirty < 50 || disk_maxdirty > 10000 ) {
        disk_maxdirty = 10000 ;
    }

    // empty disk list
    disk_disklist.empty();
    disk_llen = 0 ;
    disk_tlen = 0 ;
    disk_busy = 0 ;

    disk_archdiskfile = dvrconfig.getvalue("system", "archdisk");
    if( disk_archdiskfile.length()<2) {
        disk_archdiskfile=VAR_DIR"/dvrarchdisk" ;
    }

    disk_archive_unlock = dvrconfig.getvalueint("system", "arch_unlock");

    disk_arch_mincpuusage = dvrconfig.getvalueint("system", "arch_mincpuusage");
    if( disk_arch_mincpuusage<50 ) {
        disk_arch_mincpuusage = 50;
    }

    // to start archiving task
    disk_archive_run = 0 ;
    disk_archive_start();

    disk_clean_run = 1 ;
    //    pthread_create(&disk_cleanthreadid, NULL, disk_cleanthread, NULL);

    dvr_log("Disk initialized.");

}

void disk_uninit()
{
    // to stop disk cleaning thread ;
    disk_clean_run = 0 ;
    // wait cleaning threading to finish ;
//    pthread_join( disk_cleanthreadid, NULL );

    // to stop archiving
    disk_archive_stop();

    if( rec_basedir.length()>0 ) {
        ;
    }
    rec_basedir="";
    disk_base="";
    disk_disklist.empty();
    disk_busy=0 ;

// make these file persistant
    // un-mark current recording disk
//    unlink( disk_curdiskfile );

    // un-mark archieving disk
//    unlink( disk_archdiskfile );

    sync();

    dvr_log("Disk uninitialized.");

}
