export CROSS_COMPILE=arm-linux-
export CC=$(CROSS_COMPILE)gcc
export CXX=$(CROSS_COMPILE)g++
export AR=$(CROSS_COMPILE)ar
export AS=$(CROSS_COMPILE)as
export LD=$(CROSS_COMPILE)ld
export NM=$(CROSS_COMPILE)nm
export OBJCOPY=$(CROSS_COMPILE)objcopy
export OBJDUMP=$(CROSS_COMPILE)objdump
export RANLIB=$(CROSS_COMPILE)ranlib
export STRIP=$(CROSS_COMPILE)strip

CFLAGS := -Wall -O2
LDFLAGS := -lstdc++ -lpthread

OUT = smartftp
# add here
OBJS = smartftp.o debug.o crc.o md5.o parseconfig.o gforce_peak.o hikfile.o

$(OUT): $(OBJS)
#	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS)
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJS)

#inetsock.o: lib/inetsock.c include/inetsock.h
#	$(CC) -c $(CFLAGS) -o inetsock.o lib/inetsock.c

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.cpp
#	$(CXX) -c $(CFLAGS) $< -o $@
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean: 
	rm -f $(OUT) *.o *~
