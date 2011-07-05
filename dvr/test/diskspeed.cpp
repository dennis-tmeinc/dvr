
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <stdlib.h>


#define MAXFILE	(50)

int filenumber = 1 ;
char prefix[256]="sp_" ;
char filename[256] ;
int buffersize = 0 ;
int byterate = 0 ;
int freespace = 5 ;
int filesize = 100*1024*1024 ;

int app_quit=0 ;

void sig_handler(int sig)
{
    app_quit=1 ;
}

int nfile_old = 0 ;
int nfile_new = 0 ;

void file_done( int channel ) 
{
	char nfname[256] ;
	char cfname[256] ;
	nfile_new++ ;
	if( nfile_new>=1000000 ) nfile_new = 1 ;
	sprintf( nfname, "%s%06d", prefix, nfile_new );
	sprintf( cfname, "%sc%d", prefix, channel );
	rename( cfname, nfname );
}	

int file_clean() 
{
	char ofname[256] ;
	if( nfile_new != nfile_old ) {
		nfile_old++ ;
		if( nfile_old>=1000000 ) nfile_old = 1 ;
		sprintf( ofname, "%s%06d", prefix, nfile_old );
		remove( ofname );
		return 1 ;
	}
	return 0 ;
}

// generate random number less then 64k
int random_number(  )
{
    unsigned x ;
	static int randfile = 0 ;
    if( randfile == 0 ) {
        randfile  = open( "/dev/urandom", O_RDONLY ) ;
    }
    read( randfile, &x, sizeof(x));
	return (int)( (x&0x0ffff)|4 ) ;
}

int sync_dirty = 1000 ;

// Parse /proc/meminfo
void check_sync()
{
    int dirty = 0 ;
    FILE *fproc=NULL;
    char buf[256];
    fproc = fopen("/proc/meminfo", "r");
    if (fproc == NULL)
        return ;
    while (fgets(buf, 256, fproc)) {
        if (memcmp(buf, "Dirty:", 6) == 0) {
            if( sscanf(buf + 6, "%d", &dirty) ) {
                if( dirty>sync_dirty ) {
                    sync();
                }
            }
        }
    }
    fclose(fproc);
    return ;
}

// return time in seconds from first call to this function
double gettime()
{
	struct timeval tv ;
    gettimeofday( &tv, NULL );
    return (double)(unsigned long)tv.tv_sec + (double)(int)tv.tv_usec/1000000.0 ;
}

// generate bitrate bytes, may sleep for a while
int byterategen()
{
    static double t1=0, t2=0 ;
    static int bytesgen = 0 ;
    double b ;
    int i, gen ;
    
    if( byterate<=0 ) {
        return random_number() ;
    }
    else {
        for( i= 0 ; i<50 ; i++ ) {
            t2 = gettime();
            if( (t2-t1) > 30.0 ) {
                t1 = t2 ;
                bytesgen=0 ;
            }
            b =  (t2-t1) * (double)byterate ;
            if( (int)b >= bytesgen ) {
                gen = random_number();
                bytesgen += gen ;
                return gen ;
            }
            else {
                usleep(10000);
            }
        }
        bytesgen++ ;
        return 1 ;
    }
}

#define AVGTIMES	(30)
double starttime ;

int total_m = 0 ;
int total_b = 0 ;

double t2s_t[AVGTIMES] ;
int t2s_m[AVGTIMES] ;
int t2s_b[AVGTIMES] ;
int t2s_idx ;

int count_init()
{
	int i ;
	starttime=gettime();
	t2s_idx=0 ;
	for( i=0; i<AVGTIMES; i++ ) {
		t2s_t[i] = starttime ;
		t2s_b[i]=t2s_m[i]=0 ;
	}
}

void count_speed( int w )
{
	int i;
	double tt;
	tt = gettime();
	
    total_b += w ;
	if( total_b>=1000000 ) {
		total_m++ ;
		total_b-=1000000 ;
	}

	t2s_b[t2s_idx] += w ;
	if( t2s_b[t2s_idx]>=1000000 ) {
		t2s_m[t2s_idx]++ ;
		t2s_b[t2s_idx]-=1000000 ;
	}

	if( (tt-t2s_t[t2s_idx]) > 2.0 ) {
		double t2sm = (double)t2s_m[t2s_idx]  + (double) t2s_b[t2s_idx]/1000000.0 ;
		double t2sspeed = t2sm/(tt-t2s_t[t2s_idx]);

		if( ++t2s_idx>=AVGTIMES ) t2s_idx=0 ;

		int    t10sm=0, t10sb=0 ;
		for( i=0; i<AVGTIMES; i++) {
			t10sm += t2s_m[i] ;
			t10sb += t2s_b[i] ;
		}
		double t10sspeed = ((double)t10sm  + (double) t10sb/1000000.0)/(tt - t2s_t[t2s_idx]) ;
		
		t2s_t[t2s_idx] = tt ;
		t2s_b[t2s_idx] = 0 ;
		t2s_m[t2s_idx] = 0 ;

		printf("Speed: %.3f MB/s  Time: %ds  Total: %d MB Average: %.3f MB/s   \r",
		       t2sspeed, (int)(tt-starttime), total_m, t10sspeed );
		fflush( stdout );

	}
}

void count_total()
{
	double tt;
	tt = gettime();

	double totalm = (double)total_m  + (double) total_b/1000000.0 ;
    double speed = totalm / (tt - starttime );
	printf("\nTime: %ds Total: %d MB Average: %.3f MB/s\n",
		(int)(tt-starttime), total_m, speed );

}

void spdtest_file()
{
	int  wsize ;
	FILE * handle = NULL ;
    
	handle = fopen( filename, "wb" );
	if( buffersize>1024 ) {
		setvbuf( handle, NULL, _IOFBF, buffersize );
	}
	if( handle==NULL ) {
		return ;
	}

	while( app_quit==0 ) {
        wsize = byterategen() ;
        if( wsize>0 ) {
            char * buf = (char *)malloc( wsize );
            wsize = fwrite( buf, 1, wsize, handle ) ;
            free( buf );
        }
		if( wsize<=0 || ftell( handle )>filesize ) {
            fflush( handle );
			fseek( handle, 0, SEEK_SET );
            wsize=0 ;
		}
        // counting
        count_speed( wsize ) ;
        check_sync();
	}

	fclose( handle );
	sync();
    count_total();

	return ;
}

// return free disk space in percentage 
int disk_freespace_percent(char *path)
{
    struct statfs stfs;
    
    if (statfs(path, &stfs) == 0) {
		return stfs.f_bavail * 100 / stfs.f_blocks ;
    }
	return 0;
}

void spdtest()
{
	int i;
	char filename[256] ;
	char lfilename[256] ;
	int  wsize ;
	FILE * filehandle[MAXFILE] ;

	sprintf(lfilename, "%s0000", prefix ) ;
	filehandle[0] = fopen( lfilename, "wb" );
	fprintf(filehandle[0], "%d", 1 );
	fclose( filehandle[0] );

	for( i=0; i<filenumber; i++) {
		filehandle[i]=NULL ;
	}

	while( app_quit==0 ) {
		for( i=0; i<filenumber; i++ ) {
			if( filehandle[i]==NULL ) {
				sprintf(filename, "%sc%d", prefix, i) ;
				filehandle[i] = fopen( filename, "wb" );
				if( filehandle[i] && buffersize>1024 ) {
					setvbuf( filehandle[i], NULL, _IOFBF, buffersize );
				}
			}
			wsize = 0 ;
            if( filehandle[i] ) {
                wsize = byterategen() ;
                if( wsize > 0 ) {
                    char * buf = (char *)malloc( wsize ) ;
                    wsize = fwrite( buf, 1, wsize, filehandle[i] ) ;
//                    wsize = write( fileno(pfilet[i].filehandle), buf, wsize );
                    free( buf );
                }

				if( ftell( filehandle[i] )>=filesize ) {
					fclose( filehandle[i] ) ;
					filehandle[i]=NULL ;
					file_done(i);
				}
            }
            count_speed( wsize );
		}

		check_sync();

		// check disk space
		if( disk_freespace_percent(lfilename)<freespace ) {
			file_clean();
		}
    }

	// close files
	for( i=0; i<filenumber; i++ ) {
		if( filehandle[i] ) {
			fclose( filehandle[i] ) ;
			file_done(i);
		}
	}

	// clean files
	while( file_clean() );
	remove( lfilename );
	
	sync();

    count_total();
	return ;
}

void ratetest()
{
	while( app_quit==0 ) {
        // counting
        count_speed( byterategen() );
	}
    count_total();
	return ;
}

void usage(char * appname)
{
	printf("Usage:\n"
	       "  %s -f<filesize> -b<buffersize> -n<filenumber> -d<devicename|filename> -p<fileprefix> -r<byterate> -s<freespace>\n", appname );
	return ;
}	

int main(int argc, char * argv[])
{
    int i;

    int testrate = 0 ;
    
	if( argc<2 ) {
		usage( argv[0] );
		return 1 ;
	}

    for( i=1; i<argc; i++ ) {
        if( argv[i][0] == '-' ) {
            char unit ;
            switch( argv[i][1] ) {
				case 'h' :
					usage( argv[0] );
					return 1 ;

				case 'f' :
                    unit = 'b' ;
                    sscanf(&(argv[i][2]), "%d%c", &filesize, &unit);
                    if( unit=='K' || unit=='k' ) {
                        filesize*=1024 ;
                    }
                    else if( unit=='M' || unit=='m' ) {
                        filesize*=1024*1024 ;
                    }
                    else if( unit=='G' || unit=='g' ) {
                        filesize*=1024*1024*1024 ;
                    }
                    if( filesize<1 ) {
                        filesize=1 ;
                    }
                    else if( filesize>1024*1024*1024) {
                        filesize=1024*1024*1024 ;
                    }
                    break;

                case 'b' :
                    unit = 'b' ;
                    sscanf(&(argv[i][2]), "%d%c", &buffersize, &unit);
                    if( unit=='K' || unit=='k' ) {
                        buffersize*=1024 ;
                    }
                    else if( unit=='M' || unit=='m' ) {
                        buffersize*=1024*1024 ;
                    }
                    if( buffersize<0 ) {
                        buffersize=0 ;
                    }
                    else if( buffersize>16*1024*1024) {
                        buffersize=16*1024*1024 ;
                    }
                    break ;

                case 'n' :
                    sscanf(&(argv[i][2]), "%d", &filenumber );
                    if( filenumber<1 ) {
                        filenumber=1 ;
                    }
					else if( filenumber>MAXFILE ) {
						filenumber = MAXFILE ;
					}
                    break;

                case 'd' :
                    strcpy( filename, &(argv[i][2]) ) ;
                    break;
                    
                case 'p' :
                    strcpy( prefix, &(argv[i][2]) ) ;
                    break;
                    
                case 'r' :
                    unit = 'b' ;
                    sscanf(&(argv[i][2]), "%d%c", &byterate, &unit);
                    if( unit=='K' || unit=='k' ) {
                        byterate*=1024 ;
                    }
                    else if( unit=='M' || unit=='m' ) {
                        byterate*=1024*1024 ;
                    }
                    if( byterate<0 || byterate>1024*1024*1024 ) {
                        byterate=0 ;
                    }
                    printf( "byterate : %d \n", byterate );
                    break ;

                case 'y' :
                    sscanf(&(argv[i][2]), "%d", &sync_dirty);
                    break ;

				case 's' :
					sscanf(&(argv[i][2]), "%d", &freespace);
					break ;

				case 'x' :
                    testrate = 1 ;
                    break ;

                default :
                    printf( "Unknonw parameter : %s \n", argv[i] );
                    return 1 ;
            }
        }
        else {
            break ;
        }		
    }
    
	signal( SIGINT, sig_handler );

	sync();		// clear dirty buffers
	// initial time
	count_init() ;
	
    if( testrate ) {
        ratetest();
    }
	else if( filename[0] ) {
		spdtest_file();
	}
	else {
		spdtest();
	}

	return 0;
}

