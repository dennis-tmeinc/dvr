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

char * sfxfile ="sfx" ;
char * sfxlistfile = "sfxlist";
char * outfile ;

int main(int argc, char * argv[])
{
    int executesize ;
    FILE * selfext ;
    FILE * listfile ;
    struct file_head fhd ;
    struct stat filestat ;
    char * buf ;
    int bufsize ;
    char line[256] ;
    char ifilename[256] ;
    char ofilename[256] ;
    int  r ;
    
    if( argc>1 ) {
        if( strcmp(argv[1], "-?")==0 ||
           strcmp(argv[1], "?" )==0 ||
           strcmp(argv[1], "-h" )==0 ||
           strcmp(argv[1], "-help" )==0 ) {
               printf( "Usage : mksfx [sfx] [sfxlist] [outputfile]\n");
               return 1;
           }
    }
    
    if( argc>=2 ) {
        sfxfile=argv[1] ;
    }
    if( argc>=3 ) {
        sfxlistfile=argv[2] ;
    }
    outfile = sfxfile ;
    if( argc>=4 ) {
        outfile = argv[3] ;
    }
    
    buf = NULL ;
    bufsize=0 ;
    selfext = fopen( sfxfile, "r" );
    executesize = 0 ;
    if( selfext ) {
        fseek( selfext, 0, SEEK_END );
        executesize = ftell( selfext );
        fseek( selfext, 0, SEEK_SET );
        
        if( executesize>0 ) {
            buf = malloc( executesize );
            bufsize = fread( buf, 1, executesize, selfext ) ;
            if( bufsize!=executesize ) {
                printf("Error: sfx file read.");
                free( buf );
                fclose( selfext );
                return 1;
            }
        }
        fclose( selfext );
    }
    
    selfext = fopen( outfile, "w" );
    if( selfext == NULL ) {
        printf("Can't create output file!\n");
        return 2;
    }
    
    if( executesize>0 && buf ) {
        fwrite( buf, 1, executesize, selfext );
        free( buf );
    }
    else {
        printf("No sfx file.\n");
        executesize=0;
    }
    
    listfile = fopen( sfxlistfile, "r" );
    if( listfile == NULL ) {
        printf("Can't open sfxlist.\n");
        fclose( selfext );
        return 1;
    }
    
    while( fgets( line, sizeof(line), listfile ) ) {
        r = sscanf(line, "%s %s", ifilename, ofilename ) ;
        if( r < 1 ) {
            continue ;
        }
        else if( r<2 ) {
            if( ifilename[0]=='.' && ifilename[1]=='/' ) {
                strcpy( ofilename, &ifilename[2] );
            }
            else {
                strcpy( ofilename, ifilename );
            }
        }
        
        if( strcmp(ifilename, ".")==0 ) {		// current directory
            continue ;
        }
        
        if( stat( ifilename, &filestat ) != 0 ) {
            printf("File error : %s\n", ifilename);
            continue ;
        }
        
        fhd.tag=SFX_TAG ;
        fhd.filesize = (uint) filestat.st_size ;
        fhd.filemode = (uint) filestat.st_mode ;
        fhd.namesize = strlen( ofilename );
        if(  S_ISDIR(filestat.st_mode) ) {
            fhd.compsize=0 ;
            fwrite( &fhd, 1, sizeof(fhd), selfext );
            fwrite( ofilename, 1, fhd.namesize, selfext );
            printf("dir  : %s\n", ofilename );
        }
        else if( S_ISREG(filestat.st_mode) ) {
            int lzmaenc( unsigned char * lzmabuf, int lzmasize, unsigned char * src, int srcsize );
            FILE * srcfile ;
            char * lzmabuf ;
            int    lzmabufsize ;
            srcfile = fopen( ifilename, "r" );
            if( srcfile ) {
                lzmabufsize = fhd.filesize+16 ;
                buf = (unsigned char *)malloc( lzmabufsize );
                fread( buf, 1, fhd.filesize, srcfile );
                fclose( srcfile );
                lzmabuf = (unsigned char *)malloc( lzmabufsize );
                fhd.compsize = lzmaenc( lzmabuf, lzmabufsize, buf, fhd.filesize );
                if( fhd.compsize >= fhd.filesize || fhd.compsize<=0 ) {
                    // write original file data
                    fhd.compsize = fhd.filesize ;
                    fwrite( &fhd, 1, sizeof(fhd), selfext );	// file header
                    fwrite( ofilename, 1, fhd.namesize, selfext);	// file name
                    fwrite( buf, 1, fhd.compsize, selfext );
                    printf("file : %s\t\tsize: %d->%d\n", ofilename, fhd.filesize, fhd.compsize );
                }
                else {
                    // write compressed data
                    fwrite( &fhd, 1, sizeof(fhd), selfext );	// file header
                    fwrite( ofilename, 1, fhd.namesize, selfext);	// file name
                    fwrite( lzmabuf, 1, fhd.compsize, selfext );
                    printf("file : %s\t\tsize: %d->%d\n", ofilename, fhd.filesize, fhd.compsize );
                }
                free( lzmabuf );
                free( buf );
            }
            else {
                printf( "Error open file : %s\n", ifilename );
                fhd.compsize=0 ;
                fhd.filesize=0 ;
                fwrite( &fhd, 1, sizeof(fhd), selfext );
                fwrite( ofilename, 1, fhd.namesize, selfext );
            }
        }
    }
    fprintf( selfext, "\n%16d", (int)executesize );
    fclose( listfile );
    fclose( selfext );
    printf("Done!\n");
    return 0;
}
