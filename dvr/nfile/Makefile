
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

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


