#
# Custom makefile file dropbear ssh server
#

include ../config
include $(PROJDIR)/arm_tool

default: dropbear

src:
	tar xvjf dropbear-0.52.tar.bz2
	echo 1 > src

cfg: src
	cd dropbear-0.52 ; ./configure --host=arm --disable-zlib --disable-openpty
	cp options.h dropbear-0.52/
	echo 1 > cfg
   
dropbear: cfg
	cd dropbear-0.52 ; make PROGRAMS="dropbear"
	cp dropbear-0.52/dropbear .
	$(STRIP) dropbear

dropbearkey: dropbear
	cd dropbear-0.52 ; make dropbearkey
	cp dropbear-0.52/dropbearkey .

clean:
	rm -f -r dropbear-0.52
	rm -f dropbear dropbearkey cfg src
