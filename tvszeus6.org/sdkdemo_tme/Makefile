#
# Makefile for v2pthread
#

SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h)

OBJS = $(patsubst %.c, %.o, $(SOURCES))

IFLAG 	= -I ./include -I .

TOOL_PREFIX = arm-linux-
#TOOL_PREFIX = arm_v5t_le-
COMPILE_TOOLS= $(TOOL_PREFIX)gcc
AR 		= $(TOOL_PREFIX)ar
RANLIB 		= $(TOOL_PREFIX)ranlib

CFLAGS = $(IFLAG) -D_REENTRANT -DDAVINCI -DDEV -O2 -Wall  

LDFLAGS= -L ./lib -lpthread -lsdk -ldadsp  ./lib/dsplink.lib 
LIB = davinci

all : $(LIB) 

$(LIB) : $(OBJS)
	$(COMPILE_TOOLS)  -o $@ $(OBJS)  $(LDFLAGS)
	cp $@ ../package_sdk_tme/davinci -rf
	cp $@ /home/work/davinci/filesys/davinci/ -rf
%.o:%.c
	$(COMPILE_TOOLS) $(CFLAGS) -c $^ -o $@

clean :
	rm -f *.o  $(LIB)

