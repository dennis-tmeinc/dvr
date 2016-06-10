#
# Makefile for Eagle32 DVR server app
#
include ../config
include $(PROJDIR)/arm_tool

#no eagle boards
ifeq ($(EAGLE),mv)
SOURCES=main.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
		msg.cpp \
		ptz.cpp \
		record.cpp \
		capture.cpp \
		netcapture.cpp \
		memory.cpp \
		dvrfile.cpp \
		playback.cpp \
		string.cpp \
		disk.cpp \
		idx.cpp \
		config.cpp \
		event.cpp \
		live.cpp \
		crypt.cpp \
		dio.cpp \
		md5.cpp \
		fifoswap.cpp \
		screen.cpp

LDFLAGS= -lpthread -lstdc++ -lrt -Wl,-rpath,.

else
SOURCES=main.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
		ptz.cpp \
		recordbig.cpp \
		capture.cpp \
		netcapture.cpp \
		memory.cpp \
		dvrfilebig.cpp \
		playback.cpp \
		string.cpp \
		disk.cpp \
		idx.cpp \
		config.cpp \
		event.cpp \
		crypt.cpp \
		dio.cpp \
		fifoswap.cpp \
		screen.cpp \
		md5.cpp \
		cppinitpatch.cpp \
		draw.cpp

#LDFLAGS= -lpthread -lstdc++ -L./$(EAGLE) -lsdk -ldadsp  ./$(EAGLE)/dsplink.lib -Wl,-rpath,.
LDFLAGS= -lpthread -lstdc++ -lrt

endif

EXTRASOURCES=../ioprocess/diomap.cpp ../lzmasdk/decx.cpp ../lzmasdk/LzmaDec.c

HEADERS=dvr.h \
		dvrprotocol.h \
		genclass.h \
		capture.h \
		netcapture.h \
		config.h \
		fbwindow.h \
		draw.h \
		fifoswap.h \
		crypt.h \
		dir.h \
	../ioprocess/diomap.h \
	../cfg.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

# LIBRARY=libdio.so

CFLAGS+= -Wall

all : dvrsvr

# dvrsvr : $(OBJS) $(LIBRARY)
#dvrsvr : $(OBJS) ../fb/libfbdraw.so
#	$(CC)  $(CFLAGS) -o $@  $(OBJS) ../fb/libfbdraw.so $(LDFLAGS)
#	cp dvrsvr  $(debug_folder)

dvrsvr : $(OBJS) $(EXTRASOURCES)
	$(CC)  $(CFLAGS) -o $@  $^ $(LDFLAGS)
	cp dvrsvr  $(debug_folder)

strip: dvrsvr
	$(STRIP) dvrsvr

../fb/libfbdraw.so :
	cd ../fb ; make

eagleinit : eagleinit.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	cp eagleinit  $(debug_folder)

md5 : md5.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	cp md5  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# libdio.so : dio.c
# 	$(CC) -g -fPIC -c -Wall -o dio.o dio.c
# 	$(CC) -g -shared -lpthread -Wl,-soname,libdio.so -o $@ dio.o -lc

clean :
	rm -f *.o
	rm -f dvrsvr
