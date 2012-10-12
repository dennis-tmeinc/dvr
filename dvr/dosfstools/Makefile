#
# Custom makefile file dropbear ssh server
#

include ../config
include $(PROJDIR)/arm_tool

DOSFSTOOLS=dosfstools-3.0.12
TARBALL=$(DOSFSTOOLS).tar.bz2

default: fstools

src:
	tar xjf $(TARBALL)
	echo 1 > src

fstools: src
	$(MAKE) -C $(DOSFSTOOLS)

clean:
	rm -f -r $(DOSFSTOOLS)
	rm -f src
