
#include <sys/mman.h> 
#include "dvr.h"

static pthread_mutex_t mem_mutex;

// Parse /proc/meminfo
static int MemTotal, MemFree, Cached, Buffers, Inactive, SwapTotal, SwapFree;
static int ParseMeminfo()
{
    FILE *fproc=NULL;
    char buf[256];
    int rnum = 0;
    fproc = fopen("/proc/meminfo", "r");
    if (fproc == NULL)
        return 0;
    while (fgets(buf, 256, fproc)) {
        if (memcmp(buf, "MemTotal:", 9) == 0) {
            rnum += sscanf(buf + 9, "%d", &MemTotal);
        } else if (memcmp(buf, "MemFree:", 8) == 0) {
            rnum += sscanf(buf + 8, "%d", &MemFree);
        } else if (memcmp(buf, "Cached:", 7) == 0) {
            rnum += sscanf(buf + 7, "%d", &Cached);
        } else if (memcmp(buf, "Buffers:", 8) == 0) {
            rnum += sscanf(buf + 8, "%d", &Buffers);
        } else if (memcmp(buf, "SwapTotal:", 10) == 0) {
            rnum += sscanf(buf + 10, "%d", &SwapTotal);
        } else if (memcmp(buf, "SwapFree:", 9) == 0) {
            rnum += sscanf(buf + 9, "%d", &SwapFree);
        } else if (memcmp(buf, "Inactive:", 9) == 0) {
            rnum += sscanf(buf + 9, "%d", &Inactive);
        }
    }
    fclose(fproc);
    return rnum;
}

// return kbytes of available memory (Usable Ram)
int mem_available()
{
    if (ParseMeminfo() == 0)
        return 0;
    return MemFree + Inactive ;
}

// dump cache memory, hope to free up main memory
//		"echo 1 > /proc/sys/vm/drop_caches"
void mem_dropcaches()
{
	FILE * f=fopen( "/proc/sys/vm/drop_caches", "w" );
	if( f ) {
		fwrite( "3", 1, 1, f );
		fclose( f );
	}
}

void *mem_cpy(void *dst, void const *src, size_t len)
{
	return memcpy( dst, src, len );
}

static int mem_alloc_st = 0 ;

#define MEM_TAG( p )	(*(((int *)p)-1))
#define MEM_REF( p )	(*(((int *)p)-2))

void *mem_alloc(int size)
{
    int *pmemblk;
    
	if (size < 0) {
        size=0 ;
    }
    
    // experiments, drop caches once a while !
    if( mem_alloc_st > 50000000 ) {
		mem_dropcaches();
		mem_alloc_st = 0 ;
	}
	mem_alloc_st += size ;
    
    pmemblk = (int *) malloc(size+2*sizeof(int));
    if (pmemblk == NULL) {
        // no enough memory error
        dvr_log("!!!!!mem_alloc failed!");
        return NULL;
    }
    pmemblk+=2 ;						// 2 integer over head
    MEM_TAG( pmemblk ) = (int)pmemblk ;	// memory tag
    MEM_REF( pmemblk ) = 1 ;			// reference counter
    return (void *)pmemblk;
}

// check if this memory is allock by mem_alloc
int mem_check(void *pmem)
{
    return pmem != NULL && MEM_TAG( pmem ) == (int)pmem && MEM_REF( pmem )>0 && MEM_REF( pmem )<100 ;
}

void mem_free(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL)
        return;
    
    pthread_mutex_lock(&mem_mutex);
    if( mem_check( pmem ) ) {
		if( --MEM_REF( pmem ) <= 0 ) {
			MEM_TAG(pmem)=0 ;		// clear memory tag
			free( & MEM_REF( pmem ) ) ;
		}
	}
    pthread_mutex_unlock(&mem_mutex);
}

void *mem_addref(void *pmem, int memsize)
{
    pthread_mutex_lock(&mem_mutex);
    if( mem_check( pmem ) ) {
		MEM_REF(pmem)++ ;
	}
	else if( memsize>0 ){
		void * nmem = mem_alloc(memsize) ;
		if( nmem ) {
			mem_cpy( nmem, pmem, memsize );
			pmem = nmem ;
		}
		else {
			pmem = NULL ;
		}
	}
	else {
		pmem = NULL ;
	}
    pthread_mutex_unlock(&mem_mutex);
    return pmem;
}

void mem_init()
{
    pthread_mutex_init(&mem_mutex, NULL);
}

void mem_uninit()
{
    pthread_mutex_destroy(&mem_mutex);
}
