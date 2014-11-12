
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

CFLAGS=-g

LDFLAGS= 

all: slaveip eaglehost

slaveip: slaveip.c 
	$(CC) $(CFLAGS) -o $@ slaveip.c $(LDFLAGS)
#	cp slaveip /home/jianpeng/eagle34/dvrtest/

eaglehost: eaglehost.c
	$(CC) $(CFLAGS) -o $@ eaglehost.c $(LDFLAGS)
#	cp eaglehost /home/jianpeng/eagle34/dvrtest/

rawlisten:
	$(CC) -g -o rawlisten rawlisten.c
#	cp rawlisten /home/jianpeng/eagle34/dvrtest/

rawsend:
	$(CC) -g -o rawsend rawsend.c
#	cp rawsend /home/jianpeng/eagle34/dvrtest/

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f slaveip
	rm -f eaglehost


