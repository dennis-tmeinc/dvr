
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=rconn.cpp adbclient.cpp channel.cpp net.cpp

HEADERS=rconn.h net.h adbclient.h channel.h adbclient.h

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -DANDROID_CLIENT -DCHANNEL_BUFSIZE=8192
LDFLAGS= -lstdc++ -lpthread

rconn : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) -g $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@

%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f rconn
