/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:    Macro_def.h
Author:        Version :          Date:   07-12-17
Description:   define some macro and the protocol
Version:    	// 版本信息
Function 
History:      	
    <author>  <time>   <version >   <desc>
***********************************************************************************************/


#ifndef	__MACRO_DEF_H__
#define	__MACRO_DEF_H__

#define ERROR		                    -1
#define OK		                            0

#define NETCMD_HEADER_LEN	       8
#define MAX_CLIENT_CMDLEN		2048
#define MAX_MASK_REGION		4
#define MAX_CHNL_NUM			4
#define MAX_LINK_NUM			6
#define GLOBAL_MAX_LINK              24
#define MAX_OSD_LINE			8
#define MAX_OSD_CHAR_PERLINE	88
#define MAX_MOVERECT_NUM		100
#define MEDIA_BUFF_SIZE		1024*1024


typedef void *ENTRYPOINT;
#define MAX_TASK_PRIORITY	       255
#define DEFAULT_TASK_PRI	       100
#define MIN_STACK_SIZE		       16384
#define NETWORK_TIMEOUT	       10
#define LOW_BANDWIDTH		       512

//osd type
#define OSD_TYPE_ENGYYMMDD	0
#define OSD_TYPE_ENGMMDDYY	1
#define OSD_TYPE_CHNYYMMDD	2
//osd attribute
#define OSD_DISABLE				0		/*OSD not display */
#define OSD_TRANS_WINK			1		/*transparent and wink */
#define OSD_TRANS_NO_WINK		2		/*transparent,not wink */
#define OSD_NO_TRANS_WINK		3		/*not transparent,but wink*/
#define OSD_NO_TRANS_NO_WINK	4		/*not transparent,not wink*/
//stream type
#define STREAM_TYPE_VIDOE 		1
#define STREAM_TYPE_AUDIO 		2
#define STREAM_TYPE_AVSYN 		3
//channel type
#define MAIN_CHANNEL			1
#define SUB_CHANNEL			0
//
#define MODE_VBR                           0 
#define MODE_CBR                           1




//net command definition
#define NETCMD_GET_IMAGE		      0x00000201		//net get image.
#define NETCMD_PREVIEW			      0x00000202		//net video preview.
#define NETCMD_VOICETALK		      0x00000203		//net voice talk.

//dsp setting
#define NETCMD_OSDCFG			      0x00000204		//OSD option.
#define NETCMD_OSDCFG_GET		      0x00000205		//OSD option.
#define NETCMD_STREAMCFG		      0x00000206		//stream parameter option.
#define NETCMD_STREAMCFG_GET	      0x00000207		//stream parameter option.
#define NETCMD_VIDEOCOLOR		      0x00000208		//video color option.
#define NETCMD_VIDEOCOLOR_GET	      0x00000209		//video color option.
#define NETCMD_COMPRESS		      0x0000020a		//compress option.
#define NETCMD_COMPRESS_GET	      0x0000020b		//compress option.
#define NETCMD_DSPTIME			      0x0000020c		//dsp time setup.
#define NETCMD_485CFG			      0x0000020d		//serial 485 option.
#define NETCMD_232ModeCFG		      0x0000020e		//serial 232 work mode set.
#define NETCMD_232CFG			      0x0000020f		//serial 232 option.

//alarm option
#define NETCMD_MASK_ALARMCFG		0x00000301		//mask alarm,setting mask area.
#define NETCMD_MOVE_ALARMCFG		0x00000302		//move detect alarm, setting area to detect.
#define NETCMD_SIGNAL_ALARMCFG	0x00000303		//if no signal,alarm.
#define NETCMD_MASKSET_AREA		0x00000304		//set mask area,it not display at video.
#define NETCMD_MASKCMD_CANCEL	0x00000305		//cancel mask area.
#define NETCMD_ALARM_SET                 0x00000306           //
#define NETCMD_ALARMPARA_GET        0x00000307          /**/ 

#define NETCMD_SWITCHHDCMD            0x00000401         /*switch ATA to FLASH or switch FLASH to ATA*/

#define NETCMD_LOCA_RECORD              0x00000501        /*local record*/

//for play back
#define NETCMD_PLAYBACK                        0x00000601
#define NETCMD_PLAYBACK_NEXTFRAME   0x00000602
#define NETCMD_PLAYBACK_SPEEDDOWN  0x00000603
#define NETCMD_PLAYBACK_SPEEDUP        0x00000604

//FOR PREVIEW 
#define NETCMD_SET_PREVIEWORDER         0x00000701

//for osd display
#define NETCMD_SET_OSDDISPLAY              0x00000801

//net command at preview
#define NETCMD_PREVIEW_STOP	       0x00000021		//net preview stop.
#define NETCMD_MEDIADATA		       0x00000022		//net media data exchange.

//alarm type,for udp send to host
/*
#define ALARM_TYPE_NO_SIGNAL		0x01
#define ALARM_TYPE_MASK			0x02
#define ALARM_TYPE_MOVE			0x03
*/
/*异常处理类型*/
#define NO_ACTION                    0x0/*无响应*/
#define SOUND_WARN                0x01/*声音警告*/
#define TRIGGER_ALARMOUT     0x02/*触发报警输出*/
#define SEND_TO_HOST             0x04/*上传主机*/
#define TRIGGER_RECORD          0x08/*触发录像*/

/*报警类型*/
#define NOSIGNAL_ALARM         0x1
#define ALARMIN_ALARM           0x2
#define MASK_ALARM                 0x3
#define MOTDET_ALARM             0x4
#define MAX_ALARM_TYPE         5

#define DVR_DEMO
#ifdef DVR_DEMO
#define MAIN_OUTPUT 0
#define VIDEOS_NTSC 1
#define VIDEOS_PAL 2
#endif
//TCP/IP
#ifndef IPPROTO_TCP
#define IPPROTO_TCP                      	6
#endif


#define CFG_FILENAME		"/davinci/devCfg.bin"

#endif
