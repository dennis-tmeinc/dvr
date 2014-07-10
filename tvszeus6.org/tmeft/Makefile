
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=arm_v5t_le-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=tmefile.c

EXTHEADERS=
EXTSOURCES=

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS=

CFLAGS=-g -Wall

all: tmefile

tmefile : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
	cp tmefile /home/jianpeng/eagle34/dvrtest/

%.o : %.c $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f tmefile


