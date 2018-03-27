
#include <sys/mman.h> 
#include "dvr.h"

int g_memfree = 0;  			// free memory in kb
int g_memdirty = 0 ;			// dirty memory 

static pthread_mutex_t mem_mutex;

void mem_dropcache() 
{
    FILE * fdropcache ;
    fdropcache = fopen("/proc/sys/vm/drop_caches", "w");
    if( fdropcache ) {
		fwrite( "3\n", 1, 2, fdropcache );
		fclose( fdropcache );
    }
}

// return kbytes of available memory (Usable Ram)
int mem_available()
{
	int memfree = 0 ;
	int m ;
    FILE * fmem ;
    char buf[256];
    fmem = fopen("/proc/meminfo", "r");
    if( fmem ) {
		while (fgets(buf, 256, fmem)) {
			if (memcmp(buf, "MemFree:", 8) == 0) {
				sscanf(buf + 8, "%d", &m);
				memfree += m ;
			}
			else if (memcmp(buf, "Inactive:", 9) == 0) {
				sscanf(buf + 9, "%d", &m);
				if( m > 5000 && memfree<30000 ) 
					mem_dropcache();
				memfree += m ;
			}
			else if (memcmp(buf, "Dirty:", 6) == 0) {
				sscanf(buf + 6, "%d", &m);
				g_memdirty = m ;
			}			
		}
		fclose( fmem );
    }
    g_memfree = memfree ;
    return memfree ;
}

#define MEM_TAG( p )	(*(((int *)p)-1))
// high byte: refcount, lower 24bits: blocksize, 0: malloc, >0 mmap
#define MEM_REF( p )	(*(((int *)p)-2))
#define MEM_SIZ( p )	( MEM_REF(p)  & 0x00ffffff )
#define MEM_ADDREF( p ) ( MEM_REF(p) += 0x01000000 )
#define MEM_DECREF( p ) ( MEM_REF(p) -= 0x01000000 )
#define MEM_CHECK( p )  ( MEM_TAG( p ) == (int)(p) )

void *mem_alloc(int size)
{
	if (size < 0 || size>0x00800000 ) {
		return NULL ;
    }
    
    int *pmemblk;
    size = (size/sizeof(int) + 3) * sizeof(int) ;
    
    if( size>8000 ) {
		pmemblk = (int*) mmap( NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
		if( pmemblk ) *pmemblk = size ;
	}
	else {
		pmemblk = (int *) malloc( size );
		if( pmemblk ) *pmemblk = 0 ;
	}
    if (pmemblk == NULL) {
        // no enough memory error
        dvr_log("!!!!!mem_alloc failed!");
        return NULL;
    }
    pmemblk+=2 ;						// 2 integer over head
    MEM_TAG( pmemblk ) = (int)pmemblk ;	// memory tag
    // MEM_REF( pmemblk ) = 0 ;			// reference counter initialize to 0
    return (void *)pmemblk;
}

// check if this memory is allock by mem_alloc
int mem_check(void *pmem)
{
    return MEM_CHECK( pmem ) ;
}

void mem_free(void *pmem)
{
    if (pmem == NULL)
        return;
    pthread_mutex_lock(&mem_mutex);
    if( MEM_CHECK( pmem ) ) {
		if( MEM_DECREF( pmem ) < 0 ) {
			MEM_TAG(pmem)=0 ;		// clear memory tag
			int siz = MEM_SIZ( pmem )	;
			if( siz > 0 ) { 		// mmap
				munmap( & MEM_REF( pmem ), siz );
			}
			else {
				free( & MEM_REF( pmem ) ) ;
			}
		}
	}
    pthread_mutex_unlock(&mem_mutex);
}

// optimized memory copy (to be used in mem_addref only)
inline void mem_cp32( int *dst, int *src, int len )
{
	len = (len+(sizeof(int)-1))/sizeof(int) ;
	do {
		dst[len-1] = src[len-1] ;
	}
	while( --len ) ;
}

void *mem_clone(void *pmem, int memsize)
{
	void * nmem = mem_alloc(memsize) ;
	if( nmem ) {
		if( (((int)pmem)&3)==0 ) {
			mem_cp32( (int*)nmem, (int *)pmem, memsize ) ;
		}
		else {
			memcpy( (void *)nmem, (void *)pmem, memsize );
		}
	}
	return nmem ;
}

void *mem_addref(void *pmem, int memsize)
{
    pthread_mutex_lock(&mem_mutex);
    if( MEM_CHECK( pmem ) ) {
		MEM_ADDREF(pmem) ;
		pthread_mutex_unlock(&mem_mutex);
		return pmem ;
	}
	else {
		pthread_mutex_unlock(&mem_mutex) ;
		return mem_clone(pmem, memsize);
	}
}

void mem_init()
{
	mem_mutex = mutex_init ;
}

void mem_uninit()
{
    pthread_mutex_destroy(&mem_mutex);
}
