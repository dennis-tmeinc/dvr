
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

EXTHEADERS=../cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

SOURCES=ioprocess.cpp diomap.cpp mcu.cpp cdc.cpp netdbg.cpp gforce.cpp iomisc.cpp yagf.cpp
HEADERS=diomap.h netdbg.h mcu.h cdc.h gforce.h iomisc.h yagf.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

ifeq ($(EAGLE),eagle32)
LDFLAGS= -lpthread -lstdc++ -L../eaglesvr/$(EAGLE) -lsdk -ldadsp ../eaglesvr/$(EAGLE)/dsplink.lib
else
LDFLAGS= -lpthread -lstdc++ -lrt -Wl,-rpath,.
endif

CFLAGS+= -I../dvrsvr -Wall
TARGET=ioprocess panelled devicepower buzzer iowait

all: $(TARGET)

ioprocess : $(OBJS) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS) -lm

panelled : panelled.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ panelled.o diomap.o $(EXTSOURCES) $(LDFLAGS)

devicepower : devicepower.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ devicepower.o diomap.o $(EXTSOURCES) $(LDFLAGS)

iowait : iowait.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ iowait.o diomap.o $(EXTSOURCES) $(LDFLAGS)

buzzer : buzzer.o diomap.o $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ buzzer.o diomap.o $(EXTSOURCES) $(LDFLAGS)

iodebug : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -DMCU_DEBUG -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS) -lm

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(OBJS)
	rm -f $(TARGET)


