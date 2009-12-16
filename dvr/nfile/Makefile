
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

CFLAGS=

SOURCES=nfile.c

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS= 


nfile:  $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	cp nfile  $(debug_folder)


%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f nfile


