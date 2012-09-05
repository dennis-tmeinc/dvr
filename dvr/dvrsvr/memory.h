
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
void * mem_shm_init(const char * shm_file, int shm_size);
void * mem_shm_share(const char * shm_file);
char * mem_shm_filename();
void mem_shm_close();
void * mem_shm_alloc(int size);
int mem_shm_index(void * pmem);
void * mem_shm_byindex(int index);

#endif      // __MEMORY_H__
