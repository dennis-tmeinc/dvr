
#ifndef __MEMORY_H__
#define __MEMORY_H__

// atomic swap
/*
    result=*m ;
    *m = v ;
    return result ;
*/
inline int arm_atomic_swap( int v, int *m )
{
    asm volatile (
                " swp %0, %0, [%1]\n"
                : "+r" (v)
                : "r" (m)
                ) ;
    return v ;
}

// dvr memory allocations
extern volatile int g_memfree;
extern volatile int g_memdirty;
extern volatile int g_memused;

void mem_available() ;
// copy aligned 32 bit memory.
void mem_cpy32(void *dest, const void *src, size_t count);
void *mem_alloc(int size);
//int mem_check(void *pmem);
void mem_free(void *pmem);
void *mem_addref(void *pmem, int size);
int mem_refcount(void *pmem);
void mem_init();
void mem_uninit();

// shared memory support
void * mem_shm_init(char * shm_file, int shmsize);
void * mem_shm_share(char * shm_file);
void mem_shm_close( void * shmem);
void * mem_shm_alloc(void * shmem, int size);
void * mem_shm_addref(void * shmem, void * pmem, int size);
void mem_shm_free(void * shmem, void * pmem);

#endif      // __MEMORY_H__
