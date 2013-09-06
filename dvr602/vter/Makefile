# try 'make -d' to debug make rules

# $@: The filename of a rule's target
# $<: The name of the first dependency in a rule
# $^: Space-delimited list of all the dependencies in a rule
# $?: same as $^ but only those newer than the target

PLATFORM=
PLATFORM=eagle32

ifeq ($(PLATFORM),eagle32)
MVTOOL_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
else
MVTOOL_PREFIX=
endif

CC := $(MVTOOL_PREFIX)gcc
CXX := $(MVTOOL_PREFIX)g++
CFLAGS := -Wall -O2 -I../libev

ifeq ($(PLATFORM),eagle32)
CFLAGS := -Wall -O2 -Ilibev
LDFLAGS :=  libev-eagle32.a -lstdc++ -lm
else
LDFLAGS := -L../libev/.libs-x86 --static -lev --dynamic -lstdc++
#LDFLAGS := -L../libev/.libs-x86 -lstdc++ -lev -Wl,-rpath=$(ORIGIN)
endif

OUT = vter
# add here
OBJS = vter.o crc.o debug.o token.o

$(OUT): $(OBJS)
#	$(CXX) -o $(OUT) $(OBJS) $(LDFLAGS) 
	$(CC) -o $(OUT) $(OBJS) $(LDFLAGS) 

#inetsock.o: lib/inetsock.c include/inetsock.h
#	$(CC) -c $(CFLAGS) -o inetsock.o lib/inetsock.c

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.cpp
#	$(CXX) -c $(CFLAGS) $< -o $@
	$(CC) -c $(CFLAGS)  $< -o $@

.PHONY: clean

clean: 
	rm -f $(OUT) *.o *~
