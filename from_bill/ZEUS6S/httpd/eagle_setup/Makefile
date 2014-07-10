
# Makefile for Eagle32 DVR web base setup app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=eagle_setup.cpp

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp

HEADERS=$(EXTHEADERS)

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS=-I../dvrsvr -g -Wall -O2

all: eagle_setup  mcufwupd partition

eagle_setup : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
#	cp eagle_setup /home/jianpeng/eagle34/dvrtest/www/

mcufwupd : mcufwupd.cpp
	$(CC) $(CFLAGS) -o $@ mcufwupd.cpp $(LDFLAGS)

partition : partition.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ partition.cpp $(EXTSOURCES) $(LDFLAGS)

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f eagle_setup mcufwupd partition
	rm -f *.o


