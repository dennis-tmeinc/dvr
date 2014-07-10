#!/bin/sh

insmod /drivers/led-class.ko
insmod /drivers/compat.ko
#insmod /drivers/compat_firmware_class.ko
insmod /drivers/rfkill_backport.ko
insmod /drivers/cfg80211.ko
insmod /drivers/mac80211.ko
insmod /drivers/ath.ko
insmod /drivers/ath9k_hw.ko
insmod /drivers/ath9k_common.ko
insmod /drivers/ath9k.ko
#insmod /drivers/cw1200_core.ko
#insmod /drivers/cw1200_wlan_sdio.ko

