
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


#include "../cfg.h"
#include "diomap.h"

/*
   share memory lock implemented use atomic swap

    operations between lock() and unlock() should be quick enough and do only memory access .

*/

void dio_lock()
{
}

void dio_unlock()
{
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
