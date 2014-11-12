#!/bin/sh

#PATH=/bin:/sbin:/usr/bin:/usr/sbin:/mnt/nand:/mnt/nand/dvr
#export PATH

#if [ -d /var/dvr ]; then
#	exit 1
#fi


# make working directory
#mkdir /var/dvr
#mkdir /var/dvr/disks


# set debugging ip address
#boardid=`cat /davinci/ID/BOARDID`
#ifconfig eth0 192.168.247.${boardid}

# setup network ethernet ip address (as a alias)
#if [ -f /mnt/nand/dvr/eth_ip ]; then
#    ifconfig eth0 `cat /mnt/nand/dvr/eth_ip`
#fi

#if [ -f /mnt/nand/dvr/eth_mask ]; then
#    ifconfig eth0 netmask `cat /mnt/nand/dvr/eth_mask`
#    ifconfig eth0 broadcast `cat /mnt/nand/dvr/eth_broadcast`
#fi

#if [ -f /mnt/nand/dvr/broadcast ]; then
#    ifconfig eth0 broadcast `cat /mnt/nand/dvr/eth_broadcast`
#fi

ifconfig lo 127.0.0.1

#copy web files
mkdir /home/www
cd /home/www
# install web server
/mnt/nand/dvr/httpd.sfx

# setup router
cd /mnt/nand/dvr
#for gw in `ls gateway*` ; do 
#   route add default gw `cat $gw`   
#done

#telnet support
#cp /mnt/nand/dvr/passwd /etc

# restart inetd
#echo "Restart inetd."
inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`


kill -TERM ${inetdpid}
sleep 1
cp /mnt/nand/xinetd  /usr/sbin/
#/usr/sbin/inetd &  < /dev/null > /dev/null 2> /dev/null
/usr/sbin/xinetd &  < /dev/null > /dev/null 2> /dev/nul

echo "inetd done"

# ext3 file system modules

# mount debugging disks
#mount -t nfs 192.168.247.100:/home/dennis/nfsroot /mnt/nfs0 -o nolock
#mkdir /mnt/nfs
#mount -t nfs 192.168.1.106:/nfs /mnt/nfs -o nolock

# see if I want to debug it
#if [ -f /mnt/nfs0/eagletest/debugon ]; then
#    echo Enter debugging mode.
#    exit ;
#fi

mkdir /etc/dvr

# setup DVR configure file
ln -sf /mnt/nand/dvr/dvrsvr.conf /etc/dvr/dvr.conf

#cd /mnt/nand/dvr

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
cp /mnt/nand/dvr/libipcamerasdk.so /usr/lib
# start dvr server
dvrsvr  

sleep 5

# wifi driver
#insmod /dav/rt73.ko

#smartftp support
#ln -sf /dav/dvr/librt-0.9.28.so /lib/librt.so.0
cp /mnt/nand/dvr/libcurl.so.4 /usr/lib

#cd /mnt/nand/dvr

# start gps log process, because glog use ttyS0, so execute it
#exec /mnt/dvr/glog > /dev/null 2> /dev/null &
#sleep 6
#echo "start glog"
#glog



