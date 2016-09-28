#!/bin/sh

if [ "$1" != "" ]; then
  video_system=$1
else
  video_system=NTSC
fi

video_frontend=nvp6114

if [ "$video_system" != "NTSC" ] && [ "$video_system" != "PAL" ]; then
    echo "Invalid argument for NTSC/PAL."
    exit
fi

/sbin/insmod /lib/modules/frammap.ko
cat /proc/frammap/ddr_info

/sbin/insmod /lib/modules/log.ko mode=0 log_ksize=512
/sbin/insmod /lib/modules/ms.ko
/sbin/insmod /lib/modules/em.ko

/sbin/insmod /lib/modules/think2d.ko
/sbin/insmod /lib/modules/flcd200-common.ko
/sbin/insmod /lib/modules/flcd200-pip.ko input_res=9 output_type=53 bf=0 fb0_fb1_share=1 gui_clone_fps=5

camsd=`/mnt/nand/dvr/cfg get system camsd`
ch14mode="7"
ch58mode="7"
ch14norm="5"
ch58norm="5"
if [ "$video_system" == "NTSC" ]  ; then
    /sbin/insmod /lib/modules/flcd200-pip2.ko input_res=0 output_type=0 bf=0 d1_3frame=1 fb0_fb1_share=1 lcd_disable=0
  if [ "$camsd" == "0" ] ; then
    ch14mode="7"
    ch14norm="5"
    ch58mode="7"
    ch58norm="5"
  else
    ch14mode="1"
    ch14norm="1"
    ch58mode="1"
    ch58norm="1"
  fi
elif [ "$video_system" == "PAL" ] ; then
    /sbin/insmod /lib/modules/flcd200-pip2.ko input_res=1 output_type=1 bf=0 d1_3frame=1 fb0_fb1_share=1 lcd_disable=0
  if [ "$camsd" == "0" ] ; then
    ch14mode="15"
    ch14norm="4"
    ch58mode="15"
    ch58norm="4"
  else
    ch14mode="9"
    ch14norm="0"
    ch58mode="9"
    ch58norm="0"
  fi
fi

/sbin/insmod /lib/modules/sli10121.ko  #HDMI
/sbin/insmod /lib/modules/fe_common.ko

case "$video_frontend" in

	nvp6114)
            if [ "$video_system" == "NTSC" ] ; then
                /sbin/insmod /lib/modules/nvp6114.ko dev_num=2 clk_src=4 clk_used=1 vmode=7,7 ch_map=0 notify=1 audio_chnum=8
                /sbin/insmod /lib/modules/nvp6114a.ko dev_num=2 clk_src=4 clk_used=1 vmode=${ch14mode},${ch58mode} ch_map=0 notify=1 audio_chnum=8
                /sbin/insmod /lib/modules/vcap300_common.ko
                /sbin/insmod /lib/modules/vcap0.ko vi_mode=2,2,2,2 cap_md=1
                /sbin/insmod /lib/modules/vcap300_nvp6114.ko mode=1,1 norm=5,5 inv_clk=1,1
                /sbin/insmod /lib/modules/vcap300_nvp6114a.ko mode=1,1 norm=${ch14norm},${ch58norm} inv_clk=1,1
            else
                /sbin/insmod /lib/modules/nvp6114.ko dev_num=2 clk_src=4 clk_used=1 vmode=15,15 ch_map=0 notify=1 audio_chnum=8
                /sbin/insmod /lib/modules/nvp6114a.ko dev_num=2 clk_src=4 clk_used=1 vmode=${ch14mode},${ch58mode} ch_map=0 notify=1 audio_chnum=8
                /sbin/insmod /lib/modules/vcap300_common.ko
                /sbin/insmod /lib/modules/vcap0.ko vi_mode=2,2,2,2 cap_md=1
                /sbin/insmod /lib/modules/vcap300_nvp6114.ko mode=1,1 norm=4,4 inv_clk=1,1
                /sbin/insmod /lib/modules/vcap300_nvp6114a.ko mode=1,1 norm=${ch14norm},${ch58norm} inv_clk=1,1
            fi
            ;;
    *)
        echo "Invalid argument for video frontend: $video_frontend"
        exit
        ;;
esac

#Scaler max_vch_num=64 (16CH mainstream + 16CH substream + 16CH datain + 16CH reserved)
#Scaler max_minors=52 (16CH playback + 16CH scaler_sub + 16CH scaler_sub_private + 4CH reserved)
/sbin/insmod /lib/modules/sw_osg.ko idn=65 mem=450
sh /mnt/mtd/init_osg_mark_idx.sh
/sbin/insmod /lib/modules/fscaler300.ko max_vch_num=64 max_minors=52 temp_width=1280 temp_height=720
/sbin/insmod /lib/modules/ftdi220.ko
/sbin/insmod /lib/modules/osd_dispatch.ko
/sbin/insmod /lib/modules/fmcp_drv.ko
/sbin/insmod /lib/modules/favc_enc.ko h264e_max_b_frame=0 h264e_max_width=1280 h264e_max_height=720 h264e_slice_offset=1
/sbin/insmod /lib/modules/favc_rc.ko
/sbin/insmod /lib/modules/decoder.ko
/sbin/insmod /lib/modules/favc_dec.ko log_level=0 mcp300_max_buf_num=0 mcp300_codec_padding_size=4 mcp300_b_frame=0 mcp300_max_width=1280 reserve_buf=1 chn_num=4
/sbin/insmod /lib/modules/fmjpeg_drv.ko
/sbin/insmod /lib/modules/codec.ko

/sbin/insmod /lib/modules/audio_drv.ko audio_ssp_num=0,2 audio_ssp_chan=8,1 bit_clock=2048000,3072000 sample_rate=8000,48000 audio_out_enable=1,1

/sbin/insmod /lib/modules/gs.ko reserved_ch_cnt=16 flow_mode=1
/sbin/insmod /lib/modules/loop_comm.ko
# datain = 16CH + 2CH audio + 16CH apply
# dataout = 32CH + 32CH apply + 16CH audio
/sbin/insmod /lib/modules/vpd_slave.ko vpslv_dbglevel=0 ddr0_sz=0 ddr1_sz=0 config_path="/mnt/mtd/" datain_minors=34 dataout_minors=80
/sbin/insmod /lib/modules/vpd_master.ko vpd_dbglevel=0 gmlib_dbglevel=0

#echo /mnt/nfs > /proc/videograph/dumplog   #configure log path
#rm -f /mnt/nfs/log.txt /mnt/nfs/log_slave.txt # clear previous log
mdev -s
#cat /proc/modules
#echo 0 > /proc/frammap/free_pages   #should not free DDR1 for performance issue

#Disable denoise (temporal/spatial)
#echo 6 0 > /proc/ftdi220/param
#echo 10 0 > /proc/ftdi220/param 

echo 1 > /proc/flcd200/pip/pip_mode
#echo 0 > /proc/flcd200_2/pip/pip_mode

devmem 0x99300030 32 0x9f000009

echo 1 > /proc/vcap300/vcap0/dbg_mode  #need debug mode to detect capture overflow
echo 0 > /proc/videograph/em/dbglevel
echo 0 > /proc/videograph/gs/dbglevel
echo 0 > /proc/videograph/ms/dbglevel
echo 0 > /proc/videograph/datain/dbglevel
echo 0 > /proc/videograph/dataout/dbglevel
echo 0 > /proc/videograph/vpd/dbglevel
echo 0 > /proc/videograph/gmlib/dbglevel

echo 1 1 > /proc/videograph/vcap0/group_info

rootfs_date=`ls /|grep 00`

#echo 1 2 4 8 > /proc/flcd200/sharpness
#echo 0 > /proc/audio/au_buffer_chase
#devmem 0X9A800320 32 0x1000000;
#devmem 0X9B800320 32 0x1000000;


#devmem 0x99300018 32 0x32351424
#devmem 0x99300034 32 0x060e0606

devmem 0x9900005c 32 0x0000a000

#insmod /opt/app/modules/gpio_drv.ko
#insmod /opt/app/modules/irda.ko
#insmod /opt/app/modules/keyscan.ko




mkdir /var/tmp



echo =========================================================================
echo "  Video Front End: $video_frontend"
echo "  Video Mode: $video_system"
echo "  RootFS Version: $rootfs_date"
echo =========================================================================

sh /mnt/nand/dvr/dvrinit.sh
