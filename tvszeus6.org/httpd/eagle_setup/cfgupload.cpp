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

#define CFGTAG (0x5f7da992)

int main()
{
    int i ;
    int d ;
    struct chunkheader {
        unsigned int tag ;
        int filenamesize ;
        int filesize ;
        int filemode ;
    } chd ;
    char * cfgfilename ;
    FILE * cfgfile ;
    FILE * fp ;
    char filename[256] ;
    int nvfile = 0;
    cfgfilename = getenv( "POST_FILE_cfgupload_file" );
    if( cfgfilename==NULL )
        goto upcfgreport ;
    cfgfile = fopen( cfgfilename, "r" ) ;
    if( cfgfile ) {
        while( fread(&chd, 1, sizeof(chd), cfgfile )==sizeof(chd) ) {
            if( chd.tag==CFGTAG && chd.filenamesize<255 ) {
                fread( filename, 1, chd.filenamesize, cfgfile );
                filename[chd.filenamesize]=0 ;
                fp = fopen(filename,"w");
                if( fp ) {
                    for( i=0; i<chd.filesize; i++ ) {
                        d=fgetc(cfgfile);
                        if( d==EOF ) break;
                        fputc(d, fp);
                    }
                    fclose( fp );
                    nvfile++;
                }
            }
            else 
                break;
        }
        fclose( cfgfile );
    }

upcfgreport:    
    if( nvfile>0 ) {
        printf("Configuration uploaded!" );
    }
    else {
        printf("Configuration uploading failed!" );
    }
    return 0;
}
