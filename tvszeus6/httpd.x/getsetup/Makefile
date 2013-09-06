
# Makefile for Eagle32 DVR dvrsetup app
#

include ../../config

SOURCES=

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ -lcrypt

CFLAGS += -I../dvrsvr

all: getsetup applysetup

getsetup: getsetup.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ getsetup.cpp $(EXTSOURCES) $(LDFLAGS)
#	cp getsetup /home/jianpeng/eagle34/dvrtest/www/

applysetup: applysetup.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ applysetup.cpp $(EXTSOURCES) ../../dvrsvr/crypt.cpp $(LDFLAGS)
#	cp applysetup /home/jianpeng/eagle34/dvrtest/www/

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f applysetup
	rm -f getsetup
	rm -f *.o


