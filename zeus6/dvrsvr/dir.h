
#ifndef __DIR_H__
#define __DIR_H__

#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


class dir {
protected:
    DIR * m_pdir ;
    char * m_path ;
    int  m_dirlen ;
    unsigned char m_type ;

public:

    // close dir handle
    void close() {
        if( m_pdir ) {
            closedir( m_pdir );
            m_pdir=NULL ;
        }
        if( m_path ) {
			delete m_path ;
			m_path = NULL ;
		}
    }
    
    // open an dir for reading
    void open( const char * path ) {
        close();

        m_path = new char [1024] ;
        strcpy( m_path, path );
        
        m_dirlen=strlen( m_path ) ;
        
        if( m_dirlen>0 ) {
            if( m_path[m_dirlen-1]!='/' ) {
                m_path[m_dirlen]='/' ;
                m_dirlen++;
                m_path[m_dirlen]='\0';
            }
        }
        m_pdir = opendir(m_path);
    }
    
    dir() {
        m_pdir = NULL ;
        m_path = NULL ;
    }
    
    dir( const char * path ) {
        m_pdir = NULL ;
        m_path = NULL ;		
        open( path );
    }

    ~dir() {
        close();
    }
    
    int isopen(){
        return m_pdir!=NULL ;
    }
    
    // find directory.
    // return 1: success
    //        0: end of file. (or error)
    int find(const char * pattern=NULL) {
        if( m_pdir ) {
            struct dirent * ent ;
            while( (ent=readdir(m_pdir))!=NULL  ) {
                // skip . and .. directory and any hidden files
                if( ent->d_name[0]=='.' )
                    continue ;
                if( pattern && fnmatch(pattern, ent->d_name, 0 )!=0 ) {
                    continue ;
                }
                strcpy( m_path+m_dirlen, ent->d_name );
                m_type = ent->d_type ;
                return 1 ;
            }
        }
        return 0 ;
    }
    
    void rewind()
    {
		if( m_pdir ) {
			rewinddir(m_pdir);
		}
	}

    char * pathname()  {
        return m_path ;
    }

    char * filename() {
        return m_path+m_dirlen ;
    }

    // check if found a dir
    int    isdir() {
        if( m_type == DT_DIR ) {
			return 1 ;
		}
		else if( m_type == DT_UNKNOWN ) {
			struct stat st ;
			if( stat( m_path, &st )==0 ) {
				return S_ISDIR(st.st_mode) ;
			}
		}
        return 0 ;
    }

    // check if found a regular file
    int    isfile(){
        if( m_type == DT_REG ) {
			return 1 ;
		}
		else if( m_type == DT_UNKNOWN ) {
			struct stat st ;
			if( stat( m_path, &st )==0 ) {
				return S_ISREG(st.st_mode) ;
			}
		}
		return 0;
    }

    // check if found a device file
    int    isdev(){
        if( m_type == DT_CHR || m_type == DT_BLK ) {
			return 1 ;
		}
		else if( m_type == DT_UNKNOWN ) {
			struct stat st ;
			if( stat( m_path, &st )==0 ) {
				return S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)  ;
			}
		}
        return 0;
    }
};


#endif		// __DIR_H__
