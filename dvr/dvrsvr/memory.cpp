
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

// return kbytes of available memory (Usable Ram)
int mem_available()
{
    int memfree=0;
    char buf[256];
    FILE * fproc = fopen("/proc/meminfo", "r");
    if (fproc) {
        while (fgets(buf, 256, fproc)) {
            char header[20] ;
            int  v ;
            if( sscanf( buf, "%19s%d", header, &v )==2 ) {
                if( strcmp( header, "MemFree:")==0 ) {
                    memfree+=v ;
                }
                else if( strcmp( header, "Inactive:")==0 ) {
                    memfree+=v ;
                }
                else if( strcmp( header, "Dirty:")==0 ) {
                    g_memdirty=v ;
                }
            }
        }
        fclose(fproc);
    }
    return memfree;
}

void *mem_alloc(int size)
{
    int *pmemblk;
    
    if (size < 0) {
        size=0 ;
    }
    size += 8 ;
    pmemblk = (int *) malloc(size);
    if (pmemblk == NULL) {
        // no enough memory error
        dvr_log("!!!!!mem_alloc failed!");
        return NULL;
    }
    pmemblk[0] = MEMTAG	; 		// memory tag
    pmemblk[1] = 1 ;			// reference counter
    g_memused++;
    return (void *)(&pmemblk[2]);
}

void mem_free(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL)
        return;

    mem_lock();
    pmemblk = (int *)pmem ;
    pmemblk-=2 ;
    if( pmemblk[0]==MEMTAG ) {
        if( --(pmemblk[1]) <=0 ) {	// reference counter = 0
            g_memused--;
            pmemblk[0]=0;		// clear memory tag
            free( (void *)pmemblk );
        }
    }
    mem_unlock();
}

void *mem_addref(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL){
        return NULL;
    }
    
    mem_lock();
    pmemblk = ((int *)pmem)-2 ;
    if (pmemblk[0] == MEMTAG){
        pmemblk[1]++ ;
        mem_unlock();
        return pmem;
    }
    else {
        mem_unlock();
        return NULL;			// not an allocated memory block
    }
}

int mem_refcount(void *pmem)
{
    int *pmemblk;
    int refcount=0 ;
    if (pmem == NULL)
        return 0 ;
    mem_lock();
    pmemblk = ((int *)pmem)-2 ;
    if (pmemblk[0] == MEMTAG){
        refcount=pmemblk[1] ;
    }
    mem_unlock();
    return refcount ;
}

// check if this memory is allock by mem_alloc
int mem_check(void *pmem)
{
    if (pmem == NULL)
        return 0;
    return *(((int *)pmem)-2) == MEMTAG ;
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

