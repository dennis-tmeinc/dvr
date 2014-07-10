
# Makefile for Eagle32 DVR ioprocess app
#

include ../config


SOURCES=nfile.c

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS=


nfile:  $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
#	cp nfile /home/jianpeng/eagle34/dvrtest/


%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f nfile


