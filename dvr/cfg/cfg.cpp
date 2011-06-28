
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <memory.h>
#include <dirent.h>
#include <pthread.h>
#include <termios.h>
#include <stdarg.h>
#include <time.h>

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

char dvrconfigfile[] = CFG_FILE ;

int main( int argc, char * argv[] )
{
    if( argc<4 ) {
        printf("Usage: cfg [-fFILENAME] get|set|list section key [value]\n");
        exit(1);
    }
    int i;
    string filename ;
    string cmd ;
    string section ;
    string key ;
    string value ;
    string str ;
    
    for(i=1; i<10; i++) {
        if( argv[i] && argv[i][0]=='-' ) { // a options
            if( argv[i][1]=='f' || argv[i][1]=='F' ) {
                filename=&argv[i][2] ;
            }
        }
        else {
            break;
        }
    }
    if( filename.length()<=0 ) {
        filename=dvrconfigfile ;
    }
    
    if( argv[i] ) {
        cmd = argv[i] ;
        if( argv[i+1] ) {
            if( strcasecmp(argv[i+1], "nil")!=0 ) {
                section = argv[i+1] ;
            }
            if( argv[i+2] ) {
                key = argv[i+2] ;
                if( key.length()<=0 ) {
                    key="noexistkey";
                }
                if( argv[i+3] ) {
                    if( strcasecmp(argv[i+3], "nil")!=0 ) {
                        value = argv[i+3] ;
                    }
                }
            }
        }
    }

	config dvrconfig(filename);

    if( strcasecmp(cmd, "get")==0 ) {
        str=dvrconfig.getvalue( section, key);
        if( str.length()>0 ) {
            printf("%s", (char *)str);
            return 0;
        }
        else {
            return 2;
        }
    }
    else if(  strcasecmp(cmd, "set")==0 ) {
        dvrconfig.setvalue( section, key, value);
        str=dvrconfig.getvalue( section, key);
        if( str.length()>0 ) {
            printf("%s\n", (char *)str);
        }
        dvrconfig.save();
        return 0 ;
    }
    else if(  strcasecmp(cmd, "list")==0 ) {
        struct config_enum en_section ;
        char * section ;
        en_section.line=0;
        while( (section=dvrconfig.enumsection( &en_section ))!=NULL ){
            struct config_enum en_key ;
            char * key ;
            printf("[%s]\n", section);
            en_key.line=0;
            while( (key=dvrconfig.enumkey( section, &en_key ))!=NULL ){
                char * v = dvrconfig.getvalue( section, key );
                printf("%s = %s\n", key, v ); 
            }
        }
        return 0 ;
    }
    return 3 ;
}
