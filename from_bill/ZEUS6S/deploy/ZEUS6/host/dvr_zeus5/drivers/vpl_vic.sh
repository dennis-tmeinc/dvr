################################################
# Generated by Code Generator Version 3.0.0.0 #
################################################
MODNAME="vpl_vic"

rmmod vpl_vic > /dev/null 2>&1

#rm /dev/$MODNAME
rm /dev/${MODNAME}0 > /dev/null 2>&1
rm /dev/${MODNAME}1 > /dev/null 2>&1
rm /dev/${MODNAME}2 > /dev/null 2>&1
rm /dev/${MODNAME}3 > /dev/null 2>&1
rm /dev/${MODNAME}4 > /dev/null 2>&1
rm /dev/${MODNAME}5 > /dev/null 2>&1
rm /dev/${MODNAME}6 > /dev/null 2>&1
rm /dev/${MODNAME}7 > /dev/null 2>&1

insmod vpl_vic.ko bAEEn=0 bIrisEn=0 bAFEn=0 bIICSyncVsyncFallingEn=0 gsdwBusFreq=200000000 gsdwSignalWaitTime=4000 > /dev/null 2>&1



MAJOR=`cat /proc/devices | grep $MODNAME | sed -e 's/vpl_vic//' | sed -e 's/ //'`

if test -z $MAJOR; then
	echo "The device specified is not found !"
else
	echo "Let's make a node here for $MODNAME with major number $MAJOR"
	#mknod /dev/$MODNAME c $MAJOR 0
    mknod /dev/${MODNAME}0 c $MAJOR 0 > /dev/null 2>&1
    mknod /dev/${MODNAME}1 c $MAJOR 1 > /dev/null 2>&1
    mknod /dev/${MODNAME}2 c $MAJOR 2 > /dev/null 2>&1
    mknod /dev/${MODNAME}3 c $MAJOR 3 > /dev/null 2>&1
    mknod /dev/${MODNAME}4 c $MAJOR 4 > /dev/null 2>&1
    mknod /dev/${MODNAME}5 c $MAJOR 5 > /dev/null 2>&1
    mknod /dev/${MODNAME}6 c $MAJOR 6 > /dev/null 2>&1
    mknod /dev/${MODNAME}7 c $MAJOR 7 > /dev/null 2>&1
fi