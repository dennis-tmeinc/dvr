
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

SOURCES=ioprocess.cpp mcu.cpp mcucmd.cpp tab102.cpp serial.cpp 

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

#LDFLAGS= -lstdc++  -L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib
LDFLAGS= -lstdc++ -lpthread

CFLAGS+= -I../dvrsvr -Wno-write-strings

all: ioprocess panelled devicepower buzzer dvrlog dumpio

ioprocess : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
#	cp ioprocess /home/jianpeng/eagle34/dvrtest/

dumpio : dumpio.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ dumpio.o $(EXTSOURCES) $(LDFLAGS)

panelled : panelled.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ panelled.o $(EXTSOURCES) $(LDFLAGS)
#	cp panelled /home/jianpeng/eagle34/dvrtest/

devicepower : devicepower.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ devicepower.o $(EXTSOURCES) $(LDFLAGS)
#	cp devicepower /home/jianpeng/eagle34/dvrtest/

buzzer : buzzer.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ buzzer.o $(EXTSOURCES) $(LDFLAGS)
#	cp buzzer /home/jianpeng/eagle34/dvrtest/

dvrlog : dvrlog.o $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ dvrlog.o $(LDFLAGS)


%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f ioprocess panelled devicepower buzzer dvrlog


