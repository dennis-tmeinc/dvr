
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

CFLAGS=-g

LDFLAGS= 

all: slaveip eaglehost

slaveip: slaveip.c 
	$(CC) $(CFLAGS) -o $@ slaveip.c $(LDFLAGS)
	cp slaveip ~/nfsroot/eagletest/

eaglehost: eaglehost.c
	$(CC) $(CFLAGS) -o $@ eaglehost.c $(LDFLAGS)
	cp eaglehost ~/nfsroot/eagletest/

rawlisten:
	$(CC) -g -o rawlisten rawlisten.c
	cp rawlisten ~/nfsroot/eagletest/

rawsend:
	$(CC) -g -o rawsend rawsend.c
	cp rawsend ~/nfsroot/eagletest/

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f slaveip
	rm -f eaglehost


