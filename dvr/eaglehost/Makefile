
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config
include $(PROJDIR)/arm_tool

CFLAGS=-g
LDFLAGS= 

all: slaveip eaglehost

slaveip: slaveip.c 
	$(CC) $(CFLAGS) -o $@ slaveip.c $(LDFLAGS)
	cp slaveip  $(debug_folder)

eaglehost: eaglehost.c
	$(CC) $(CFLAGS) -o $@ eaglehost.c $(LDFLAGS)
	cp eaglehost  $(debug_folder)

rawlisten:
	$(CC) -g -o rawlisten rawlisten.c
	cp rawlisten  $(debug_folder)

rawsend:
	$(CC) -g -o rawsend rawsend.c
	cp rawsend  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f slaveip
	rm -f eaglehost


