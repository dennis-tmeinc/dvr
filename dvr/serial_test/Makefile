
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=serialtest.c

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS= -lpthread -lstdc++ -L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib

CFLAGS=-I../dvrsvr -g -Wall

all : gentest

serialtest : $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	cp serialtest  $(debug_folder)

gentest : gentest.cpp $(EXTHEADERS) $(EXTSOURCES)
	$(CC) $(CFLAGS) -o $@ gentest.cpp $(EXTSOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)

%.o : %.c $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f serialtest gentest

