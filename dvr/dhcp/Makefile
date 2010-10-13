#
# Custom makefile file dropbear ssh server
#

include ../config
include $(PROJDIR)/arm_tool

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
