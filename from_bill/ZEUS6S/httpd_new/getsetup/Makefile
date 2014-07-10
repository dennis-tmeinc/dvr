
# Makefile for Eagle32 DVR dvrsetup app 
#

CROSS_COMPILE_PREFIX=arm_v5t_le-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ -lcrypt

CFLAGS=-I../dvrsvr -g -Wall

all: getsetup applysetup

getsetup: getsetup.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) -g -o $@ getsetup.cpp $(EXTSOURCES) $(LDFLAGS)
#	cp getsetup /home/jianpeng/eagle34/dvrtest/www/

applysetup: applysetup.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) -g -o $@ applysetup.cpp $(EXTSOURCES) ../../dvrsvr/crypt.cpp $(LDFLAGS) 
#	cp applysetup /home/jianpeng/eagle34/dvrtest/www/

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

clean:
	rm -f applysetup
	rm -f getsetup
	rm -f *.o


