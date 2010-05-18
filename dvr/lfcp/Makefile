
# Makefile for Eagle32 DVR server app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc
STRIP=$(CROSS_COMPILE_PREFIX)strip

INCS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTRASOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS=-Wall -g -lstdc++
LDFLAGS= 

all: filert iomsg

filert : filert.cpp parseconfig.cpp
	$(CC) $(CFLAGS) -o $@ filert.cpp parseconfig.cpp $(LDFLAGS)
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
