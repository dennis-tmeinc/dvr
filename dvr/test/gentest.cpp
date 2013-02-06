
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

double gettime()
{
    struct timespec ts ;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double)ts.tv_nsec/1000000000.0 ;
}

int main()
{
    int i, j ;
    double t1, t2 ;
    char * memp ;
    memp=new char[1000000] ;
    t1 = gettime();
    for(i=0;i<130;i++) {
        memp[0] = i ;
        for(j=1;j<1000000;j++) {
            memp[j]=memp[j-1] ;
        }
    }
    t2=gettime();
    printf("mem test time : %f s\n", (t2-t1)) ;
    return 0;
}
