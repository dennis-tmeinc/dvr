#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#include "../../cfg.h"

int main()
{
    char * mcu_firmwarefilename ;
    pid_t  mcu_upd_pid ;
    int res = 0 ;
    
    mcu_firmwarefilename = getenv( "POST_FILE_mcu_firmware_file" );
    if( mcu_firmwarefilename ) {
        FILE * mcufirmfile = fopen( mcu_firmwarefilename, "r" ) ;
        if( mcufirmfile ) {
            if( fgetc(mcufirmfile)==':' ) {
                res=1 ;
            }
            fclose( mcufirmfile );
        }
    }

    char mcumsgfile[128] ;
    FILE * fmsg ;
    // msg file
    sprintf( mcumsgfile, "%s/mcumsg", getenv("DOCUMENT_ROOT") ); 
    setenv( "MCUMSG", mcumsgfile, 1 );
    fmsg=fopen(mcumsgfile, "w");
    if( fmsg ) {
        fprintf(fmsg,"\n");
        fclose( fmsg );
    }
    // progress file
    sprintf( mcumsgfile, "%s/mcuprog", getenv("DOCUMENT_ROOT") ); 
    setenv( "MCUPROG", mcumsgfile, 1 );
    fmsg=fopen(mcumsgfile, "w");
    if( fmsg ) {
        fprintf(fmsg,"0");
        fclose( fmsg );
    }

    if( res==0 ) {
        printf( "Invalid firmware file!" );
        return 0;
    }

    // make a hard link of firmware file, so it wont be deleted by http process
    sprintf( mcumsgfile, "%s/mcufw", getenv("DOCUMENT_ROOT") ); 
    link( mcu_firmwarefilename, mcumsgfile );

    // install MCU firmware
    mcu_upd_pid = fork() ;
    if( mcu_upd_pid == 0 ) {
        // disable stdin , stdout
        int fd = open("/dev/null", O_RDWR );
        dup2(fd, 0);                 // set dummy stdin stdout, also close old stdin (socket)
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        // if dvrsvr running, suspend it
        FILE * dvrpidfile ;
        pid_t dvrpid ;
        dvrpidfile=fopen(VAR_DIR"/dvrsvr.pid", "r");
        if( dvrpidfile ) {
            dvrpid=0 ;
            fscanf(dvrpidfile, "%d", &dvrpid);
            fclose( dvrpidfile );
            if( dvrpid>0 ) {
                kill( dvrpid, SIGUSR1 );
            }
        }
        // updating MCU firmware
        execlp( APP_DIR"/ioprocess", "ioprocess", "-fw", mcumsgfile, NULL );
        exit(2);    // error exec
    }

/*    
    if( res ) {
    printf( "MCU firmware update succeed. <br /> Please un-plug FORCEON cable. <br />System reboot... " );
    }
    else {
        printf( "MCU firmware update failed. Reset system!" );
    }
    
    if( fork() == 0 ) {
        // disable stdin , stdout
        int fd = open("/dev/null", O_RDWR );
        dup2(fd, 0);                 // set dummy stdin stdout, also close old stdin (socket)
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        // reboot system
        execlp( APP_DIR"/ioprocess", "ioprocess", "-reboot", "5", NULL );
        return 2;
    }
*/
    return 0;
}
