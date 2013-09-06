#!/bin/sh

# Changes:
# 09/09/2009 Harrison
#   1. Added Tab102 (post processing) support
#
# 09/25/2009 Harrison
#   1. changes required for console=ttyS2
#

PATH=/bin:/davinci:/davinci/dvr
export PATH

if [ -d /var/dvr ]; then
	exit 1
fi

#clean some ram disk space. (hikvison files)
rm -r /opt/*

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

# check if we need to reconfigure console port
/bin/ln -s /davinci/dvr/fw_printenv /davinci/dvr/fw_setenv
v_bootargs=`/davinci/dvr/fw_printenv bootargs`
old_bootargs=${v_bootargs#bootargs=}
old_console=${old_bootargs%%initrd*M}
old_initrd=${old_bootargs#console=*n8}

if [ $old_console = console=ttyS0,115200n8 ]; then
  new_bootargs="console=ttyS2,115200n8 "$old_initrd 
  echo $new_bootargs
  /davinci/dvr/fw_setenv bootargs $new_bootargs
  /bin/reboot
fi

mkdir /etc/dvr

# setup DVR configure file
ln -sf /davinci/dvr/dvrsvr.conf /etc/dvr/dvr.conf

# set debugging ip address
boardid=`cat /davinci/ID/BOARDID`
ifconfig eth0 192.168.247.${boardid}

# setup network ethernet ip address (as a alias)
ifconfig eth0:1 up `/davinci/dvr/cfg get network eth_ip`
ifconfig eth0:1 netmask `/davinci/dvr/cfg get network eth_mask`
route add default gw `/davinci/dvr/cfg get network gateway_1`

#telnet support
cp /davinci/dvr/passwd /etc

# restart inetd
echo "Restart inetd."
inetdpid=`ps | awk '$5 ~ /inetd/ {print $1}'`
kill -TERM ${inetdpid}
sleep 1
cp /davinci/dvr/inetd.conf /etc/inetd.conf
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

# install usb-serial driver
insmod /davinci/usbserial.ko
insmod /davinci/mos7840.ko

sleep 2

# seed random number gererator for udhcpc
ifconfig eth0 > /dev/urandom

cd /davinci/dvr

# in case dvrsrv.conf is corrupted
/davinci/dvr/cfgsync >/dev/null 2>&1

# start io module
echo 1 > /var/dvr/gforcecheck
/davinci/dvr/ioprocess

sleep 2

# check Tab102 data download
mknod /dev/ttyUSB0 c 188 0
mknod /dev/ttyUSB1 c 188 1
mknod /dev/ttyUSB2 c 188 2
mknod /dev/ttyUSB3 c 188 3

/davinci/dvr/tab102 -down >/dev/null 2>&1

# setup ip network for ipcamera board. (slave boards)
/davinci/dvr/eaglehost `cat /davinci/ID/BOARDNUM`

# wait until the gforce download is done
while [ -f /var/dvr/gforcecheck ]
do
  sleep 1
done

# remove mos7840
/bin/rmmod mos7840
/bin/rmmod usbserial
 
# start hotplug daemond
/davinci/dvr/tdevd /davinci/dvr/tdevhotplug
# mount disk already found
/davinci/dvr/tdevmount /davinci/dvr/tdevhotplug < /dev/null > /dev/null 2> /dev/null &

sleep 1

# copy files for Hybrid disks (and /var/dvr/*.log /var/dvr/*.peak files)
cd /davinci/dvr
/davinci/dvr/dvrsvr -d >/dev/null 2>&1

# start dvrsvr daemon
/davinci/dvr/dvrsvr

/bin/insmod /davinci/usbserial.ko
/bin/insmod /davinci/mos7840.ko
/davinci/dvr/tab102 -start >/dev/null 2>&1

disable_wifi=`/davinci/dvr/cfg get network disable_wifi`
if [ $disable_wifi -a $disable_wifi = "1" ]; then
  # don't use wifi during IGN on
  echo "wifi disabled until ignition off" >/dev/null
else
  # wifi driver
  insmod /davinci/rt73.ko
fi

# install web server
cd /home
mkdir www
cd www
/davinci/dvr/httpd.sfx

#smartftp support
ln -sf /davinci/dvr/librt-0.9.28.so /lib/librt.so.0

sleep 1

cd /davinci/dvr
/davinci/dvr/glog
/davinci/dvr/vter

# avoid problems related to console=ttyS2
echo "#" > /etc/profile
echo "#" > /home/start.sh
