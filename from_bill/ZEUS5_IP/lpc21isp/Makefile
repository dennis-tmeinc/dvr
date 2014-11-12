
# Makefile for Eagle32 DVR ioprocess app 
#

#include ../config
#include $(PROJDIR)/arm_tool

export CROSS_COMPILE=arm-linux-
export CC=$(CROSS_COMPILE)gcc
export AR=$(CROSS_COMPILE)ar
export AS=$(CROSS_COMPILE)as
export LD=$(CROSS_COMPILE)ld
export NM=$(CROSS_COMPILE)nm
export OBJCOPY=$(CROSS_COMPILE)objcopy
export OBJDUMP=$(CROSS_COMPILE)objdump
export RANLIB=$(CROSS_COMPILE)ranlib
export STRIP=$(CROSS_COMPILE)strip
export GPP=$(CROSS_COMPILE_PREFIX)g++


SOURCES=adprog.c lpc21isp.c lpcprog.c lpcterm.c
HEADERS=$(patsubst %.c, %.h, $(SOURCES))
OBJS = $(patsubst %.c, %.o, $(SOURCES))

CFLAGS=-I../dvrsvr -g -Wall

lpc21isp: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	#cp lpc21isp  $(debug_folder)

clean :
	rm -f *.o
	rm -f lpc21isp
