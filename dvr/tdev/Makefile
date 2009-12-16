
# Makefile for Eagle32 DVR server app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=tdevd.c

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS=-Wall -g
LDFLAGS= 

all: tdevd tdevmount

tdevd : tdevd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	cp tdevd ~/nfsroot/eagletest

tdevmount : tdevmount.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lstdc++
	cp tdevmount ~/nfsroot/eagletest

clean :
	rm -f tdevd
	rm -f tdevmount
	rm -f *.o
