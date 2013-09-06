#!/bin/sh

#EAGLE34
ln -sf /dav /davinci

PATH=/bin:/davinci:/davinci/dvr
export PATH

if [ -d /var/dvr ]; then
	exit 1
fi

# make working directory
mount -t ramfs ramfs /var
mkdir /var/dvr
mkdir /var/dvr/disks

#clean some ram disk space. (hikvison files)
rm -r /opt/*

# setup DVR configure file
mkdir /etc/dvr
ln -sf /davinci/dvr/dvrsvr.conf /etc/dvr/dvr.conf

# install davinci drivers

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
ifconfig eth0 172.19.112.68

# setup initial TZ environment
cd /davinci/dvr
tzn=`./cfg get system timezone`
tzl=`./cfg get timezones ${tzn}`
TZ=${tzl%% *}
export TZ

# setup network ethernet ip address (as a alias)
setnetwork

ifconfig lo up 127.0.0.1

#telnet support
cp /davinci/dvr/passwd /etc

# install web server
mkdir /home/www
cd /home/www
/davinci/dvr/httpd.sfx
ln -sf /davinci/dvr/www/cgi cgi

# restart inetd
echo "Restart inetd."
inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`
kill -TERM ${inetdpid}
sleep 1
/bin/inetd  /davinci/dvr/inetd.conf < /dev/null > /dev/null 2> /dev/null &

#smartftp support
#ln -sf /davinci/dvr/librt-0.9.28.so /lib/librt.so.0
ln -sf /davinci/dvr/libcurl.so.4 /lib/libcurl.so.4
ln -sf /lib/librt.so /lib/librt.so.0

#start eaglesvr
/davinci/dvr/eaglesvr < /dev/null > /dev/null 2> /dev/null &

# start io module
/davinci/dvr/ioprocess < /dev/null > /dev/null 2> /dev/null &
sleep 2
/davinci/dvr/iowait

# ext3 file system modules
insmod /davinci/jbd.ko
insmod /davinci/mbcache.ko
#insmod /davinci/ext2.ko
insmod /davinci/ext3.ko

# start hotplug deamond
tdevd /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &

# setup ip network for ipcamera board. (slave boards)
eaglehost `cat /davinci/ID/BOARDNUM`

# start dvr server
dvrsvr < /dev/null > /dev/null 2> /dev/null &

sleep 3

# wifi driver
# /davinci/dvr/wifi_up

# mount disk already found
tdevmount /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &

sleep 100
# install usb-serial driver
insmod /davinci/usbserial.ko
insmod /davinci/mos7840.ko
sleep 20
glog < /dev/null > /dev/null 2> /dev/null &

# do endless loop
while true ; do sleep 1000 ; done
