
# Makefile for Eagle32 DVR dvrsetup app
#

include ../../config
include $(PROJDIR)/arm_tool

SOURCES=

EXTHEADERS=../../dvrsvr/config.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ -lcrypt

CFLAGS+= -I../dvrsvr -Wall

all: getsetup applysetup tzoption

getsetup: getsetup.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ getsetup.cpp $(EXTSOURCES) $(LDFLAGS)

applysetup: applysetup.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ applysetup.cpp $(EXTSOURCES) ../../dvrsvr/crypt.cpp $(LDFLAGS)

tzoption: tzoption.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ tzoption.cpp $(EXTSOURCES) $(LDFLAGS)
	
%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f applysetup
	rm -f getsetup
	rm -f *.o


