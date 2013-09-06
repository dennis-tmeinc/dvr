#
# Custom makefile file dropbear ssh server
#

include ../config
include $(PROJDIR)/arm_tool

DOSFSTOOLS=dosfstools-3.0.14
TARBALL=$(DOSFSTOOLS).tar.xz

default: fstools

src:
	tar --xz -xf $(TARBALL)
	echo 1 > src

fstools: src
	$(MAKE) -C $(DOSFSTOOLS)
	cp $(DOSFSTOOLS)/mkdosfs $(DOSFSTOOLS)/dosfsck $(DOSFSTOOLS)/dosfslabel .
	$(STRIP) mkdosfs dosfsck dosfslabel

clean:
	rm -f -r $(DOSFSTOOLS)
	rm -f src mkdosfs dosfsck dosfslabel
