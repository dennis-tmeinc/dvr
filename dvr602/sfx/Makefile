
# Makefile for Eagle32 sfx app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc
STRIP=$(CROSS_COMPILE_PREFIX)strip

include ../config

SOURCES=sfx.c lzmadec.c LzmaDecode.c

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

all: sfx mksfx

sfx : sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	$(CC) $(CFLAGS) -o $@ sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)
	$(STRIP) sfx

sfx86 : sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	gcc -o $@ sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c 
	strip sfx86

mksfx : mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c
	gcc -o $@ mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c 

extractsfx : extractsfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	gcc -o $@ -g extractsfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c 

clean :
	rm -f mksfx
	rm -f sfx
	rm -f sfx86
	rm -f extractsfx
	rm -f *.o
