#!/bin/sh

#EAGLE34
ln -sf /dav /davinci

PATH=/bin:/sbin:/usr/bin:/usr/sbin:/davinci:/davinci/dvr
export PATH

if [ -d /var/dvr ]; then
	exit 1
fi

# make working directory
# mount -t ramfs ramfs /var
mkdir /var/dvr
mkdir /var/dvr/disks

#clean some ram disk space. (hikvison files)
# rm -r /opt/*

# reboot utility (the original one on zeus3 doesn't work)
ln -sf /davinci/dvr/reboot.sh /sbin/reboot
ln -sf /davinci/dosfsck /sbin/fsck.vfat
ln -sf /davinci/mkdosfs /sbin/mkdosfs

# setup DVR configure file
mkdir /etc/dvr
ln -sf /davinci/dvr/dvrsvr.conf /etc/dvr/dvr.conf

# set debugging ip address
boardid=`cat /davinci/ID/BOARDID`
ifconfig eth0 192.168.243.${boardid}

# setup initial TZ environment
cd /davinci/dvr
tzn=`./cfg get system timezone`
tzl=`./cfg get timezones ${tzn}`
TZ=${tzl%% *}
export TZ

# setup network ethernet ip address (as a alias)
setnetwork

ifconfig lo up 127.0.0.1

# install web server
mkdir /home/www
cd /home/www
/davinci/dvr/httpd.sfx
ln -sf /davinci/dvr/www/cgi cgi

#login password support
cd /davinci/dvr
cp /davinci/dvr/shadow /etc/
cp /davinci/dvr/inetd.conf /etc/

# restart inetd
echo "Restart inetd."
/etc/rc.d/rc3.d/S20inetd restart
#inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`
#kill -TERM ${inetdpid}
#sleep 1
#/usr/sbin/xinetd < /dev/null > /dev/null 2> /dev/null  &

cd /davinci/dvr

ln -sf /davinci/dvr/libeagle368dvr.so /usr/lib/libeagle368dvr.so

#smartftp support
#ln -sf /davinci/dvr/librt-0.9.28.so /lib/librt.so.0
#ln -sf /davinci/dvr/libcurl.so.4 /lib/libcurl.so.4
#ln -sf /lib/librt.so /lib/librt.so.0

#start eaglesvr
eaglesvr < /dev/null > /dev/null 2> /dev/null &

# start io module
ioprocess < /dev/null > /dev/null 2> /dev/null &
sleep 2
iowait

# start hotplug deamond
tdevd /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &

# start dvr server
dvrsvr < /dev/null > /dev/null 2> /dev/null &

sleep 3

# wifi driver
# /davinci/dvr/wifi_up

# mount disk already found
tdevmount /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &

sleep 20
glog < /dev/null > /dev/null 2> /dev/null &

