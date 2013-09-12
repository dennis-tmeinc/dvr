
# Makefile for Eagle32 DVR ioprocess app
#

include ../config
include $(PROJDIR)/arm_tool

CFLAGS+= -Wall

SOURCES=fsckwrapper.c

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS=


fsckwrapper:  $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	cp fsckwrapper  $(debug_folder)


%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f fsckwrapper


