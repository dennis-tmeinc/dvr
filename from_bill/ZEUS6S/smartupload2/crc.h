#ifndef _CRC_H_
#define _CRC_H_

#ifdef __cplusplus
extern "C" {
#endif

unsigned int 
crc32(unsigned int crc, const unsigned char* buf, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif
