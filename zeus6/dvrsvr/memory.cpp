
#include <sys/mman.h> 
#include "dvr.h"


#define MEMTAG	0x462021dc

pthread_mutex_t g_mem_mutex;

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

// dump cache memory, hope to free up main memory
//		"echo 1 > /proc/sys/vm/drop_caches"
void mem_dropcaches()
{
	FILE * f=fopen( "/proc/sys/vm/drop_caches", "w" );
	if( f ) {
		fwrite( "1", 1, 1, f );
		fclose( f );
	}
}

// return kbytes of available memory (Usable Ram)
int mem_available()
{
    int memfree;
    if (ParseMeminfo() == 0)
        return 0;
    memfree = MemFree + Inactive ;
    return memfree;
}

void *mem_alloc(int size)
{
    int *pmemblk;
    
   if (size < 0) {
        size=0 ;
    }
    
    pmemblk = (int *) malloc(size+12);
    if (pmemblk == NULL) {
        // no enough memory error
        dvr_log("!!!!!mem_alloc failed!");
        return NULL;
    }
    pmemblk[0] = size+12 ;		// block size
    pmemblk[1] = 1 ;			// reference counter
    pmemblk[2] = MEMTAG	; 		// memory tag
    return (void *)(&pmemblk[3]);
}

void mem_free(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL)
        return;
    
    pthread_mutex_lock(&g_mem_mutex);
    pmemblk = (int *)pmem ;
    pmemblk-=3 ;
    if( pmemblk[2]!=MEMTAG ) {
      pthread_mutex_unlock(&g_mem_mutex);
        return ;			 // not an allocated memory block
    }
    if( --(pmemblk[1]) <=0 ) {	// reference counter = 0
        pmemblk[2]=0;		// clear memory tag
        free( (void *)pmemblk );
    }
    pthread_mutex_unlock(&g_mem_mutex);
}

// return memory block size
int mem_size(void * pmem)
{
    int *pmemblk;
    if (pmem == NULL)
        return 0 ;
    
    pmemblk = (int *)pmem ;
    pmemblk-=3 ;
    if( pmemblk[2]!=MEMTAG ) {
        return 0 ;
    }
    return pmemblk[0]-12 ;
}

void *mem_addref(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL){
        return NULL;
    }
    
    pthread_mutex_lock(&g_mem_mutex);
    pmemblk = (int *) pmem;
    if (*--pmemblk != MEMTAG){
      pthread_mutex_unlock(&g_mem_mutex);
        return NULL;			// not an allocated memory block
    }
    (*--pmemblk)++;
    pthread_mutex_unlock(&g_mem_mutex);
    return pmem;
}

int mem_refcount(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL)
        return 0 ;
    
    pmemblk = (int *)pmem ;
    pmemblk-=3 ;
    if( pmemblk[2]!=MEMTAG ) {
        return 0 ;
    }
    return pmemblk[1] ;
}

// check if this memory is allock by mem_alloc
int mem_check(void *pmem)
{
    if (pmem == NULL)
        return 0;
    return *(((int *)pmem)-1) == MEMTAG ;
}

void mem_init()
{
    pthread_mutex_init(&g_mem_mutex, NULL);
}

void mem_uninit()
{
    pthread_mutex_destroy(&g_mem_mutex);
}

void *mem_cpy(void *dst, void const *src, size_t len)
{
	return memcpy( dst, src, len );
}

