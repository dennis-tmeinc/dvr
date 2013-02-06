
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

#define MEM_ISALIGN( m )    (((int)(m)&3)==0)

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
void mem_cpy32( void  *dest, const void *src, size_t count)
{
    if( ((unsigned long)dest&3)==0 &&
        ((unsigned long)src&3)==0 &&
        ((unsigned long)count&3)==0 )
    {
        unsigned long * dst32 = (unsigned long *)dest ;
        unsigned long * src32 = (unsigned long *)src ;
        count /= sizeof(unsigned long) ;
        while (count-->0) {
            *dst32++ = *src32++ ;
        }
    }
    else {
        // memory not aligned
        memcpy(dest,src,count);
    }
}

/*
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
*/

// shared memory management

static int * shmem ;                    // shared memory heap
static char  shmem_file[100] ;

// shared heap support
static inline void mem_shm_lock()
{
    while( arm_atomic_swap(1, shmem) ) {
        sched_yield();
    }
}

static inline void mem_shm_unlock()
{
    *shmem=0 ;         // 32bit int assignment is atomic already
}

char * mem_shm_filename()
{
    return shmem_file ;
}

void * mem_shm_init(const char * shm_file, int shm_size )
{
    if( shmem ) {
        return (void *)shmem ;
    }

    strcpy( shmem_file, shm_file );

    int shm_fd = open(shm_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
    if( shm_fd>0 ) {
        shm_size &= 0x3ffffff0 ;
        ftruncate(shm_fd, shm_size);
        shmem = (int *)mmap(0, shm_size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if( shmem ) {
            // init heap
            shmem[0] = 0 ;                            // lock
            shmem[1] = shm_size/sizeof(int) ;          // total heap size in numbers of int

            // init first empty block
            int * fblk = shmem+MEM_SHARE_FIRSTBLOCK ;
            MEM_SIZE (fblk) = shmem[1] - MEM_SHARE_FIRSTBLOCK ;   // block size (in sizeof int)
            MEM_TAG(fblk) = MEM_TAG_FREE ;
            MEM_REFCOUNT(fblk) = 0 ;
        }
        close( shm_fd ) ;
    }
    return (void *)shmem ;
}

void * mem_shm_share(const char * shm_file)
{
    if( shmem ) {
        return (void *)shmem ;
    }

    strcpy( shmem_file, shm_file );

    // init shared memory
    int shm_fd = open(shm_file, O_RDWR );
    if( shm_fd>0 ) {
        shmem = (int *)mmap(0, 16, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0); // map first page for testing
        if(shmem ) {
            int shsize = shmem[1]*sizeof(int) ;
            if( shsize>100000 && shsize<20000000 &&
                ( MEM_TAG(shmem+MEM_SHARE_FIRSTBLOCK)==MEM_TAG_FREE || MEM_TAG(shmem+MEM_SHARE_FIRSTBLOCK)==MEM_TAG_SHARE ) )
            {
                munmap(shmem, 16);
                shmem = (int *)mmap(0, shsize, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
                close( shm_fd ) ;

                printf("Shared memory : %s : %d\n", shm_file, shsize) ;

                return (void *)shmem ;
            }
            munmap(shmem, 4096);
        }
        close( shm_fd ) ;
    }
    shmem=NULL ;
    return NULL ;
}

void mem_shm_close()
{
    if( shmem ) {
        munmap(shmem, ((int *)shmem)[1]*sizeof(int));
        shmem=NULL ;
    }
}

void * mem_shm_alloc(int size)
{
    // init heap
    if( shmem==NULL ) {
        return NULL ;
    }
    mem_shm_lock() ;

    // adjust size to number of int
    size = size/sizeof(int)+1  ;

    int * nextblock ;
    int * block = shmem + MEM_SHARE_FIRSTBLOCK ;
    int * eblock = shmem + shmem[1] - MEM_HEADERSIZE ;      // (-MEM_HEADERSIZE) to leave a bit more room at the end

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

                mem_shm_unlock();
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

    mem_shm_unlock();
    return NULL ;

}

int mem_shm_index(void * pmem)
{
    return (int *)pmem - shmem ;
}

void * mem_shm_byindex(int index)
{
    if( index>=MEM_SHARE_FIRSTBLOCK && index<shmem[1] ) {
        int * p = &shmem[index] ;
        if( MEM_TAG(p) == MEM_TAG_SHARE ) {
            return p;
        }
    }
    return NULL ;
}


// local memory management

void *mem_alloc(int size)
{
    size = size/sizeof(int) + 1 ;
    int * m = (int *)malloc( (size+MEM_HEADERSIZE) * sizeof(int)) ;
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
static inline int mem_check(void *pmem)
{
    return ( MEM_TAG(pmem)==MEM_TAG_LOCAL && MEM_ID(pmem) == (int)pmem ) ;
}

// free memory ( local and shared )
void mem_free(void *pmem)
{
    if( MEM_TAG(pmem) == MEM_TAG_LOCAL &&  MEM_ID(pmem) == (int)pmem   ) {
        // free local memory
        mem_lock();
        if( --MEM_REFCOUNT(pmem) <= 0 ) {
            MEM_TAG( pmem ) = MEM_TAG_FREE ;
            free(MEM_HEADER(pmem)) ;
            g_memused-- ;			// for debugging
        }
        mem_unlock();
    }
    else if( MEM_TAG(pmem)==MEM_TAG_SHARE ) {
        // free shared memory
        mem_shm_lock() ;
        if( -- MEM_REFCOUNT( pmem ) <= 0 ) {
            MEM_TAG( pmem ) = MEM_TAG_FREE ;
        }
        mem_shm_unlock() ;
    }
}

// add memory reference count. (local and shared)
void *mem_addref(void *pmem, int size)
{
    if(  MEM_TAG(pmem)==MEM_TAG_LOCAL && MEM_ID(pmem) == (int)pmem  ) {       // is local ?
        mem_lock();
        MEM_REFCOUNT(pmem)++ ;
        mem_unlock();
        return pmem ;
    }
    else if( MEM_TAG(pmem) == MEM_TAG_SHARE ) {
        mem_shm_lock() ;
        MEM_REFCOUNT(pmem)++ ;
        mem_shm_unlock() ;
        return pmem ;
    }
    void * nmem = mem_alloc( size ) ;
    mem_cpy32( nmem, pmem, size );
    return nmem ;
}

int mem_refcount(void *pmem)
{
    if( MEM_TAG(pmem)==MEM_TAG_LOCAL || MEM_TAG(pmem)==MEM_TAG_SHARE ) {
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
    shmem = NULL ;
    return;
}

void mem_uninit()
{
    mem_shm_close();
    pthread_mutex_destroy(&mem_mutex);
    return ;
}


