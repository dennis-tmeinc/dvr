
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

SOURCES=

EXTHEADERS=
EXTSOURCES=

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS=

CFLAGS += -I../dvrsvr

all: fat32vol

fat32vol : fat32vol.c $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ fat32vol.c $(EXTSOURCES) $(LDFLAGS)

clean :
	rm -f *.o
	rm -f fat32vol


