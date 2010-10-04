
# Makefile for Eagle32 DVR web base setup app 
#

include ../../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

#SOURCES=eagle_setup.cpp md5.cpp
SOURCES=eagle_setup.cpp ../../dvrsvr/md5.cpp

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp ../../dvrsvr/crypt.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS=-I../dvrsvr -g -Wall

all: eagle_setup

eagle_setup : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(EXTSOURCES)
	cp eagle_setup  $(debug_folder)/www

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f eagle_setup
	rm -f $(OBJS)
	rm -f *.o


