
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=glog.cpp netdbg.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h ../dvrsvr/dir.h
EXTSOURCES=../ioprocess/diomap.cpp ../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=netdbg.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

#no eagle boards
ifeq ($(EAGLE),mv)
LDFLAGS= -lpthread -lstdc++ -lrt -Wl,-rpath,.
else
LDFLAGS= -lpthread -lstdc++ -L../dvrsvr/$(EAGLE) -lsdk -ldadsp ../dvrsvr/$(EAGLE)/dsplink.lib
endif

CFLAGS=-I../dvrsvr -g -Wall

all : glog gpsdetect

glog : $(OBJS) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
	cp glog  $(debug_folder)

gpsdetect : gpsdetect.cpp $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ gpsdetect.cpp $(LDFLAGS)
	cp gpsdetect  $(debug_folder)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f glog gpsdetect

