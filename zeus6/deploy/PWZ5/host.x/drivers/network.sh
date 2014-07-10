#!/bin/sh

ifconfig wlan0 up
iwconfig wlan0 essid PWTEST3
ifconfig wlan0 192.168.3.221 netmask 255.255.255.0

