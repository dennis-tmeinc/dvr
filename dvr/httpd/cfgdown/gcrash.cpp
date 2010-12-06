
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "../../ioprocess/diomap.h"

void dio_wait_cmd()
{
    // wait till rtc_cmd = 0 ;
    int delay ;
    for( delay=0; delay<200; delay++ ) {    // 20 seconds waiting
        if( p_dio_mmap->rtc_cmd <=0 ) break;
        usleep(100000);
    }
}

int main()
{
    if( dio_mmap() ) {
        // wait till rtc_cmd = 0 ;
        dio_wait_cmd();
        dio_lock();
        p_dio_mmap->rtc_year = 0 ;
        p_dio_mmap->rtc_cmd = 11 ;          // saving gforce crash data command
        dio_unlock();
        dio_munmap();
    }
    return 0 ;
}
