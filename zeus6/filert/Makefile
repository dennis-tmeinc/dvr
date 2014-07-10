
include ../config

all: filert

EXTRA_CFLAGS =
EXTRA_LDFLAGS = -lpthread

filert: filert.o parseconfig.o
	$(CC) $(CFLAGS) -o filert filert.o parseconfig.o $(EXTRA_LDFLAGS)

filert.o: filert.c
	$(CC) $(CFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -c filert.c

parseconfig.o: parseconfig.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c parseconfig.c

strip:
	$(STRIP) filert
clean:
	rm -f *.o filert
