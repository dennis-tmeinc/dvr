
# Makefile for Eagle32 DVR web base setup app
#

include ../../config

SOURCES=eagle_setup.cpp

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp

HEADERS=$(EXTHEADERS)

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS +=-I../../dvrsvr

all: eagle_setup  mcufwupd

eagle_setup : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
#	cp eagle_setup /home/jianpeng/eagle34/dvrtest/www/

mcufwupd : mcufwupd.cpp
	$(CC) $(CFLAGS) -o $@ mcufwupd.cpp $(LDFLAGS)

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f eagle_setup mcufwupd
	rm -f *.o


