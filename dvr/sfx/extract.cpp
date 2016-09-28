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
#define LZMA_FLAG (0x10000000)
#define BLOCKSIZE	(64*1024)			// copy block size

struct sfx_head {
    uint tag ;
    uint filesize ;
    uint filemode ;
    uint namesize ;
    uint compsize ;
} ;

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

#define TMP_F	".xf"

int extract( const char * sfxfile, const char * pattern )
{
	int extfiles = 0 ;
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
        
		if( pattern!=NULL && fnmatch(pattern, filename, 0) != 0 ) {
			// no match filename, skip it
		}
		else if( mk_pdir( filename ) ) {
			// make parent dir failed
			printf("ERROR : pdir - %s!\n", filename );
		}
		else if( S_ISREG(fhd.filemode) ) {
			fp_file = fopen( TMP_F, "w" );
			if( fp_file ) {
	            if( fhd.compsize>0 ) {
					if( (fhd.filemode & LZMA_FLAG) ) {
						/*
						bufcomp = (unsigned char *)malloc( fhd.compsize ) ;
						fread( bufcomp, 1, fhd.compsize, fp );
						buffile = (unsigned char *)malloc( fhd.filesize ) ;
						if( lzmadec( bufcomp, fhd.compsize, buffile, fhd.filesize )==fhd.filesize ) {
							fwrite( buffile, 1, fhd.filesize, fp_file );
							printf("lzma : %s\n", filename );
						}
						else {
							printf("ERROR : lzma - %s !!!\n", filename );
						}
						free( buffile );
						free( bufcomp );
						*/
						
						bufcomp = (unsigned char *)malloc( BLOCKSIZE ) ;
						unsigned char * decout ;
						int decoutsize = 0 ;
						
						while( fhd.compsize>0 && fhd.filesize>0 ) {
							int bsize = fhd.compsize ;
							if( bsize>BLOCKSIZE ) bsize=BLOCKSIZE ;
							if( bsize>0 ) {
								fread( bufcomp, 1, bsize, fp );
								fhd.compsize -= bsize ;
							}
							int dpos = 0 ;
							while( dpos < bsize ) {
								int r = lzmadec_dec( bufcomp+dpos, bsize-dpos, &decout, &decoutsize );
								if( decoutsize>0 ) {
									if( decoutsize > fhd.filesize ) {
										decoutsize = fhd.filesize ;
									}
									fwrite( decout, 1, decoutsize, fp_file );
									fhd.filesize -= decoutsize;
								}
								if( r>0 ) {
									dpos+=r ;
								}
								else {
									break;
								}
							}
						}
						// end decoding
						lzmadec_dec( bufcomp, 0, &decout, &decoutsize );
						if( decoutsize>0 ) {
							if( decoutsize > fhd.filesize ) {
								decoutsize = fhd.filesize ;
							}							
							fwrite( decout, 1, decoutsize, fp_file );
							fhd.filesize -= decoutsize;							
						}
						lzmadec_free();
						free( bufcomp );
						if( fhd.filesize == 0 )
							printf("lzma : %s\n", filename );
						else 
							printf("ERROR : lzma - %s !!!\n", filename );
					}
					else {
						buffile = (unsigned char *)malloc( BLOCKSIZE ) ;
						while( fhd.compsize > 0 ) {
							int bsize = fhd.compsize ;
							if( bsize>BLOCKSIZE ) bsize=BLOCKSIZE ;
							fread( buffile, 1, bsize, fp );
							fwrite( buffile, 1, bsize, fp_file );
							fhd.compsize -= bsize ;
						}
						free( buffile );
						printf("copy : %s\n", filename );
					}
				}
				fclose( fp_file ) ;
				remove( filename );
				rename( TMP_F, filename );
				chmod( filename, fhd.filemode&0777 );
				
				extfiles++;
			}
			else {
				printf("ERROR : can't create %s\n", filename);
			}
        }
        else if( S_ISLNK(fhd.filemode) ) {
            bufcomp = (unsigned char *)malloc( fhd.compsize+1 ) ;
            fread( bufcomp, 1, fhd.compsize, fp );
            bufcomp[ fhd.compsize ]=0;
			remove( filename );
			symlink( (char *)bufcomp, filename );
			printf("link : %s -> %s\n", filename, bufcomp );
			free( bufcomp );
			extfiles++;
			fhd.compsize = 0 ;
        }
        else {
			printf("ERROR : file type %x - %s\n", fhd.filemode, filename );
		}

		if(fhd.compsize>0)
			fseek( fp, fhd.compsize, SEEK_CUR );
    }

    fclose( fp );
    printf("%d files extracted!\n", extfiles );
    return 0;
}
