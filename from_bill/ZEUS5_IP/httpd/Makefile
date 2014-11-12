
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc
CFLAGS=-g

HEADERS=

LDFLAGS= 

eaglehttpd: eaglehttpd.c
	$(CC) $(CFLAGS) -o $@ eaglehttpd.c $(LDFLAGS) -lcrypt
#	cp eaglehttpd /home/jianpeng/eagle34/dvrtest/www/
    
# eagle_setup:
	cd eagle_setup; make

# getsetup:
# applysetup:
	cd getsetup; make

# cfgdown.cgi
	cd cfgdown; make   

clean :
	rm -f eaglehttpd
	rm -f *.o
	cd eagle_setup; make clean
	cd getsetup; make clean
	cd cfgdown; make clean



