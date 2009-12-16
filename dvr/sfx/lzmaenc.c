
#include <stdlib.h>

#include "../lzmasdk/LzmaEnc.h"

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree } ;

int lzmaenc( unsigned char * lzmabuf, int lzmasize, unsigned char * src, int srcsize )
{
    SRes res ;
    SizeT destlen = (SizeT)lzmasize-LZMA_PROPS_SIZE-8 ;
    CLzmaEncProps props ;
    SizeT propsSize ;
    int i;

    if( destlen<10 || srcsize<16 ) 
        return 0;
    
    LzmaEncProps_Init(&props);
    propsSize = LZMA_PROPS_SIZE ;
    
    res = LzmaEncode((Byte *)lzmabuf+LZMA_PROPS_SIZE+8, &destlen ,
                     (const Byte *)src, (SizeT)srcsize ,
                     &props, (Byte *)lzmabuf, &propsSize, 0, NULL, &g_Alloc, &g_Alloc );

    if( res == SZ_OK ) {   
        for (i = 0; i < 8; i++)
            lzmabuf[propsSize++] = (Byte)(srcsize >> (8 * i));
        return destlen+LZMA_PROPS_SIZE+8 ;
    }
    else {
        return 0 ;
    }
}
