
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=

EXTHEADERS=
EXTSOURCES=

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS=

CFLAGS=-I../dvrsvr -g -Wall

all: volumeid md5check

volumeid : volumeid.c $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ volumeid.c $(EXTSOURCES) $(LDFLAGS)
#	cp volumeid /home/jianpeng/eagle34/dvrtest/

md5check :  md5check.c md5.c $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ md5check.c md5.c $(EXTSOURCES) $(LDFLAGS)
#	cp md5check /home/jianpeng/eagle34/dvrtest/

%.o : %.c $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f volumeid md5check


