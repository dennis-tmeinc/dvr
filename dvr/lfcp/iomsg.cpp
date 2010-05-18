#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "../ioprocess/diomap.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

struct dio_mmap * p_dio_mmap ;
char dvriomap[128] = "/var/dvr/dvriomap" ;
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;


/*
   share memory lock implemented use atomic swap

    operations between lock() and unlock() should be quick enough and only memory access only.
 
*/ 

// atomically swap value
int atomic_swap( int *m, int v)
{
    register int result ;
/*
    result=*m ;
    *m = v ;
*/
    asm volatile (
        "swp  %0, %2, [%1]\n\t"
        : "=r" (result)
        : "r" (m), "r" (v)
    ) ;
    return result ;
}

void dio_lock()
{
    if( p_dio_mmap ) {
        int c=0;
        while( atomic_swap( &(p_dio_mmap->lock), 1 ) ) {
            if( c++<20 ) {
                sched_yield();
            }
            else {
                // yield too many times ?
                usleep(1);
            }
        }
    }
}

void dio_unlock()
{
    if( p_dio_mmap ) {
        p_dio_mmap->lock=0;
    }
}

// return 
//        0 : failed
//        1 : success
int appinit()
{
    int fd ;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    v = dvrconfig.getvalue( "system", "iomapfile");
    char * iomapfile = v.getstring();
    if( iomapfile && strlen(iomapfile)>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    p_dio_mmap=NULL ;
    fd = open(dvriomap, O_RDWR );
    if( fd<=0 ) {
        printf("Can't open io map file!\n");
        return 0 ;
    }
    p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );								// fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    return 1 ;
}
 
// app finish, clean up
void appfinish()
{
    // clean up shared memory
    if( p_dio_mmap ) {
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}

int main(int argc, char * argv[])
{
   int susp = -1 ;
	if( argc<2 ) {
		printf("Usage: iomsg <1|0> <iomessage>\n");
		return 1 ;
	}
	
    if( appinit()==0 ) {
        return 1;
    }
    susp = atoi( argv[1] );

    dio_lock();
    if( argc>2 ) {
        strcpy( p_dio_mmap->iomsg, argv[2] );
    }
    else {
        p_dio_mmap->iomsg[0]=0 ;
    }
    if( susp == 0 ) {
        p_dio_mmap->iomode = IOMODE_RUN;
    }
    else if( susp==1 ) {
        p_dio_mmap->iomode = IOMODE_SUSPEND;
    }
    dio_unlock();

	appfinish();
    return 0;
}

