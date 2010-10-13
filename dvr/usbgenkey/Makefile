
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=usbgenkey.c crypt.c

HEADERS=

OBJS =
CFLAGS=-Wall -g
LDFLAGS=

all: usbgenkey usbcheck

usbgenkey : usbgenkey.c crypt.c
	gcc $(CFLAGS) -o $@ usbgenkey.c crypt.c $(LDFLAGS)
	cp usbgenkey ~/nfsroot/eagletest

usbcheck : usbcheck.c crypt.c
	$(CC) $(CFLAGS) -o $@  usbcheck.c crypt.c $(LDFLAGS)
	cp usbcheck ~/nfsroot/eagletest

clean :
	rm -f *.o
	rm -f usbgenkey
	rm -f usbcheck


