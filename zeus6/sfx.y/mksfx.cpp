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

#define BUF_SIZE	(4096)

#define SFX_TAG (0xed3abd05)
#define COMPRESS_LZMA (0x10000000)

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

#define ZBUFFLAG	(S_IFREG)
#define ZBUFSIZE	(1024*1024)
unsigned char * zbuf ;
int             zbufsize ;
int             zbufpos ;

const char * sfxfile ="sfx" ;
const char * sfxlistfile = "sfxlist";

int totalsavedspace = 0;
int totalfiles = 0;

// write block data to archive
void archive_write( FILE * archive, const char * ofilename, uint filesize, uint filemode, void * zdata, int zsize )
{
	struct sfx_head fhd ;
	unsigned char * lzmabuf ;

	fhd.tag=SFX_TAG ;
	fhd.filesize = filesize ;
	fhd.filemode = filemode ;
	fhd.namesize = strlen( ofilename );
	fhd.compsize = (uint)zsize ;
	fwrite( &fhd, 1, sizeof(fhd), archive );		// file header
	fwrite( ofilename, 1, fhd.namesize, archive);	// file name
	if( fhd.compsize > 0 )
		fwrite( zdata, 1, fhd.compsize, archive );
}
	
// lzma zip and archive data
void archive_zdata( FILE * archive, const char * ofilename, uint filemode, void * srcbuf, int srcsize  )
{
	struct sfx_head fhd ;
	unsigned char * lzmabuf ;
	uint compsize ;

	if( srcsize>100  ) {
		lzmabuf = (unsigned char *)malloc( srcsize );
		compsize = lzmaenc( lzmabuf, srcsize, (unsigned char *)srcbuf, srcsize);
	}
	else {
		lzmabuf = NULL ;
		compsize = srcsize  ;
	}
			
	if( lzmabuf && compsize>0 && compsize<srcsize ) {
		// write compressed data
		filemode |= COMPRESS_LZMA ;
		archive_write( archive, ofilename, srcsize, filemode, lzmabuf, compsize );
		printf("lzma : %s\t\tsize: %d->%d\n", ofilename, srcsize, compsize );
		totalsavedspace += srcsize - compsize ;
	}
	else {
		// write original file data
		filemode &= !COMPRESS_LZMA ;
		archive_write( archive, ofilename, srcsize, filemode, srcbuf, srcsize );
		printf("copy : %s\t\tsize: %d\n", ofilename, fhd.filesize );
	}
	if( lzmabuf != NULL ) 
		free( lzmabuf );
}

void zbuf_write( FILE * archive, const char * ofilename, unsigned int filemode, void * sdata, int ssize  )
{
	struct z_head zh ;

	zh.mode = (unsigned short) filemode ;
	zh.namesize = strlen( ofilename ) ;
	zh.filesize = ssize ;
	memcpy( zbuf+zbufpos, &zh, sizeof(zh) );
	zbufpos+=sizeof(zh);
	memcpy( zbuf+zbufpos, ofilename, zh.namesize );
	zbufpos+=zh.namesize;
	memcpy( zbuf+zbufpos, sdata, zh.filesize );
	zbufpos+=zh.filesize ;
	
	// dump out if necessary
	if( zbufsize - zbufpos  < zbufsize/2 ) {
		archive_zdata( archive, "*", ZBUFFLAG, zbuf, zbufpos );
		zbufpos = 0 ;
	}
}

// zipmode: 0=auto (new method), 1=single file compress (old method), 2=force no compress
int archive_file( FILE * archive, char * ifilename, char * ofilename, int zipmode )
{
	unsigned char * filebuf ;
    struct stat filestat ;
    int filemode ;
    int filesize ;
    int namesize ;

	if( ofilename[0] == '.' && ofilename[1]=='/' ) {
		ofilename+=2 ;
	}

	if( lstat( ifilename, &filestat ) != 0 ) {
		printf("ERROR : state %s\n", ifilename);
		return 0 ;
	}
	
	namesize = strlen( ofilename ) ;
	filesize = (uint) filestat.st_size ;
	filemode = (uint) filestat.st_mode ;
	
    if( S_ISREG(filemode) ) {
		
		// set compress flag by extension name
		if( strcmp( ofilename+namesize-3, ".gz" ) == 0 ||
			strcmp( ofilename+namesize-4, ".png" ) == 0 ||
			strcmp( ofilename+namesize-4, ".jpg" ) == 0  ) {
			zipmode = 2 ;		// don't compress these files (already compressed ^^)
		}


		FILE *  srcfile = fopen( ifilename, "r" );
		if( srcfile ) {
			
			// read source file
			int lzmabufsize = filestat.st_size ;
			filebuf = (unsigned char *)malloc(  filesize );
			filesize = fread( filebuf, 1, filesize, srcfile );
			fclose( srcfile );
			if( filesize < 0 ) {
				free(filebuf);
				printf("Error : read %s\n", ifilename);
				return 0 ;
			}
			
			if( zipmode==0 && filesize > zbufsize/32 ) {
				zipmode = 1 ;		// old method for bigger file
			}
			
			if( zipmode == 0 ) {	// copy to zbuf for later compress
				zbuf_write( archive, ofilename, filemode, filebuf, filesize ) ;
			}
			else if( zipmode== 1 ) {	// single compress
				archive_zdata( archive, ofilename, filemode, filebuf, filesize );
			}
			else {
				// copy data
				archive_write( archive, ofilename, filesize, filemode, filebuf, filesize );
				printf("copy : %s\t\tsize: %d\n", ofilename, filesize );				
			}
			free( filebuf );
			totalfiles ++ ;
		}
		else {
			printf("ERROR : can't open %s\n", ifilename );
		}
	}
	else if( S_ISLNK(filemode) ) {
		filebuf = (unsigned char *)malloc( BUF_SIZE );
		filesize = readlink( ifilename, (char *)filebuf, BUF_SIZE-1 );
		filebuf[filesize]=0;		
		if( zipmode==0 ) {
			// copy to zbuf
			zbuf_write( archive, ofilename, filemode, filebuf, filesize ) ;
		}
		else {
			// copy data of symlink
			archive_write( archive, ofilename, filesize, filemode, filebuf, filesize );
			printf("link : %s -> %s\n", ofilename, filebuf );
			totalfiles ++ ;
		}
		printf("link : %s -> %s\n", ofilename, filebuf );
		free( filebuf );
		totalfiles ++ ;
	}
    else if(  S_ISDIR(filemode) ) {
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
				
				archive_file( archive, ifilename, ofilename, zipmode );

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
    int zipmode ;		//	0: auto, (new zip), 1: single, (old method), 2: force copy

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
    
    char *linebuf    = new char[BUF_SIZE] ;
	char *ifilename = new char[BUF_SIZE] ;
	char *ofilename = new char[BUF_SIZE] ;
	
	zbufsize = 1024*1024 ;			// 1 Mbytes for zbuffer ?
	zbufpos = 0 ;
	zbuf = (unsigned char *)malloc( ZBUFSIZE );
	if( zbuf==NULL ) {
		printf("No enough memory!\n");
		exit(2);
	}

    while( fgets( linebuf, BUF_SIZE, listfile ) ) {
		zipmode = 0 ;
        int n = sscanf(linebuf, "%s %s %d", ifilename, ofilename, &zipmode ) ;

        if( n<1 || ifilename[0]=='#' || ifilename[0]==';' || strcmp(ifilename, ".")==0 ) {		// comment, or . (current directory)
            continue ;
        }

        if( n<2 || *ofilename=='*' ) {
			strcpy( ofilename, ifilename );
        }
        
        if( n<3 ) zipmode = 0;

		archive_file(selfext, ifilename, ofilename, zipmode ) ;
	}
	
	if( zbufpos > 0 ) {
		archive_zdata( selfext, "*", ZBUFFLAG, zbuf, zbufpos );
	}
	free(zbuf);

    fprintf( selfext, "\n%16d", (int)executesize );
    fclose( listfile );
    fclose( selfext );
    printf("Total %d files, %d bytes saved.\nDone!\n", totalfiles, totalsavedspace );
    
    delete linebuf ;
    delete ifilename ;
    delete ofilename ;
	
    return 0;
}
