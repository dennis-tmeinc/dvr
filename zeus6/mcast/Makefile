
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=

HEADERS=

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -I. -I../revserver
LDFLAGS= -lstdc++ -lpthread

all: asender alistener

asender : sender.c
	$(CC)  $(CFLAGS) -o $@ sender.c $(LDFLAGS)
	$(STRIP) $@
	
alistener : listener.c
	$(CC)  $(CFLAGS) -o $@ listener.c $(LDFLAGS)
	$(STRIP) $@

clean :
	rm -f *.o
	rm -f asender alistener
