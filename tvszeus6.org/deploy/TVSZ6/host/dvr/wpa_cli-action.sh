#!/bin/sh

CMD=$2
if [ "$CMD" = "CONNECTED" ]; then
	ifconfig wlan0 `cat /mnt/nand/dvr/wifi_ip` netmask `cat /mnt/nand/dvr/wifi_mask` up
	ifconfig wlan0 broadcast `cat /mnt/nand/dvr/wifi_broadcast`
fi