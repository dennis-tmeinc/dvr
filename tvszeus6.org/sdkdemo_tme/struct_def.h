/******************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:    Struct_def.h
Author:        Version :          Date: 07-12-16
Description:  define  struct
Version:    	// 版本信息
Function List:	NULL
History:      	
    <author>  <time>   <version >   <desc>
******************************************************************************************/

#ifndef __STRUCT_DEF_H__
#define __STRUCT_DEF_H__
#define _XOPEN_SOURCE 500


typedef unsigned char  	UINT8;
typedef unsigned short  	UINT16;
typedef unsigned int		UINT32;
typedef int	                     BOOL;

#define TRUE               	1
#define FALSE             	0
#pragma pack ( 1 )
typedef struct
{
	UINT16	x;
	UINT16	y;
}COORDINATE;

typedef struct
{
	COORDINATE	upLeft;
	UINT8		osdType;
	UINT8		dispWeek;
	UINT8		osdAttrib;
}OSDPARA;

typedef struct
{
	UINT8		osdLineNum;
	int			brightness;	/*亮度，255最亮，0最暗*/
	int			translucent;	/*是否半透明处理*/
	int			otherPara;	/*是否颜色反转、垂直、水平放大率*/
	UINT16	osdLine[MAX_OSD_LINE][MAX_OSD_CHAR_PERLINE];	//
}OSDCHARPARA;

typedef struct
{
	int	mainStreamType;
	int	subStreamType;
}STREAMTYPE;

typedef struct
{
	UINT8 	brightness;	/*亮度(0-255)*/
	UINT8 	saturation;	/*饱和度(0-255)*/
	UINT8 	hue;			/*色调(0-255)*/
	UINT8 	contrast;		/*对比度(0-255)*/
}VIDEO_COLOR;

typedef struct
{
	int			streamAttrib;		/*stream attribute.*/
	int 			intervalFrameI;		/*I frame interval.*/
	int			numFrameB;		/*B frame number.*/
	int			numFrameP;		/*P frame number(not in effect).*/
	int			frameRate;			/*frame rate.*/
	int			maxBpsLimit;		/*max bps be limited.*/
	int			format;				/*picture_format,refet to enum element of format.*/
	int			streamType;
}STREAMPARA;

typedef struct
{
	UINT16 	streamAttr;					/* 码流属性(见定义) */
	UINT16 	intervalFrameI;					/* I帧间隔 */
	UINT32 	maxVideoBitrate;				/* 视频码率上限(单位：bps) */
	UINT32 	videoFrameRate;					/* 视频帧率，PAL：1/16-25；NTCS：1/16-30 */
	UINT8		BFrameNum;					/* B帧个数: 0:BBP, 1:BP, 2:P */
	UINT8		res;
	UINT16	quality;					/* 图象质量 */
	UINT8		picFormat;	/*0--4*///还有一个分辩率没有设置
}COMPRESSPARA;

typedef struct
{
	UINT8			detectNum;				/**/
	UINT8		 	level;					/*级别0(灵敏度最高)-6(灵敏度最低),默认为2*/
	UINT8		 	fastInterval;			/*快速检测侦间隔，默认为3,[0,12],0不做高速运动检测*/
	UINT8		 	slowInterval;			/*慢速检测帧间隔，[0,13]，0不做低速运动检测*/
	UINT32	 	      delay;					/*延时*/
	struct RECT	moveRect[MAX_MOVERECT_NUM];
}MOVEDETECT;

typedef struct
{
	UINT32		alarmSendAddr;	//alarm send to this addr,UDP or TCP.
	UINT16		alarmSendPort;	//alarm server listen with this port.
}NETWORK;



/*the masked area, max o 4*/
typedef struct
{
	struct RECT	rect[MAX_MASK_REGION];	/*allow max 4 area to set mask alarm.*/
	UINT8			nAreaNum;	/*setted area number.[0,4]*/
}HIDEAREACFG;

typedef struct
{
       MOVEDETECT   MoveDetect;
	UINT32                MotHandleType;/*处理方式，每一位表示一种处理方式*/
	UINT32                MotAlarmOutTrig;/*触发的报警输出的通道，每一位表示一个报警输出*/
	BOOL               bMotHandle;/*是否处理*/
}MOTDETCFG;

typedef struct
{
       BOOL               bNoSigHandle;/*是否处理*/
	UINT32                NoSigHandleType;/*处理方式*/
	UINT32                NoSigAlarmOutTrig;/*出发的报警输出通道*/
}NOSIGNALCFG;

typedef struct
{
       BOOL                  bMaskHandle;/*是否处理遮挡报警*/
	UINT32                MaskHandleType;/*处理方式*/
	UINT32                MaskAlarmOutTrig;/*触发的报警输出通道，每一位表示一个报警输出通道*/
	struct RECT        MaskRect;
	
}MASKCFG;

typedef struct
{
       UINT32               Sensortype;/*报警器类型 0为常开，1 为常闭*/
	UINT32                bAlarmInHandle;/*是否处理报警输入*/
	UINT32                 AlarmInHandleType;
	UINT32                AlarmInAlarmOutTrig;
}ALARMINCFG;

typedef struct
{
	UINT32	nTotalLen;
	UINT32	nCmdType;
}NETCMD_HEADER;

typedef struct
{
	UINT32	length;
	UINT32	returnVal;
}NET_RETURN_HEADER;

typedef struct
{
	UINT8		quality;	/*[0,2],three level.*/
	UINT8		mode;	/*[0,2],three mode,2--D1,1--QCIF,0--CIF.*/
}JPEGCFG;

typedef struct
{
	int			nTotalLen;	/*total length of the udp packet,include self*/
	UINT8		alarmType;	//alarm type
}ALARMDATA;

typedef struct
{
       pthread_mutex_t mutex;
	pthread_cond_t   cond;
	BOOL       bHaveDate;
}SyncSem;

typedef struct connectcfg
{
	struct connectcfg  *next;
	UINT32                   readIndex;
       UINT32                   Totalreads;
	int                          streamfd;
	pthread_mutex_t      mutex;
	pthread_cond_t        cond;
	BOOL                      bMaster;
}CONNECTCFG;

typedef struct
{
	UINT8		dataBit;
	UINT8		stopBit;
	UINT8		parity;
	UINT8		flowCtrl;
	UINT32	baudrate;
}SERIALPARA;

//video file header.
typedef union
{
	unsigned int   resolution;     /* cif or qcif*/
	struct
	{
		unsigned short width;
		unsigned short height;
	}size;
}IMAGE_SIZE;

typedef struct
{
	unsigned int   start_code;         /* 开始代码 "HKH4"(MPEG4), "HKH4"(H.264)*/
	unsigned int   magic_number;       /* 魔术数*/
	unsigned int   version;            /* 版本的内部标识号*/
	unsigned int   check_sum;          /* 校验和*/
	unsigned short nStreamType;        /* 数据流类型；*/
	unsigned short nVideoFormat;       /* 视频制式；PAL  OR NTSC*/
	unsigned short nChannels;          /* number of channels (i.e. mono, stereo, etc.) */
	unsigned short nBitsPerSample;     /* 8 or 16;*/
	unsigned int   nSamplesPerSec;     /* sample rate*/
	IMAGE_SIZE		image_size;
	unsigned int   nAudioFormat;       /* ADPCM, G.729, G.722(16K,24K,32K)(贾永华，2003-1-20) */
	unsigned int   system_param;	   /*码流参数，是否包含CRC，水印，RSA等*/
}HIKVISION_MEDIA_FILE_HEADER;

typedef struct mediaDataPara
{
	int       readIndex;
	int       writeIndex;
	char    *dataBuf;
	pthread_mutex_t	       mutexSem;
	pthread_cond_t		condElem;
}MEDIADATA;

typedef struct
{
	char *	netBufBase;
	int		writePos;
	int		readPos;
	int		netBufSize;
	UINT32   Totalwrites;

}CHNLNETBUF;

typedef struct
{
      UINT8       PreviewOrder[16];/*如果为0xff，说明该通道不预览*/
      UINT8      videoStandard;
      UINT8      ScreenNum;/*显示窗口个数*/
      UINT8       res[2];
}VIDEOOUT;

typedef struct
{
	HIKVISION_MEDIA_FILE_HEADER	       mainVideoHeader;
	HIKVISION_MEDIA_FILE_HEADER	       subVideoHeader;
	CONNECTCFG                    *NetConnect;
	CONNECTCFG                     *SubNetConnect;
	CHNLNETBUF				netBuffer;
	CHNLNETBUF				netBufferSub;
	CHNLNETBUF                       recBuffer;
	int                                       IFrameIdx;
	UINT16                                linknums;
	UINT16                                sublinknums;
	pthread_mutex_t                  write_mutexSem;
	pthread_mutex_t                  subwrite_mutexSem;
	
}CHNLPARA;

	
typedef struct
{
	int 						nDevType;
	#ifdef DVR_DEMO
	VIDEOOUT                          VideoOut;
	#endif
	OSDPARA				osdPara;
	VIDEO_COLOR		       videoColor;
	STREAMTYPE			       streamType[MAX_CHNL_NUM];
	COMPRESSPARA	              compressPara[MAX_CHNL_NUM];
	COMPRESSPARA	              compressParaSub[MAX_CHNL_NUM];
	NETWORK			       netPara;
	MOTDETCFG                        MotdetCfg[MAX_CHNL_NUM];
	NOSIGNALCFG                     NoSignalCfg[MAX_CHNL_NUM];
	MASKCFG                            MaskCfg[MAX_CHNL_NUM];
	ALARMINCFG                       AlarmInCfg[4];
	HIDEAREACFG                     HideArea[MAX_CHNL_NUM];
	BOOL					bSerialMode;	/*serial 232 work mode:TRUE--console mode, FALSE--trans mode.*/
	BOOL					bChnlStart[MAX_CHNL_NUM];
	BOOL					bSubChnlStart[MAX_CHNL_NUM];
}DEVCFG;

typedef struct
{
		int channel;
		struct RECT mastrect;
		
}MASKCMD;

typedef struct
{
	int chnl;
	MOTDETCFG    MotdetCfg;
	NOSIGNALCFG NoSignalCfg;
	MASKCFG        MaskCfg;
	ALARMINCFG   AlarmInCfg[4];
}ALARM_CMDPARA;

typedef struct
{
     UINT8    databuf[8*1024];
     UINT32   readlen;
}PB_BUF;

typedef struct
{
    PB_BUF   PlayBackBuf[3];
    UINT16    ReadIdx;
    UINT16    WriteIdx;
    UINT32    TotalRead;
    UINT32    TotalWrite;
}PLAY_BACK;

#pragma pack ()

#endif

