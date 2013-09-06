
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=ioprocess.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=diomap.h 

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++  -L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib

CFLAGS=-I../dvrsvr -g -Wall

all: ioprocess panelled devicepower buzzer

ioprocess : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
	cp ioprocess ~/nfsroot/eagletest


panelled : panelled.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ panelled.o $(EXTSOURCES) $(LDFLAGS)
	cp panelled ~/nfsroot/eagletest

devicepower : devicepower.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ devicepower.o $(EXTSOURCES) $(LDFLAGS)
	cp devicepower ~/nfsroot/eagletest

buzzer : buzzer.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ buzzer.o $(EXTSOURCES) $(LDFLAGS)
	cp buzzer ~/nfsroot/eagletest

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f ioprocess panelled devicepower buzzer


