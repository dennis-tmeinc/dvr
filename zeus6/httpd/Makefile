
# Makefile for ZEUS 6 DVR
#

ifndef PROJDIR
include ../config
export PROJDIR
endif

HEADERS=

LDFLAGS+= -lcrypt -lstdc++

all: eaglehttpd- eagle_setup- getsetup- cfgdown- livecam-

# eaglehttpd: eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp eaglehttpd.h
#	$(CC) $(CFLAGS) -O2 -DMOD_SUPPORT -o $@ eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp $(LDFLAGS)

eaglehttpd- :
	make -C eaglehttpd
	
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
	make -C eaglehttpd clean
	make -C eagle_setup clean
	make -C getsetup clean
	make -C livecam clean
	make -C cfgdown clean



