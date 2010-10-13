
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

EXTHEADERS=../cfg.h ../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

SOURCES=ioprocess.cpp diomap.cpp mcu.cpp netdbg.cpp gforce.cpp
HEADERS=diomap.h netdbg.h mcu.h gforce.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++  -L../dvrsvr/$(EAGLE) -lsdk -ldadsp ../dvrsvr/$(EAGLE)/dsplink.lib

CFLAGS=-I../dvrsvr -g -Wall

all: ioprocess panelled devicepower buzzer

ioprocess : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
	cp ioprocess  $(debug_folder)


panelled : panelled.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ panelled.o diomap.o $(EXTSOURCES) $(LDFLAGS)
	cp panelled  $(debug_folder)

devicepower : devicepower.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ devicepower.o diomap.o $(EXTSOURCES) $(LDFLAGS)
	cp devicepower  $(debug_folder)

buzzer : buzzer.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ buzzer.o diomap.o $(EXTSOURCES) $(LDFLAGS)
	cp buzzer  $(debug_folder)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f ioprocess panelled devicepower buzzer


