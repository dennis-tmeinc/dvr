#!/bin/sh

mount -t usbfs none /proc/bus/usb
mount --bind /mnt/mtd/etc /etc
mount --bind /mnt/nand/dvr/lib/firmware /lib/firmware
ifconfig lo up
insmod /lib/modules/rtc-pt7c4307.ko
hwclock -s
sh /mnt/nand/vg_boot.sh NTSC
