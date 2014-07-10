#!/bin/sh

insmod crc-itu-t.ko
insmod led-class.ko
insmod compat.ko
insmod compat_firmware_class.ko
insmod rfkill_backport.ko
insmod cfg80211.ko
insmod mac80211.ko
insmod ath.ko
insmod ath9k_hw.ko
insmod ath9k_common.ko
insmod ath9k.ko
insmod ath9k_htc.ko
insmod rt2x00lib.ko
insmod rt2x00usb.ko
insmod rt73usb.ko
#insmod /drivers/cw1200_core.ko
#insmod /drivers/cw1200_wlan_sdio.ko

