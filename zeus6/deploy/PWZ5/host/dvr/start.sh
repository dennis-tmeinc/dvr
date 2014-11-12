#!/bin/sh

#PATH=/bin:/sbin:/usr/bin:/usr/sbin:/mnt/nand:/mnt/nand/dvr
#export PATH

ifconfig lo 127.0.0.1

#preset ether net address
ifconfig eth0 192.168.42.42 netmask 255.255.255.0

#copy web files
mkdir /home/www
cd /home/www
# install web server
/mnt/nand/dvr/httpd.sfx

# restart inetd
#echo "Restart inetd."
inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`

kill -TERM ${inetdpid}
sleep 1
/davinci/xinetd &  < /dev/null > /dev/null 2> /dev/nul

echo "inetd done"

mkdir /etc/dvr

# setup DVR configure file
ln -sf /mnt/nand/dvr/dvrsvr.conf /etc/dvr/dvr.conf

# start io module
#echo "Start IO PROCESS"
ioprocess

echo "start hotplug"
# start hotplug deamond
/mnt/nand/dvr/tdevd /mnt/nand/dvr/tdevhotplug 
# mount disk already found
/mnt/nand/dvr/tdevmount /mnt/nand/dvr/tdevhotplug 

sleep 4
echo "start dvr"
cp /mnt/nand/dvr/libfbdraw.so /usr/lib
# start dvr server
dvrsvr  

sleep 5

# wifi driver
#insmod /dav/rt73.ko

#smartftp support
#ln -sf /dav/dvr/librt-0.9.28.so /lib/librt.so.0
cp /mnt/nand/dvr/libcurl.so.4 /usr/lib




