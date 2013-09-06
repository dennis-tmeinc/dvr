
# Makefile for ZEUS 6 DVR
#

include ../config

HEADERS=

LDFLAGS+= -lcrypt

all: eaglehttpd eagle_setup- getsetup- cfgdown- livecam-

eaglehttpd: eaglehttpd.c
	$(CC) $(CFLAGS) -o $@ eaglehttpd.c $(LDFLAGS)
	cp eaglehttpd  $(debug_folder)/www

eagle_setup- :
	make -C eagle_setup

livecam- :
	make -C livecam

getsetup- :
# applysetup:
	make -C getsetup

cfgdown- :
	make -C cfgdown

clean :
	rm -f eaglehttpd
	rm -f *.o
	make -C eagle_setup clean
	make -C getsetup clean
	make -C livecam clean
	make -C cfgdown clean



