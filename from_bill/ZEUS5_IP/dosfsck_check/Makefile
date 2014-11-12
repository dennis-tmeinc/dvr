
CROSS_COMPILE_PREFIX=arm-linux-
CC=$(CROSS_COMPILE_PREFIX)gcc

SOURCES=dosfsckcheck.cpp

HEADERS=


OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS= -g -Wall
LDFLAGS= -lstdc++


dosfsckcheck : $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $(OBJS)  $(LDFLAGS)

%.o : %.cpp 
	$(CC) $(DBGFLAGS) -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f dosfsckcheck
