#########################################################
#             Makefile for MOS7840   LINUX 2.6          # 
#########################################################


# For davinci

ARCH_TYPE = 0
HOST = arm-linux-
PLATFORM = ARM
#EXTRA_CFLAGS += -DARM_FIX

#KDIR:=/lib/modules/$(shell uname -r)/build
KDIR:=/home/dennis/proj/eagle32/driver/linux-headers
#MDIR:=/lib/modules/$(shell uname -r)/kernel/drivers/usb/serial

EXTRA_CFLAGS += -I$(KDIR)/drivers/usb/serial

EXTRA_CFLAGS += -I$(KDIR)/include

obj-m := usbserial.o

usbserial-objs := usb-serial.o generic.o bus.o

obj-m +=mos7840.o 

default: eagle32

eagle34:
	$(MAKE)  CROSS_COMPILE=arm_v5t_le- -C $(KDIR) $(EXTRA_CFLAGS) SUBDIRS=$(PWD) modules

eagle32:
	$(MAKE)  CROSS_COMPILE=arm-linux- -C $(KDIR) $(EXTRA_CFLAGS) SUBDIRS=$(PWD) modules

clean:
	rm -rf .tmp_versions Module.symvers *.mod.c *.o *.ko .*.cmd

load:
	modprobe usbserial
	insmod mos7840.ko

unload:
	rmmod mos7840

install:
	install -m 744 mos7840.ko $(MDIR)/mos7840.ko
	depmod -a
	cp -f /etc/modprobe.conf /etc/modprobe.conf~
	grep 'install mos7840' /etc/modprobe.conf || echo "install mos7840 /sbin/modprobe --ignore-install mos7840" >>/etc/modprobe.conf

uninstall:
	$(RM) $(MDIR)/mos7840.ko
	depmod -a
	cp -f /etc/modprobe.conf /etc/modprobe.conf~
	grep -v 'install mos7840' /etc/modprobe.conf~ >/etc/modprobe.conf


