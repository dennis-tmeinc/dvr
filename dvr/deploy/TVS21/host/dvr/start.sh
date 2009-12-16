#!/bin/sh

PATH=/bin:/davinci:/davinci/dvr
export PATH

if [ -d /var/dvr ]; then
	exit 1
fi

#clean some ram disk space. (hikvison files)
rm -r /opt/*
rm /bin/chat /bin/pppd /bin/pppoe /bin/t1 /bin/update

# make working directory
mount -t ramfs ramfs /var
mkdir /var/dvr
mkdir /var/dvr/disks

# install davinci drivers
cd /davinci
insmod dsplinkk.ko ddr_start=0x83100080 ddr_size=0x00efff80

rm -f /dev/dsplink
mknod /dev/dsplink c `/bin/awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0

rm -f /dev/hikgpio
mknod /dev/hikgpio c `/bin/awk "\\$2==\"hikgpio\" {print \\$1}" /proc/devices` 0

check_rs232

fmupdate=0 

# check if we need to upgrade firmware
if [ -d /davinci/upgrade ]; then
    cd /davinci/upgrade
    for updfirmware in `ls` ; do 
	if [ -x /davinci/upgrade/${updfirmware} ]; then
		echo Upgrade firmware : ${updfirmware}
		mkdir /home/upgrade
		cd /home/upgrade
		/davinci/upgrade/${updfirmware}
		fmupdate=1
		cd /home
		rm -r upgrade
	fi
    done
    rm -r /davinci/upgrade
fi

if [ $fmupdate = 1 ]; then
    reboot
    sleep 10
    exit 1
fi

# set debugging ip address
boardid=`cat /davinci/ID/BOARDID`
ifconfig eth0 192.168.247.${boardid}

# setup network ethernet ip address (as a alias)
if [ -f /davinci/dvr/eth_ip ]; then
    ifconfig eth0:1 `cat /davinci/dvr/eth_ip`
fi

if [ -f /davinci/dvr/eth_mask ]; then
    ifconfig eth0:1 netmask `cat /davinci/dvr/eth_mask`
fi

# setup router
cd /davinci/dvr
for gw in `ls gateway*` ; do 
   route add default gw `cat $gw`   
done

#telnet support
cp /davinci/dvr/passwd /etc

# restart inetd
echo "Restart inetd."
inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`
kill -TERM ${inetdpid}
sleep 1
cp ./inetd.conf /etc/inetd.conf
/bin/inetd  < /dev/null > /dev/null 2> /dev/null

# ext3 file system modules
insmod /davinci/jbd.ko
insmod /davinci/mbcache.ko
insmod /davinci/ext2.ko
insmod /davinci/ext3.ko

# mount debugging disks
mount -t nfs 192.168.247.100:/home/dennis/nfsroot /mnt/nfs0 -o nolock

# see if I want to debug it
if [ -f /mnt/nfs0/eagletest/debugon ]; then
    echo Enter debugging mode.
    exit ;
fi

mkdir /etc/dvr

# setup DVR configure file
ln -sf /davinci/dvr/dvrsvr.conf /etc/dvr/dvr.conf

cd /davinci/dvr

# start io module
/davinci/dvr/ioprocess < /dev/null > /dev/null 2> /dev/null &

# start hotplug deamond
/davinci/dvr/tdevd /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &
# mount disk already found
/davinci/dvr/tdevmount /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null

# install usb-serial driver
insmod /davinci/usbserial.ko
insmod /davinci/mos7840.ko

# setup ip network for ipcamera board. (slave boards)
/davinci/dvr/eaglehost `cat /davinci/ID/BOARDNUM`

sleep 1

# start dvr server
/davinci/dvr/dvrsvr < /dev/null > /dev/null 2> /dev/null &

sleep 3

# wifi driver
# insmod /davinci/rt73.ko

# install web server
cd /home
mkdir www
cd www
/davinci/dvr/httpd.sfx

#smartftp support
ln -sf /davinci/dvr/librt-0.9.28.so /lib/librt.so.0

# start gps log process, because glog use ttyS0, so execute it
cd /davinci/dvr
/davinci/dvr/glog > /dev/null 2> /dev/null &

# do endless loop
while true ; do sleep 1000 ; done 

