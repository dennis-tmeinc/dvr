#
# Makefile for Eagle32 DVR server app
#
include ../config
include $(PROJDIR)/arm_tool

SOURCES=eaglesvr.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
		capture.cpp \
		draw.cpp \
		screen.cpp \
		../dvrsvr/memory.cpp \
		../dvrsvr/string.cpp \
		../lzmasdk/dec.cpp \
		../lzmasdk/LzmaDec.c

LDFLAGS=  -lpthread -lstdc++ -Wl,-rpath,.

HEADERS=eaglesvr.h \
		draw.h \
		../dvrsvr/genclass.h \
		../dvrsvr/dvrprotocol.h \
		../dvrsvr/memory.h \
	../cfg.h

ifeq ($(EAGLE),zeus3)
SOURCES += eagle368.cpp
LDFLAGS += -L./eagle368 -leagle368dvr -lrt
else
# eagle34 or eagle32
SOURCES += eaglecapture.cpp
ifeq ($(EAGLE),eagle34)
SOURCES += ../dvrsvr/cppinitpatch.cpp
endif
LDFLAGS += -L./$(EAGLE) -lsdk -ldadsp  ./$(EAGLE)/dsplink.lib
endif

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS+= -Wall -DSUPPORT_UNIXSOCKET

# dvrsvr : $(OBJS) $(LIBRARY)
#dvrsvr : $(OBJS) ../fb/libfbdraw.so
#	$(CC)  $(CFLAGS) -o $@  $(OBJS) ../fb/libfbdraw.so $(LDFLAGS)
#	cp dvrsvr  $(debug_folder)

eaglesvr : $(OBJS) $(HEADERS)
	$(CC)  $(CFLAGS) -o $@  $(SOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

strip : eaglesvr
	$(STRIP)  eaglesvr

clean :
	rm -f *.o
	rm -f eaglesvr
