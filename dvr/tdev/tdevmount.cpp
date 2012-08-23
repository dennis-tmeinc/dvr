#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include "../dvrsvr/dir.h"

char * mountcmd ;

void tdev_mount(char * path, int level )
{
    dir_find dfind(path);
    pid_t childid ;
    int devfound=0 ;
    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if( level>0 && strncmp(dfind.filename(), "sd", 2)==0 ) {
                tdev_mount( dfind.pathname(), level-1 ) ;
            }
        }
        else if( dfind.isfile() && strcmp( dfind.filename(), "dev" )==0 ) {
            devfound=1 ;
            break;
        }
    }

    if( devfound ) {
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

    while( wait(NULL) > 0 ) ;

    return 0 ;
}


