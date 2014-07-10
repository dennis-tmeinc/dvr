#!/bin/sh

wifi_proto=`/mnt/nand/dvr/cfg get network wifi_proto`

killall udhcpc

case $2 in
CONNECTED)
  case $wifi_proto in
    "0")
        wifi_ip=`/mnt/nand/dvr/cfg get network wifi_ip`
        wifi_mask=`/mnt/nand/dvr/cfg get network wifi_mask`
        wifi_brd=`broadcast $wifi_ip $wifi_mask`
	gateway_dev=`cfg get network gateway_dev`
	gateway_addr=`cfg get network gateway_1`
	ifconfig $1 $wifi_ip netmask $wifi_mask broadcast $wifi_brd up
	if [ $gateway_dev = "1" ]
	then
	    route del default
	    route add default gw $gateway_addr $1
	fi
        ;;
    "1")
	udhcpc -i $1 -p /var/run/udhcpc.pid -s /mnt/nand/dvr/udhcpc.sh
        ;;
  esac
;;
esac
