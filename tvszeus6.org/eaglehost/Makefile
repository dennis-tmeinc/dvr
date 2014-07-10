
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

LDFLAGS=

all: slaveip eaglehost

slaveip: slaveip.c
	$(CC) $(CFLAGS) -o $@ slaveip.c $(LDFLAGS)
#	cp slaveip /home/jianpeng/eagle34/dvrtest/

eaglehost: eaglehost.c
	$(CC) $(CFLAGS) -o $@ eaglehost.c $(LDFLAGS)
#	cp eaglehost /home/jianpeng/eagle34/dvrtest/

rawlisten:
	$(CC) $(CFLAGS) -o rawlisten rawlisten.c
#	cp rawlisten /home/jianpeng/eagle34/dvrtest/

rawsend:
	$(CC) $(CFLAGS) -o rawsend rawsend.c
#	cp rawsend /home/jianpeng/eagle34/dvrtest/

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f slaveip
	rm -f eaglehost


