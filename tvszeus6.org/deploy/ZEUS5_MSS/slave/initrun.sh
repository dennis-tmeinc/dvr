#sleep 3
#reboot
cd /home
cp /dav/dsplinkk.ko /home/dsplinkk.ko
cp /dav/spidrv.ko /home/spidrv.ko
#cp /dav/davinci_demo /home/davinci_demo
cp /dav/92_92.yuv /home/92_92.yuv
insmod dsplinkk.ko
insmod spidrv.ko
mknod /dev/spi c 242 0
/dav/dvr/start.sh
#./davinci_demo

