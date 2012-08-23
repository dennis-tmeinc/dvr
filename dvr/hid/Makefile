
# Makefile for Eagle32 DVR ioprocess app
#

include ../config
include $(PROJDIR)/arm_tool

SOURCES=hid.cpp config.cpp string.cpp

HEADERS=cfg.h genclass.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

#LDFLAGS= -lstdc++ -L./eagle32 -lsdk -ldadsp ./eagle32/dsplink.lib
LDFLAGS= -lstdc++
CFLAGS+= -Wall


hid : $(OBJS) $(SOURCES_C)
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(OBJS) -Wl,-rpath,.
	cp hid  $(debug_folder)


%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -Wall -c $< -o $@

clean :
	rm *.o
	rm libdraw.so
	rm fb
