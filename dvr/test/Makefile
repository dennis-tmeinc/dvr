
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc
CPP=$(CROSS_COMPILE_PREFIX)g++

SOURCES=serialtest.c

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

#LDFLAGS= -lpthread -lstdc++ -L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib
LDFLAGS= -lpthread -lstdc++

#CFLAGS=-I../dvrsvr -g -Wall
CFLAGS=-g -Wall

all : gentest

serialtest : $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	cp serialtest  $(debug_folder)

gentest : gentest.cpp
	gcc $(CFLAGS) -o gentest gentest.cpp $(LDFLAGS)
	$(CC) $(CFLAGS) -o gentest1 gentest.cpp $(LDFLAGS)
	cp gentest1  $(debug_folder)
	
#gentest : gentest.cpp $(EXTHEADERS) $(EXTSOURCES)
#	$(CPP) $(CFLAGS) -o $@ gentest.cpp $(EXTSOURCES) $(LDFLAGS)
#	cp $@  $(debug_folder)

%.o : %.c $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f serialtest gentest


