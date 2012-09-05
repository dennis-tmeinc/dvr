
# Makefile for Eagle32 DVR server app
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=dvrtime.cpp

HEADERS=

EXTHEADERS=../ioprocess/diomap.h ../dvrsvr/config.h ../dvrsvr/genclass.h
EXTSOURCES=../ioprocess/diomap.cpp ../dvrsvr/config.cpp ../dvrsvr/string.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+=-I../dvrsvr -Wall
LDFLAGS= -lstdc++

dvrtime : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -D DVR_APP -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	cp dvrtime  $(debug_folder)

dtime : $(SOURCES)
	gcc $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

clean :
	rm -f dvrtime dtime
