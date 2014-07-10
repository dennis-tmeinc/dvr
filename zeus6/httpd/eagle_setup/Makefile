
# Makefile for Eagle32 DVR web base setup app
#

include ../../config

#SOURCES=eagle_setup.cpp md5.cpp
SOURCES=eagle_setup.cpp ../../dvrsvr/md5.cpp

EXTHEADERS=../../dvrsvr/config.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp ../../dvrsvr/crypt.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS+= -I../dvrsvr -Wall

TARGET= login\
	pagevalue\
	cfgupload\
	firmwareupd\
	mcufwupd\
	mfid


all: $(TARGET)

login : login.cpp getquery.cpp
	$(CC) $(CFLAGS) -o $@ login.cpp getquery.cpp $(LDFLAGS) -lcrypt

pagevalue : pagevalue.cpp getquery.cpp
	$(CC) $(CFLAGS) -o $@ pagevalue.cpp  getquery.cpp $(LDFLAGS)

cfgupload : cfgupload.cpp
	$(CC) $(CFLAGS) -o $@ cfgupload.cpp $(LDFLAGS)

firmwareupd : firmwareupd.cpp
	$(CC) $(CFLAGS) -o $@ firmwareupd.cpp $(LDFLAGS)

mcufwupd : mcufwupd.cpp
	$(CC) $(CFLAGS) -o $@ mcufwupd.cpp $(LDFLAGS)

mfid : mfid.cpp ../../dvrsvr/md5.cpp getquery.cpp
	$(CC) $(CFLAGS) -o $@ mfid.cpp ../../dvrsvr/md5.cpp getquery.cpp $(EXTSOURCES) $(LDFLAGS)

gangle.cgi : gangle.cpp getquery.cpp ../../ioprocess/diomap.cpp ../../ioprocess/diomap.h
	$(CC) $(CFLAGS) -o $@ gangle.cpp getquery.cpp ../../ioprocess/diomap.cpp -lm $(LDFLAGS)

getjpeg.cgi : getjpeg.cpp getquery.cpp ../net.cpp ../../dvrsvr/dvr.h
	$(CC) $(CFLAGS) -o $@ getjpeg.cpp getquery.cpp ../net.cpp  $(LDFLAGS)

eagle_setup : eagle_setup.cpp ../../dvrsvr/md5.cpp
	$(CC) $(CFLAGS) -o $@ eagle_setup.cpp ../../dvrsvr/md5.cpp $(EXTSOURCES) $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f $(TARGET)
	rm -f eagle_setup *.cgi
	rm -f $(OBJS)
	rm -f *.o

