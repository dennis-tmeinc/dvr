
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

HEADERS=

CFLAGS=

default: libfbdraw.so

fb : $(OBJS) fb.cpp libfbdraw.so 
	$(CC) $(CFLAGS) -o $@ fb.cpp libfbdraw.so -Wl,-rpath,. -lstdc++
	cp fb ~/nfsroot/
	cp libfbdraw.so  $(debug_folder)

libfbdraw.so : fbdraw.c ../lzmasdk/dec.c ../lzmasdk/LzmaDec.c
	$(CC) $(CFLAGS) -fPIC -c -Wall fbdraw.c
	$(CC) $(CFLAGS) -fPIC -c -Wall ../lzmasdk/dec.c
	$(CC) $(CFLAGS) -fPIC -c -Wall ../lzmasdk/LzmaDec.c
	$(CC) $(CFLAGS) -shared -Wl,-soname,libfbdraw.so -o $@ fbdraw.o dec.o LzmaDec.o
	cp libfbdraw.so $(debug_folder)

clean :
	rm -f *.o
	rm -f libfbdraw.so
	rm -f fb
