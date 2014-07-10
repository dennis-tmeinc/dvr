/******************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:    Struct_def.h
Author:        Version :          Date: 07-12-16
Description:  define  struct
Version:    	// �汾��Ϣ
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
	int			brightness;	/*���ȣ�255������0�*/
	int			translucent;	/*�Ƿ��͸������*/
	int			otherPara;	/*�Ƿ���ɫ��ת����ֱ��ˮƽ�Ŵ���*/
	UINT16	osdLine[MAX_OSD_LINE][MAX_OSD_CHAR_PERLINE];	//
}OSDCHARPARA;

typedef struct
{
	int	mainStreamType;
	int	subStreamType;
}STREAMTYPE;

typedef struct
{
	UINT8 	brightness;	/*����(0-255)*/
	UINT8 	saturation;	/*���Ͷ�(0-255)*/
	UINT8 	hue;			/*ɫ��(0-255)*/
	UINT8 	contrast;		/*�Աȶ�(0-255)*/
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
	UINT16 	streamAttr;					/* ��������(������) */
	UINT16 	intervalFrameI;					/* I֡��� */
	UINT32 	maxVideoBitrate;				/* ��Ƶ��������(��λ��bps) */
	UINT32 	videoFrameRate;					/* ��Ƶ֡�ʣ�PAL��1/16-25��NTCS��1/16-30 */
	UINT8		BFrameNum;					/* B֡����: 0:BBP, 1:BP, 2:P */
	UINT8		res;
	UINT16	quality;					/* ͼ������ */
	UINT8		picFormat;	/*0--4*///����һ���ֱ���û������
}COMPRESSPARA;

typedef struct
{
	UINT8			detectNum;				/**/
	UINT8		 	level;					/*����0(���������)-6(���������),Ĭ��Ϊ2*/
	UINT8		 	fastInterval;			/*���ټ��������Ĭ��Ϊ3,[0,12],0���������˶����*/
	UINT8		 	slowInterval;			/*���ټ��֡�����[0,13]��0���������˶����*/
	UINT32	 	      delay;					/*��ʱ*/
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
	UINT32                MotHandleType;/*����ʽ��ÿһλ��ʾһ�ִ���ʽ*/
	UINT32                MotAlarmOutTrig;/*�����ı��������ͨ����ÿһλ��ʾһ���������*/
	BOOL               bMotHandle;/*�Ƿ���*/
}MOTDETCFG;

typedef struct
{
       BOOL               bNoSigHandle;/*�Ƿ���*/
	UINT32                NoSigHandleType;/*����ʽ*/
	UINT32                NoSigAlarmOutTrig;/*�����ı������ͨ��*/
}NOSIGNALCFG;

typedef struct
{
       BOOL                  bMaskHandle;/*�Ƿ����ڵ�����*/
	UINT32                MaskHandleType;/*����ʽ*/
	UINT32                MaskAlarmOutTrig;/*�����ı������ͨ����ÿһλ��ʾһ���������ͨ��*/
	struct RECT        MaskRect;
	
}MASKCFG;

typedef struct
{
       UINT32               Sensortype;/*���������� 0Ϊ������1 Ϊ����*/
	UINT32                bAlarmInHandle;/*�Ƿ���������*/
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
	unsigned int   start_code;         /* ��ʼ���� "HKH4"(MPEG4), "HKH4"(H.264)*/
	unsigned int   magic_number;       /* ħ����*/
	unsigned int   version;            /* �汾���ڲ���ʶ��*/
	unsigned int   check_sum;          /* У���*/
	unsigned short nStreamType;        /* ���������ͣ�*/
	unsigned short nVideoFormat;       /* ��Ƶ��ʽ��PAL  OR NTSC*/
	unsigned short nChannels;          /* number of channels (i.e. mono, stereo, etc.) */
	unsigned short nBitsPerSample;     /* 8 or 16;*/
	unsigned int   nSamplesPerSec;     /* sample rate*/
	IMAGE_SIZE		image_size;
	unsigned int   nAudioFormat;       /* ADPCM, G.729, G.722(16K,24K,32K)(��������2003-1-20) */
	unsigned int   system_param;	   /*�����������Ƿ����CRC��ˮӡ��RSA��*/
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
      UINT8       PreviewOrder[16];/*���Ϊ0xff��˵����ͨ����Ԥ��*/
      UINT8      videoStandard;
      UINT8      ScreenNum;/*��ʾ���ڸ���*/
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

