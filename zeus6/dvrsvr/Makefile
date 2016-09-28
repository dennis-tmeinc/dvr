#
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=main.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
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
		config.cpp \
		event.cpp \
		live.cpp \
		crypt.cpp \
		dio.cpp \
		vri.cpp \
		fifoswap.cpp \
		archive.cpp \
		ipstream.cpp 

HEADERS=../cfg.h\
		dvr.h \
		genclass.h \
		cfg.h \
		fbwindow.h \
		crypt.h \
		mpegps.h \
		fifoswap.h \
		block.h  \
		record.h \
		dir.h \
		vri.h \
		archive.h \
		../ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS += -Os

ifeq ($(EAGLE),zeus8)
  LDFLAGS += -lpthread -lm -lstdc++ -lipcamerasdk -luuid -L./libzeus8 -leagle -lgm  
  
else
  LDFLAGS += -lpthread -lm -lstdc++ \
	-L./eagle8851 -leagle8851 -lxmlwrapper -lexpat -lmemmgr -lmsgreader \
	-lvideocap -lsharedbuffmgr -lframeratectrl -lfreetype \
	-lmassmemaccess -lh264enc -ljpegenc -lresize \
	-ldeinterlacer -limgbackendprocess -lpbengine -lvideodisp -lROISelector \
	-lipcamerasdk  -luuid -lcrypto

endif

all : dvrsvr 

# dvrsvr : $(OBJS) $(LIBRARY)
dvrsvr : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

md5 : md5.cpp
	$(CC) $(CFLAGS) -o md5 md5.cpp 

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f dvrsvr
