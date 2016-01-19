
# Makefile for polarssl ssl_server
#

include ../config
include $(PROJDIR)/arm_tool

CFLAGS+=

HEADERS=

LDFLAGS=

programs/ssl/ssl_server:
	cd library  && $(MAKE) all && cd ..
	cd programs && $(MAKE) ssl/ssl_server && cd ..
	cp programs/ssl/ssl_server .

clean:
	cd library  && $(MAKE) clean && cd ..
	cd programs && $(MAKE) clean && cd ..
	rm -r ssl_server
