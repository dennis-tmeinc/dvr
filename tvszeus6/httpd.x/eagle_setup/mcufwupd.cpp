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
#include "../../ioprocess/diomap.h"
#include <sys/mman.h>
#include <sys/ioctl.h>

void killioprocess()
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );                         // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
	if(p_dio_mmap->iopid>0){
	   kill(p_dio_mmap->iopid, SIGTERM);
	}
	munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    }
    return;
}

void runapp(char *const argv[]) 
{
	int status ;
	pid_t pid ;
	pid=fork();
	if( pid==0 ) {
		// child process
		execv( argv[0], argv );
		exit(1);
	}
	else if( pid>0 ) {
		wait(&status);
	}
}

int main()
{
    char * mcu_firmwarefilename ;
	char * msg ;
    pid_t  mcu_upd_pid ;
    int res = 0 ;
    char mcufirmwarefile[128] ;
    char mcumsgfile[128] ;
    
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

    FILE * fmsg ;
    // progress file
    sprintf( mcumsgfile, "%s/mcuprog", getenv("DOCUMENT_ROOT") ); 
    setenv( "MCUPROG", mcumsgfile, 1 );
    fmsg=fopen(mcumsgfile, "w");
    if( fmsg ) {
        fprintf(fmsg,"0");
        fclose( fmsg );
    }

	// msg file
    sprintf( mcumsgfile, "%s/mcumsg", getenv("DOCUMENT_ROOT") ); 
    setenv( "MCUMSG", mcumsgfile, 1 );
    fmsg=fopen(mcumsgfile, "w");
    if( fmsg ) {
        fprintf(fmsg,"\n");
        fclose( fmsg );
    }

	if( res==0 ) {
        printf( "Invalid firmware file!" );
        return 0;
    }

    // make a hard link of firmware file, so it wont be deleted by http process
    sprintf( mcufirmwarefile, "%s/mcufw", getenv("DOCUMENT_ROOT") ); 
    link( mcu_firmwarefilename, mcufirmwarefile );

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
        dvrpidfile=fopen("/var/dvr/dvrsvr.pid", "r");
        if( dvrpidfile ) {
            dvrpid=0 ;
            fscanf(dvrpidfile, "%d", &dvrpid);
            fclose( dvrpidfile );
            if( dvrpid>0 ) {
                kill( dvrpid, SIGTERM );
            }
        }

		// updating MCU firmware
        fd = open(mcumsgfile, O_RDWR );

	msg = "Start updating MCU firmware, please wait....\n" ;
	write(fd, msg, strlen(msg)) ; 
	msg = "(Try press RESET button when message \"Synchronizing\" appear)\n\n" ;
	write(fd, msg, strlen(msg)) ; 

	killioprocess();
	
	char * zargs[10] ;
	/*
	zargs[0] = "/dav/dvr/ioprocess" ;
	zargs[1] = "-fwreset" ;
	zargs[2] = NULL ;
	runapp(zargs);
	*/
	dup2(fd, 1);
        dup2(fd, 2);
		
	zargs[0] = "/mnt/nand/dvr/lpc21isp" ;
	zargs[1] = "-try1000" ;				// wait for sync, 1000 times
	zargs[2] = "-wipe" ;				// erase flash
	zargs[3] = "-hex" ;					// input .hex file
	zargs[4] = mcufirmwarefile ;		// firmware file
	zargs[5] = "/dev/ttyS3" ;			// MCU connection port
	zargs[6] = "115200" ;				// MCU connection baud
	zargs[7] = "12000" ;				// MCU clock 
	zargs[8] = NULL ;
	runapp(zargs);

	lseek(fd, 0, SEEK_END);
	msg = "\nMCU firmware update finished!\n" ;
	write(fd, msg, strlen(msg)) ; 
	msg = "Please reset unit.\n" ;
	write(fd, msg, strlen(msg)) ; 
	close( fd );		
	exit(2);    // error exec
    }
    return 0;
}
