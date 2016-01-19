#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <dirent.h>

class dir_find {
    protected:
        DIR * m_pdir ;
        char m_pathname[PATH_MAX] ;
        int  m_ldir ;
        unsigned char  d_type;

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
            m_pdir = opendir( path );
            if( m_pdir != NULL ) {
				strcpy( m_pathname, path );
				m_ldir=strlen( m_pathname ) ;
				if( m_pathname[m_ldir-1]!='/' ) {
					m_pathname[m_ldir++]='/' ;
				}
			}
        }
        dir_find( const char * path ) {
            m_pdir=NULL;
            open( path );
        }
        ~dir_find() {
            close();
        }
        int isopen(){
            return m_pdir!=NULL ;
        }
        // find directory.
        // return 1: success
        //        0: end of file. (or error)
        int find() {
            if( m_pdir ) {
                struct dirent * ent ;

                while( (ent = readdir(m_pdir))!=NULL ) {
                    if( ent->d_name[0]=='.' ) {				// ignor all hidden files
                         continue ;
                    }
					strcpy( m_pathname+m_ldir, ent->d_name );
					d_type = ent->d_type ;
                    
                    if( d_type == DT_UNKNOWN ) {			// d_type not available
						struct stat st ;

                        if( lstat( m_pathname, &st )==0 ) {
							if( S_ISREG(st.st_mode) ) {
                                d_type = DT_REG ;
                            }
                            else if( S_ISDIR(st.st_mode) ) {
                                d_type = DT_DIR ;
                            }
                            else if( S_ISLNK(st.st_mode) ) {
                                d_type = DT_LNK ;
                            }
                        }
                    }
                    return 1 ;
                }
            }
            return 0 ;
        }
        
        char * pathname() {
			return m_pathname ;
        }
        char * filename() {
			return m_pathname+m_ldir ;
        }

        // check if found a dir
        int    isdir() {
            return (d_type == DT_DIR) ;
        }

        // check if found a regular file
        int    isfile() {
            return (d_type == DT_REG) ;
        }

};	

char * mountcmd ;

int tdev_mount(const char * path)
{
    dir_find dfind(path);
    pid_t childid ;
    int devfound=0 ;
    int mounted=0 ;		// partition mounte
    
    while( dfind.find() ) {
        if( dfind.isdir() ) {
			if( strncmp(dfind.filename(), "sd", 2)==0 || strncmp(dfind.filename(), "mmc", 3)==0 ) {
				if( tdev_mount( dfind.pathname() ) ) {
					mounted = 1 ;
				}
			}
        }
        else if( dfind.isfile() && strcmp( dfind.filename(), "dev" )==0 ) {
            devfound=1 ;
        }
    }

    if( devfound && mounted == 0 ) {
		char cmdbuf[1024] ;
		sprintf( cmdbuf, "%s add %s", mountcmd, path+4 ) ;
		system( cmdbuf ) ;
        return 1 ;
    }
    return mounted ;
    
}

int main(int argc, char *argv[])
{
    if( argc>1 ) {
        mountcmd = argv[1] ;
    }
    else {
        mountcmd = (char *)"tdevhotplug" ;
    }
    tdev_mount("/sys/block");
    return 0 ;
}


