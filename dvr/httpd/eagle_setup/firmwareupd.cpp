#include "../../cfg.h"

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

#define SFX_TAG (0xed3abd05)

// firware update page
int main()
{
    char * firmwarefilename ;
    FILE * firmwarefile ;
    int firmwareok = 0 ;
    firmwarefilename = getenv( "POST_FILE_firmware_file" );
    if( firmwarefilename==NULL )
        return 0;
    firmwarefile = fopen( firmwarefilename, "r" ) ;
    if( firmwarefile ) {
        // check if it is a firmware file?
        int executesize = -1 ;
        fseek( firmwarefile, -16, SEEK_END );
        if( fscanf( firmwarefile, "%d", &executesize )==1 ) {
            if( executesize<(int)ftell(firmwarefile) && executesize>0 ) {
                struct file_head {
                    uint tag ;
                    uint filesize ;
                    uint filemode ;
                    uint namesize ;
                    uint compsize ;
                } fhd ;
                char filename[256] ;

                fseek( firmwarefile, executesize, SEEK_SET );
                while( fread( &fhd, 1, sizeof( fhd), firmwarefile ) == sizeof( fhd ) ) {
                    if( fhd.tag != SFX_TAG || fhd.namesize<=0 || fhd.namesize>=sizeof(filename) ) {
                        break;
                    }
                    fread( filename, 1, fhd.namesize, firmwarefile);
                    if( S_ISREG(fhd.filemode) ) {
                        filename[fhd.namesize]=0;
                        if( strstr( filename, "firmwareid" ) ) {
                            firmwareok=1 ;
                            break;
                        }
                    }
                    fseek(firmwarefile, fhd.compsize, SEEK_CUR );
                }
            }
        } 
        
        fclose( firmwarefile );
    }

    char updmsgfile[128] ;
    sprintf( updmsgfile, "%s/fwupdmsg", getenv("DOCUMENT_ROOT") ); 
    setenv( "FWUPDMSG", updmsgfile, 1 );
    firmwarefile=fopen(updmsgfile, "w");
    if( firmwarefile ) {
        fprintf(firmwarefile,"\n");
        fclose( firmwarefile );
    }
    
    if( firmwareok ) {
        // install the firmware
        printf( "Start Updating firmware! Please wait......" );
        
        link( firmwarefilename, "firmware_x" );
        if( fork()==0 ) {

            firmwarefile=fopen(updmsgfile, "w");
            if( firmwarefile ) {
                fprintf(firmwarefile,"Prepare new firmware...\n");
                fclose( firmwarefile );
            }
            
            // disable stdin , stdout
            int fd = open("/dev/null", O_RDWR );
            dup2(fd, 0);                 // set dummy stdin stdout, also close old stdin (socket)
            dup2(fd, 1);
            dup2(fd, 2);

            // if dvrsvr running, kill it, (Suspend it)
            FILE * dvrpidfile ;
            pid_t dvrpid ;
            dvrpidfile=fopen("/var/dvr/dvrsvr.pid", "r");
            if( dvrpidfile ) {
                dvrpid=0 ;
                fscanf(dvrpidfile, "%d", &dvrpid);
                fclose( dvrpidfile );
                if( dvrpid>0 ) {
                    kill( dvrpid, SIGUSR1 );
                }
            }

            // update firmware
            chmod( "firmware_x", 0755 );
            system( "mkdir updfw; cd updfw; ../firmware_x; sync; sleep 1; sync; reboot;" );
            exit(0);
        }
    }
    else {
        printf( "Error : Invalid firmware file!" );
    }

    return 0;
}
