
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

EXTHEADERS=../cfg.h ../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

SOURCES=ioprocess.cpp diomap.cpp mcu.cpp netdbg.cpp gforce.cpp iomisc.cpp
HEADERS=diomap.h netdbg.h mcu.h gforce.h iomisc.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))


#no eagle boards
ifeq ($(EAGLE),mv)
LDFLAGS= -lpthread -lstdc++ -lrt -Wl,-rpath,.
else
LDFLAGS= -lstdc++  -L../dvrsvr/$(EAGLE) -lsdk -ldadsp ../dvrsvr/$(EAGLE)/dsplink.lib
endif

CFLAGS=-I../dvrsvr -g -Wall
TARGET=ioprocess panelled devicepower buzzer iowait

all: $(TARGET)

ioprocess : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS) -lm
	cp $@  $(debug_folder)


panelled : panelled.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ panelled.o diomap.o $(EXTSOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)

devicepower : devicepower.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ devicepower.o diomap.o $(EXTSOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)

iowait : iowait.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ iowait.o diomap.o $(EXTSOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)

buzzer : buzzer.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ buzzer.o diomap.o $(EXTSOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(TARGET)


