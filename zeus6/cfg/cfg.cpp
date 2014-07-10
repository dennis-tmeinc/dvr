
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

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;

int main( int argc, char * argv[] )
{
    if( argc<4 ) {
        printf("Usage: cfg [-fFILENAME] get|set section key [value]\n");
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

	config dvrconfig(filename.getstring());
    if( strcasecmp(cmd.getstring(), "get")==0 ) {
        str=dvrconfig.getvalue( section.getstring(), key.getstring());
        if( str.length()>0 ) {
            printf("%s\n", str.getstring());
            return 0;
        }
        else {
            return 2;
        }
    }
    else if(  strcasecmp(cmd.getstring(), "set")==0 ) {
        dvrconfig.setvalue( section.getstring(), key.getstring(), value.getstring());
        str=dvrconfig.getvalue( section.getstring(), key.getstring());
        if( str.length()>0 ) {
            printf("%s\n", str.getstring());
        }
        dvrconfig.save();
        return 0 ;
    }
    return 3 ;
}
