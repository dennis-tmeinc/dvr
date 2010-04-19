#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/timex.h>

static int abc ;
void doingnothing()
{
    abc+=1 ;
}

void yield()
{
//    struct timeval timeout ;
//    timeout.tv_sec = 0 ;
//    timeout.tv_usec = 0 ;
//        sleep(0);
//        sleep(1);
//    select(1,  NULL, NULL, NULL, &timeout);
    struct timespec tmReq;
    tmReq.tv_sec = 0;
    tmReq.tv_nsec = 0;
    // we're not interested in remaining time nor in return value
    (void)nanosleep(&tmReq, NULL);
//        sched_yield();
}

int main()
{
    int i ;
    for(i=0; i<0x7fff0000; i++) {
        yield();
        if( i%100000==0 ) {
            struct timeval tv ;
            gettimeofday(&tv, NULL);
            printf("%d.%06d\n", (int)tv.tv_sec, (int)tv.tv_usec);
        }
    }

	return 0;
}

