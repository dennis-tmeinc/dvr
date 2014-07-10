################################################
# Generated by Code Generator Version 3.3.0.13 #
################################################
MODNAME="vpl_dmac"

rmmod vpl_dmac  > /dev/null 2>&1

rm /dev/$MODNAME  > /dev/null 2>&1

insmod vpl_dmac.ko

MAJOR=`cat /proc/devices | grep $MODNAME | sed -e 's/vpl_dmac//' | sed -e 's/ //'`

if test -z $MAJOR; then
	echo "The device specified is not found !"
else
	echo "Let's make a node here for $MODNAME with major number $MAJOR"
	mknod /dev/$MODNAME c $MAJOR 0
fi