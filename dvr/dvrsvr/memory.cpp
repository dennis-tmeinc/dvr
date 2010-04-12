
#include <sys/mman.h> 
#include "dvr.h"

int g_memdirty = 0;

int g_memused = 0 ;

#define MEMTAG	0x462021dc

static pthread_mutex_t mem_mutex;
static void mem_lock()
{
    pthread_mutex_lock(&mem_mutex);
}

static void mem_unlock()
{
    pthread_mutex_unlock(&mem_mutex);
}

// Parse /proc/meminfo
static int MemTotal, MemFree, Cached, Buffers, SwapTotal, SwapFree;
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
        } else if (memcmp(buf, "Dirty:", 6) == 0) {
            rnum += sscanf(buf + 6, "%d", &g_memdirty);
        }
    }
    fclose(fproc);
    return rnum;
}

// return kbytes of available memory (Usable Ram)
int mem_available()
{
    int memfree;
    if (ParseMeminfo() == 0)
        return 0;
//	memfree = MemFree + Cached + Buffers + SwapFree - SwapTotal;
    memfree = MemFree + Cached + Buffers ;
//    if (memfree < MemFree)
//        memfree = MemFree;
    return memfree;
}

#define MEM_ALLOC_MMAP_SIZE (0x4000) 

void *mem_alloc(int size)
{
    int *pmemblk;
    
    if (size < 0) {
        size=0 ;
    }
    size+=12 ;
/*    
    if( size >= MEM_ALLOC_MMAP_SIZE ) {
        pmemblk = (int *) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );;
    }
    else {
        pmemblk = (int *) malloc(size);
    }
*/
    pmemblk = (int *) malloc(size);
    if (pmemblk == NULL) {
        // no enough memory error
        dvr_log("!!!!!mem_alloc failed!");
        return NULL;
    }
    pmemblk[0] = size ;		    // block size
    pmemblk[1] = 1 ;			// reference counter
    pmemblk[2] = MEMTAG	; 		// memory tag
    g_memused++;
    return (void *)(&pmemblk[3]);
}

void mem_free(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL)
        return;

    mem_lock();
    pmemblk = (int *)pmem ;
    pmemblk-=3 ;
    if( pmemblk[2]==MEMTAG ) {
        if( --(pmemblk[1]) <=0 ) {	// reference counter = 0
            g_memused--;
            pmemblk[2]=0;		// clear memory tag
            free( (void *)pmemblk );
/*
            if( pmemblk[0] >= MEM_ALLOC_MMAP_SIZE ) {
                munmap( (void *)pmemblk, pmemblk[0] );
            }
            else {
                free( (void *)pmemblk );
            }
*/             
        }
    }
    mem_unlock();
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
    
    mem_lock();
    pmemblk = (int *) pmem;
    if (*--pmemblk != MEMTAG){
        mem_unlock();
        return NULL;			// not an allocated memory block
    }
    (*--pmemblk)++;
    mem_unlock();
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

// copy aligned 32 bit memory.
void mem_cpy32(void *dest, const void *src, size_t count) 
{
    if( (count&3) ||
       ((unsigned long)dest&3) || 
       ((unsigned long)src&3) ) 
    {
        memcpy(dest,src,count);
    }
    else {
        long * dst32 = (long *)dest;
        const long * src32 = (const long * )src;
        count /= sizeof(long);
        while (count--) {
            *dst32++ = *src32++;
        }
    }
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

