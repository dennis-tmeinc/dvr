
# Makefile for Eagle32 DVR dvrsetup app 
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=usbnotify.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=
HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

CFLAGS= -g -Wall

all: usbnotify

usbnotify: $(SOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	$(STRIP) $@
	cp $@ $(debug_folder)

clean:
	rm -f usbnotify
	rm -f *.o


