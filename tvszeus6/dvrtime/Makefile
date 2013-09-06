
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=dvrtime.cpp

HEADERS=

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -I../dvrsvr
LDFLAGS= -lstdc++


dvrtime : $(OBJS) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
#	cp dvrtime /home/jianpeng/eagle34/dvrtest/

dtime : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	gcc $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)


%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f dvrtime
