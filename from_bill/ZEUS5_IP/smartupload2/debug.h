#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#define dprintf(...) fprintf(stderr, __VA_ARGS__)
int dump(unsigned char *s, int len);

#endif
