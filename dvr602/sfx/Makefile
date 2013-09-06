
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc
STRIP=$(CROSS_COMPILE_PREFIX)strip

SOURCES=sfx.c lzmadec.c LzmaDecode.c

HEADERS=

OBJS = $(patsubst %.c, %.o, $(SOURCES))

#LDFLAGS= -lstdc++ -L./eagle32 -lsdk -ldadsp ./eagle32/dsplink.lib
LDFLAGS= 
CCFLAGS=

all: sfx mksfx sfx86 extractsfx

sfx : $(SOURCES)
	$(CC) $(CCFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	$(STRIP) sfx

sfx86 : $(SOURCES)
	gcc -o $@ $(SOURCES) $(LDFLAGS)

mksfx : mksfx.c
	gcc -o mksfx mksfx.c

extractsfx : extractsfx.c
	gcc -o extractsfx extractsfx.c lzmadec.c LzmaDecode.c

clean :
	rm -f mksfx
	rm -f sfx
	rm -f sfx86
	rm -f extractsfx
	rm -f *.o
