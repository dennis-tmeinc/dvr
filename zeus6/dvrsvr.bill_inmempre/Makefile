#
# Makefile for Eagle32 DVR server app 
#

CROSS_COMPILE_PREFIX=arm-linux-
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
                mpegps.h \
                block.h  \
		../ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

# LIBRARY=libdio.so

#LDFLAGS= -lpthread -lstdc++ -L./eagle8851 -leagle8851 libfbdraw.so -Wl,-rpath,.

LDFLAGS = -lpthread -lm -lstdc++ \
	-L./eagle8851 -leagle8851 libfbdraw.so -lxmlwrapper -lexpat -lmemmgr -lmsgreader \
	-lvideocap -lsharedbuffmgr -lframeratectrl -lfreetype \
	-lmassmemaccess -lh264enc -ljpegenc -lresize \
	-ldeinterlacer -limgbackendprocess -lpbengine -lvideodisp \
        -lROISelector 

# dvrsvr : $(OBJS) $(LIBRARY)
dvrsvr : $(OBJS)
	$(CC) -g -o $@ $(OBJS) $(LDFLAGS)
	#cp dvrsvr /home/jianpeng/eagle34/dvrtest/


md5 : md5.cpp
	$(CC) -g -o md5 md5.cpp $(LDFLAGS)
#	cp md5 /home/jianpeng/eagle34/dvrtest/


%.o : %.cpp $(HEADERS)
	$(CC) -g -Wall -O2 -c $< -o $@

# libdio.so : dio.c
# 	$(CC) -g -fPIC -c -Wall -o dio.o dio.c
# 	$(CC) -g -shared -lpthread -Wl,-soname,libdio.so -o $@ dio.o -lc

clean :
	rm -f *.o
	rm -f dvrsvr
