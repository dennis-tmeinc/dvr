
# Makefile for Eagle32 DVR ioprocess app 
# Changes:
# 08/14/2009 Harrison
#    1. copy cfgpc to /deploy/MDVR5100
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=cfg.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h 
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ 

CFLAGS=-I../dvrsvr -g -Wall

all : cfg cfgpc

cfg : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	cp cfg ~/nfsroot/eagletest

cfgpc : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	gcc $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	cp cfgpc ../deploy/MDVR5100

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :  
	rm -f *.o
	rm -f cfg cfgpc







