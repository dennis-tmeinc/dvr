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

all: smartftp

EXTRA_CFLAGS = -I./curl-7.20.1/include
EXTRA_LDFLAGS = -L./curl-7.20.1/lib/.libs -lcurl -lrt -lpthread

smartftp: smartftp.o upload.o parseconfig.o gforce_peak.o hikfile.o
	$(CC) -o smartftp smartftp.o upload.o parseconfig.o gforce_peak.o hikfile.o $(EXTRA_LDFLAGS) -Wl,-rpath=.

smartftp.o: smartftp.c 
	$(CC) -Wall $(CFLAGS) $(EXTRA_CFLAGS) -c -o smartftp.o smartftp.c

upload.o: upload.c upload.h
	$(CC) -Wall $(CFLAGS) $(EXTRA_CFLAGS) -c upload.c

parseconfig.o: parseconfig.c parseconfig.h
	$(CC) -Wall $(CFLAGS) $(EXTRA_CFLAGS) -c parseconfig.c

gforce_peak.o: gforce_peak.c gforce_peak.h
	$(CC) -Wall $(CFLAGS) $(EXTRA_CFLAGS) -c gforce_peak.c

hikfile.o: hikfile.c hikfile.h
	$(CC) -Wall $(CFLAGS) $(EXTRA_CFLAGS) -c hikfile.c

strip:
	$(STRIP) smartftp
clean:
	rm -f *.o smartftp
