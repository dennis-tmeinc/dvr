
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=tmefile.c

EXTHEADERS=
EXTSOURCES=

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS=

CFLAGS=-g -Wall

all: tmefile

tmefile : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
	cp tmefile ~/nfsroot/eagletest

%.o : %.c $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f tmefile


