
# Makefile for Eagle32 DVR ioprocess app
#

include ../config
include $(PROJDIR)/arm_tool

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

CFLAGS+= -Wall

all: sfx mksfx extractsfx

sfx : sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	$(CC) $(CFLAGS) -o $@ sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)
	$(STRIP) sfx

sfx86 : sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	gcc $(CFLAGS) -o $@ sfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)
	strip sfx86

mksfx : mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c
	gcc $(CFLAGS) -o $@ mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c $(LDFLAGS)

extractsfx : extractsfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	gcc $(CFLAGS) -o $@ -g extractsfx.cpp lzmasfx.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)

clean :
	rm -f mksfx
	rm -f sfx
	rm -f sfx86
	rm -f extractsfx
	rm -f *.o
