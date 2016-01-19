# Makefile for ZEUS6 DVR server app
#

include ../config

default: udhcpc

src:
	tar xvjf udhcp_0.9.8.tar.bz2
	echo 1 > src

udhcpc: src
	$(MAKE) -C udhcp udhcpc
	cp udhcp/udhcpc .
	$(STRIP) udhcpc

clean:
	rm -f -r udhcp
	rm -f udhcpc src
