
include ../config

SOURCES=dosfsckcheck.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++


dosfsckcheck : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)  $(LDFLAGS)

%.o : %.cpp
	$(CC) $(CFLAGS) -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f dosfsckcheck
