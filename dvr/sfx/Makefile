
# Makefile for Eagle32 DVR project
#

include ../config
include $(PROJDIR)/arm_tool

ifeq ($(EAGLE),eagle32)
LDFLAGS= -lpthread -lstdc++ 
else
LDFLAGS= -lpthread -lstdc++ -lrt -Wl,-rpath,.
endif

CFLAGS+= -Wall

sfxfiles: sfx mksfx

all: sfx mksfx sfx86 extractsfx

sfx : sfx.cpp extract.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	$(CC) $(CFLAGS) -Os -o $@ sfx.cpp extract.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c $(LDFLAGS)
	$(STRIP) sfx

sfx86 : sfx.cpp extract.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	gcc -o $@ sfx.cpp extract.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c 
	strip sfx86

mksfx : mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c
	gcc -o $@ mksfx.cpp ../lzmasdk/enc.cpp ../lzmasdk/LzmaEnc.c ../lzmasdk/LzFind.c -lstdc++

extractsfx : extractsfx.cpp extract.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c
	gcc -o $@ -g extractsfx.cpp extract.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c -lstdc++

clean :
	rm -f mksfx
	rm -f sfx
	rm -f sfx86
	rm -f extractsfx
	rm -f *.o
