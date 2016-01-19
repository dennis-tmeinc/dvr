
# Makefile for Eagle32 DVR server app
#

include ../config

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

serial : serial.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
#	cp tdevd /home/jianpeng/eagle34/dvrtest/

clean :
	rm -f serial
	rm -f *.o
