#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef uint
#define uint unsigned int
#endif

const uint extract_tag = 0xed3abd05 ;

#define MAX_FILENAME	(1024)

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
char * cmptmpfile = "/tmp/sfxtmpfile_eagle" ;

int main(int argc, char * argv[])
{
    int executesize ;
    FILE * selfext ;
    FILE * listfile ;
    FILE * compressedfile ;
    struct file_head fhd ;
    struct stat filestat ;
    char * buf ;
    int bufsize ;
    char line[MAX_FILENAME] ;
    char ifilename[MAX_FILENAME] ;
    char ofilename[MAX_FILENAME] ;
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
        return 1;
    }
    
    while( fgets( line, sizeof(line), listfile ) ) {
        r = sscanf(line, "%s %s", ifilename, ofilename ) ;
        if( r < 1 ) {
            continue ;
        }
        else if( r<2 ) {
            strcpy( ofilename, ifilename );
        }
        
        if( strcmp(ifilename, ".")==0 ) {		// current directory
            continue ;
        }
        
        if( stat( ifilename, &filestat ) != 0 ) {
            printf("File error : %s\n", ifilename);
            continue ;
        }
        
        fhd.tag=extract_tag ;
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
#if 0
            sprintf( line, "lzma -z -c -k -f %s > %s ", ifilename, cmptmpfile );
            system(line);
            compressedfile = fopen(cmptmpfile, "r" );
            if( compressedfile ) {
                fseek( compressedfile, 0, SEEK_END );
                fhd.compsize = (uint) ftell( compressedfile );
                printf("compsize=%d fhdsize=%d\n",fhd.compsize,fhd.filesize);

                if( fhd.compsize>=fhd.filesize || fhd.compsize<=0 ) {
                    printf("closed\n");
                    fclose( compressedfile );
                    fhd.compsize = fhd.filesize ;
                    compressedfile = fopen( ifilename, "r" );
                }
                
                buf = (char *)malloc( fhd.compsize );
                fseek( compressedfile, 0, SEEK_SET );
                if( fread( buf, 1, fhd.compsize, compressedfile) == fhd.compsize ) {
                    fwrite( &fhd, 1, sizeof(fhd), selfext );	// file header
                    fwrite( ofilename, 1, fhd.namesize, selfext);	// file name
                    fwrite( buf, 1, fhd.compsize, selfext );
                    printf("file : %s\t\tsize: %d->%d\n", ofilename, fhd.filesize, fhd.compsize );
                }
                free(buf);
                fclose( compressedfile );
            }
            unlink(cmptmpfile);
#else
            fhd.compsize = fhd.filesize ;
            compressedfile = fopen( ifilename, "r" );
            buf = (char *)malloc( fhd.compsize );
            fseek( compressedfile, 0, SEEK_SET );
            if( fread( buf, 1, fhd.compsize, compressedfile) == fhd.compsize ) {
                    fwrite( &fhd, 1, sizeof(fhd), selfext );	// file header
                    fwrite( ofilename, 1, fhd.namesize, selfext);	// file name
                    fwrite( buf, 1, fhd.compsize, selfext );
                    printf("file : %s\t\tsize: %d->%d\n", ofilename, fhd.filesize, fhd.compsize );
            }
            free(buf);
            fclose( compressedfile );
#endif
        }
    }
    fprintf( selfext, "\n%16d\n", (int)executesize );
    fclose( listfile );
    fclose( selfext );
    printf("Done!\n");
    return 0;
}
