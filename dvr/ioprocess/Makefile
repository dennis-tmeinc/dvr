
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

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


panelled : panelled.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ panelled.o $(EXTSOURCES) $(LDFLAGS)
	cp panelled  $(debug_folder)

devicepower : devicepower.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ devicepower.o $(EXTSOURCES) $(LDFLAGS)
	cp devicepower  $(debug_folder)

buzzer : buzzer.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ buzzer.o $(EXTSOURCES) $(LDFLAGS)
	cp buzzer  $(debug_folder)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f ioprocess panelled devicepower buzzer


