
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=webtun.cpp

HEADERS=

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../rconn/channel.h ../rconn/net.h
EXTSOURCES=../dvrsvr/config.cpp ../rconn/rconn.cpp ../rconn/channel.cpp ../rconn/net.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -I../rconn
LDFLAGS= -Lcurl -lcurl -lstdc++ -lpthread

webtun : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -DRCMAIN -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@

testcurl : testcurl.cpp $(EXTSOURCES) $(EXTHEADERS)
	gcc -o $@ testcurl.cpp $(EXTSOURCES) -I. -lcurl -lstdc++
	
%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f webtun
