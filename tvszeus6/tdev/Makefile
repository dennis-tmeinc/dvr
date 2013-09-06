
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=tdevd.c

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

all: tdevd tdevmount

tdevd : tdevd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
#	cp tdevd /home/jianpeng/eagle34/dvrtest/

tdevmount : tdevmount.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lstdc++
#	cp tdevmount /home/jianpeng/eagle34/dvrtest/

clean :
	rm -f tdevd
	rm -f tdevmount
	rm -f *.o
