#!/bin/sh

export LD_LIBRARY_PATH=/davinci/dvr/lib
export PATH=/davinci/dvr:/davinci:/bin:/sbin:/usr/bin:/usr/sbin

cd /

hostname `cfg get system hostname`
if [ ! -f /etc/TZ ] ; then
	timezone=`cfg get system timezone`
	tzx=`cfg get timezones $timezone`
	echo ${tzx%% *} > /etc/TZ
fi

ifconfig lo 127.0.0.1

#preset ether net address (use default)
ifconfig eth0 192.168.1.100 netmask 255.255.255.0

setnetwork

# start io module
echo "Start IO PROCESS"
zdaemon ioprocess

echo "start hotplug"
# start hotplug deamond
zdaemon tdevd /mnt/nand/dvr/tdevhotplug
# mount disk already found
zdaemon tdevmount /mnt/nand/dvr/tdevhotplug 

echo "start dvr"
# start dvr server
zdaemon dvrsvr

# start remote access
echo "start rconn"
zdaemon rconn

# to restart wpa_supplicant
killall wpa_supplicant
killall wpa_cli

sleep 2

echo "ctrl_interface=/var/run/wpa_supplicant" > /var/run/wpa.conf
wpa_supplicant -Dwext -iwlan0 -c/var/run/wpa.conf -B
wpa_cli -B -i wlan0 -a /mnt/nand/dvr/wpa_cli-action.sh

#link web server 
#mkdir /home/www
#ln -sf /davinci/dvr/eaglehttpd /home/www/eaglehttpd
#ln -sf /davinci/dvr/www/cgi /home/www/cgi
mkdir /var/www
ln -sf /davinci/dvr/www/cgi /var/www/cgi

# restart inetd (not necessary now !!!)
# echo "Restart inetd."
# inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`
# kill -TERM ${inetdpid}
# sleep 2
# zdaemon inetd /davinci/etc/inetd.conf
# echo "inetd restarted."
# sleep 1
