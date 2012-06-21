
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


int filenumber = 1 ;
char prefix[256]="ftt" ;
char filename[256] ;
int buffersize = 0 ;
int byterate = 0 ;
int freespace = 5 ;
int filesize = 100*1024*1024 ;
int filesync = 0 ;
char * buf ;

int app_quit=0 ;

struct filet_struct {
	FILE * filehandle ;
	int del_file ;
	int cur_file ;
    char filename[512] ;
} * pfilet ;

void sig_handler(int sig)
{
    app_quit=1 ;
}

int randfile = 0 ;
int rand( int max )
{
    unsigned long x ;
    if( randfile == 0 ) {
        randfile  = open( "/dev/urandom", O_RDONLY ) ;
    }
    read( randfile, &x, sizeof(x));
    return (int)(x%max) ;
}

int sync_dirty = 1000 ;

// Parse /proc/meminfo
void check_sync()
{
/*    
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
*/
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
        return (rand( 20000 )+5)&0xfffffc ;
    }
    else {
        for( i= 0 ; i<20 ; i++ ) {
            t2 = gettime();
            if( (t2-t1) > 20.0 ) {
                t1 = t2 ;
                bytesgen=0 ;
            }
            b =  (t2-t1) * (double)byterate ;
            if( (int)b >= bytesgen ) {
                gen = (rand( 20000 )+5)&0xfffffc ;
                bytesgen += gen ;
                return gen ;
            }
            else {
                usleep(50000);
            }
        }
        bytesgen++ ;
        return 1 ;
    }
}

double starttime ;
double prevtime ;

int total_m = 0 ;
int total_b = 0 ;

void count_speed( int w, double tt )
{
    total_b += w ;
    total_m += total_b/1000000 ;
    total_b %= 1000000 ;

    double speed = (double)w/(tt-prevtime)/1000000.0;
    double ttsize = (double)total_m  + (double) total_b/1000000.0 ;
    double avg = ttsize / (tt - starttime );
    printf("Speed: %.3f MB/s  Time: %ds  Total: %d MB Average: %.3f MB/s   \r",
           speed, (int)(tt-starttime), total_m, avg );
    fflush( stdout );
    prevtime=tt ;
}

void count_total()
{
    double ttsize = (double)total_m  + (double) total_b/1000000.0 ;
    double avg = ttsize / (prevtime - starttime );
	printf("\nTime: %ds Total: %d MB Average: %.3f MB/s\n",
		(int)(prevtime-starttime), total_m, avg );

}

void spdtest_file()
{
	int  wsize ;
    int  sum ;
	FILE * handle = NULL ;
	double thistime ;

	sum = 0 ;

    sync();
    
	// initial time
    starttime=prevtime = gettime() ;
    
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
            buf = (char *)malloc( wsize );
            wsize = fwrite( buf, 1, wsize, handle ) ;
            free( buf );
            if( filesync ) {
                sync();
            }
            
        }
		if( wsize<=0 || ftell( handle )>filesize ) {
            fflush( handle );
			fseek( handle, 0, SEEK_SET );
            wsize=0 ;
		}
        sum+=wsize ;

        // counting
		thistime = gettime(); 
		if( (thistime-prevtime)>2.0) {
            count_speed( sum, thistime );
            sum = 0 ;
            check_sync();
        }
	}

	fclose( handle );

	sync();
	sync();

	thistime = gettime();
    count_speed( sum, thistime );
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
	int  wsize ;
	int  sum ;
	double thistime ;

	pfilet = new struct filet_struct [filenumber] ;

	for( i=0; i<filenumber; i++ ) {
		pfilet[i].filehandle=NULL ;
		pfilet[i].cur_file=0 ;
		pfilet[i].del_file=0 ;
	}

	sum = 0 ;

	sync();		// clear dirty buffers

	// initial time
	starttime = prevtime = gettime() ;

	sprintf(pfilet[0].filename, "%s_lock", prefix ) ;
	pfilet[0].filehandle = fopen( pfilet[0].filename, "wb" );
	fprintf(pfilet[0].filehandle, "%d", 1 );
	fclose( pfilet[0].filehandle );
	pfilet[0].filehandle = NULL ;

	while( app_quit==0 ) {
		for( i=0; i<filenumber; i++ ) {
			if( pfilet[i].filehandle==NULL ) {
				pfilet[i].cur_file++ ;
				sprintf(pfilet[i].filename, "%s_%d_%d", prefix, pfilet[i].cur_file, i) ;
				pfilet[i].filehandle = fopen( pfilet[i].filename, "wb" );
				if( pfilet[i].filehandle && buffersize>1024 ) {
//					setvbuf( pfilet[i].filehandle, NULL, _IOFBF, buffersize );
				}
			}
			wsize = 0 ;
            if( pfilet[i].filehandle ) {
                wsize = byterategen() ;
                wsize = buffersize ;
                if( wsize > 0 ) {
                    buf = (char *)malloc( wsize ) ;
                    wsize = fwrite( buf, 1, wsize, pfilet[i].filehandle ) ;
//                    wsize = write( fileno(pfilet[i].filehandle), buf, wsize );
                    free( buf );
                    if( filesync ) {
                        sync();
                    }
                }

				sprintf(filename, "%s_lock", prefix ) ;

				if( disk_freespace_percent(filename)<freespace || wsize<=0 ) {
                    if( pfilet[i].filehandle ) {
                        fclose( pfilet[i].filehandle ) ;
                        pfilet[i].filehandle=NULL ;
                    }
                    // delete one file 
                    if( pfilet[i].del_file<pfilet[i].cur_file ) {
                        pfilet[i].del_file++ ;
                        sprintf(filename, "%s_%d_%d", prefix, pfilet[i].del_file, i) ;
                        remove(filename);
                    }
                    wsize=0;
                }
            }

            
            if( pfilet[i].filehandle && ftell( pfilet[i].filehandle )>filesize ) {
				fclose( pfilet[i].filehandle ) ;
				pfilet[i].filehandle=NULL ;
			}

            
            sum+=wsize ;
            // counting
            thistime = gettime(); 
            if( (thistime-prevtime)>2) {
                count_speed( sum, thistime );
                sum = 0 ;
                check_sync();
            }
            if( app_quit ) {
                break ;
            }
        }
//		if( getch()!=ERR ) {
//			app_quit=1 ;
//		}
    }

	// close files
	for( i=0; i<filenumber; i++ ) {
		if( pfilet[i].filehandle ) {
			fclose( pfilet[i].filehandle ) ;
		}
	}

	// clean files
	for( i=0; i<filenumber; i++ ) {
		while( pfilet[i].del_file<pfilet[i].cur_file ) {
			pfilet[i].del_file++ ;
			sprintf(filename, "ftt_%d_%d", pfilet[i].del_file, i );
			remove(filename);
		}
	}
	delete [] pfilet ;

	sprintf(filename, "%s_lock", prefix ) ;
	remove( filename );
	
	sync();
	sync();

	thistime = gettime();
    count_speed( sum, thistime );
    count_total();
	return ;
}

void ratetest()
{
    int  wsize ;
    int  sum ;
	double thistime ;

	sum = 0 ;

	// initial time
    starttime=prevtime = gettime() ;
    
	while( app_quit==0 ) {
        wsize = byterategen() ;
        sum+=wsize ;

        // counting
		thistime = gettime(); 
		if( (thistime-prevtime)>2.0) {
            count_speed( sum, thistime );
            sum = 0 ;
        }
	}

    thistime = gettime();
    count_speed( sum, thistime );
    count_total();

	return ;

}

void usage(char * appname)
{
	printf("Usage:\n"
	       "  %s -f<filesize> -b<buffersize> -n<filenumber> -d<devicename|filename> -p<fileprefix> -r<byterate> -s<freespace> -c\n", appname );
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
                    if( filenumber<=0 || filenumber>50 ) {
                        filenumber=1 ;
                    }
                    break;

                case 'd' :
                    strcpy( filename, &(argv[i][2]) ) ;
                    break;
                    
                case 'c' :
                    filesync=1 ;
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

