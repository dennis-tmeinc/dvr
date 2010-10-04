
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=

EXTHEADERS=
EXTSOURCES=

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS=

CFLAGS=-I../dvrsvr -g -Wall

all: volumeid md5check

volumeid : volumeid.cpp $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ volumeid.cpp $(EXTSOURCES) $(LDFLAGS)
	cp volumeid ~/nfsroot/eagletest

md5check :  md5check.cpp ../dvrsvr/md5.cpp $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ md5check.cpp ../dvrsvr/md5.cpp $(EXTSOURCES) $(LDFLAGS)
	cp md5check ~/nfsroot/eagletest

%.o : %.c $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f volumeid md5check


