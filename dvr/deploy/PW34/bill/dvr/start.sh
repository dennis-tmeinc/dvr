#!/bin/sh

PATH=/bin:/dav:/dav/dvr
export PATH

if [ -d /var/dvr ]; then
	exit 1
fi

#clean some ram disk space. (hikvison files)
rm -r /opt/*

# make working directory
#mount -t ramfs ramfs /var
mkdir /var/dvr
mkdir /var/dvr/disks

#check_rs232

fmupdate=0 

# check if we need to upgrade firmware
if [ -d /dav/upgrade ]; then
    cd /dav/upgrade
    for updfirmware in `ls` ; do 
	if [ -x /dav/upgrade/${updfirmware} ]; then
		echo Upgrade firmware : ${updfirmware}
		mkdir /home/upgrade
		cd /home/upgrade
		/dav/upgrade/${updfirmware}
		fmupdate=1
		cd /home
		rm -r upgrade
	fi
    done
    rm -r /dav/upgrade
fi

if [ $fmupdate = 1 ]; then
    reboot
    sleep 10
    exit 1
fi

# set debugging ip address
#boardid=`cat /davinci/ID/BOARDID`
#ifconfig eth0 192.168.247.${boardid}

# setup network ethernet ip address (as a alias)
if [ -f /dav/dvr/eth_ip ]; then
    ifconfig eth0 `cat /dav/dvr/eth_ip`
fi

if [ -f /dav/dvr/eth_mask ]; then
    ifconfig eth0 netmask `cat /dav/dvr/eth_mask`
fi
ifconfig lo 127.0.0.1

#copy web files
mkdir /home/www
cd /home/www
# install web server
/dav/dvr/httpd.sfx

# setup router
cd /dav/dvr
for gw in `ls gateway*` ; do 
   route add default gw `cat $gw`   
done

#telnet support
cp /dav/dvr/passwd /etc

# restart inetd
echo "Restart inetd."
inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`


kill -TERM ${inetdpid}
sleep 1
cp ./inetd.conf /etc/inetd.conf
/bin/inetd &  < /dev/null > /dev/null 2> /dev/null
echo "inetd done"

# ext3 file system modules
insmod /dav/jbd.ko
insmod /dav/mbcache.ko
insmod /dav/ext3.ko

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
ln -sf /dav/dvr/dvrsvr.conf /etc/dvr/dvr.conf

cd /dav/dvr

# start io module
/dav/dvr/ioprocess < /dev/null > /dev/null 2> /dev/null &

echo "start hotplug"
# start hotplug deamond
/dav/dvr/tdevd /dav/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &
# mount disk already found
/dav/dvr/tdevmount /dav/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null

# install usb-serial driver
#insmod /dav/usbserial.ko
#insmod /dav/mos7840.ko

# setup ip network for ipcamera board. (slave boards)
/dav/dvr/eaglehost `cat /dav/ID/BOARDNUM`

#sleep 2
# set Tab102 RTC and check data to download
#/dav/dvr/tab102 -rtc >/dev/null 2>&1

sleep 4
echo "start dvr"
# start dvr server
/dav/dvr/dvrsvr < /dev/null > /dev/null 2> /dev/null &

sleep 3

# wifi driver
insmod /dav/rt73.ko

#smartftp support
#ln -sf /dav/dvr/librt-0.9.28.so /lib/librt.so.0

# start gps log process, because glog use ttyS0, so execute it
cd /dav/dvr
exec /dav/dvr/glog > /dev/null 2> /dev/null
