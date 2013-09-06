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

all: broadcast

EXTRA_CFLAGS =
EXTRA_LDFLAGS = 

broadcast: broadcast.c
	$(CC) -Wall -O2 -o broadcast broadcast.c

strip:
	$(STRIP) broadcast
clean:
	rm -f *.o broadcast
