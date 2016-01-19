
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <string.h>

#ifndef __DIR_H__
#define __DIR_H__

class dir_find {
protected:
    DIR * m_pdir ;
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

class dir : public dir_find {
};

#endif		// __DIR_H__
