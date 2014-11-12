
#include <stdlib.h>

#include "LzmaDecode.h"

int lzmadecsize( unsigned char * lzmabuf )
{
  UInt32 outSize = 0;
  CLzmaDecoderState state; 

  /* Decode LZMA properties */
  if (LzmaDecodeProperties(&state.Properties, lzmabuf, LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK) {
    return 0 ;		// "Incorrect stream properties"
  }

  /* Read uncompressed size */
  int i;
  for (i = 0; i < 4; i++)
  {
      outSize += (UInt32)(lzmabuf[LZMA_PROPERTIES_SIZE+i]) << (i*8) ;
  }
  if( outSize > 0x10000000 ) {		// maximum size 256Mb
      return 0 ;	// size too big
  }
  return (int)outSize ;
}

int lzmadec( unsigned char * lzmabuf, int lzmasize, unsigned char * lzmaoutbuf, int * lzmaoutsize )
{
  UInt32 outSize = 0;
  CLzmaDecoderState state; 
  int res;
  SizeT outSizeFull;
  unsigned char *outStream;
  SizeT compressedSize;
  unsigned char *inStream;
  SizeT inProcessed;
  SizeT outProcessed;

  /* Decode LZMA properties */
  if (LzmaDecodeProperties(&state.Properties, lzmabuf, LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK) {
    return 0 ;		// "Incorrect stream properties"
  }

  /* Read uncompressed size */
  int i;
  for (i = 0; i < 4; i++)
  {
      outSize += (UInt32)(lzmabuf[LZMA_PROPERTIES_SIZE+i]) << (i*8) ;
  }

  if( (int)outSize > *lzmaoutsize ) {
	outSize = (UInt32) *lzmaoutsize ;
  }

  if( outSize<5 || outSize > 0x10000000 ) {		// maximum size 256Mb
      return 0 ;	// size too big, or too small
  }

  if( (int)outSize > *lzmaoutsize ) {
	outSize = (UInt32) *lzmaoutsize ;
  }

  compressedSize = (SizeT)(lzmasize - (LZMA_PROPERTIES_SIZE + 8));
  outSizeFull = (SizeT)outSize;

  /* allocate memory */
  state.Probs = (CProb *)malloc(LzmaGetNumProbs(&state.Properties) * sizeof(CProb));
  if (state.Probs == 0 )
  {
    return 0 ;		// no enough memory !
  }

  /* Decompress */

   res = LzmaDecode(&state,
      lzmabuf+(LZMA_PROPERTIES_SIZE + 8), compressedSize, &inProcessed,
      lzmaoutbuf, outSizeFull, &outProcessed);
    free(state.Probs);
    if (res != 0)
    {
	return 0 ;
    }
    else {
        return outProcessed ;
    }
    return res;
}
 

