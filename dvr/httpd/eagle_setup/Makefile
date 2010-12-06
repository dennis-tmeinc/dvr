
# Makefile for Eagle32 DVR web base setup app 
#

include ../../config
include $(PROJDIR)/arm_tool

#SOURCES=eagle_setup.cpp md5.cpp
SOURCES=eagle_setup.cpp ../../dvrsvr/md5.cpp

EXTHEADERS=../../dvrsvr/cfg.h ../../dvrsvr/genclass.h
EXTSOURCES=../../dvrsvr/config.cpp ../../dvrsvr/string.cpp ../../dvrsvr/crypt.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++

CFLAGS=-I../dvrsvr -g -Wall

TARGET= login\
        pagevalue\
        cfgupload\
        firmwareupd\
        mcufwupd\
        mfid\
        gangle.cgi
        

all: $(TARGET)

login : login.cpp getquery.o
	$(CC) $(CFLAGS) -o $@ login.cpp getquery.o $(LDFLAGS) -lcrypt
	cp $@ $(debug_folder)/www
	$(STRIP) $(debug_folder)/www/$@

pagevalue : pagevalue.cpp getquery.o
	$(CC) $(CFLAGS) -o $@ pagevalue.cpp  getquery.o $(LDFLAGS)
	cp $@ $(debug_folder)/www
	$(STRIP) $(debug_folder)/www/$@

cfgupload : cfgupload.cpp
	$(CC) $(CFLAGS) -o $@ cfgupload.cpp $(LDFLAGS)
	cp $@ $(debug_folder)/www
	$(STRIP) $(debug_folder)/www/$@

firmwareupd : firmwareupd.cpp
	$(CC) $(CFLAGS) -o $@ firmwareupd.cpp $(LDFLAGS)
	cp $@ $(debug_folder)/www
	$(STRIP) $(debug_folder)/www/$@

mcufwupd : mcufwupd.cpp
	$(CC) $(CFLAGS) -o $@ mcufwupd.cpp $(LDFLAGS)
	cp $@ $(debug_folder)/www
	$(STRIP) $(debug_folder)/www/$@

mfid : mfid.cpp ../../dvrsvr/md5.cpp getquery.o
	$(CC) $(CFLAGS) -o $@ mfid.cpp ../../dvrsvr/md5.cpp getquery.o $(LDFLAGS) $(EXTSOURCES)
	cp $@  $(debug_folder)/www
	$(STRIP) $(debug_folder)/www/$@

gangle.cgi : gangle.cpp getquery.o ../../ioprocess/diomap.cpp ../../ioprocess/diomap.h
	$(CC) $(CFLAGS) -o $@ gangle.cpp getquery.o ../../ioprocess/diomap.cpp -lm $(LDFLAGS)
	cp $@  $(debug_folder)/www

eagle_setup : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(EXTSOURCES)
	cp eagle_setup  $(debug_folder)/www
	$(STRIP) $(debug_folder)/www/$@

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f $(TARGET)
	rm -f eagle_setup *.cgi
	rm -f $(OBJS)
	rm -f *.o


