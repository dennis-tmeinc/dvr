PATH=/bin:/davinci:/etc

if [ -f "/home/pidfile" ]; then
	exit 1
fi

cd /davinci

insmod ./dsplinkk.ko ddr_start=0x83100080 ddr_size=0x00efff80

rm -f /dev/dsplink
mknod /dev/dsplink c `/bin/awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0

rm -f /dev/hikgpio
mknod /dev/hikgpio c `/bin/awk "\\$2==\"hikgpio\" {print \\$1}" /proc/devices` 0

check_rs232
#cp /davinci/rt73.ko /home/
cp /davinci/rt73.bin /home/
cp /davinci/rt73sta.dat /home/
cp /davinci/iwconfig /home/
cp /davinci/iwlist /home/
cp /davinci/iwpriv /home/
#insmod /davinci/rt73.ko
#ifconfig rausb0 192.168.5.87 netmask 255.255.240.0 up
#route add -net 0.0.0.0 gw 192.168.5.1 netmask 0.0.0.0 dev rausb0

#iwconfig rausb0 essid babynet key off
cp /davinci/davinci /home/
#/bin/ppoed&
#/home/davinci&
if [ -f "/home/usage232" ]; then
	/home/davinci
else
	/home/davinci&
fi

