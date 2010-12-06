#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "../ioprocess/diomap.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"


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

struct dio_mmap * p_dio_mmap ;
char dvriomap[128] = "/var/dvr/dvriomap" ;
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;


// verify if destination folder available, or create them
int lf_createfolder( char * destfile )
{
    int res=0 ;
    char * r  ;
    dir_find df ;
    r = strrchr(destfile, '/');
    if( r ) {
        *r = 0 ;
        df.open( destfile ) ;
        if( df.isopen() ) {
            res=1;
        }
        else {
            if( mkdir(destfile, 0755)==0 ) {    // success
                res=1;
            }
            else {
                if( lf_createfolder( destfile ) ) {
                    mkdir(destfile, 0755) ;
                    res=1;
                }
            }
        }
        *r='/' ;
    }
    return res ;
}

#define LF_BUFSIZE  (0x4000)

// return 1: success, 0: fail
int lf_copyfile( char * srcfile, char * destfile )
{
    FILE * fsrc ;
    FILE * fdest ;
    char * filebuf ;
    int r ;

    fdest = fopen( destfile, "wb" );
    if( fdest == NULL ) {       
        // try create the folders
        lf_createfolder(destfile);
        fdest = fopen( destfile, "wb" );
    }
    fsrc = fopen( srcfile, "rb" );
    if( fsrc==NULL || fdest==NULL ) {
        if( fsrc ) fclose( fsrc );
        if( fdest ) fclose( fdest );
        return 0 ;
    }
    
    filebuf=(char *)malloc( LF_BUFSIZE ) ;
    if( filebuf==NULL ) {
        fclose( fsrc );
        fclose( fdest );
        return 0 ;
    }        

    while( (r=fread( filebuf, 1, LF_BUFSIZE, fsrc ))>0 ) {
        fwrite( filebuf, 1, r, fdest ) ;
    }
    free( filebuf );
    fclose( fsrc );
    fclose( fdest ) ;
    return 1 ;
}

void lf_copy( char * sdir, char *ddir )
{
    char * l ;
    char dfile[PATH_MAX] ;
    dir_find df ;
    df.open( sdir ) ;
    while( df.find() ) {
        if( df.isdir() ) {
            sprintf( dfile, "%s/%s", ddir, df.filename() );
            lf_copy( df.pathname(), dfile );
        }
        else {
            if( (l=strstr( df.filename(), "_L_" ))==0 ) 
            {
                sprintf( dfile, "%s/%s", ddir, df.filename() );
                lf_copyfile( df.pathname(), dfile );
                // rename _L to _N ;
                strcpy( dfile, df.pathname() );
                l[1]='N' ;
                rename( dfile, df.pathname() );
            }
            else if( (l=strstr( df.filename(), "_L.0" ))==0 ) 
            {
                sprintf( dfile, "%s/%s", ddir, df.filename() );
                lf_copyfile( df.pathname(), dfile );
                // rename _L to _N ;
                strcpy( dfile, df.pathname() );
                l[1]='N' ;
                rename( dfile, df.pathname() );
            }
        }
    }
}

// return 
//        0 : failed
//        1 : success
int appinit()
{
    int fd ;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    v = dvrconfig.getvalue( "system", "iomapfile");
    char * iomapfile = v.getstring();
    if( iomapfile && strlen(iomapfile)>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    p_dio_mmap=NULL ;
    fd = open(dvriomap, O_RDWR );
    if( fd<=0 ) {
        printf("Can't open io map file!\n");
        return 0 ;
    }
    p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );								// fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    return 1 ;
}

// app finish, clean up
void appfinish()
{
    // clean up shared memory
    if( p_dio_mmap ) {
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}

int main(int argc, char * argv[])
{
   
	if( argc<3 ) {
		printf("Usage: lfcp <src_dir> <dest_dir>\n");
		return 1 ;
	}
	
    if( appinit()==0 ) {
        return 1;
    }

    dio_lock();
    strcpy( p_dio_mmap->iomsg, "Copying Video Files" );
    p_dio_mmap->usage++ ;
    p_dio_mmap->iomode = IOMODE_SUSPEND ;
    dio_unlock();

    sleep(2);
    
    // do copying
    lf_copy(argv[1], argv[2]);

    dio_lock();
    p_dio_mmap->iomode = IOMODE_RUN  ;
    p_dio_mmap->iomsg[0]=0 ;
    p_dio_mmap->usage-- ;
    dio_unlock();

	appfinish();
    return 0;
}

