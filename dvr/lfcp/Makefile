
# Makefile for Eagle32 DVR server app 
#

include ../config
include $(PROJDIR)/arm_tool

INCS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTRASOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp ../ioprocess/diomap.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS=-Wall -g -lstdc++
LDFLAGS= 

all: filert iomsg

filert : filert.cpp $(EXTRASOURCES) $(INCS)
	$(CC) $(CFLAGS) -o $@ filert.cpp $(EXTRASOURCES) $(LDFLAGS)
	cp $@  smartdl/
	$(STRIP) smartdl/$@

iomsg : iomsg.cpp $(EXTRASOURCES) $(INCS)
	$(CC) $(CFLAGS) -o $@ iomsg.cpp $(EXTRASOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)
	cp $@  smartdl/
	$(STRIP) smartdl/$@

lfcp : lfcp.cpp  $(EXTRASOURCES) $(INCS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

clean :
	rm -f filert lfcp iomsg *.o
