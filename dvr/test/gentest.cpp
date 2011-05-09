
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <fnmatch.h>
#include <termios.h>
#include <stdarg.h>

#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

unsigned short map[576][720] ;

unsigned rgb2y( int r, int g, int b )
{
    return (unsigned)((0.257 * r) + (0.504 * g) + (0.098 * b) + 16) ;
}

unsigned rgb2u( int r, int g, int b )
{
    return (unsigned)(-(0.148 * r) - (0.291 * g) + (0.439 * b) + 128) ;
}

unsigned rgb2v( int r, int g, int b )
{
    return (unsigned)((0.439 * r) - (0.368 * g) - (0.071 * b) + 128) ;
}

int test_722_logo()
{
    FILE * f422 ;
    f422 = fopen("720.422.org", "r");
    fread( map, 1, sizeof( map ), f422 );
    fclose( f422 );

    int x, y ;
    unsigned int v ;
    v=0 ;
    for( y=0; y<576; y++) {
        for( x=0; x<720; x++ ) {
            if( y>=100 && y<(100+256) && x>=100 && x<(100+2*256) ) {
                int r, g, b ;
                r=y-100 ;
                g=0 ;
                b=(x-100)/2 ;
                unsigned Y, U, V ;
                Y=rgb2y(r,g,b);
                U=rgb2u(r,g,b);
                V=rgb2v(r,g,b);
                if( x%2==0 ) {
//                    map[y][x]= (y-100)*256 + 128;
                    map[y][x]= Y*256+U ;
                }
                else { 
//                    map[y][x]= (y-100)*256 + (x-100)/2;
                    map[y][x]= Y*256+V ;
                }
            }
        }
    }
    
    f422 = fopen( "720.422", "w");
    fwrite( map, 1, sizeof( map ), f422 );
    fclose( f422 );
    return 0 ;
}

class ts {
	public:
		int member ;
		ts() {
			printf(" Construct ts %p\n", this );
			member=55;
		}
		~ts() {
			printf(" Destruct ts %p\n", this );
		}
		void print() {
			printf( "member = %d\n", member );
		}
} ;

ts t1, t2 ;
int x = 50 ;	

int main()
{

	printf("x=%d\n", x );
  t1.print();  
//    test_722_logo();
     ts t ;
	t.print();
    return 0;
}
