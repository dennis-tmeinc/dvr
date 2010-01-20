#
# Makefile for Eagle32 DVR  application for slave eagle boards 
#
include ../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=main.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
		capture.cpp \
		eaglecapture.cpp \
		memory.cpp \
		string.cpp 

HEADERS=dvr.h genclass.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

# LIBRARY=libdio.so

LDFLAGS= -lpthread -lstdc++ -L../dvrsvr/eagle32 -lsdk -ldadsp  ../dvrsvr/eagle32/dsplink.lib -Wl,-rpath,.

# dvrsvr : $(OBJS) $(LIBRARY)
eaglecap : $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LDFLAGS)
	cp eaglecap  $(debug_folder)

%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -c $< -o $@

# libdio.so : dio.c
# 	$(CC) -g -fPIC -c -Wall -o dio.o dio.c
# 	$(CC) -g -shared -lpthread -Wl,-soname,libdio.so -o $@ dio.o -lc

clean :
	rm -f *.o
	rm -f eaglecap
