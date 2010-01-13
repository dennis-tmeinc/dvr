#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef uint
#define uint unsigned int
#endif

#define SFX_TAG (0xed3abd05)

struct file_head {
    uint tag ;
    uint filesize ;
    uint filemode ;
    uint namesize ;
    uint compsize ;
} ;

extern int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int lzmaoutsize );
extern int lzmadecsize( unsigned char * lzmabuf );

int extract( char * sfxfile )
{
    int executesize ;
    struct file_head fhd ;

    FILE * fp ;
    FILE * fp_file ;
    char * bufcomp ;
    char * buffile ;
    char filename[256] ;
    
    fp = fopen( sfxfile, "r" );
    if( fp==NULL ) {
        printf("Can't open sfx.\n");
        return 1;
    }
    
    fseek( fp, -16, SEEK_END );
    if( fscanf( fp, "%d", &executesize )<1 ) {
        printf("Error sfx format.\n" );
        return 1;
    } ;
    
    fseek( fp, executesize, SEEK_SET );
    while( fread( &fhd, 1, sizeof( fhd), fp ) == sizeof( fhd ) ) {
        if( fhd.tag != SFX_TAG || fhd.namesize<=0 || fhd.namesize>=sizeof(filename) ) {
            break;
        }
        fread( filename, 1, fhd.namesize, fp);
        filename[fhd.namesize]=0;
        if(  S_ISDIR(fhd.filemode) && fhd.compsize==0 ) {
            mkdir( filename, 0777 );
            printf("dir:%s\n", filename);
        }
        else if( S_ISREG(fhd.filemode) ) {
            fp_file = fopen( filename, "w" );
            if( fp_file == NULL ) {
                printf("ERROR:%s\n", filename);
                continue ;
            }
            if( fhd.compsize>0 ) {
                bufcomp = malloc( fhd.compsize ) ;
                fread( bufcomp, 1, fhd.compsize, fp );
                if( fhd.compsize<fhd.filesize ) {
                    buffile = malloc( fhd.filesize ) ;
                    if( lzmadec( bufcomp, fhd.compsize, buffile, fhd.filesize )==fhd.filesize ) {
                        fwrite( buffile, 1, fhd.filesize, fp_file );
                    }
                    free( buffile );
                }
                else {
                    fwrite( bufcomp, 1, fhd.compsize, fp_file );
                }
                free( bufcomp );
            }
            fclose( fp_file ) ;
            chmod( filename, fhd.filemode&0777 );
            printf("file:%s\n", filename );
        }
    }
    
    fclose( fp );
    printf("Finish.\n");
    return 0;
}
