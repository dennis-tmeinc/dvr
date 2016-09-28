# try 'make -d' to debug make rules

# $@: The filename of a rule's target
# $<: The name of the first dependency in a rule
# $^: Space-delimited list of all the dependencies in a rule
# $?: same as $^ but only those newer than the target

include ../config

LDFLAGS := -lpthread -lstdc++

SOURCE = tab102.cpp
EXTRASOURCE = ../dvrsvr/string.cpp ../dvrsvr/config.cpp
EXTRAHEADER = ../dvrsvr/genclass.h ../dvrsvr/cfg.h ../ioprocess/diomap.h

OUT = tab102
OBJS = tab102.o

$(OUT): $(SOURCE) $(EXTRASOURCE) $(EXTRAHEADER) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUT) $(SOURCE) $(EXTRASOURCE)

%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

string.o: ../dvrsvr/string.cpp ../dvrsvr/genclass.h
	$(CC) -c $(CFLAGS) $< -o $@

config.o: ../dvrsvr/config.cpp ../dvrsvr/cfg.h
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm -f $(OUT) *.o *~
