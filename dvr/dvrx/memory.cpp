
#include <sys/mman.h> 
#include "dvr.h"

int g_memdirty = 0;             // kb
int g_memfree = 0;              // kb
int g_memused = 0 ;

static pthread_mutex_t mem_mutex;
inline void mem_lock()
{
    pthread_mutex_lock(&mem_mutex);
}

inline void mem_unlock()
{
    pthread_mutex_unlock(&mem_mutex);
}

void mem_check()
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
    g_memfree = memfree ;
}

void *mem_alloc(int size)
{
    int *pmemblk;
    
    if (size < 0) {
        size=0 ;
    }
    size += 8 ;
    mem_lock();
    pmemblk = (int *) malloc(size);
    if (pmemblk == NULL) {
        // no enough memory error
        dvr_log("!!!!!mem_alloc failed!");
        mem_unlock();
        return NULL;
    }
    pmemblk[0] = (int)(&pmemblk[2]);    // memory tag
    pmemblk[1] = 1 ;			        // reference counter
    g_memused++;                        // for debug
    mem_unlock();
    return (void *)(&pmemblk[2]);
}

// check if this memory is allock by mem_alloc
int mem_check(void *pmem)
{
    if (pmem == NULL || (((int)pmem)&3)!=0 )
        return 0;
    return *(((int *)pmem)-2) == (int)pmem ;
}

void mem_free(void *pmem)
{
    int *pmemblk;
    if (pmem == NULL || (((int)pmem)&3)!=0 ) {
        dvr_log("!!!!memory release failed!");
        return;
    }

    mem_lock();
    pmemblk = ((int *)pmem)-2 ;
    if( pmemblk[0]==(int)pmem ) {
        if( --(pmemblk[1]) <=0 ) {	// reference counter = 0
            pmemblk[0]=0;		    // clear memory tag
            free((void *)pmemblk );
            g_memused--;
        }
        mem_unlock();
    }
    else {
        mem_unlock();
        dvr_log("!!!!memory release failed!");
    }
}

void *mem_ref(void *pmem, int size)
{
    int *pmemblk;
    mem_lock();
    if (pmem && (((int)pmem)&3)==0 ){
        pmemblk = ((int *)pmem)-2 ;
        if (pmemblk[0] == (int)pmem){
            pmemblk[1]++ ;
            mem_unlock();
            return pmem ;
        }
    }
    mem_unlock();
    // allock a new memory and copy contents   
    pmemblk = (int *)mem_alloc(size) ;
    mem_cpy32( pmemblk, pmem, size );
    return (void *)pmemblk ;
}

int mem_refcount(void *pmem)
{
    int *pmemblk;
    int refcount=0 ;
    if (pmem == NULL || (((int)pmem)&3)!=0 )
        return 0 ;
    mem_lock();
    pmemblk = ((int *)pmem)-2 ;
    if (pmemblk[0] == (int)pmem){
        refcount=pmemblk[1] ;
    }
    mem_unlock();
    return refcount ;
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
    pthread_mutex_init(&mem_mutex, NULL);
    return;
}

void mem_uninit()
{
    pthread_mutex_destroy(&mem_mutex);
    return ;
}

