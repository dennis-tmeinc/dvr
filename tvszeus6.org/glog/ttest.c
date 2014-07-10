
// GPS LOGING 


#include <sys/mman.h>
#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <memory.h>
#include <dirent.h>
#include <pthread.h>
#include <termios.h>
#include <stdarg.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

int main()
{
   struct timeval v ;
   int i;
   int v1 ;
   gettimeofday( &v, NULL );
   v1 = v.tv_sec+3 ;
   for (i=0; i<500000 ; i++ ) {
        gettimeofday( &v, NULL );
        printf( "%d.%06d\n", (int)v.tv_sec, (int)(v.tv_usec) );
//        usleep(1);
        if( (int)v.tv_sec > v1 ) break;
   }
    return 0 ;
}
