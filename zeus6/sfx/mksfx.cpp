#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef uint
#define uint unsigned int
#endif

// lzma encoder
extern int lzmaenc( unsigned char * lzmabuf, int lzmasize, unsigned char * src, int srcsize );

#define LINEBUF_SIZE	(4096)

#define SFX_TAG (0xed3abd05)
#define LZMA_FLAG (0x10000000)

struct sfx_head {
    uint tag ;
    uint filesize ;
    uint filemode ;
    uint namesize ;
    uint compsize ;
} ;

const char * sfxfile ="sfx" ;
const char * sfxlistfile = "sfxlist";

int totalsavedspace = 0;
int totalfiles = 0;

int archive_file( FILE * archive, char * ifilename, char * ofilename )
{
	int nocompress ;
	int l ;
	unsigned char * buf ;
    struct stat filestat ;
	struct sfx_head fhd ;

	if( ofilename[0] == '.' && ofilename[1]=='/' ) {
		ofilename+=2 ;
	}

	if( lstat( ifilename, &filestat ) != 0 ) {
		printf("ERROR : File not exist : %s\n", ifilename);
		return 0 ;
	}
	
	fhd.tag=SFX_TAG ;
	fhd.filemode = (uint) filestat.st_mode & ~LZMA_FLAG ;
	fhd.filesize = (uint) filestat.st_size ;
	fhd.namesize = strlen( ofilename );
	
	// set compress flag by extension name
	l = fhd.namesize ;
	if( filestat.st_size < 100 ||
	strcmp( ofilename+l-3, ".gz" ) == 0 ||
	strcmp( ofilename+l-4, ".png" ) == 0 ||
	strcmp( ofilename+l-4, ".jpg" ) == 0  ) {
		nocompress = 1 ;
	}
	else {
		nocompress = 0 ;
	}

    if( S_ISREG(filestat.st_mode) ) {
		FILE *  srcfile = fopen( ifilename, "r" );
		if( srcfile ) {
			
			// read source file
			int lzmabufsize = filestat.st_size ;
			buf = (unsigned char *)malloc( lzmabufsize );
			fhd.filesize = fread( buf, 1, lzmabufsize, srcfile );
			fclose( srcfile );
			if( fhd.filesize < 0 ) {
				free(buf);
				printf("error : %s\n", ifilename);
				return 0 ;
			}
			
			unsigned char * lzmabuf = NULL ;

			if( nocompress == 0 && fhd.filesize>100  ) {
				lzmabuf = (unsigned char *)malloc( lzmabufsize );
				fhd.compsize = lzmaenc( lzmabuf, lzmabufsize, buf, fhd.filesize );
			}
			else {
				lzmabuf = NULL ;
				fhd.compsize = fhd.filesize  ;
			}
			
			if( lzmabuf && fhd.compsize>0 && fhd.compsize<fhd.filesize ) {
				// write compressed data
				fhd.filemode |= LZMA_FLAG ;
				fwrite( &fhd, 1, sizeof(fhd), archive );		// file header
				fwrite( ofilename, 1, fhd.namesize, archive);	// file name
				if( fhd.compsize>0 )
					fwrite( lzmabuf, 1, fhd.compsize, archive );
				printf("lzma : %s\t\tsize: %d->%d\n", ofilename, fhd.filesize, fhd.compsize );
				totalsavedspace += fhd.filesize - fhd.compsize ;
			}
			else {
				// write original file data
				fhd.compsize = fhd.filesize ;					// copy in same size
				fwrite( &fhd, 1, sizeof(fhd), archive );		// file header
				fwrite( ofilename, 1, fhd.namesize, archive);	// file name
				if(fhd.filesize>0)
					fwrite( buf, 1, fhd.filesize, archive );
				printf("copy : %s\t\tsize: %d\n", ofilename, fhd.filesize );
			}
			
			if( lzmabuf != NULL ) 
				free( lzmabuf );
			free( buf );
			totalfiles ++ ;
		}
	}
	else if( S_ISLNK(filestat.st_mode) ) {
		buf = (unsigned char *)malloc( LINEBUF_SIZE );
		fhd.filesize = readlink( ifilename, (char *)buf, LINEBUF_SIZE-1 );
		buf[fhd.filesize]=0;
		fhd.compsize = fhd.filesize ;
		fwrite( &fhd, 1, sizeof(fhd), archive );	// file header
		fwrite( ofilename, 1, fhd.namesize, archive);	// file name
		fwrite( buf, 1, fhd.filesize, archive );
		printf("link : %s -> %s\n", ofilename, buf );
		free(buf);
		totalfiles ++ ;
	}
    else if(  S_ISDIR(filestat.st_mode) ) {
		DIR * pdir = opendir(ifilename); 
		if( pdir ) {
			printf("dir: %s\n", ifilename);
			struct dirent * ent ;
			while( (ent=readdir(pdir))!=NULL  ) {
				if( ent->d_name[0] == '.' && 
					( ent->d_name[1] == '.' || ent->d_name[1] == 0 ) ) {
 					continue ;
				}

				// append filenames
				int li = strlen(ifilename);
				ifilename[li] = '/' ;
				strcpy( ifilename+li+1, ent->d_name );
				
				int lo = strlen(ofilename);
				ofilename[lo] = '/' ;
				strcpy( ofilename+lo+1, ent->d_name );
				
				archive_file( archive, ifilename, ofilename );

				// restore filenames
				ifilename[li] = 0 ;				
				ofilename[lo] = 0 ;				
			}
			closedir( pdir );
		}
    }
	else {
		printf("Unsupported file mode %x - file : %s\n",filestat.st_mode, ifilename );
	}
	return 1 ;
}
   
   
int main(int argc, char * argv[])
{
    int executesize ;
    FILE * selfext ;
    FILE * listfile ;
    unsigned char * buf ;
    int bufsize ;
    const char *outfile ;

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
		outfile = argv[3] ;
    }
    else {
		outfile = sfxfile ;
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
    
    char *linebuf    = new char[LINEBUF_SIZE] ;
	char *ifilename = new char[LINEBUF_SIZE] ;
	char *ofilename = new char[LINEBUF_SIZE] ;

    while( fgets( linebuf, LINEBUF_SIZE, listfile ) ) {
        int n = sscanf(linebuf, "%s %s", ifilename, ofilename ) ;

        if( n<1 || ifilename[0]=='#' || ifilename[0]==';' || strcmp(ifilename, ".")==0 ) {		// comment, or . (current directory)
            continue ;
        }

        if( n<2 ) {
			strcpy( ofilename, ifilename );
        }

		archive_file(selfext, ifilename, ofilename ) ;
	}

    fprintf( selfext, "\n%16d", (int)executesize );
    fclose( listfile );
    fclose( selfext );
    printf("Total %d files, %d bytes saved.\nDone!\n", totalfiles, totalsavedspace );
    
    delete linebuf ;
    delete ifilename ;
    delete ofilename ;
	
    return 0;
}
