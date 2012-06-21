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

# setup DVR configure file
mkdir /etc/dvr
ln -sf /davinci/dvr/dvrsvr.conf /etc/dvr/dvr.conf

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

# setup initial TZ environment
cd /davinci
tzn=`./cfg get system timezone`
tzl=`./cfg get timezones ${tzn}`
TZ=${tzl%% *}
export TZ

# setup network ethernet ip address (as a alias)
/davinci/dvr/setnetwork

#telnet support
cp /davinci/dvr/passwd /etc

# install web server
cd /home
mkdir www
cd www
/davinci/dvr/httpd.sfx
ln -sf /davinci/dvr/www/cgi cgi

# restart inetd
echo "Restart inetd."
inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`
kill -TERM ${inetdpid}
sleep 1
/bin/inetd /davinci/dvr/inetd.conf  < /dev/null > /dev/null 2> /dev/null

cd /davinci/dvr

./eaglesvr &

#smartftp support
ln -sf /davinci/dvr/librt-0.9.28.so /lib/librt.so.0

ln -sf /lib/libm.so /lib/libm.so.0

# start io module
/davinci/dvr/ioprocess < /dev/null > /dev/null 2> /dev/null &
sleep 1
/davinci/dvr/iowait

# ext3 file system modules
insmod /davinci/jbd.ko
insmod /davinci/mbcache.ko
insmod /davinci/ext2.ko
insmod /davinci/ext3.ko

# start hotplug deamond
/davinci/dvr/tdevd /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &
# setup ip network for ipcamera board. (slave boards)
/davinci/dvr/eaglehost `cat /davinci/ID/BOARDNUM`

# start dvr server
/davinci/dvr/dvrsvr < /dev/null > /dev/null 2> /dev/null &

# mount disk already found
/davinci/dvr/tdevmount /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &

sleep 3

# wifi driver
# insmod /davinci/rt73.ko

sleep 100
# install usb-serial driver
insmod /davinci/usbserial.ko
insmod /davinci/mos7840.ko
sleep 20
cd /davinci/dvr
/davinci/dvr/glog < /dev/null > /dev/null 2> /dev/null &

# do endless loop
while true ; do sleep 1000 ; done 

