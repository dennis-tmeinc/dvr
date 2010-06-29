#
# Makefile for Eagle32 DVR server app 
#
include ../arm_tool

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
		screen.cpp \
		md5.cpp

HEADERS=dvr.h \
		genclass.h \
		cfg.h \
		fbwindow.h \
		crypt.h \
        ../ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

# LIBRARY=libdio.so

CCFLAGS= -Wall -g
LDFLAGS= -lpthread -lstdc++ -L./$(EAGLE) -lsdk -ldadsp  ./$(EAGLE)/dsplink.lib -Wl,-rpath,.


all : dvrsvr

# dvrsvr : $(OBJS) $(LIBRARY)
dvrsvr : $(OBJS) ../fb/libfbdraw.so
	$(CC)  $(CCFLAGS) -o $@  $(OBJS) ../fb/libfbdraw.so $(LDFLAGS)
	cp dvrsvr  $(debug_folder)

../fb/libfbdraw.so : 
	cd ../fb ; make

eagleinit : eagleinit.cpp
	$(CC) $(CCFLAGS) -o $@ eagleinit.cpp $(LDFLAGS)
	cp eagleinit  $(debug_folder)

md5 : md5.cpp
	$(CC) -g -o md5 md5.cpp $(LDFLAGS)
	cp md5  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

# libdio.so : dio.c
# 	$(CC) -g -fPIC -c -Wall -o dio.o dio.c
# 	$(CC) -g -shared -lpthread -Wl,-soname,libdio.so -o $@ dio.o -lc

clean :
	rm -f *.o
	rm -f dvrsvr
