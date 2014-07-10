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
                   // printf("name:%s type=%d\n",m_pent->d_name,m_pent->d_type);
                    if( m_pent->d_type == DT_UNKNOWN ) {			// d_type not available
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
        char * pathname() {
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
        int    isfile() {
            if( m_pent ) {
                return (m_pent->d_type == DT_REG) ;
            }
            else {
                return 0;
            }
        }

        // return file stat
        struct stat * filestat() {
            char * path = pathname();
            if( path ) {
                if( stat( path, &m_statbuf )==0 ) {
                    return &m_statbuf ;
                }
            }
            return NULL ;
        }
};	


char * mountcmd ;


void tdev_mount(char * path, int level )
{
    dir_find dfind(path);
    pid_t childid ;
    int devfound=0 ;
    char dirpath[256];
 //   printf("path:%s\n",path);
    while( dfind.find() ) {
 	//printf("level:%d filename:%s\n",level,dfind.filename());     
        if( dfind.isdir() ) {
	  //  printf("this is dir\n");
	    if(level>0){
		if( strncmp(dfind.filename(), "sd", 2)==0 ) {
		  
		    sprintf(dirpath,"%s",dfind.pathname());
		    //printf("path:%s\n",dirpath);
		    dir_find dfind1(dirpath);
		    //printf("dfind1\n");
		    while(dfind1.find()){
		    //   printf("filename:%s\n",dfind1.filename()); 
		      if(dfind1.isdir()){
			    if( level>0 && strncmp(dfind1.filename(), "sd", 2)==0 ){
				tdev_mount( dfind1.pathname(), level-1 ) ;
			    }
		      }
		    }	      
		  // tdev_mount( dfind.pathname(), level-1 ) ;
		} else if(strncmp(dfind.filename(), "mmcblk", 6)==0){
		  
		    sprintf(dirpath,"%s",dfind.pathname());
		    //printf("path:%s\n",dirpath);
		    dir_find dfind1(dirpath);
		    //printf("dfind1\n");
		    while(dfind1.find()){
		    //   printf("filename:%s\n",dfind1.filename()); 
		      if(dfind1.isdir()){
			    if( level>0 && strncmp(dfind1.filename(), "mmcblk", 6)==0 ){
				tdev_mount( dfind1.pathname(), level-1 ) ;
			    }
		      }
		    }	   		  		  		  
		}
	    }
        }
        else if( dfind.isfile() && strcmp( dfind.filename(), "dev" )==0 ) {
            devfound=1 ;
            break;
        } 
    }

    if( devfound ) {
        printf("add path:%s\n",path+4);
        if( (childid=fork())==0 ) {
            execl(mountcmd, mountcmd, "add", path+4, NULL );	// will not return
            exit(1);
        }
        else {
            waitpid(childid, NULL, 0);
        }
    }
}


int main(int argc, char *argv[])
{
    if( argc>1 ) {
        mountcmd = argv[1] ;
    }
    else {
        mountcmd = "tdevmount" ;
    }
    tdev_mount("/sys/block", 2 );
    return 0 ;
}


