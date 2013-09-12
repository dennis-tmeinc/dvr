
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>


int main( int argc, char * argv[])
{
    pid_t pid_fsck ;
    int timeout ;

    if( argc<3 ) {
        printf("Arg error!\n");
        return 0 ;
    }

    timeout = atoi( argv[1] ) ;
    if( timeout<50 ) timeout=100 ;
    if( timeout>1800 ) timeout=1800 ;

    pid_fsck = fork();
    if( pid_fsck==0 ) {
        // child process
        char ** cargv = (char **)argv ;
        cargv+=2 ;
        execvp( cargv[0], cargv ) ;
        exit(0);
    }
    else if( pid_fsck>0 ) {
        int runmod = 1 ;
        int status = 0 ;

        printf("Child :%d\n", pid_fsck);

        // wait for specified timeout
        while( runmod ) {
            sleep(1) ;
            pid_t ret = waitpid(pid_fsck, &status, WNOHANG ) ;
            if( ret == pid_fsck ) {
                if( WIFEXITED(status) ) {
                    printf("child exit %d.\n" , WEXITSTATUS(status)  );
                    return WEXITSTATUS(status) ;
                }
                else {
                    printf("child quit %x.\n" , status  );
                    return 2 ;          // failed, fsck was killed
                }
            }
            if( timeout--<=0 ) {
                if( runmod==1 ) {   // fsck time out, kill -TERM
                    kill( pid_fsck, SIGTERM ) ;

                    printf("sig term\n");


                    timeout=30 ;    // wait 30 for fsck to termainate
                    runmod=2 ;
                }
                else if( runmod==2 ) {  // SIGTERM not working, kill -KILL
                    kill( pid_fsck, SIGKILL ) ;

                    printf("sig kill\n");


                    timeout=30 ;        // wait 30s for fsck tobe KILLED
                    runmod=3 ;
                }
                else {
                    runmod=0 ;          // quit, fsck stuck, can't be killed, just leave it.
                }
            }
        }
    }
    return 1 ;          // error execut fsck, but continue
}

