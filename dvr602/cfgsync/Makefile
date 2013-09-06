
# Makefile for Eagle32 DVR ioprocess app 
# Changes:
# 08/14/2009 Harrison
#    1. copy cfgpc to /deploy/MDVR5100
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=cfgsync.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h 
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ 

CFLAGS=-I../dvrsvr -g -Wall

all : cfgsync cfgsyncpc

cfgsync : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	cp cfgsync ~/nfsroot/eagletest

cfgsyncpc : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	gcc $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :  
	rm -f *.o
	rm -f cfgsync cfgsyncpc







