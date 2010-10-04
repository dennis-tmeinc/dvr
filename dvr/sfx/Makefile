
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc
STRIP=$(CROSS_COMPILE_PREFIX)strip

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=-lstdc++

CCFLAGS=-Os

all: sfx mksfx extractsfx

sfx : sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	$(CC) $(CCFLAGS) -o $@ sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)
	$(STRIP) sfx

sfx86 : sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c 
	gcc -o $@ sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)
	strip sfx86

mksfx : mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c
	gcc -g -o $@ mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c $(LDFLAGS)

extractsfx : extractsfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	gcc -o $@ -g extractsfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)

clean :
	rm -f mksfx
	rm -f sfx
	rm -f sfx86
	rm -f extractsfx
	rm -f *.o
