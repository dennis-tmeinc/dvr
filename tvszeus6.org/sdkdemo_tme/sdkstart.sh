PATH=/bin:/davinci:/etc

if [ -f "/home/pidfile" ]; then
	exit 1
fi

#cd /davinci

insmod /davinci/dsplinkk.ko ddr_start=0x83100080 ddr_size=0x00efff80

rm -f /dev/dsplink
mknod /dev/dsplink c `/bin/awk "\\$2==\"dsplink\" {print \\$1}" /proc/devices` 0

rm -f /dev/hikgpio
mknod /dev/hikgpio c `/bin/awk "\\$2==\"hikgpio\" {print \\$1}" /proc/devices` 0
/davinci/davinci

