
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

CFLAGS=-g -Wall

HEADERS=

LDFLAGS= 

all: eaglehttpd eagle_setup- getsetup- cfgdown-

eaglehttpd: eaglehttpd.c
	$(CC) $(CFLAGS) -o $@ eaglehttpd.c $(LDFLAGS) -lcrypt
	cp eaglehttpd  $(debug_folder)/www
    
eagle_setup- :
	cd eagle_setup; make

getsetup- :
# applysetup:
	cd getsetup; make

cfgdown- :
	cd cfgdown; make   

clean :
	rm -f eaglehttpd
	rm -f *.o
	cd eagle_setup; make clean
	cd getsetup; make clean
	cd cfgdown; make clean



