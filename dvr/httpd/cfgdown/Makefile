
# Makefile for Eagle32 DVR dvrsetup app
#

include ../../config
include $(PROJDIR)/arm_tool

SOURCES=

EXTHEADERS=../../dvrsvr/config.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp ../net.cpp
HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS=

CFLAGS+= -Wall -Wno-write-strings
TARGET=cfgdown.cgi dvrstatus.cgi cfgreport gcal.cgi gcrash.cgi

all: $(TARGET)

cfgdown.cgi: cfgdown.c $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ cfgdown.c $(LDFLAGS)

gcal.cgi : gcal.cpp ../../ioprocess/diomap.cpp ../../ioprocess/diomap.h
	$(CC) $(CFLAGS) -o $@ gcal.cpp ../../ioprocess/diomap.cpp $(LDFLAGS)

gcrash.cgi : gcrash.cpp ../../ioprocess/diomap.cpp ../../ioprocess/diomap.h
	$(CC) $(CFLAGS) -o $@ gcrash.cpp ../../ioprocess/diomap.cpp $(LDFLAGS)

dvrstatus.cgi : dvrstatus.cpp ../eagle_setup/getquery.cpp $(EXTHEADERS) $(EXTSOURCES) ../../ioprocess/diomap.cpp ../../ioprocess/diomap.h
	$(CC) $(CFLAGS) -o $@ dvrstatus.cpp ../net.cpp ../eagle_setup/getquery.cpp ../../ioprocess/diomap.cpp $(LDFLAGS)

cfgreport : cfgreport.cpp $(EXTHEADERS) $(EXTSOURCES) ../../ioprocess/diomap.cpp ../../ioprocess/diomap.h
	$(CC) $(CFLAGS) -o $@ cfgreport.cpp ../../ioprocess/diomap.cpp $(EXTSOURCES) $(LDFLAGS)  -lstdc++

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -f *.o


