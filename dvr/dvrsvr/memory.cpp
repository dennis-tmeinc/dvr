
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>

#include "memory.h"

volatile int g_memdirty = 0;             // kb
volatile int g_memfree = 0;              // kb
volatile int g_memused = 0 ;

/*
struct mem_block {
    int     id;             // size of block on shared heap
    int     refcnt ;
    int     tag;
} ;

#define MEM_HEADERSIZE       ( sizeof(struct mem_block)/sizeof(int) )
#define MEM_TAG( m )       ( ((struct mem_block *)(((int*)(m))-MEM_HEADERSIZE))->tag )
#define MEM_SIZE( m )      ( ((struct mem_block *)(((int*)(m))-MEM_HEADERSIZE))->id  )
#define MEM_REFCOUNT( m )  ( ((struct mem_block *)(((int*)(m))-MEM_HEADERSIZE))->refcnt )
*/

#define MEM_HEADERSIZE      ( 3 )
#define MEM_TAG( m )        ( ((int*)(m))[-3] )
#define MEM_ID( m )         ( ((int*)(m))[-2] )
#define MEM_SIZE( m )       ( MEM_ID( m ) )
#define MEM_REFCOUNT( m )   ( ((int*)(m))[-1] )
#define MEM_HEADER( m )     ( ((int*)(m))-MEM_HEADERSIZE )

#define MEM_TAG_LOCAL  (0x47eb530f)
#define MEM_TAG_SHARE  (0x2d421056)
#define MEM_TAG_FREE   (0x5348fd20)
#define MEM_SHARE_FIRSTBLOCK (8)

static pthread_mutex_t mem_mutex;
inline void mem_lock()
{
    pthread_mutex_lock(&mem_mutex);
}

inline void mem_unlock()
{
    pthread_mutex_unlock(&mem_mutex);
}

void mem_available()
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
    if( ((unsigned long)dest&3)==0 &&
        ((unsigned long)src&3)==0 )
    {
        // use 32 bits copy would be much faster than memcpy()
        unsigned long * dst32 = (unsigned long *)dest;
        const unsigned long * src32 = (const unsigned long * )src;
        int i = count/sizeof(unsigned long);
        while (i-->0) {
            *dst32++ = *src32++;
        }
        i = count%sizeof(unsigned long);
        while(i-->0) {
            ((unsigned char *)dst32)[i] = ((unsigned char *)src32)[i] ;
        }
    }
    else {
        // memory not aligned
        memcpy(dest,src,count);
    }
}

void *mem_alloc(int size)
{
    if (size < 4) {
        size = 4 ;
    }
    int * m = (int *)malloc( size+MEM_HEADERSIZE * sizeof(int)) ;
    if( m ) {
        m+=MEM_HEADERSIZE ;
        MEM_TAG(m) = MEM_TAG_LOCAL ;
        MEM_ID(m) = (int)m ;
        MEM_REFCOUNT(m) = 1 ;
        g_memused++ ;
        return (void *)m ;
    }
    return NULL ;
}

// check if this memory is allocated by mem_alloc
static int mem_check(void *pmem)
{
    if ( (((unsigned long)pmem)&3)!=0  || pmem==NULL )
        return 0;
    if( MEM_TAG(pmem)==MEM_TAG_LOCAL && MEM_ID(pmem) == (int)pmem ) {
        return 1 ;
    }
    else {
        return 0 ;
    }
}

void mem_free(void *pmem)
{
    if( mem_check( pmem ) ) {
        mem_lock();
        if( --MEM_REFCOUNT(pmem) <= 0 ) {
            MEM_TAG( pmem ) = MEM_TAG_FREE ;
            free(MEM_HEADER(pmem)) ;
            g_memused-- ;			// for debugging
        }
        mem_unlock();
    }
}

void *mem_addref(void *pmem, int size)
{
    if( mem_check( pmem ) ) {
        mem_lock();
        MEM_REFCOUNT(pmem)++ ;
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
    if( mem_check(pmem) ) {
        return MEM_REFCOUNT(pmem) ;
    }
    else {
        return 0;
    }
}

void mem_init()
{
    // initial mutex
    pthread_mutex_init(&mem_mutex, NULL);
    mem_available();
    return;
}

void mem_uninit()
{
    pthread_mutex_destroy(&mem_mutex);
    return ;
}


// shared heap support
static inline void mem_shm_lock(void * shmem)
{
    while( arm_atomic_swap(1, (int *)shmem) ) {
        sched_yield();
    }
}

static void mem_shm_unlock(void * shmem)
{
    *((int *)shmem)=0 ;         // 32bit int assignment is atomic already
}

void * mem_shm_init(char * shm_file, int shmsize)
{
    // init shared memory
    void * sharedmem = NULL ;
    int shm_fd = open(shm_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if( shm_fd>0 ) {
        shmsize &= 0x3ffffff0 ;
        ftruncate(shm_fd, shmsize);
        sharedmem = (char *)mmap(0, shmsize, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if( sharedmem ) {
            // init heap
            int * shmem = (int *)sharedmem ;
            shmem[0] = 0 ;                            // lock
            shmem[1] = shmsize/sizeof(int) ;          // total heap size in numbers of int

            // init first empty block
            int * fblk = shmem+MEM_SHARE_FIRSTBLOCK ;
            MEM_SIZE (fblk) = shmem[1] - MEM_SHARE_FIRSTBLOCK ;   // block size (in sizeof int)
            MEM_TAG(fblk) = MEM_TAG_FREE ;
            MEM_REFCOUNT(fblk) = 0 ;
        }
        close( shm_fd ) ;
    }
    return sharedmem ;
}

void * mem_shm_share(char * shm_file)
{
    // init shared memory
    void * sharedmem = NULL ;
    int shm_fd = open(shm_file, O_RDWR );
    if( shm_fd>0 ) {
        sharedmem = (void *)mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0); // map first page for testing
        if(sharedmem ) {
            int * shmem = (int *)sharedmem ;
            int shsize = shmem[1]*sizeof(int) ;
            if( shsize>100000 && shsize<20000000 &&
                ( MEM_TAG(shmem+MEM_SHARE_FIRSTBLOCK)==MEM_TAG_FREE || MEM_TAG(shmem+MEM_SHARE_FIRSTBLOCK)==MEM_TAG_SHARE ) )
            {
                munmap(sharedmem, 4096);
                sharedmem = (void *)mmap(0, shsize, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
                close( shm_fd ) ;
                return sharedmem ;
            }
            munmap(sharedmem, 4096);
        }
        close( shm_fd ) ;
    }
    return NULL ;
}

void mem_shm_close( void * shmem)
{
    munmap(shmem, ((int *)shmem)[1]*sizeof(int));
}

// check if this memory is allock by mem_alloc
static int mem_shm_check(void *pmem)
{
    if (pmem == NULL || (((unsigned long)pmem)&3)!=0 )
        return 0;
    return ( MEM_TAG(pmem) == MEM_TAG_SHARE ) ;
}

void * mem_shm_alloc(void * shmem, int size)
{
    // init heap
    int * psh = (int *)shmem ;
    mem_shm_lock( shmem ) ;

    // adjust size to number of int
    size = size/sizeof(int)+1  ;

    int * nextblock ;
    int * block = psh + MEM_SHARE_FIRSTBLOCK ;
    int * eblock = psh + psh[1] - MEM_HEADERSIZE ;      // (-MEM_HEADERSIZE) to leave a bit more room at the end

    while( block < eblock ) {
        if( MEM_TAG( block ) == MEM_TAG_FREE ){
            if( MEM_SIZE( block ) >= size  ){
                if( MEM_SIZE( block ) - size > MEM_HEADERSIZE+2 ) {     //  big enough to split into two blocks
                    // split this block into two
                    nextblock = block + size + MEM_HEADERSIZE ;
                    MEM_TAG( nextblock ) = MEM_TAG_FREE ;
                    MEM_REFCOUNT( nextblock ) = 0 ;
                    MEM_SIZE( nextblock ) = MEM_SIZE( block ) - size - MEM_HEADERSIZE ;

                    // adjust block size
                    MEM_SIZE( block ) = size ;
                }

                MEM_TAG( block ) = MEM_TAG_SHARE ;
                MEM_REFCOUNT( block ) = 1 ;                 // refs = 1

                mem_shm_unlock(shmem);
                return (void *) block ;                     // success
            }

            nextblock = block + MEM_SIZE(block) + MEM_HEADERSIZE ;
            if( nextblock >= eblock ) {
                break ;         // no more memory
            }

            if( MEM_TAG( nextblock ) == MEM_TAG_FREE ) {
                // merge
                MEM_SIZE(block) += MEM_SIZE( nextblock ) + MEM_HEADERSIZE ;
            }
            else {
                block = nextblock ;
            }
        }
        else if( MEM_TAG( block ) == MEM_TAG_SHARE ) {
            block += MEM_HEADERSIZE + MEM_SIZE(block) ;
        }
        else {
            // error, block link corrupted
            break;
        }
    }

    mem_shm_unlock(shmem);
    return NULL ;

}

void * mem_shm_addref(void * shmem, void * pmem, int size)
{
    if( mem_shm_check( pmem ) ) {
        mem_shm_lock( shmem );
        ++ MEM_REFCOUNT( pmem );
        mem_shm_unlock( shmem );
        return pmem ;
    }
    else {
        void * nmem = mem_shm_alloc(shmem, size ) ;
        mem_cpy32( nmem, pmem, size );
        return nmem ;
    }
}

void mem_shm_free(void * shmem, void * pmem)
{
    if( mem_shm_check( pmem ) ) {
        mem_shm_lock( shmem ) ;
        if( -- MEM_REFCOUNT( pmem ) <= 0 ) {
            MEM_TAG( pmem ) = MEM_TAG_FREE ;
        }
        mem_shm_unlock( shmem ) ;
    }
}


