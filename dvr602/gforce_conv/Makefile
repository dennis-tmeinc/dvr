# try 'make -d' to debug make rules

# $@: The filename of a rule's target
# $<: The name of the first dependency in a rule
# $^: Space-delimited list of all the dependencies in a rule
# $?: same as $^ but only those newer than the target

PLATFORM=
PLATFORM=eagle32

ifeq ($(PLATFORM),eagle32)
TOOL_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
#TOOL_PREFIX=~/marvelSDK2.1/gcc/bin/arm-none-linux-gnueabi-
#TOOL_PREFIX=arm-mv5sft-linux-gnueabi-
else
TOOL_PREFIX=
endif

CC := $(TOOL_PREFIX)gcc
CXX := $(TOOL_PREFIX)g++
CFLAGS := -Wall -O2

ifeq ($(PLATFORM),eagle32)
CFLAGS := -Wall -g
LDFLAGS := -lstdc++  #-lpthread
else
LDFLAGS := -lstdc++ #-lpthread
endif

OUT = gforce_conv
# add here
OBJS = gforce_conv.o 

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
