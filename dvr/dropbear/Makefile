#
# Custom makefile file dropbear ssh server
#

include ../config
include $(PROJDIR)/arm_tool

DROPBEARTARFILE=dropbear-0.53.1.tar.bz2
DROPBEARDIR=dropbear-0.53.1

default: dropbear

src:
	tar xvjf $(DROPBEARTARFILE)
	echo 1 > src

cfg: src
	cd $(DROPBEARDIR) ; ./configure --host=arm --disable-zlib --disable-openpty
	cp options.h $(DROPBEARDIR)/
	echo 1 > cfg
   
dropbear: cfg
	cd $(DROPBEARDIR) ; make PROGRAMS="dropbear"
	cp $(DROPBEARDIR)/dropbear .
	$(STRIP) dropbear

dropbearkey: dropbear
	cd $(DROPBEARDIR) ; make dropbearkey
	cp $(DROPBEARDIR)/dropbearkey .

clean:
	rm -f -r $(DROPBEARDIR)
	rm -f dropbear dropbearkey cfg src
