
#include <sys/mman.h> 
#include "dvr.h"


#define MEMTAG	0x462021dc

pthread_mutex_t g_mem_mutex;

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
    memfree = MemFree + Cached + Buffers ;
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
    //fprintf(stderr, "xyk-alloc:%p\n",pmemblk); 
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
	//fprintf(stderr, "xyk-free:%p\n",pmemblk); 
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
#if 0
void *mem_cpy(void *dst, void const *src, size_t len)
{
  long *plDst = (long *)dst;
  long const *plSrc = (long const *)src;

  //fprintf(stderr, "mem_cpy:%p,%p\n",plSrc,plDst);

  if (!((unsigned int)plSrc % sizeof(int)) &&
      !((unsigned int)plDst  % sizeof(int))) {
    //fprintf(stderr, "mem_cpy2\n");
    while (len >= 4) {
      *plDst++ = *plSrc++;
      len -= 4;
    }
  }
  /*
  if (len >= 4) {
    fprintf(stderr, "mem_cpy3\n");
    exit(1);
  }
  */
  char *pcDst = (char *)plDst;
  char const *pcSrc = (char const *)plSrc;

  while (len--) {
    *pcDst++ = *pcSrc++;
  }

  return (dst);
}
#else
void *mem_cpy(void *dst, void const *src, size_t len)
{
  char *plDst = (char*)dst;
  char *plSrc = (char*)src;
  int left=len;
  
  int n=left/256;
  while(n>0){
    memcpy(plDst,plSrc,256);
    plDst+=256;
    plSrc+=256;
    left-=256;
    n--;
  }
  if(left>0){
    memcpy(plDst,plSrc,left); 
  }
  return (dst);
}
#endif