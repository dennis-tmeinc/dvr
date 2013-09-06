
# Makefile for Eagle32 DVR web base setup app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=eagle_setup.cpp

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp

HEADERS=$(EXTHEADERS)

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS=-I../dvrsvr -g -Wall

all: eagle_setup dvrstatus.cgi

eagle_setup : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	cp eagle_setup ~/nfsroot/eagletest/www

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f eagle_setup
	rm -f *.o


