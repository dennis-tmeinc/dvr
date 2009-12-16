#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef uint
#define uint unsigned int
#endif

#define MAX_FILENAME	(1024)

struct file_head {
    uint tag ;
    uint filesize ;
    uint filemode ;
    uint namesize ;
    uint compsize ;
} ;

const uint extract_tag = 0xed3abd05 ;

char   defaultrun[] = "sfxrun" ;

char * runscript = NULL ;

extern int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int lzmaoutsize );
extern int lzmadecsize( unsigned char * lzmabuf );

int main(int argc, char * argv[])
{
    int executesize ;
    struct file_head fhd ;
    char filename[MAX_FILENAME] ;
    FILE * fp ;
    FILE * fp_file ;
    char * bufcomp ;
    char * buffile ;
    
    if( argc>1 ) {
        runscript=argv[1] ;
    }
    
    fp = fopen( "/proc/self/exe", "r" );
    if( fp==NULL ) {
        printf("Can't open exe file\n");
        return 1;
    }
    
    fseek( fp, -16, SEEK_END );
    if( fscanf( fp, "%d", &executesize )<1 ) {
        printf("Error file format.\n" );
        return 1;
    } ;
    if( executesize<5000 || executesize>(int)ftell(fp) ) {
        printf("Error file format.\n" );
        return 1;
    }
    
    fseek( fp, executesize, SEEK_SET );
    while( fread( &fhd, 1, sizeof( fhd), fp ) == sizeof( fhd ) ) {
        if( fhd.tag !=extract_tag ) {
            break;
        }
        if( fhd.namesize<=0 || fhd.namesize>=sizeof(filename) ) {
            break;
        }
        fread( filename, 1, fhd.namesize, fp);
        filename[fhd.namesize]=0;
        if(  S_ISDIR(fhd.filemode) && fhd.compsize==0 ) {
            mkdir( filename, 0777 );
            printf("dir  : %s\n", filename);
        }
        else if( S_ISREG(fhd.filemode) ) {
            fp_file = fopen( filename, "w" );
            if( fp_file == NULL ) {
                printf("ERROR: %s\n", filename);
                return 1;
            }
            if( fhd.compsize>0 && fhd.compsize<fhd.filesize ) {
                bufcomp = malloc( fhd.compsize );
                fread( bufcomp, 1, fhd.compsize, fp ) ;
                if( lzmadecsize( bufcomp )==fhd.filesize ) {	//correct size
                    buffile = malloc( fhd.filesize ) ;
                    if( lzmadec( bufcomp, fhd.compsize, buffile, fhd.filesize )>0 ) {
                        fwrite( buffile, 1, fhd.filesize, fp_file );
                    }
                    free( buffile );
                }
                else {
                    printf( "Wrong file size : %s\n", filename );
                }
                free( bufcomp );
            }
            else if( fhd.filesize>0 && fhd.filesize==fhd.compsize) {
                buffile = malloc( fhd.compsize );
                fread( buffile, 1, fhd.compsize, fp ) ;
                fwrite( buffile, 1, fhd.compsize, fp_file );
                free( buffile );
            }
            fclose( fp_file ) ;
            chmod( filename, fhd.filemode );
            printf("file : %s\n", filename );
        }
    }
    
    fclose( fp );
    printf("File end.\n");
    if( runscript ) {
        printf("Executing script.\n");
        execl( runscript, runscript, NULL );
        // may not return here.
    }
    else {
        execl(defaultrun, defaultrun, NULL );
    }
    return 0;
}
