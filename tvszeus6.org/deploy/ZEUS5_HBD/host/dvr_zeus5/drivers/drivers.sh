#!/bin/sh

SENSORTYPE=TW2866
#SENSORTYPE=NVP1114A
DRIVER_DIR=/mnt/nand/dvr/drivers
cd $DRIVER_DIR
insmod $DRIVER_DIR/jbd.ko
insmod $DRIVER_DIR/ext3.ko
insmod $DRIVER_DIR/i2c-algo-bit.ko
# Mozart controls the auido codec (SSM2603) through i2c bus1. The control pins of the bus  is gpio #12 and #13.
# According to  default setting of gpio on Mozart, gpio #6 and #7 is as the control pins of VIC for capturing vidoe data from sensor or video chip.
insmod $DRIVER_DIR/i2c-gpio.ko  bus_num=2 scl0=6 sda0=7 scl1=12 sda1=13 
insmod $DRIVER_DIR/Godshand.ko
insmod $DRIVER_DIR/dwc_otg.ko
insmod $DRIVER_DIR/irrc.ko
insmod $DRIVER_DIR/vpl_mmc.ko
insmod $DRIVER_DIR/vpl_edmc.ko dwMediaMemSize=192 #64 for all, 80 for 5M , 5M: cat /proc/cmdline from mem=64M to mem=48M
sh $DRIVER_DIR/vpl_tmrc.sh
insmod $DRIVER_DIR/vpl_dmac.ko
sh $DRIVER_DIR/gpio_i2c.sh
sh $DRIVER_DIR/snddevices
#insmod $DRIVER_DIR/NVP1114A_AUDIO.ko CodecNum=2
insmod $DRIVER_DIR/TW2866_AUDIO.ko CodecNum=2
#sh $DRIVER_DIR/vpl_vic.sh
if [ -f $DRIVER_DIR/vma_die.ko ];then
	insmod $DRIVER_DIR/vma_die.ko
fi
insmod $DRIVER_DIR/vma_dce.ko
if [ -f $DRIVER_DIR/vma_ibpe.ko ];then
	insmod $DRIVER_DIR/vma_ibpe.ko
fi
insmod $DRIVER_DIR/vma_ire.ko
if [ -f $DRIVER_DIR/vma_jebe.ko ];then
	insmod $DRIVER_DIR/vma_jebe.ko
fi
if [ -f $DRIVER_DIR/vma_h4ee.ko ];then
	insmod $DRIVER_DIR/vma_h4ee.ko
fi
insmod $DRIVER_DIR/vma_meae.ko
#sh alsa.sh
# load voc after vic
#if [ -f $DRIVER_DIR/vpl_voc.sh ];then
#	sh $DRIVER_DIR/vpl_voc.sh
#fi 
insmod $DRIVER_DIR/buff_mgr.ko gsdwMaxReaderNum=6  gsdwNumNodes=16
#sh $DRIVER_DIR/buff_mgr.sh

insmod $DRIVER_DIR/wdt.ko
#$DRIVER_DIR/audioctrl -s 
#$DRIVER_DIR/audioctrl -e 1 -t 0 -m 1

insmod $DRIVER_DIR/IICCtrl.ko
insmod $DRIVER_DIR/AutoExposure.ko
if [ -f $DRIVER_DIR/ad5602.ko ];then
  insmod $DRIVER_DIR/ad5602.ko
fi
if [ -f $DRIVER_DIR/nagano.ko ];then
  insmod $DRIVER_DIR/nagano.ko
fi
if [ -f $DRIVER_DIR/autofocus.ko ];then
  insmod $DRIVER_DIR/autofocus.ko
fi
insmod $DRIVER_DIR/$SENSORTYPE.ko 
sh $DRIVER_DIR/vpl_vic.sh

if [ -f $DRIVER_DIR/vpl_voc.sh ];then
	sh $DRIVER_DIR/vpl_voc.sh
fi 
if [ -f $DRIVER_DIR/uart-gpio.ko ];then
  insmod $DRIVER_DIR/uart-gpio.ko
fi 

#insmod rr.ko

if [ -f $DRIVER_DIR/wifi.sh ];then
	sh $DRIVER_DIR/wifi.sh
fi 

cd -

