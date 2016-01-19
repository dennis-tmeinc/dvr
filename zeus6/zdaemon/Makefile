
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=zdaemon.cpp

HEADERS=

EXTHEADERS= 
EXTSOURCES= 

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -I. 
LDFLAGS= -lstdc++ 

zdaemon : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@

%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f zdaemon
