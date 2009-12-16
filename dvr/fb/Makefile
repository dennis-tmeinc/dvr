
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=fb.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ 
CFLAGS= -g

default: libfbdraw.so

fb : $(OBJS) $(SOURCES_C) libfbdraw.so 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(OBJS) libfbdraw.so -Wl,-rpath,.
	cp fb ~/nfsroot/
	cp libfbdraw.so  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) $(CCFLAGS) -Wall -c $< -o $@

libfbdraw.so : fbdraw.c ../sfx/lzmadec.c ../lzmasdk/LzmaDec.c
	$(CC) $(CCFLAGS) -fPIC -c -Wall fbdraw.c ../sfx/lzmadec.c ../lzmasdk/LzmaDec.c
	$(CC) $(CCFLAGS) -shared -Wl,-soname,libfbdraw.so -o $@ fbdraw.o lzmadec.o LzmaDec.o
	cp libfbdraw.so ../dvrsvr/	

clean :
	rm -f *.o
	rm -f libfbdraw.so
	rm -f fb
