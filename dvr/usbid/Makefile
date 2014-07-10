
# Makefile for Eagle32 DVR server app
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=usbid.c

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS+= -Wall
LDFLAGS=

all: usbid officerid officerkey

usbid : usbid.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)


officerid : officerid.cpp blowfish.cpp ../dvrsvr/md5.cpp ../dvrsvr/crypt.cpp
	$(CC) $(CFLAGS) -o $@ officerid.cpp blowfish.cpp ../dvrsvr/md5.cpp ../dvrsvr/crypt.cpp $(LDFLAGS)
	$(STRIP) $@

officerkey: officerkey.cpp 
	$(CC) $(CFLAGS) -o $@ officerkey.cpp $(LDFLAGS)
	$(STRIP) $@

clean :
	rm -f usbid officerid officerkey
	rm -f *.o
