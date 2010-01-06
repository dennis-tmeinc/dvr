#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef uint
#define uint unsigned int
#endif

#define MAXCHUNKSIZE    (8000000)
#define MINCHUNKSIZE    (1000000)

#define CHUNK_FLAT ('F')
#define CHUNK_LZMA ('Z')
#define CHUNK_SFX  ('S')
#define CHUNK_END  (CHUNK_SFX)

#define FILEMODE_REG    (1)
#define FILEMODE_EXEC   (2)
#define FILEMODE_DIR    (4)
#define FILEMODE_SYMLINK    (8)

char * sfxfile ="sfx" ;
char * sfxlistfile = "sfxlist";
char * outfile ;
char * cmptmpfile = "/tmp/sfxtmpfile_eagle" ;

int main(int argc, char * argv[])
{
    FILE * fsfx ;
    FILE * fout ;
    FILE * flist ;
    int executesize ;
    char * chunk_buf ;
    char   chunk_tag ;
    int    chunk_size ;
    
    struct stat filestat ;
    int fsize ;
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
 
    fout = fopen( outfile, "w" );
    if( fout == NULL ) {
        printf("Can't create output file!\n");
        return 2;
    }

    executesize = 0 ;
    fsfx = fopen( sfxfile, "r" );
    if( fsfx ) {
        fseek( fsfx, 0, SEEK_END );
        executesize = ftell( fsfx );
        fseek( fsfx, 0, SEEK_SET );
        
        if( executesize>0 && executesize<100000 ) {
            buf = malloc( executesize );
            bufsize = fread( buf, 1, executesize, fsfx ) ;
            if( bufsize!=executesize ) {
                printf("Error: read sfx file.");
                fclose( fsfx );
                fclose( fout );
                free( buf );
                return 1;
            }
            fwrite( buf, 1, executesize, fout );
            free( buf );
//            while(executesize & 3 ) {     // make executesize uint aligned
//                fwrite("", 1, 1, fout);
//                executesize++ ;
//            }
        }
        else {
            executesize=0 ;
        }
        fclose( fsfx );
    }
     
    if( executesize == 0 ) {
        printf("Compress with no sfx file.\n");
    }
    
    flist = fopen( sfxlistfile, "r" );
    if( flist == NULL ) {
        printf("Can't open sfxlist.\n");
        return 1;
    }

    chunk_buf = (char *)malloc( MAXCHUNKSIZE );
    buf = (char *)malloc( MAXCHUNKSIZE );
    if( chunk_buf==NULL || buf==NULL ) {
        printf("No enough memory!\n");
        if( chunk_buf ) free( chunk_buf );
        if( buf ) free(buf);
        return 1;
    }
    
    while( 1 ) {
        char * cp = chunk_buf ;
        while( fgets( line, sizeof(line), flist ) ) {
            r = sscanf(line, "%s %s", ifilename, ofilename ) ;
            if( r < 1 ) {
                continue ;
            }
            else if( r<2 ) {
                if( ifilename[0] == '.' && ifilename[1] == '/' ) {
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

            if(  S_ISDIR(filestat.st_mode) ) {
                * cp++ = FILEMODE_DIR ;     // fmode DIR
                strcpy(cp, ofilename) ;
                cp+=strlen( cp )+1;
                printf( "DIR : %s -> %s\n", ifilename, ofilename);
            }
            else if( S_ISREG(filestat.st_mode) ) {
                if( filestat.st_mode & S_IXUSR ) {
                    * cp++ = FILEMODE_REG|FILEMODE_EXEC ; 
                }
                else {
                    * cp++ = FILEMODE_REG ;     

                }
                strcpy(cp, ofilename) ;
                cp+=strlen( cp )+1;
                fsfx = fopen( ifilename, "rb");
                fsize=0 ;
                if( fsfx ) {
                    fsize = fread( cp+4, 1, filestat.st_size, fsfx );
                    fclose( fsfx );
                }
                * cp++ = fsize & 0xff ;
                * cp++ = (fsize>>8) & 0xff ;
                * cp++ = (fsize>>16) & 0xff ;
                * cp++ = (fsize>>24) & 0xff ;
                cp+=fsize ;
                printf( "FILE: %s -> %s size: %d\n", ifilename, ofilename, fsize);
            }
            else if( S_ISLNK(filestat.st_mode) ) {
                * cp++ = FILEMODE_SYMLINK ;     
                strcpy(cp, ofilename) ;
                cp+=strlen( cp )+1;
                fsize = readlink( ifilename, cp+4, 256 )+1;
                * cp++ = fsize & 0xff ;
                * cp++ = (fsize>>8) & 0xff ;
                * cp++ = (fsize>>16) & 0xff ;
                * cp++ = (fsize>>24) & 0xff ;
                cp+=fsize ;
                *(cp-1)=0 ;                                 // make it null terminated
                printf( "LINK: %s -> %s (%s)\n", ifilename, ofilename, cp-fsize);
            }
            else {
                printf("Unsupported file :%s!\n", ifilename );
            }

            if( (cp-chunk_buf) > MINCHUNKSIZE ) {
                break ;
            }
        }

        if( cp!=chunk_buf ) {
            fsize = (cp-chunk_buf) ;
            chunk_size = lzmaenc( buf, MAXCHUNKSIZE, chunk_buf, fsize );
            if( chunk_size <=0 || chunk_size >= fsize ) {
                chunk_tag = CHUNK_FLAT ;
                fwrite( &chunk_tag, 1, 1, fout);
                fwrite( &fsize, 1, 4, fout);
                fwrite( chunk_buf, 1, fsize , fout );
                printf( "Flat : %d\n", fsize );
            }
            else {
                chunk_tag = CHUNK_LZMA ;
                fwrite( &chunk_tag, 1, 1, fout);
                fwrite( &chunk_size, 1, 4, fout);
                fwrite( buf, 1, chunk_size , fout );
                printf( "Lzma : %d -> %d\n", fsize, chunk_size );
            }
        }
        else {
            break ;
        }
    }

    fclose( flist );

    chunk_tag = CHUNK_END ;
    chunk_size = executesize ;
    fwrite( &chunk_tag, 1, 1, fout);
    fwrite( &chunk_size, 1, 4, fout);

    printf("SFX size: %d\n", (int)ftell( fout ));
    
    fclose( fout );
    free( buf );
    free( chunk_buf );

    printf("Done!\n");

    return 0;
}
