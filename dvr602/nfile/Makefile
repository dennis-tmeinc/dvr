
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

CFLAGS=

SOURCES=nfile.c

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS= 


nfile:  $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	cp nfile ~/nfsroot/eagletest/


%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f nfile


