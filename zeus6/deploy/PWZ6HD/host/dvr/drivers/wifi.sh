#!/bin/sh

insmod /mnt/nand/dvr/drivers/crc-itu-t.ko
insmod /mnt/nand/dvr/drivers/rfkill.ko
insmod /mnt/nand/dvr/drivers/cfg80211.ko
insmod /mnt/nand/dvr/drivers/mac80211.ko
insmod /mnt/nand/dvr/drivers/ath.ko
insmod /mnt/nand/dvr/drivers/ath9k_hw.ko
insmod /mnt/nand/dvr/drivers/ath9k_common.ko
insmod /mnt/nand/dvr/drivers/ath9k_htc.ko
insmod /mnt/nand/dvr/drivers/rt2x00lib.ko
insmod /mnt/nand/dvr/drivers/rt2x00usb.ko
insmod /mnt/nand/dvr/drivers/rt73usb.ko

