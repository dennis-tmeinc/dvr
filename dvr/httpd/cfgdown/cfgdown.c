
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../cfg.h"

#define CFGTAG (0x5f7da992)
    
char * cfgfiles[] = {    
 "system_value",
 "network_value",
 "sensor_value",
 "camera_value_1",
 "camera_value_2",
 "camera_value_3",
 "camera_value_4",
 "camera_value_5",
 "camera_value_6",
 "camera_value_7",
 "camera_value_8",
 NULL
} ;

struct chunkheader {
    unsigned int tag ;
    int filenamesize ;
    int filesize ;
    int filemode ;
} ;

// output cfg file
void outputcfg()
{
    int i;
    struct chunkheader chd ;
    FILE * fp ;
    char * mp ;
    for( i=0; i<100; i++ ) {
        if( cfgfiles[i]==NULL || strlen(cfgfiles[i])<1 ) {
            break;
        }
        fp = fopen(cfgfiles[i], "r");
        if( fp ) {
            fseek( fp, 0, SEEK_END ) ;
            chd.tag = CFGTAG;
            chd.filenamesize=strlen(cfgfiles[i]) ;
            chd.filesize=ftell(fp) ;
            chd.filemode = 0644 ;                          // not used
            if( chd.filesize>0 ) {
                fwrite( &chd, 1, sizeof(chd), stdout);
                fwrite( cfgfiles[i], 1, chd.filenamesize, stdout);
                mp = malloc( chd.filesize );
                fseek( fp, 0, SEEK_SET );
                fread( mp, 1, chd.filesize, fp );
                fwrite( mp, 1, chd.filesize, stdout );
                free( mp );
            }
            fclose( fp );
        }
    }
}            

// calculate cfg file length
long calcfglength()
{
    long len = 0 ;
    int i;
    FILE * fp ;
    long fsize ;
    for( i=0; i<100; i++ ) {
        if( cfgfiles[i]==NULL || strlen(cfgfiles[i])<1 ) {
            break;
        }
        fp = fopen(cfgfiles[i], "r");
        if( fp ) {
            fseek( fp, 0, SEEK_END ) ;
            fsize = ftell( fp ) ;
            if( fsize>0 ) {
                len+=sizeof(struct chunkheader)+strlen(cfgfiles[i])+fsize ;
            }
            fclose( fp );
        }
    }
    return len ;
}

int main()
{
    // printf headers
    printf( "HTTP/1.1 200 OK\r\n" );
    printf( "Content-Type: application/octet-stream\r\n" );
    printf( "Cache-Control: no-cache\r\n" );        // no cache on cgi contents
    printf( "Content-Disposition: attachment; filename= mdvr.cfg\r\n" );        // this would force IE to save file
    printf( "Content-Length: %ld\r\n", (long) calcfglength() );
    printf( "\r\n" );
    
    outputcfg();
    return 0;
}

