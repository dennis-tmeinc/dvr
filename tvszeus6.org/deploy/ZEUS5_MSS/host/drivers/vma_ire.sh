################################################
# Generated by Code Generator Version 3.2.0.0 #
################################################
MODNAME="vma_ire"

rmmod vma_ire  > /dev/null 2>&1

rm /dev/$MODNAME  > /dev/null 2>&1

insmod vma_ire.ko

MAJOR=`cat /proc/devices | grep $MODNAME | sed -e 's/vma_ire//' | sed -e 's/ //'`

if test -z $MAJOR; then
	echo "The device specified is not found !"
else
	echo "Let's make a node here for $MODNAME with major number $MAJOR"
	mknod /dev/$MODNAME c $MAJOR 0
fi
