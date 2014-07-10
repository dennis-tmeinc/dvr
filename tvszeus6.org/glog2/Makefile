# try 'make -d' to debug make rules

# $@: The filename of a rule's target
# $<: The name of the first dependency in a rule
# $^: Space-delimited list of all the dependencies in a rule
# $?: same as $^ but only those newer than the target

CROSS_COMPILE_PREFIX=~/proj/eagle32/hik_toolchain/bin/arm-linux-
CC := $(CROSS_COMPILE_PREFIX)gcc
CFLAGS := -Wall -O2
LDFLAGS := -lpthread -lstdc++

OUT = glog2 
OBJS = glog2.o string.o config.o # add here

$(OUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJS)

%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

string.o: ../dvrsvr/string.cpp ../dvrsvr/genclass.h
	$(CC) -c $(CFLAGS) $< -o $@

config.o: ../dvrsvr/config.cpp ../dvrsvr/cfg.h
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean: 
	rm -f $(OUT) *.o *~
