
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=glog.cpp netdbg.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../ioprocess/diomap.cpp ../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=netdbg.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lpthread -lstdc++ -L../dvrsvr/$(EAGLE) -lsdk -ldadsp ../dvrsvr/$(EAGLE)/dsplink.lib

CFLAGS=-I../dvrsvr -g -Wall

glog : $(OBJS) $(EXTSOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	cp glog  $(debug_folder)


%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f glog

