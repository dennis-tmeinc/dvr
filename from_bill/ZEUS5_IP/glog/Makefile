
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=glog.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lpthread -lstdc++ 
#-L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib

CFLAGS=-I../dvrsvr -g -Wall

glog : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
#	cp glog /home/jianpeng/eagle34/dvrtest/


%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -O2 -c $< -o $@

clean :
	rm -f *.o
	rm -f glog

ttest : ttest.c $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ ttest.c $(EXTSOURCES) $(LDFLAGS)
#	cp ttest /home/jianpeng/eagle34/dvrtest/
