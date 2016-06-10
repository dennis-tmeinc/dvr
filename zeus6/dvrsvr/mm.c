

void mem_cp32( int *dst, int *src, int len )
{
	len = (len+(sizeof(int)-1))/sizeof(int) ;
	do {
		dst[len-1] = src[len-1] ;
	}
	while( --len ) ;
}

