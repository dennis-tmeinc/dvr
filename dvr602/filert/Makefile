export CROSS_COMPILE=~/proj/eagle32/hik_toolchain/bin/arm-linux-
export CC=$(CROSS_COMPILE)gcc
export AR=$(CROSS_COMPILE)ar
export AS=$(CROSS_COMPILE)as
export LD=$(CROSS_COMPILE)ld
export NM=$(CROSS_COMPILE)nm
export OBJCOPY=$(CROSS_COMPILE)objcopy
export OBJDUMP=$(CROSS_COMPILE)objdump
export RANLIB=$(CROSS_COMPILE)ranlib
export STRIP=$(CROSS_COMPILE)strip

all: filert

EXTRA_CFLAGS =
EXTRA_LDFLAGS = -lpthread

filert: filert.o parseconfig.o
	$(CC) -o filert filert.o parseconfig.o $(EXTRA_LDFLAGS)

filert.o: filert.c 
	$(CC) -Wall $(CFLAGS) $(EXTRA_CFLAGS) -c filert.c

parseconfig.o: parseconfig.c 
	$(CC) -Wall $(CFLAGS) $(EXTRA_CFLAGS) -c parseconfig.c

strip:
	$(STRIP) filert
clean:
	rm -f *.o filert
