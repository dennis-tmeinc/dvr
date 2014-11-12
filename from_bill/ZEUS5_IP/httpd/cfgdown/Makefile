
# Makefile for Eagle32 DVR dvrsetup app 
#

CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=

EXTHEADERS=
EXTSOURCES=
HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

CFLAGS= -g -Wall -O2

all: cfgdown.cgi dvrstatus.cgi

cfgdown.cgi: cfgdown.c $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ cfgdown.c $(EXTSOURCES) $(LDFLAGS)
#	cp cfgdown.cgi /home/jianpeng/eagle34/dvrtest/www/

dvrstatus.cgi : dvrstatus.cpp $(EXTHEADERS) $(EXTSOURCES)
	$(CC) $(CFLAGS) -o $@ dvrstatus.cpp $(LDFLAGS)
#	cp $@ /home/jianpeng/eagle34/dvrtest/www/

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.cgi
	rm -f *.o


