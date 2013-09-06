
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

HEADERS=

LDFLAGS=

eaglehttpd: eaglehttpd.c
	$(CC) $(CFLAGS) -o $@ eaglehttpd.c $(LDFLAGS) -lcrypt
#	cp eaglehttpd /home/jianpeng/eagle34/dvrtest/www/

# eagle_setup:
	make -C eagle_setup

# getsetup:
# applysetup:
	make -C getsetup

# cfgdown.cgi
	make -C cfgdown

clean :
	rm -f eaglehttpd
	rm -f *.o
	make -C eagle_setup clean
	make -C getsetup clean
	make -C cfgdown clean



