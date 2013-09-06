#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define WIDTH   16
#define DBUFSIZE 1024
int dump(unsigned char *s, int len)
{
    char buf[DBUFSIZE],lbuf[DBUFSIZE],rbuf[DBUFSIZE];
    unsigned char *p;
    int line,i;
#if 0    // debug
    struct timeval tv;
    gettimeofday(&tv, NULL);
    dprintf("time:%d.%06d\n", tv.tv_sec, tv.tv_usec);
#endif

    p =(unsigned char *)s;
    for(line = 1; len > 0; len -= WIDTH, line++) {
      memset(lbuf, '\0', DBUFSIZE);
      memset(rbuf, '\0', DBUFSIZE);
      for(i = 0; i < WIDTH && len > i; i++,p++) {
	sprintf(buf,"%02x ",(unsigned char) *p);
	strcat(lbuf,buf);
	sprintf(buf,"%c",(!iscntrl(*p) && *p <= 0x7f) ? *p : '.');
	strcat(rbuf,buf);
      }
      dprintf("%04x: %-*s    %s\n",line - 1, WIDTH * 3, lbuf, rbuf);
    }
    if(!(len%16)) {
	dprintf("\n");
    }
    return line;
}
