/* diskarchive.cpp
 *     disk archiving support for PWZ5, (some codes are copied from dvr project)
 */

#include "dvr.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

class x_dir_find {
protected:
    DIR * m_pdir ;
    int  m_dirlen ;
    char m_pathname[PATH_MAX] ;

public:
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
    
    x_dir_find() {
        m_pdir=NULL ;
    }
    x_dir_find( const char * path ) {
        m_pdir=NULL;
        open( path );
    }
    ~x_dir_find() {
        close();
    }
    int isopen(){
        return m_pdir!=NULL ;
    }
    // find directory.
    // return 1: success
    //        0: end of file. (or error)
    int find(char * pattern=NULL) {
        if( m_pdir ) {
            struct dirent * dir ;
            while( (dir=readdir(m_pdir))!=NULL  ) {
                // skip . and .. directory and any hidden files
                if( dir->d_name[0]=='.' )
                    continue ;
                if( pattern && fnmatch(pattern, dir->d_name, 0 )!=0 ) {
                    continue ;
                }
                strcpy( &m_pathname[m_dirlen], dir->d_name );
                return 1 ;
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
        struct stat st ;
        if( stat( m_pathname, &st )==0 ) {
            return S_ISDIR(st.st_mode) ;
        }
        return 0 ;
    }

    // check if found a regular file
    int    isfile(){
        struct stat st ;
        if( stat( m_pathname, &st )==0 ) {
            return S_ISREG(st.st_mode) ;
        }
        return 0;
    }

    // check if found a device file
    int    isdev(){
        struct stat st ;
        if( stat( m_pathname, &st )==0 ) {
            return S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)  ;
        }
        return 0;
    }
};


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

#define F264TIME( filename )	(basefilename( filename ) + 5 )
#define F264CHANNEL( filename ) (basefilename( filename ) + 2 )

static int f264time(const char *filename, struct dvrtime *dvrt)
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

static int f264length(const char *filename)
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

// remove all files and directory
static void archive_rmdir(const char *dir, int delete_dvrlog)
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
static int x_disk_rmemptydir(char *dir)
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

static void x_disk_removefile( const char * file264 ) 
{
    string f264 ;
    char * extension ;
    f264 = file264 ;
    if( remove( file264 ) != 0 ) {
    	dvr_log( "Delete file failed: %s", file264 );
    	return ;
    }
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

// find any 264 files
// return 1: found at least one .264 file
//        0: can't find any .264 file
static int x_disk_find264(char *dir)
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
static void x_disk_rmempty264dir(char *dir)
{
    if( disk_find264(dir)==0 ) {
      disk_rmdir(dir, 0);
    }
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

// for PWII_APP

// to do,  change ID on new media

// archiving help file (copy from dvr project)
static void x_disk_getdaylist_help( char * dir, array <int> & daylist, int ch = -1 )
{
    int i;
    int day ;
    int dup ;

    dir_find dfind(dir);

    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if( sscanf(dfind.filename(), "%d", &day) == 1 ) {
                if( day>20100101 && day<20500000 ) {
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
                            char chpath[512] ;
                            sprintf( chpath, "%s/CH%02d", dfind.pathname(), ch );
                            if( disk_find264(chpath) ) {
                                daylist.add(day);
                            }
                            else {
                                disk_rmdir(chpath);
                            }
                        }
                        else {
                            if( disk_find264(dfind.pathname()) ) {
                                daylist.add(day);
                            }
                            else {
                                disk_rmdir(dfind.pathname());
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



// return free disk space in percentage
int x_disk_freespace_percentage(char *filename)
{
    struct statfs stfs;
    
    if (statfs(filename, &stfs) == 0) {
        return stfs.f_bavail * 100 / stfs.f_blocks  ;
    }
    return 100;
}

// delete oldest archive video file,
static int x_disk_archive_deloldfile(char * archdir)
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
static int x_disk_archive_mkfreespace( char * diskdir )
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


static string archive_tmpfile ;
static int  archive_bufsize = (128*1024) ;
static char archive_basedir = "/var/dvr/arch" ;
// 0: quit archiving, 1: archiving
static int archive_run = 0 ;

// return 1: success, 0: fail, -1: to quit
static int archive_copyfile( char * srcfile, char * destfile, int level )
{
    FILE * fsrc ;
    FILE * fdest ;
    int    dest_exist = 0 ;
    char   *filebuf ;
    int r ;
    int res ;

    struct stat sfst ;		// source file stat
    struct stat dfst ;		// destfile stat
    
    if( stat( srcfile, &sfst )!=0 ) {
        return 0;
    }
    
	if( level > 2 ) {		// video files, to check if dest file exist
		if( stat( destfile, &dfst )==0 ) {
			dest_exist = 1 ;
			if( S_ISREG( dfst.st_mode ) && dfst.st_size == sfst.st_size ) {
				// destination file already exist
				return 1 ;
			}
		}
	}
	
	// copy source file to temperary file
    fsrc = fopen( srcfile, "rb" );
    fdest = fopen( archive_tmpfile, "wb" );
    if( fsrc==NULL || fdest==NULL ) {
        if( fsrc ) fclose( fsrc );
        if( fdest ) fclose( fdest );
        return 0 ;
    }

    filebuf=(char *)malloc(disk_archive_bufsize);
    res = 0 ;
    while( archive_run > 0 ) {
        if( (!rec_busy) && (!disk_busy) && g_cpu_usage < (float)disk_arch_mincpuusage/100.0 ) {
            r=fread( filebuf, 1, disk_archive_bufsize, fsrc );
            if( r>0 ) {
                fwrite( filebuf, 1, r, fdest ) ;
            }
            else {
                res = 1 ;       // success
                break ;
            }
        }
        else {
            usleep(1000);
        }
    }
    free(filebuf);
    fclose( fsrc ) ;
    fclose( fdest ) ;
    
    if( res == 1 ) {		// success
		if (dest_exist)
			unlink(destfile);
		rename( archive_tmpfile, destfile );
	}
    
    return res ;
}
			
static int archive_copydir( char * srcdir, char * destdir, int level ) 
{
	string destfile ;
	x_dir_find sdir( srcdir ) ;
	while( archive_run>0  && sdir.find() ) {
		if( sdir.isfile() ) {
			if( strstr( sdir.filename(), "_0_" ) ) {	// current writing file
				continue ;
			}
			string destfile ;
			sprintf( destfile.setbufsize(300), "%s/%s", destdir, sdir.filename() );
			archive_copyfile( sdir.pathname(), destfile, level );
		}
    }
    // scan sub dir
    sdir.open( srcdir );
	while( archive_run>0  && sdir.find() ) {
		if( sdir.isdir() ) {
			sprintf( destfile.setbufsize(300), "%s/%s", destdir, sdir.filename() );
			mkdir( destfile );
			archive_copydir( sdir.pathname(), destfile, level+1 );
		}
    }
    return 1 ;
}


// do one round of archiving
static int archive_round(char * srcdir, char * destdir)
{
	sprintf( archive_tmpfile.setbufsize(300), "%s/.archfile", destdir );
	archive_copydir( srcdir, destdir, 0 ); 
	unlink( archive_tmpfile );
	return 0 ;
}

int archive_basedisk(string & archbase)
{
    struct stat basestat;
    struct stat diskstat;
    char * diskname;

    if (stat(archive_basedir, &basestat) != 0) {      // no archive root dir
        return 0;
    }

    x_dir_find disks(archive_basedir);
    while( disks.find() ) {
        if( disks.isdir() ) {
            diskname = disks.pathname();
            if( stat(diskname, &diskstat)==0 ) {
                if( diskstat.st_dev != basestat.st_dev) {	// found a mount point
					archbase = diskname ;
					return 1 ;
                }
            }
        }
    }
    return 0 ;
}

static pthread_t archive_threadid = 0 ;

void * archive_thread(void * )
{
    nice(5) ;          // run nicely
    int wait = 50 ;		// wait 5 sec between each round

    while( archive_run>0 ) {     // run all the time?
        if( --wait<=0 ) {
            // run one round archiving
            string archbase ;
            if( rec_basedir.length()>0 && archive_basedisk( archbase ) ) {
                archive_round(rec_basedir, archbase ) ;
            }
            wait = 50 ;		// wait 5 sec between each round
        }
        else {
            usleep(100000) ;
        }
    }
    return NULL ;
}

void archive_start()
{
    disk_archive_wait = 0 ;
    archive_run = 1 ;
    if( pthread_create(&archive_threadid, NULL, archive_thread, NULL )!=0 ) {
        archive_threadid = 0 ;
        archive_run = 0 ;
    }
}

void archive_stop()
{
	int i ;
	if( archive_run > 0 ) {
		// wait until archive stopped
		archive_run = -1 ;
		pthread_join(archive_threadid, NULL);
        archive_threadid = 0 ;
	}
}

