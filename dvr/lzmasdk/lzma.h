
#ifndef __LZMA_H__
#define __LZMA_H__

#include <stdlib.h>
#include "LzmaDec.h"
#include "LzmaEnc.h"

//int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int lzmaoutsize );
int lzmaenc( unsigned char * lzmabuf, int lzmasize, unsigned char * src, int srcsize );

// free decoder environment
void lzmadec_free();
// return 0: completed, >0 : source buffer used
int lzmadec_start( unsigned char * lzmabuf, int lzmasize );
// return 0: completed, >0 : source buffer used, -1: error
int lzmadec_dec( unsigned char * lzmabuf, int lzmasize, unsigned char ** lzmaoutbuf, int * lzmaoutsize );

#endif  // __LZMA_H__
