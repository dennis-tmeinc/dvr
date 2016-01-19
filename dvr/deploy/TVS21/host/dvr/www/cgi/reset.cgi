#!/bin/sh
echo 
echo 
echo 
echo Rebooting System!
if [ -f /var/dvr/dvrsvr.pid ] ; then
   kill -USR1 `cat /var/dvr/dvrsvr.pid`
fi
sync
sync
/davinci/dvr/ioprocess -reboot 3 > /dev/null &
sleep 10
reboot
