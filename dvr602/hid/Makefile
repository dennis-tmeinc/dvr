
# Makefile for Eagle32 DVR ioprocess app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=hid.cpp config.cpp string.cpp

HEADERS=cfg.h genclass.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

#LDFLAGS= -lstdc++ -L./eagle32 -lsdk -ldadsp ./eagle32/dsplink.lib
LDFLAGS= -lstdc++ 
CCFLAGS= -g


hid : $(OBJS) $(SOURCES_C)
	$(CC) $(CCFLAGS) -o $@ $(LDFLAGS) $(OBJS) -Wl,-rpath,.
	cp hid ~/nfsroot/


%.o : %.cpp $(HEADERS)
	$(CC) $(CCFLAGS) -Wall -c $< -o $@

clean :
	rm *.o
	rm libdraw.so
	rm fb
