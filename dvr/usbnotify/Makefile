
# Makefile for Eagle32 DVR dvrsetup app
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=usbnotify.cpp

EXTHEADERS=../dvrsvr/config.h ../dvrsvr/genclass.h
EXTSOURCES=../httpd/net.cpp
HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

CFLAGS+= -Wall

all: usbnotify

usbnotify: $(SOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@
	cp $@ $(debug_folder)

clean:
	rm -f usbnotify
	rm -f *.o


