
# Makefile for Eagle32 DVR dvrsetup app 
#

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=

EXTHEADERS=
EXTSOURCES=
HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

CFLAGS= -g -Wall

all: cfgdown.cgi dvrstatus.cgi

cfgdown.cgi: cfgdown.c $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ cfgdown.c $(EXTSOURCES) $(LDFLAGS)
	cp cfgdown.cgi ~/nfsroot/eagletest/www

dvrstatus.cgi : dvrstatus.cpp $(EXTHEADERS) $(EXTSOURCES)
	$(CC) $(CFLAGS) -o $@ dvrstatus.cpp $(LDFLAGS)
	cp $@ ~/nfsroot/eagletest/www

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.cgi
	rm -f *.o


