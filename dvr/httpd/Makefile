
# Makefile for Eagle32 DVR ioprocess app
#

include ../config
include $(PROJDIR)/arm_tool

CFLAGS+= -Wall

HEADERS=

LDFLAGS+= -lcrypt -lstdc++

all: eaglehttpd- eagle_setup- getsetup- cfgdown- livecam-

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



