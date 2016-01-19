
# Makefile for Eagle32 DVR ioprocess app
#

ifndef PROJDIR
include ../../config
endif

include $(PROJDIR)/arm_tool

CFLAGS+= -Wall -I$(PROJDIR)

HEADERS=

LDFLAGS+= -lcrypt -lstdc++

eaglehttpd: eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp eaglehttpd.h
	$(CC) $(CFLAGS) -O2 -DMOD_SUPPORT -o $@ eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp $(LDFLAGS)

clean :
	rm -f eaglehttpd
	rm -f *.o



