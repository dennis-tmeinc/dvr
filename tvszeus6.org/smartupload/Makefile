
include ../config

all: smartftp

EXTRA_CFLAGS = -I./curl-7.20.1/include
EXTRA_LDFLAGS = -L./curl-7.20.1/lib/.libs -lcurl -lrt -lpthread

smartftp: smartftp.o upload.o parseconfig.o gforce_peak.o hikfile.o
	$(CC) $(CFLAGS) -o smartftp smartftp.o upload.o parseconfig.o gforce_peak.o hikfile.o $(EXTRA_LDFLAGS) -Wl,-rpath=.

smartftp.o: smartftp.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o smartftp.o smartftp.c

upload.o: upload.c upload.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c upload.c

parseconfig.o: parseconfig.c parseconfig.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c parseconfig.c

gforce_peak.o: gforce_peak.c gforce_peak.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c gforce_peak.c

hikfile.o: hikfile.c hikfile.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c hikfile.c

strip:
	$(STRIP) smartftp
clean:
	rm -f *.o smartftp
