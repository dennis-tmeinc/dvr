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
		eaglecapture.cpp \
		screen.cpp \
		../dvrsvr/cppinitpatch.cpp \
		../dvrsvr/memory.cpp \
		../dvrsvr/string.cpp \
		../lzmasdk/dec.cpp \
		../lzmasdk/LzmaDec.c
		
LDFLAGS=  -lstdc++ -lpthread -L./$(EAGLE) -lsdk -ldadsp  ./$(EAGLE)/dsplink.lib

HEADERS=eaglesvr.h \
		draw.h \
		../dvrsvr/genclass.h \
		../dvrsvr/dvrprotocol.h \
        ../cfg.h
        
OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CCFLAGS= -Wall -g

# dvrsvr : $(OBJS) $(LIBRARY)
#dvrsvr : $(OBJS) ../fb/libfbdraw.so
#	$(CC)  $(CCFLAGS) -o $@  $(OBJS) ../fb/libfbdraw.so $(LDFLAGS)
#	cp dvrsvr  $(debug_folder)

eaglesvr : $(SOURCES) $(HEADERS)
	$(CC)  $(CCFLAGS) -o $@  $(SOURCES) $(LDFLAGS)
	cp $@  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) $(CCFLAGS) -c $< -o $@

# libdio.so : dio.c
# 	$(CC) -g -fPIC -c -Wall -o dio.o dio.c
# 	$(CC) -g -shared -lpthread -Wl,-soname,libdio.so -o $@ dio.o -lc

clean :
	rm -f *.o
	rm -f eaglesvr
