#
# Makefile for Eagle32 DVR server app 
#

TOOLROOT=/mnt/udu/Dennis/zeus8/toolchain_gnueabi-4.4.0_ARMv5TE
CROSS_COMPILE := $(TOOLROOT)/usr/bin/arm-unknown-linux-uclibcgnueabi-
export LD_LIBRARY_PATH=$(TOOLROOT)/usr/lib

AR = $(CROSS_COMPILE)ar
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
STRIP = $(CROSS_COMPILE)strip

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
		dio.cpp

HEADERS=dvr.h \
		genclass.h \
		cfg.h \
		fbwindow.h \
		crypt.h \
        mpegps.h \
        block.h  \
		../ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS = -lpthread -lm -lstdc++ 

dvrsvr : $(OBJS) $(HEADERS)
	$(CC) -g -o $@ $(OBJS) $(LDFLAGS) -L./lib -leagle -lgm

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -O2  -I./include -c $< -o $@

clean :
	rm -f *.o
	rm -f dvrsvr
