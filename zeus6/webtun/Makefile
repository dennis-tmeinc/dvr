
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=webtun.cpp

HEADERS=

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -I.
LDFLAGS= -Lcurl -lcurl -lstdc++

webtun : $(OBJS) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@

testcurl : testcurl.cpp $(EXTSOURCES) $(EXTHEADERS)
	gcc -o $@ testcurl.cpp $(EXTSOURCES) -I. -lcurl -lstdc++
	
%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f webtun
