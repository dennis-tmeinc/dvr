
# Makefile for Eagle32 DVR server app 
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=tdevd.c

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS=-Wall -g
LDFLAGS= 

all: tdevd tdevmount

tdevd : tdevd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	cp tdevd $(debug_folder)

tdevmount : tdevmount.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lstdc++
	cp tdevmount $(debug_folder)

clean :
	rm -f tdevd
	rm -f tdevmount
	rm -f *.o
