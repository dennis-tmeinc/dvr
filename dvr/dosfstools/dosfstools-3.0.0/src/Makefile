# Makefile
#

OPTFLAGS = -O2 -fomit-frame-pointer -D__arm -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DPATH_MAX=4096
#WARNFLAGS = -Wall -pedantic -std=c99
WARNFLAGS = -Wall
DEBUGFLAGS = 
CFLAGS += $(OPTFLAGS) $(WARNFLAGS) $(DEBUGFLAGS)
CC = ~/proj/eagle32/hik_toolchain/bin/arm-linux-gcc

all: build

build: dosfsck dosfslabel mkdosfs

dosfsck: boot.c check.c common.c fat.c file.c io.c lfn.c dosfsck.c
	$(CC) $(CFLAGS) -o dosfsck boot.c check.c common.c fat.c file.c io.c lfn.c dosfsck.c

dosfslabel: boot.c check.c common.c fat.c file.c io.c lfn.c dosfslabel.c
	$(CC) $(CFLAGS) -o dosfslabel boot.c check.c common.c fat.c file.c io.c lfn.c dosfsck.c

mkdosfs: mkdosfs.c
	$(CC) $(CFLAGS) -o mkdosfs mkdosfs.c

