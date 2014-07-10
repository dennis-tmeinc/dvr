
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=fb.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ 
CFLAGS= -g

default: libfbdraw.so

fb : $(OBJS) $(SOURCES_C) libfbdraw.so 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(OBJS) libfbdraw.so -Wl,-rpath,.
	#cp fb /home/jianpeng/eagle34/dvrtest/
	#cp libfbdraw.so /home/jianpeng/eagle34/dvrtest/

%.o : %.cpp $(HEADERS)
	$(CC) $(CCFLAGS) -Wall -c $< -o $@

libfbdraw.so : fbdraw.c lzmadec.c LzmaDecode.c
	$(CC) $(CCFLAGS) -fPIC -c -Wall fbdraw.c lzmadec.c LzmaDecode.c
	$(CC) $(CCFLAGS) -shared -Wl,-soname,libfbdraw.so -o $@ fbdraw.o lzmadec.o LzmaDecode.o
	cp libfbdraw.so ../dvrsvr/	

clean :
	rm -f *.o
	rm -f libfbdraw.so
	rm -f fb
