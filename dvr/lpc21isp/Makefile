
# Makefile for Eagle32 DVR ioprocess app
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=adprog.c lpc21isp.c lpcprog.c lpcterm.c
HEADERS=$(patsubst %.c, %.h, $(SOURCES))
OBJS = $(patsubst %.c, %.o, $(SOURCES))

CFLAGS+= -I../dvrsvr -Wall

lpc21isp: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	cp lpc21isp  $(debug_folder)

clean :
	rm -f *.o
	rm -f lpc21isp
