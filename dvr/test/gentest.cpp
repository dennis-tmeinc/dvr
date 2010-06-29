
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

pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;
static pthread_mutex_t mem_mutex;
static void mem_lock()
{
    pthread_mutex_lock(&mem_mutex);
}

static void mem_unlock()
{
    pthread_mutex_unlock(&mem_mutex);
}

void mem_init()
{
    // initial mutex
    memcpy( &mem_mutex, &mutex_init, sizeof(mutex_init));
    return;
}

void mem_uninit()
{
    pthread_mutex_destroy(&mem_mutex);
    return ;
}


int toddiff(struct timeval *tod1, struct timeval *tod2)
{
    long long t1, t2;
    t1 = tod1->tv_sec * 1000000 + tod1->tv_usec;
    t2 = tod2->tv_sec * 1000000 + tod2->tv_usec;
    return (int)(t1 - t2);
}

static int run ;
static pthread_t threadid1;
static pthread_t threadid2;

void f1(){};
void func()
{
    int i;
    for(i=0; i<100000; i++) {
        f1();
    }
}

void *thread1(void *param)
{
    int * p = (int *)param ;
    while(run) {
        (*p)=(*p)+1 ;
        func();
    }
    return NULL ;
}

void *thread2(void *param)
{
    int * p = (int *)param ;
    while(run) {
        (*p)=(*p)+1 ;
        func();
    }
    return NULL ;
}

int main()
{
   struct timeval tod1, tod2;
   clock_t t1, t2;
    int k = 0 ;

    t1 = clock();
   // Slurp CPU for 1 second. 
   gettimeofday(&tod1, NULL);
    gettimeofday(&tod2, NULL);

    int count1=0, count2=0 ;
    run=1 ;
   pthread_create(&threadid1, NULL, thread1, &count1);

    int policy ;
    struct sched_param param ;
    int r = pthread_getschedparam(threadid1, &policy, &param );

    
    sched_
    
   pthread_create(&threadid2, NULL, thread2, &count2);

    sleep(100);
/*
    do 
   {
       k++ ;
       if( k%1000000 == 0 )
      gettimeofday(&tod2, NULL);
   } while(toddiff(&tod2, &tod1) < 1000000);
*/

    run=0 ;
    pthread_join(threadid1, NULL);
    pthread_join(threadid2, NULL);
    
   t2 = clock();
   gettimeofday(&tod2, NULL);
    float df = toddiff(&tod2, &tod1)  ;
    df/=1000000.0 ;
   printf("timeofday %5.2f seconds, clock %5.2f seconds k=%d\n", df, (t2-t1)/(double)CLOCKS_PER_SEC, k);
   printf("count1 %d, count2 %d\n", count1, count2 );

   return 0;
}
