
#include <stdio.h>

#include <asm/atomic.h>

int main()
{
	int a, b ;
	a = 0 ;
	printf("hello\n");

	b = atomic_add_return(1, (atomic_t *)&a) ;
		
//	b = __sync_fetch_and_add( &a, 1 );
	printf("a=%d, b=%d\n", a, b );
	b = atomic_add_return(1, (atomic_t *)&a) ;
	printf("a=%d, b=%d\n", a, b );
	return 0;
}