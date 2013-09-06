#
# Makefile for Eagle32 DVR server app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=main.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
		msg.cpp \
		ptz.cpp \
		record.cpp \
		capture.cpp \
		eaglecapture.cpp \
		ipcapture.cpp \
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
		screen.cpp

HEADERS=dvr.h \
		genclass.h \
		cfg.h \
		fbwindow.h \
		crypt.h \
        ../ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

# LIBRARY=libdio.so

LDFLAGS= -lpthread -lstdc++ -L./eagle32 -lsdk -ldadsp  ./eagle32/dsplink.lib libfbdraw.so -Wl,-rpath,.

# dvrsvr : $(OBJS) $(LIBRARY)
dvrsvr : $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LDFLAGS)
	cp dvrsvr ~/nfsroot/eagletest/


md5 : md5.cpp
	$(CC) -g -o md5 md5.cpp $(LDFLAGS)
	cp md5 ~/nfsroot/eagletest/


%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

# libdio.so : dio.c
# 	$(CC) -g -fPIC -c -Wall -o dio.o dio.c
# 	$(CC) -g -shared -lpthread -Wl,-soname,libdio.so -o $@ dio.o -lc

clean :
	rm -f *.o
	rm -f dvrsvr
