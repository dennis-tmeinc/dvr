
#include <stdlib.h>

#include "LzmaDec.h"

// All in memory decoder, backward comp

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree } ;

int lzmadecsize( unsigned char * lzmabuf )
{
    UInt64 unpackSize;
    int i;

    unpackSize = 0;
    for (i = 0; i < 8; i++)
        unpackSize += (UInt64)lzmabuf[LZMA_PROPS_SIZE + i] << (i * 8);

    if( unpackSize > 0 ) {
        return (int)unpackSize ;
    }
    else {
        return 0 ;
    }
}

int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int lzmaoutsize )
{
    SRes res ;
    SizeT destlen = (SizeT)lzmaoutsize ;
    SizeT srclen  = (SizeT)lzmasize - (LZMA_PROPS_SIZE+8) ;
    ELzmaStatus status ;
    
    res = LzmaDecode(lzmaoutbuf, &destlen, (const Byte *)lzmabuf+LZMA_PROPS_SIZE+8, &srclen,
                     (const Byte *)lzmabuf, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &g_Alloc );

    if( res == SZ_OK ) {
        return (int)destlen ;
    }
    else {
        return 0 ;
    }
}
