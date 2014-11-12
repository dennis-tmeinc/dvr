#include <unistd.h>

#include "diomap.h"

int main(int argc, char * argv[])
{
    int retry=5 ;
    while( retry-- ) {
        if( dio_mmap() ) break ;
        sleep(2);
    }
    if( p_dio_mmap!=NULL ) {
        while ( p_dio_mmap->iomode<=0 ) 
            sleep(1);
        dio_munmap();
    }
    return 0;
}

