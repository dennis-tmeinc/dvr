
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>

int g_memdirty = 0;             // kb
int g_memfree = 0;              // kb
int g_memused = 0 ;

#define MEM_TAG (0x47eb530f)

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

// copy aligned 32 bit memory.
void mem_cpy32(void *dest, const void *src, size_t count)
{
    if( (count&3) ||
        ((unsigned long)dest&3) ||
        ((unsigned long)src&3) )
    {	// memory not aligned
        memcpy(dest,src,count);
    }
    else {
        // use 32 bits copy is much faster than memcpy()
        unsigned long * dst32 = (unsigned long *)dest;
        const unsigned long * src32 = (const unsigned long * )src;
        count /= sizeof(unsigned long);
        while (count--) {
            *dst32++ = *src32++;
        }
    }
}

struct mem_block {
    struct mem_block * blockad ;
    int    tag;
    int	   refcnt ;
} ;

void *mem_alloc(int size)
{
    char * pm = NULL ;
    struct mem_block * mb ;
    if (size < 4) {
        size = 4 ;
    }
    mem_lock();
    mb = (struct mem_block *)malloc( size+sizeof( struct mem_block ) );
    if( mb ) {
        mb->blockad = mb ;
        mb->tag = MEM_TAG ;
        mb->refcnt = 1 ;
        pm = ((char *)mb)+sizeof(struct mem_block) ;
        g_memused++ ;
    }
    mem_unlock();
    return pm ;
}

// check if this memory is allock by mem_alloc
int mem_check(void *pmem)
{
    struct mem_block * mb ;
    if (pmem == NULL || (((unsigned long)pmem)&3)!=0 )
        return 0;
    mb = (struct mem_block *)(((char *)pmem)-sizeof(struct mem_block)) ;
    if( mb->blockad == mb && mb->tag == MEM_TAG ) {
        return 1 ;
    }
    return 0 ;
}

void mem_free(void *pmem)
{
    struct mem_block * mb ;
    if( mem_check( pmem ) ) {
        mem_lock();
        mb = (struct mem_block *)(((char *)pmem)-sizeof(struct mem_block)) ;
        if( --(mb->refcnt) <= 0 ) {
            mb->tag = 0 ;			// remove tag
            free( mb ) ;
            g_memused-- ;			// for debugging
        }
        mem_unlock();
    }
}

void *mem_addref(void *pmem, int size)
{
    struct mem_block * mb ;
    if( mem_check( pmem ) ) {
        mem_lock();
        mb = (struct mem_block *)(((char *)pmem)-sizeof(struct mem_block)) ;
        ++(mb->refcnt);
        mem_unlock();
        return pmem ;
    }
    else {
        void * nmem = mem_alloc( size ) ;
        mem_cpy32( nmem, pmem, size );
        return nmem ;
    }
}

int mem_refcount(void *pmem)
{
    struct mem_block * mb ;
    if( mem_check( pmem ) ) {
        mb = (struct mem_block *)(((char *)pmem)-sizeof(struct mem_block)) ;
        return mb->refcnt;
    }
    else {
        return 0 ;
    }
}

void mem_init()
{
    // initial mutex
    pthread_mutex_init(&mem_mutex, NULL);
    mem_check();
    return;
}

void mem_uninit()
{
    pthread_mutex_destroy(&mem_mutex);
    return ;
}


