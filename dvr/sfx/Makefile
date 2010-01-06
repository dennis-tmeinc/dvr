
# Makefile for Eagle32 DVR ioprocess app 
#

include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc
STRIP=$(CROSS_COMPILE_PREFIX)strip

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

LDFLAGS= 
CCFLAGS=-Os

all: sfx mksfx extractsfx

sfx : sfx.c lzmasfx.c lzmadec.c ../lzmasdk/LzmaDec.c
	$(CC) $(CCFLAGS) -o $@ sfx.c lzmasfx.c lzmadec.c ../lzmasdk/LzmaDec.c $(LDFLAGS)
	$(STRIP) sfx

sfx86 : sfx.c lzmadec.c lzmasfx.c ../lzmasdk/LzmaDec.c 
	gcc -o $@ sfx.c lzmadec.c lzmasfx.c ../lzmasdk/LzmaDec.c
	strip sfx86

mksfx : mksfx.c lzmaenc.c ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c
	gcc -o $@ mksfx.c lzmaenc.c ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c

extractsfx : extractsfx.c lzmasfx.c lzmadec.c ../lzmasdk/LzmaDec.c
	gcc -o $@ -g extractsfx.c lzmasfx.c lzmadec.c ../lzmasdk/LzmaDec.c

clean :
	rm -f mksfx
	rm -f sfx
	rm -f sfx86
	rm -f extractsfx
	rm -f *.o
