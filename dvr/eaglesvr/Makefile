#
# Makefile for Eagle32 DVR server app
#
include ../config
include $(PROJDIR)/arm_tool

SOURCES=	eaglesvr.cpp \
		capture.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
		draw.cpp \
		screen.cpp \
		../dvrsvr/memory.cpp \
		../dvrsvr/string.cpp \
		../lzmasdk/decx.cpp \
		../lzmasdk/LzmaDec.c


LIBSOURCES=	capture.cpp \
		dvrtime.cpp \
		draw.cpp \
		screen.cpp \
		../dvrsvr/memory.cpp \
		../dvrsvr/string.cpp \
		../lzmasdk/decx.cpp \
		../lzmasdk/LzmaDec.c

LDFLAGS=  -lpthread -lstdc++

HEADERS=eaglesvr.h \
		draw.h \
		../dvrsvr/genclass.h \
		../dvrsvr/dvrprotocol.h \
		../dvrsvr/memory.h \
	../cfg.h

ifeq ($(EAGLE),zeus3)
SOURCES += eagle368.cpp
LIBSOURCES += eagle368.cpp
LDFLAGS += -L./eagle368 -leagle368dvr -lrt
else
# eagle34 or eagle32
SOURCES += eaglecapture.cpp
LIBSOURCES += eagle368.cpp
ifeq ($(EAGLE),eagle34)
SOURCES += ../dvrsvr/cppinitpatch.cpp
endif
LDFLAGS += -L./$(EAGLE) -lsdk -ldadsp  ./$(EAGLE)/dsplink.lib -Wl,-rpath,.
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

lib: libeaglesvr.so

libeaglesvr.so : $(LIBSOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -fpic -shared -Wall $(LIBSOURCES) -Wl,-soname,libeaglesvr.so -o $@
	cp $@  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

strip : eaglesvr
	$(STRIP)  eaglesvr

clean :
	rm -f *.o
	rm -f eaglesvr libeaglesvr.so
