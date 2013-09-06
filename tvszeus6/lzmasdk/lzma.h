
#ifndef __LZMA_H__
#define __LZMA_H__

#include <stdlib.h>
#include "LzmaDec.h"
#include "LzmaEnc.h"

int lzmadecsize( unsigned char * lzmabuf );
int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int lzmaoutsize );
int lzmaenc( unsigned char * lzmabuf, int lzmasize, unsigned char * src, int srcsize );

#endif  // __LZMA_H__
