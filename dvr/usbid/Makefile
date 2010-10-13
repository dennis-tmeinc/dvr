
# Makefile for Eagle32 DVR server app 
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=usbid.c

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS=-Wall -g
LDFLAGS= 

all: usbid

usbid : usbid.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	cp usbid ~/nfsroot/eagletest

clean :
	rm -f usbid
	rm -f *.o
