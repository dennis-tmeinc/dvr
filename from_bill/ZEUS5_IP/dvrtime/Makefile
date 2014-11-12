
# Makefile for Eagle32 DVR server app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=dvrtime.cpp

HEADERS=

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp 

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS=-I../dvrsvr -g -Wall
LDFLAGS= -lstdc++


dvrtime : $(OBJS) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
#	cp dvrtime /home/jianpeng/eagle34/dvrtest/

dtime : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	gcc $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)


%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(DBGFLAGS) -Wall -O2 -c $< -o $@

clean :
	rm -f *.o
	rm -f dvrtime
