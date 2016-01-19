
#include <stdio.h>
#include <stdlib.h>

#include "LzmaEnc.h"

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree } ;

int lzmaenc( unsigned char * lzmabuf, int lzmasize, unsigned char * src, int srcsize )
{
    SRes res ;
    SizeT destlen = (SizeT)lzmasize-LZMA_PROPS_SIZE ;
    CLzmaEncProps props ;
    SizeT propsSize ;

    if( destlen<10 || srcsize<16 ) 
        return 0;
    
    LzmaEncProps_Init(&props);
    props.dictSize = 1<<19 ;			// 512k dic size
    propsSize = LZMA_PROPS_SIZE ;

    res = LzmaEncode((Byte *)lzmabuf+LZMA_PROPS_SIZE, &destlen ,
                     (const Byte *)src, (SizeT)srcsize ,
                     &props, (Byte *)lzmabuf, &propsSize, 0, NULL, &g_Alloc, &g_Alloc );

    if( res == SZ_OK ) {   
        return destlen+LZMA_PROPS_SIZE ;
    }
    else {
        return 0 ;
    }
}
