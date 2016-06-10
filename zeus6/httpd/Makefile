
# Makefile for ZEUS 6 DVR
#

include ../config

CFLAGS += -I$(PROJDIR) 
LDFLAGS+= -lcrypt -lstdc++

all: eaglehttpd eagle_setup- getsetup- cfgdown- livecam-

# eaglehttpd: eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp eaglehttpd.h
#	$(CC) $(CFLAGS) -O2 -DMOD_SUPPORT -o $@ eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp $(LDFLAGS)

eaglehttpd_src=src_httpd/eaglehttpd.cpp src_httpd/smallssi.cpp src_httpd/var.cpp src_httpd/access.cpp src_httpd/mod.cpp 
eaglehttpd_h=src_httpd/eaglehttpd.h

eaglehttpd: $(eaglehttpd_src) $(eaglehttpd_h) 
	$(CC) $(CFLAGS) -I ./src_httpd -O2 -DMOD_SUPPORT -o $@  $(eaglehttpd_src) $(LDFLAGS)
	
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
	rm   -f eaglehttpd
	make -C eagle_setup clean
	make -C getsetup clean
	make -C livecam clean
	make -C cfgdown clean



