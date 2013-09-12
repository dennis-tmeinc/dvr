
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <errno.h>

// atomically swap value
/*
    result=*m ;
    *m = v ;
    return result ;
*/
static inline int atomic_swap( int v, int *m )
{
    asm volatile (
                "swp %0, %0, [%1]\n\t"
                : "+r" (v)
                : "r" (m)
                ) ;
    return v ;
}

int main()
{
    int a, b ;
    a = 555 ;

    b =  atomic_swap( 666, &a );

//    b = atomic_add(1, &at) ;

//	b = __sync_fetch_and_add( &a, 1 );
    printf("a=%d, b=%d\n", a, b );

    b =  atomic_swap(777, &a);

    printf("a=%d, b=%d\n", a, b );
    return 0;
}
