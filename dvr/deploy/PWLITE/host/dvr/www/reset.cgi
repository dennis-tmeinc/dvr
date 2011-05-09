#!/bin/sh
echo 
echo 
echo 
echo Resetting System!
if [ -f /var/dvr/dvrsvr.pid ] ; then
   kill -USR1 `cat /var/dvr/dvrsvr.pid`
fi
sync
sync
/davinci/dvr/ioprocess -reboot 3 > /dev/null &
