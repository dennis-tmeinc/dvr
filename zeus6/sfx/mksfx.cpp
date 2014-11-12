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

struct sfx_head {
    uint tag ;
    uint filesize ;
    uint filemode ;
    uint namesize ;
    uint compsize ;
} ;

const char * sfxfile ="sfx" ;
const char * sfxlistfile = "sfxlist";

char outfile[512] ;
char line[1024] ;
char ifilename[512] ;
char ofilename[512] ;
    
int main(int argc, char * argv[])
{
    int executesize ;
    FILE * selfext ;
    FILE * listfile ;
    struct sfx_head fhd ;
    struct stat filestat ;
    unsigned char * buf ;
    int bufsize ;
    int  r ;

    if( argc>1 ) {
        if( strcmp(argv[1], "-?")==0 ||
           strcmp(argv[1], "?" )==0 ||
           strcmp(argv[1], "-h" )==0 ||
           strcmp(argv[1], "-help" )==0 ) {
               printf( "Usage : mksfx [sfx] [sfxlistfile] [outputfile]\n");
               return 1;
           }
    }

    if( argc>=2 ) {
        sfxfile=argv[1] ;
    }
    if( argc>=3 ) {
        sfxlistfile=argv[2] ;
    }
    if( argc>=4 ) {
        strncpy( outfile, argv[3], sizeof(outfile));
    }
    else {
        strncpy( outfile, sfxfile, sizeof(outfile));
    }

    buf = NULL ;
    bufsize=0 ;
    selfext = fopen( sfxfile, "rb" );
    executesize = 0 ;
    if( selfext ) {
        fseek( selfext, 0, SEEK_END );
        executesize = ftell( selfext );
        fseek( selfext, 0, SEEK_SET );

        if( executesize>0 ) {
            buf = (unsigned char *)malloc( executesize );
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

    selfext = fopen( outfile, "wb" );
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

        if( ifilename[0]=='#' || ifilename[0]==';' || strcmp(ifilename, ".")==0 ) {		// comment, or . (current directory)
            continue ;
        }

        if( lstat( ifilename, &filestat ) != 0 ) {
            printf("File error : %s\n", ifilename);
            continue ;
        }

        fhd.tag=SFX_TAG ;
        fhd.filemode = (uint) filestat.st_mode ;
        fhd.filesize = (uint) filestat.st_size ;
        fhd.namesize = strlen( ofilename );
        
        if(  S_ISDIR(filestat.st_mode) ) {
            fhd.compsize=0 ;
            fhd.filesize=0 ;
            fwrite( &fhd, 1, sizeof(fhd), selfext );
            fwrite( ofilename, 1, fhd.namesize, selfext );
            printf("dir  : %s\n", ofilename );
        }
        else if( S_ISREG(filestat.st_mode) ) {
            int lzmaenc( unsigned char * lzmabuf, int lzmasize, unsigned char * src, int srcsize );
            FILE * srcfile ;
            unsigned char * lzmabuf ;
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
        }
        else if( S_ISLNK(filestat.st_mode) ) {
        		fhd.filesize = readlink( ifilename, line, sizeof(line)-1);
        		line[fhd.filesize]=0;
            fhd.compsize = fhd.filesize ;
            fwrite( &fhd, 1, sizeof(fhd), selfext );	// file header
            fwrite( ofilename, 1, fhd.namesize, selfext);	// file name
            fwrite( line, 1, fhd.compsize, selfext );
            printf("symbolic link : %s -> %s\n", ofilename, line );
        }
        else {
            printf("Unknown file mode %x - file : %s\n",filestat.st_mode, ifilename );
        }
    }
    fprintf( selfext, "\n%16d", (int)executesize );
    fclose( listfile );
    fclose( selfext );
    printf("Done!\n");
    return 0;
}
