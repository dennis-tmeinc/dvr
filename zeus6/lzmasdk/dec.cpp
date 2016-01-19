
#include <stdlib.h>
#include <stdio.h>

#include "LzmaDec.h"

static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree } ;

/*
int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int lzmaoutsize )
{
    SRes res ;
    SizeT destlen = (SizeT)lzmaoutsize ;
    SizeT srclen  = (SizeT)lzmasize - LZMA_PROPS_SIZE ;
    ELzmaStatus status ;
    
    res = LzmaDecode(lzmaoutbuf, &destlen, (const Byte *)lzmabuf+LZMA_PROPS_SIZE, &srclen,
                     (const Byte *)lzmabuf, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &g_Alloc );

    if( res == SZ_OK ) {
        return (int)destlen ;
    }
    else {
        return 0 ;
    }
}
*/

static CLzmaDec *decEnv = NULL ;

// free decoder environment
void lzmadec_free()
{
	if( decEnv != NULL ) {
		LzmaDec_Free(decEnv, &g_Alloc);
		free(decEnv);
		decEnv=NULL ;
	}
}
 
// return 0: completed, >0 : source buffer used
int lzmadec_start( unsigned char * lzmabuf, int lzmasize )
{
	lzmadec_free() ;
	if( lzmasize<LZMA_PROPS_SIZE ) return 0 ;
	decEnv = (CLzmaDec *)malloc( sizeof(CLzmaDec) );
	LzmaDec_Construct(decEnv);
	RINOK(LzmaDec_Allocate(decEnv, (const Byte *)lzmabuf, LZMA_PROPS_SIZE, &g_Alloc ));
	LzmaDec_Init(decEnv);
	return LZMA_PROPS_SIZE ;
}

// return 0: completed, >0 : source buffer used, -1: error
int lzmadec_dec( unsigned char * lzmabuf, int lzmasize, unsigned char ** lzmaoutbuf, int * lzmaoutsize )
{
    SizeT inSize ;
    SRes res;
    ELzmaStatus status ;
    
	* lzmaoutsize = 0 ;					// not output yet
	if( decEnv == NULL ) {
		return lzmadec_start(lzmabuf,lzmasize);
	}
			
    if( lzmasize<=0 ) {
		// end decoding
		* lzmaoutbuf = (unsigned char *) decEnv->dic ;
		* lzmaoutsize = (int) decEnv->dicPos ;
		return 0 ;
	}
	inSize = (SizeT)lzmasize ;
    res = LzmaDec_DecodeToDic(decEnv, decEnv->dicBufSize, (const Byte *)lzmabuf, &inSize, LZMA_FINISH_ANY, &status);
	if( res != SZ_OK ) {
		return -1 ;
	}
    if (decEnv->dicPos == decEnv->dicBufSize) {
		* lzmaoutbuf = (unsigned char *) decEnv->dic ;
		* lzmaoutsize = (int) decEnv->dicPos ;
		decEnv->dicPos = 0;
	}
    return (int)inSize ;
}
