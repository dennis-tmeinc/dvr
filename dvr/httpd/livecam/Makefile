
# Makefile for Eagle32 DVR web base setup app
#

include ../../config
include $(PROJDIR)/arm_tool

#SOURCES=eagle_setup.cpp md5.cpp
SOURCES=livecam.cpp ../../dvrsvr/md5.cpp ../net.cpp

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp ../../dvrsvr/crypt.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS+= -I../dvrsvr -Wall

mp4live : livecam.cpp ../eagle_setup/getquery.cpp ../net.cpp
	$(CC) $(CFLAGS) -o $@ livecam.cpp ../net.cpp ../eagle_setup/getquery.cpp $(LDFLAGS) -lcrypt
	cp $@ $(debug_folder)/www


%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f mp4live
	rm -f $(OBJS)
	rm -f *.o


