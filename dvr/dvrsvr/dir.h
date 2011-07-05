
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <string.h>

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
                    // skip . and .. directory and any hidden files
                    if( m_pent->d_name[0]=='.' ) 
                        continue ;
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
                    // skip . and .. directory and any hidden files
					if( m_pent->d_name[0]=='.' ) 
						continue ;
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

		// check if found a regular file
        int    isdev(){
            if(m_pdir && m_pent) {
                return (m_pent->d_type == DT_REG) ;
            }
            else {
                return 0;
            }
        }
};
