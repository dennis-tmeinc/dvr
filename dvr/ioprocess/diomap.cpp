
#include "../cfg.h"
#include "diomap.h"

/*
   share memory lock implemented use atomic swap

    operations between lock() and unlock() should be quick enough and only memory access only.
 
*/ 

// atomically swap value
/*
    result=*m ;
    *m = v ;
 	return result ;
*/
int atomic_swap( int *m, int v)
{
    register int result ;
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
                // yield too many times, let's sleep to save CPU. 
                usleep(1);
            }
        }
    }
}

void dio_unlock()
{
    if( p_dio_mmap ) {
        p_dio_mmap->lock=0;         // 32bit int assignment is atomic already
    }
}

struct dio_mmap * p_dio_mmap=NULL ;

struct dio_mmap * dio_mmap(char * mmapfile)
{
    int fd ;
    void * p ;
    if( mmapfile==NULL ) {
		fd = open( VAR_DIR"/dvriomap", O_RDWR, S_IRWXU);
    }
    else {
        fd = open( mmapfile, O_RDWR, S_IRWXU);
    }
    if( fd<=0 ) {
        return NULL ;
    }
    p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );								// fd no more needed
    if( p==(void *)-1 ) {
        p=NULL ;
    }
    p_dio_mmap=(struct dio_mmap *)p ;
    return  p_dio_mmap ;
}

void dio_munmap()
{
    if( p_dio_mmap!=NULL ) {
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}
