#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <libgen.h>

#include "../lzmasdk/lzma.h"

#ifndef uint
#define uint unsigned int
#endif

#define SFX_TAG (0xed3abd05)
#define COMPRESS_LZMA (0x10000000)
#define BLOCKSIZE	(64*1024)			// copy block size

struct sfx_head {
    uint tag ;
    uint filesize ;
    uint filemode ;
    uint namesize ;
    uint compsize ;
} ;

struct z_head {
	unsigned short mode ;			// link / file, file mode
	unsigned short namesize ;		// name size
	unsigned int   filesize ;		// file size
};

#define ZBUFFLAG	(0x51145)
int extfiles = 0 ;

// recursively make parent dir, return 0 for success
int mk_pdir( char * path )
{
	struct stat stbuf ;
	char * slash  ;
	
	// delete the old file
	if( lstat(path, &stbuf) == 0 ) {
		return remove(path) ;
	}
	
	// try make parent dir
	slash = path ;
	while( (slash=strchr(slash, '/'))!=NULL ) {
		*slash = 0 ;
		if( lstat(path, &stbuf) != 0 ) {
			if( mkdir( path, 0777 )!=0 ) {
				return 1 ;
			}
		}
		*slash = '/' ;
		slash++ ;
	}
	return 0 ;
}

int extract_zbuf( unsigned char * zbuf, int zsize, const char * pattern )
{
	FILE * fp_file ;		// output file
	struct z_head zh ;
	char 	filename[260] ;

	int pos = 0 ;
	while( (zsize-pos) > sizeof(zh) ) {
		memcpy( &zh, zbuf+pos, sizeof(zh) );
		pos+=sizeof(zh);

		memcpy( filename, zbuf+pos, zh.namesize );
		filename[zh.namesize ] = 0;
		pos+=zh.namesize ;
		
		if( mk_pdir( filename ) ) {
			// make parent dir failed
			printf("ERROR : pdir - %s!\n", filename );				
		}
		else if( S_ISREG( zh.mode ) ) {
			fp_file = fopen( filename, "w" );
			if( fp_file ) {
				fwrite( zbuf+pos, 1, zh.filesize, fp_file );
				fchmod( fileno(fp_file), zh.mode&0777 );				
				fclose( fp_file );
				extfiles++;
			}
			else {
				printf("ERROR : write %s\n", filename );
			}
		}
        else if( S_ISLNK(zh.mode) ) {
			unsigned char savec = zbuf[ pos+zh.filesize ] ;
			zbuf[ pos+zh.filesize ] = 0 ;
			symlink( (char *)zbuf+pos, filename );
			printf("link : %s -> %s\n", filename, zbuf+pos );
			zbuf[ pos+zh.filesize ] = savec ;
			extfiles++;
        }        
        else {
			printf("ERROR: zbuf\n");
		}
		pos+=zh.filesize ;
	}
	return 0;
}

int extract( const char * sfxfile, const char * pattern )
{
    int executesize=0 ;
    struct sfx_head fhd ;

    FILE * fp ;
    FILE * fp_file ;
    unsigned char * bufcomp ;
    unsigned char * buffile ;
    char filename[260] ;

    fp = fopen( sfxfile, "r" );
    if( fp==NULL ) {
        printf("Can't open sfx.\n");
        return 1;
    }

    fseek( fp, -16, SEEK_END );
    if( fscanf( fp, "%d", &executesize )<1 ) {
        executesize = 0 ;
    }
    if(executesize<0){
        executesize=0 ;
    }

    fseek( fp, executesize, SEEK_SET );
    while( fread( &fhd, 1, sizeof( fhd), fp ) == sizeof( fhd ) ) {
        if( fhd.tag != SFX_TAG || fhd.namesize<=0 || fhd.namesize>=sizeof(filename) ) {
            break;
        }
        fread( filename, 1, fhd.namesize, fp);
        filename[fhd.namesize]=0;
        
        if( pattern==NULL || *filename=='*' || fnmatch(pattern, filename, 0) == 0 ) {
			if( *filename!='*' && mk_pdir( filename ) ) {
				// make parent dir failed
				printf("ERROR : pdir - %s!\n", filename );
			}
			else if( S_ISREG(fhd.filemode) ) {
				buffile = (unsigned char *)malloc( fhd.filesize ) ;
				if( (fhd.filemode & COMPRESS_LZMA) != 0 ) {
					bufcomp = (unsigned char *)malloc( fhd.compsize ) ;
					fread( bufcomp, 1, fhd.compsize, fp );
					if( lzmadec( bufcomp, fhd.compsize, buffile, fhd.filesize )==fhd.filesize ) {
						printf("lzma : %s\n", filename );
					}
					else {
						printf("ERROR : lzma - %s !!!\n", filename );
					}
					free( bufcomp );
				}
				else if( fhd.filesize == fhd.compsize ) {
					fread( buffile, 1, fhd.filesize, fp );
					printf("copy : %s\n", filename );
				}					
				else {
					printf("ERROR : sfx structure!\n" );
					break ;
				}

				if( *filename=='*' ) {		// ZIP block
					extract_zbuf( buffile, fhd.filesize, pattern ) ;
				}
				else {
					// write to file
					fp_file = fopen( filename, "w" );
					if( fp_file ) {
						fwrite( buffile, 1, fhd.filesize, fp_file );
						fchmod( fileno(fp_file), fhd.filemode&0777 );				
						fclose( fp_file );
						extfiles++;			
					}			
				}
				free( buffile );
				fhd.compsize = 0 ;
			}
			else if( S_ISLNK(fhd.filemode) ) {
				buffile = (unsigned char *)malloc( fhd.compsize+1 ) ;
				fread( buffile, 1, fhd.compsize, fp );
				buffile[ fhd.compsize ]=0;
				symlink( (char *)buffile, filename );
				printf("link : %s -> %s\n", filename, buffile );
				free( buffile );
				extfiles++;
				fhd.compsize = 0 ;
			}        
			else {
				printf("ERROR : file type %x - %s\n", fhd.filemode, filename );
				break ;
			}
		
		}
		if(fhd.compsize>0)
			fseek( fp, fhd.compsize, SEEK_CUR );
    }

    fclose( fp );
    printf("%d files extracted!\n", extfiles );
    return 0;
}
