
# Makefile for Eagle32 DVR web base setup app
#

include ../../config
include $(PROJDIR)/arm_tool

#SOURCES=eagle_setup.cpp md5.cpp
SOURCES=eagle_setup.cpp ../../dvrsvr/md5.cpp

EXTHEADERS=../../dvrsvr/config.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp ../../dvrsvr/crypt.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS+= -I$(PROJDIR) -I../dvrsvr -Wall

TARGET= login\
	pagevalue\
	cfgupload\
	firmwareupd\
	mcufwupd\
	mfid\
	gangle.cgi\
	getjpeg.cgi


all: $(TARGET)

login : login.cpp getquery.o
	$(CC) $(CFLAGS) -o $@ login.cpp getquery.o $(LDFLAGS) -lcrypt

pagevalue : pagevalue.cpp getquery.o
	$(CC) $(CFLAGS) -o $@ pagevalue.cpp  getquery.o $(LDFLAGS)

cfgupload : cfgupload.cpp
	$(CC) $(CFLAGS) -o $@ cfgupload.cpp $(LDFLAGS)

firmwareupd : firmwareupd.cpp
	$(CC) $(CFLAGS) -o $@ firmwareupd.cpp $(LDFLAGS)

mcufwupd : mcufwupd.cpp
	$(CC) $(CFLAGS) -o $@ mcufwupd.cpp $(LDFLAGS)

mfid : mfid.cpp ../../dvrsvr/md5.cpp getquery.o
	$(CC) $(CFLAGS) -o $@ mfid.cpp ../../dvrsvr/md5.cpp getquery.o $(EXTSOURCES) $(LDFLAGS)

gangle.cgi : gangle.cpp getquery.o ../../ioprocess/diomap.cpp ../../ioprocess/diomap.h
	$(CC) $(CFLAGS) -o $@ gangle.cpp getquery.o ../../ioprocess/diomap.cpp -lm $(LDFLAGS)

getjpeg.cgi : getjpeg.cpp getquery.o ../../dvrsvr/dvr.h
	$(CC) $(CFLAGS) -o $@ getjpeg.cpp getquery.o $(LDFLAGS)

eagle_setup : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXTSOURCES) $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f $(TARGET)
	rm -f eagle_setup *.cgi
	rm -f $(OBJS)
	rm -f *.o


