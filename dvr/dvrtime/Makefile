
# Makefile for Eagle32 DVR server app 
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=dvrtime.cpp

HEADERS=

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../ioprocess/diomap.cpp ../dvrsvr/config.cpp ../dvrsvr/string.cpp 

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS=-I../dvrsvr -g -Wall
LDFLAGS= -lstdc++


dvrtime : $(OBJS) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
	cp dvrtime  $(debug_folder)

dtime : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	gcc $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)

%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(DBGFLAGS) -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f dvrtime dtime
