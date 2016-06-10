CROSS_COMPILE := /usr/src/arm-linux-3.3/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/arm-unknown-linux-uclibcgnueabi-
AR = $(CROSS_COMPILE)ar
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
STRIP = $(CROSS_COMPILE)strip
