
# Makefile for Eagle32 DVR dvrsetup app 
#

include ../../arm_tool

CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp
HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

CFLAGS= -g -Wall

all: cfgdown.cgi dvrstatus.cgi cfgreport.cgi

cfgdown.cgi: cfgdown.c $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ cfgdown.c $(LDFLAGS)
	cp cfgdown.cgi  $(debug_folder)/www

dvrstatus.cgi : dvrstatus.cpp $(EXTHEADERS) $(EXTSOURCES)
	$(CC) $(CFLAGS) -o $@ dvrstatus.cpp $(LDFLAGS)
	cp $@  $(debug_folder)/www

cfgreport.cgi : cfgreport.cpp $(EXTHEADERS) $(EXTSOURCES)
	$(CC) $(CFLAGS) -o $@ cfgreport.cpp $(EXTSOURCES) $(LDFLAGS)  -lstdc++
	cp $@  $(debug_folder)/www

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.cgi
	rm -f *.o


