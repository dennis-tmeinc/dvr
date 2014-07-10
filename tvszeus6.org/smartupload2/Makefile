
include ../config

LDFLAGS := -lstdc++ -lpthread

OUT = smartftp
# add here
OBJS = smartftp.o debug.o crc.o md5.o parseconfig.o gforce_peak.o hikfile.o

$(OUT): $(OBJS)
#	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUT) $(OBJS)

#inetsock.o: lib/inetsock.c include/inetsock.h
#	$(CC) -c $(CFLAGS) -o inetsock.o lib/inetsock.c

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.cpp
#	$(CXX) -c $(CFLAGS) $< -o $@
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm -f $(OUT) *.o *~
