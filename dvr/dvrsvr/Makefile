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
		md5.cpp \
		screen.cpp
		
LDFLAGS= -lpthread -lstdc++ -lrt -Wl,-rpath,.
		
else
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
		md5.cpp \
		draw.cpp
		
#LDFLAGS= -lpthread -lstdc++ -L./$(EAGLE) -lsdk -ldadsp  ./$(EAGLE)/dsplink.lib -Wl,-rpath,.
LDFLAGS= -lpthread -lstdc++ -L./$(EAGLE) -lsdk -ldadsp  ./$(EAGLE)/dsplink.lib
		
endif	

EXTRASOURCES=../ioprocess/diomap.cpp ../lzmasdk/dec.cpp ../lzmasdk/LzmaDec.c

EAGLESRS=eaglesvr.cpp \
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
		draw.h \
		crypt.h \
        ../ioprocess/diomap.h \
        ../cfg.h
        
OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

# LIBRARY=libdio.so

CCFLAGS= -Wall -g

all : dvrsvr

# dvrsvr : $(OBJS) $(LIBRARY)
#dvrsvr : $(OBJS) ../fb/libfbdraw.so
#	$(CC)  $(CCFLAGS) -o $@  $(OBJS) ../fb/libfbdraw.so $(LDFLAGS)
#	cp dvrsvr  $(debug_folder)

dvrsvr : $(OBJS) $(EXTRASOURCES)
	$(CC)  $(CCFLAGS) -o $@  $^ $(LDFLAGS)
	cp dvrsvr  $(debug_folder)

../fb/libfbdraw.so : 
	cd ../fb ; make

eaglesvr : $(EAGLESRS)
	$(CC)  $(CCFLAGS) -o $@ $(EAGLESRS) ../fb/libfbdraw.so $(LDFLAGS)
	cp eaglesvr  $(debug_folder)

eagleinit : eagleinit.cpp
	$(CC) $(CCFLAGS) -o $@ $< $(LDFLAGS)
	cp eagleinit  $(debug_folder)

md5 : md5.cpp
	$(CC) $(CCFLAGS) -o $@ $< $(LDFLAGS)
	cp md5  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

# libdio.so : dio.c
# 	$(CC) -g -fPIC -c -Wall -o dio.o dio.c
# 	$(CC) -g -shared -lpthread -Wl,-soname,libdio.so -o $@ dio.o -lc

clean :
	rm -f *.o
	rm -f dvrsvr
