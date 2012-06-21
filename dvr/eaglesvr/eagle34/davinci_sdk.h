#ifndef __HIK_DAV_SDK_INTERFACE_H__
#define __HIK_DAV_SDK_INTERFACE_H__


#ifdef __cplusplus
extern "C" {
#endif

//错误代码
#define ErrorCodeSuccess                0x0000

#define ErrorCodeDSPUninit              0x0001
#define ErrorCodeDSPNotReady            0x0002
#define ErrorCodeDSPLoadFail            0x0004
#define ErrorCodeDSPSyncObjectFail      0x0005
#define ErrorCodeDSPCountOverflow       0x0009
#define ErrorCodeEncodeChannelOverflow  0x000a
#define ErrorCodeDecodeChannelOverflow  0x000b
#define ErrorCodeBoardOverflow          0x000c
#define ErrorCodeDSPHexBlockLenOverflow 0x000d
#define ErrorCodeDSPProgramCheckoutFail 0x000e
#define ErrorCodeDSPInitRecheckFail     0x000f
#define ErrorCodeDSPBusy                0x0010
#define ErrorCodeDSPCOMFail             0x0011


#define ErrorCodeNoCardInstalled        0x0102
#define ErrorCodeIoctlFail              0x0103
#define ErrorCodeMemLocateFail          0x0104
#define ErrorCodeDuplicateSN            0x0105
#define ErrorCodeCreateThreadFail       0x0106
#define ErrorCodeDSPCmdInvalid          0x0107
#define ErrorCodeHOSTCmdInvalid         0x0108
#define ErrorCodeDSPRespondCmdFail      0x0109
#define ErrorCodeOrderFail              0x010a
#define ErrorCodeKernelFail             0x010b
#define ErrorCodeStreamBufUnderflow     0x010c

#define ErrorCodeChannelOutofRange      0x0200
#define ErrorCodeInvalidEncodeChannel   0x0201
#define ErrorCodeInvalidArgument	0x0202
#define ErrorCodeNotSupport		0x0203
#define ErrorCodeMmapFail		0x0204

#define ErrorCodeFileInvalid			0x0301
#define ErrorCodeOpenFileFail			0x0302
#define ErrorCodeFileSizeZero			0x0303
#define ErrorCodeBadFileFormat			0x0304
#define ErrorCodeBadFileHeader			0x0305
#define ErrorCodeParaOver			0x0306
#define ErrorCodeCreateFileFail			0x0307
#define ErrorCodeNoSpare			0x0308
#define ErrorCodeInvalidDevice			0x0309
#define ErrorCodeInsufficientMem		0x030a
#define ErrorCodeCardSerialError		0x030b
#define ErrorCodeInvalidDecodeChannel		0x030c
#define ErrorCodeOutOfMemory			0x030d
#define ErrorCodeSemCreateFail			0x030e


//SDK版本号
#define MAJ_VER(version) ( (version&0xff00)>>8 )
#define MIN_VER(version) ( (version&&0x00ff) )

//流类型定义
#define STREAM_TYPE_VIDOE 	1	//视频流
#define STREAM_TYPE_AUDIO 	2	//音频流
#define STREAM_TYPE_AVSYN 	3	//音视频同步流

#define DECODE_FILE_MODE		1			/*文件回放模式*/

//码流控制模式
#define MODE_VBR 0 	//变码率
#define MODE_CBR 1	//恒定码率

#define SDK_MAX_MENU_DISP_NUM 3 //菜单最多显示区域个数

/////////////////////////////////////////////////////////////////////////////
//数据回调帧类型定义
#define FRAME_TYPE_VIDEO_I_PS        0x1        /*PS封装主码流视频I帧*/
#define	FRAME_TYPE_VIDEO_P_PS        0x2        /*PS封装主通道视频P帧*/
#define	FRAME_TYPE_VIDEO_BP_PS       0x3        /*PS封装主通道视频BP、BBP帧*/

#define	FRAME_TYPE_VIDEO_I_TS        0x4        /*TS封装主码流视频I帧*/
#define	FRAME_TYPE_VIDEO_P_TS        0x5        /*TS封装主码流视频P帧*/
#define	FRAME_TYPE_VIDEO_BP_TS       0x6        /*TS封装主码流视频BP、BBP帧*/
#define	FRAME_TYPE_VIDEO_I_RTP       0x7        /*RTP封装主码流视频I帧*/
#define	FRAME_TYPE_VIDEO_P_RTP       0x8        /*RTP封装主码流视频P帧*/
#define	FRAME_TYPE_VIDEO_BP_RTP      0x9        /*RTP封装主码流视频BP、BBP帧*/

#define	FRAME_TYPE_VIDEO_I_HIK       0xa        /*HIK封装主码流视频I帧*/
#define	FRAME_TYPE_VIDEO_P_HIK       0xb        /*HIK封装主码流视频P帧*/
#define	FRAME_TYPE_VIDEO_BP_HIK      0xc        /*HIK封装主码流视频BP、BBP帧*/
#define FRAME_TYPE_VIDEO_I_ORG       0xd        /*无封装主码流视频I帧*/
#define	FRAME_TYPE_VIDEO_P_ORG       0xe        /*无封装主码流视频P帧*/
#define	FRAME_TYPE_VIDEO_BP_ORG      0xf        /*无封装主码流视频BP、BBP帧*/

#define	FRAME_TYPE_VIDEO_SUB_I_PS  	 0x11        /*PS封装子码流视频I帧*/
#define	FRAME_TYPE_VIDEO_SUB_P_PS  	 0x12        /*PS封装子码流视频P帧*/
#define	FRAME_TYPE_VIDEO_SUB_BP_PS   0x13        /*PS封装子码流视频BP、BBP帧*/

#define	FRAME_TYPE_VIDEO_SUB_I_TS    0x14        /*TS封装子码流视频I帧*/
#define	FRAME_TYPE_VIDEO_SUB_P_TS    0x15        /*TS封装子码流视频P帧*/
#define	FRAME_TYPE_VIDEO_SUB_BP_TS   0x16        /*TS封装子码流视频BP、BBP帧*/

#define	FRAME_TYPE_VIDEO_SUB_I_RTP  	0x17       /*RTP封装子码流视频I帧*/
#define	FRAME_TYPE_VIDEO_SUB_P_RTP  	0x18       /*RTP封装子码流视频P帧*/
#define	FRAME_TYPE_VIDEO_SUB_BP_RTP     0x19       /*RTP封装子码流视频BP、BBP帧*/

#define	FRAME_TYPE_VIDEO_SUB_I_HIK      0x1a       /*HIK封装子码流视频I帧*/
#define	FRAME_TYPE_VIDEO_SUB_P_HIK     0x1b      /*HIK封装子码流视频P帧*/
#define	FRAME_TYPE_VIDEO_SUB_BP_HIK     0x1c       /*HIK封装子码流视频BP、BBP帧*/

#define FRAME_TYPE_VIDEO_SUB_I_ORG      0x1d       /*无封装子码流视频I帧*/
#define	FRAME_TYPE_VIDEO_SUB_P_ORG      0x1e       /*无封装子码流视频P帧*/
#define	FRAME_TYPE_VIDEO_SUB_BP_ORG     0x1f       /*无封装子码流视频BP、BBP帧*/

/* 音频相关 */
#define	FRAME_TYPE_AUDIO_PS             0x21       /*PS封装主码流音频帧*/
#define	FRAME_TYPE_AUDIO_TS             0x22       /*TS封装主码流音频帧*/
#define	FRAME_TYPE_AUDIO_RTP           0x23       /*RTP封装主码流音频帧*/
#define	FRAME_TYPE_AUDIO_HIK           0x24       /*HIK封装主码流音频帧*/

#define	FRAME_TYPE_AUDIO_SUB_PS        0x28       /*PS封装子码流音频帧*/
#define	FRAME_TYPE_AUDIO_SUB_TS        0x29       /*TS封装子码流音频帧*/
#define	FRAME_TYPE_AUDIO_SUB_RTP      0x2a       /*RTP封装子码流音频帧*/
#define	FRAME_TYPE_AUDIO_SUB_HIK      0x2b       /*HIK封装子码流音频帧*/

/* 功能性数据 */
#define	FRAME_TYPE_MD_RESULT  			0x31       /*移动侦测结果*/
#define	FRAME_TYPE_YUV_IMG          	0x32       /*抓图数据YUV*/
#define	FRAME_TYPE_JPEG_IMG         	0x33       /*抓图数据JPEG*/
#define	FRAME_TYPE_IMG_STREAM  			0x34       /*原始图像流*/
#define	FRAME_TYPE_RAW_AUDIO  			0x35       /*原始音频流*/

#define	STREAM_MPEG4_HEADER					0x38		/*mpeg4码流头*/
#define	STREAM_MPEG4_SUB_HEADER				0x39		/*mpeg4码流头*/
#define	STREAM_H264_HEADER					0x3a		/*h264sps码流头 rtp封装时用*/
#define	STREAM_H264_SUB_HEADER				0x3b		/*h264sps码流头 rtp封装时用*/
#define	FRAME_TYPE_HEADER				0x3c		/*海康封装文件头*/
#define	FRAME_TYPE_SUB_HEADER			0x3d		/*海康封装文件头*/

//bill adds start
#define FRAME_TYPE_AUDIO                       0x40
//bill adds end

//RS485数据定义
//baudrate 
#define BAUD50 		50
#define BAUD75 		75
#define BAUD110 	110
#define BAUD150		150
#define BAUD300		300
#define BAUD600		600
#define BAUD1200	1200
#define BAUD2400	2400
#define BAUD4800	4800
#define BAUD9600	9600
#define BAUD19200	19200
#define BAUD38400	38400
#define BAUD57600	57600
#define BAUD76800	76800
#define BAUD115200	115200

//data bits defines
#define DATAB5     	0
#define DATAB6     	1
#define DATAB7     	2
#define DATAB8     	3

//stop bits defines
#define STOPB0		0
#define STOPB1		1

//parity defines 
#define NOPARITY	0
#define ODDPARITY	1
#define EVENPARITY	2

//flow control defines 
//不支持流控
/* flow control defines */
#define	NOCTRL		0
#define SOFTCTRL	1		/* xon/xoff flow control */
#define HARDCTRL	2		/* RTS/CTS flow control */
typedef enum{
#define DAVINCI_SDK_DVS 0X0
    //HC
    DS_SDK_SDK_6101HC = DAVINCI_SDK_DVS + 1,
    DS_SDK_6102HC = DAVINCI_SDK_DVS + 2,
    DS_SDK_6103HC = DAVINCI_SDK_DVS + 3,
    DS_SDK_6104HC = DAVINCI_SDK_DVS + 4,
    DS_SDK_6105HC = DAVINCI_SDK_DVS + 5,
    DS_SDK_6106HC = DAVINCI_SDK_DVS + 6,
    DS_SDK_6107HC = DAVINCI_SDK_DVS + 7,
    DS_SDK_6108HC = DAVINCI_SDK_DVS + 8,
    DS_SDK_6109HC = DAVINCI_SDK_DVS + 9,
    DS_SDK_6110HC = DAVINCI_SDK_DVS + 10,
    DS_SDK_6111HC = DAVINCI_SDK_DVS + 11,
    DS_SDK_6112HC = DAVINCI_SDK_DVS + 12,
    DS_SDK_6113HC = DAVINCI_SDK_DVS + 13,
    DS_SDK_6114HC = DAVINCI_SDK_DVS + 14,
    DS_SDK_6115HC = DAVINCI_SDK_DVS + 15,
    DS_SDK_6116HC = DAVINCI_SDK_DVS + 16,

    //HT
    DS_SDK_6101HT = DAVINCI_SDK_DVS + 17,
    DS_SDK_6102HT = DAVINCI_SDK_DVS + 18,
    DS_SDK_6103HT = DAVINCI_SDK_DVS + 19,
    DS_SDK_6104HT = DAVINCI_SDK_DVS + 20,
    DS_SDK_6105HT = DAVINCI_SDK_DVS + 21,
    DS_SDK_6106HT = DAVINCI_SDK_DVS + 22,
    DS_SDK_6107HT = DAVINCI_SDK_DVS + 23,
    DS_SDK_6108HT = DAVINCI_SDK_DVS + 24,
    DS_SDK_6109HT = DAVINCI_SDK_DVS + 25,
    DS_SDK_6110HT = DAVINCI_SDK_DVS + 26,
    DS_SDK_6111HT = DAVINCI_SDK_DVS + 27,
    DS_SDK_6112HT = DAVINCI_SDK_DVS + 28,
    DS_SDK_6113HT = DAVINCI_SDK_DVS + 29,
    DS_SDK_6114HT = DAVINCI_SDK_DVS + 30,
    DS_SDK_6115HT = DAVINCI_SDK_DVS + 31,
    DS_SDK_6116HT = DAVINCI_SDK_DVS + 32,

    //HF
    DS_SDK_6101HF = DAVINCI_SDK_DVS + 33,
    DS_SDK_6102HF = DAVINCI_SDK_DVS + 34,
    DS_SDK_6103HF = DAVINCI_SDK_DVS + 35,
    DS_SDK_6104HF = DAVINCI_SDK_DVS + 36,

#define DAVINCI_SDK_DVS_A 0X40
    //HC 
    DS_SDK_6101HC_A = DAVINCI_SDK_DVS_A + 1,
    DS_SDK_6102HC_A = DAVINCI_SDK_DVS_A + 2,
    DS_SDK_6103HC_A = DAVINCI_SDK_DVS_A + 3,
    DS_SDK_6104HC_A = DAVINCI_SDK_DVS_A + 4,

    //HT
    DS_SDK_6101HT_A = DAVINCI_SDK_DVS_A + 17,
    DS_SDK_6102HT_A = DAVINCI_SDK_DVS_A + 18,
    DS_SDK_6103HT_A = DAVINCI_SDK_DVS_A + 19,
    DS_SDK_6104HT_A = DAVINCI_SDK_DVS_A + 20,

    //HF
    DS_SDK_6101HF_A = DAVINCI_SDK_DVS_A + 33,
    DS_SDK_6102HF_A = DAVINCI_SDK_DVS_A + 34,
    DS_SDK_6103HF_A = DAVINCI_SDK_DVS_A + 35,
    DS_SDK_6104HF_A = DAVINCI_SDK_DVS_A + 36,


#define DAVINCI_SDK_DVR 0x100
    //HS
    DS_SDK_7101HS = DAVINCI_SDK_DVR + 1,
    DS_SDK_7102HS = DAVINCI_SDK_DVR + 2,
    DS_SDK_7103HS = DAVINCI_SDK_DVR + 3,
    DS_SDK_7104HS = DAVINCI_SDK_DVR + 4,
    DS_SDK_7105HS = DAVINCI_SDK_DVR + 5,
    DS_SDK_7106HS = DAVINCI_SDK_DVR + 6,
    DS_SDK_7107HS = DAVINCI_SDK_DVR + 7,
    DS_SDK_7108HS = DAVINCI_SDK_DVR + 8,
    DS_SDK_7109HS = DAVINCI_SDK_DVR + 9,
    DS_SDK_7110HS = DAVINCI_SDK_DVR + 10,
    DS_SDK_7111HS = DAVINCI_SDK_DVR + 11,
    DS_SDK_7112HS = DAVINCI_SDK_DVR + 12,
    DS_SDK_7113HS = DAVINCI_SDK_DVR + 13,
    DS_SDK_7114HS = DAVINCI_SDK_DVR + 14,
    DS_SDK_7115HS = DAVINCI_SDK_DVR + 15,
    DS_SDK_7116HS = DAVINCI_SDK_DVR + 16,

    //HC
    DS_SDK_7101HC = DAVINCI_SDK_DVR + 17,
    DS_SDK_7102HC = DAVINCI_SDK_DVR + 18,
    DS_SDK_7103HC = DAVINCI_SDK_DVR + 19,
    DS_SDK_7104HC = DAVINCI_SDK_DVR + 20,
    DS_SDK_7105HC = DAVINCI_SDK_DVR + 21,
    DS_SDK_7106HC = DAVINCI_SDK_DVR + 22,
    DS_SDK_7107HC = DAVINCI_SDK_DVR + 23,
    DS_SDK_7108HC = DAVINCI_SDK_DVR + 24,
    DS_SDK_7109HC = DAVINCI_SDK_DVR + 25,
    DS_SDK_7110HC = DAVINCI_SDK_DVR + 26,
    DS_SDK_7111HC = DAVINCI_SDK_DVR + 27,
    DS_SDK_7112HC = DAVINCI_SDK_DVR + 28,
    DS_SDK_7113HC = DAVINCI_SDK_DVR + 29,
    DS_SDK_7114HC = DAVINCI_SDK_DVR + 30,
    DS_SDK_7115HC = DAVINCI_SDK_DVR + 31,
    DS_SDK_7116HC = DAVINCI_SDK_DVR + 32,

    //HF

#define DAVINCI_SDK_IPMODULE 0x200
    DS_SDK_6101HF_IP = DAVINCI_SDK_IPMODULE+ 1, 	//6446
    DS_SDK_6101H_IP  = DAVINCI_SDK_IPMODULE+ 2 	//6437


        /*-------------------------------------------------------*/

}DEVICE_TYPE;


//***********************************
//数据结构定义
//***********************************

struct version_info 
{
    unsigned short version;
    unsigned short build;
    unsigned int reserved;
};

struct board_info
{
    unsigned int board_type;
    unsigned char production_date[8];
    unsigned char sn[48];
    unsigned int flash_size;
    unsigned int mem_size;
    unsigned int dsp_count;
    unsigned int enc_chan;
    unsigned int dec_chan;
    unsigned int alarm_in;
    unsigned int alarm_out;
    unsigned int disk_num;
    unsigned char mac[6];
    unsigned char reserved[34];
};

struct frame_statistics
{
    unsigned int video_frames;      /*编码的视频帧数*/
    unsigned int audio_frames;      /*编码的音频帧数*/
    unsigned int frames_lost;       /*编码的视频帧丢失数量*/
    unsigned int queue_overflow;    /*视频输入丢失帧数*/
    unsigned int cur_bps;           /*码流*/
};

struct dec_statistics
{
    unsigned int dec_video_num;     /*解码的视频帧数*/
    unsigned int dec_passedV_num;   /*解码跳过的视频帧数*/
    unsigned int dec_passedA_num;   /*解码跳过的音频帧数*/
    unsigned int dec_audio_num;     /*解码的音频帧数*/
    unsigned int dec_sec;           /*解码秒数*/
};

struct disk_status
{
};

struct RECT 
{
    int left;
    int top;
    int right;
    int bottom;
};

struct RECT_EX
{
    int x,y,w,h;
};
struct SYSTEMTIME
{
    unsigned short year;
    unsigned short month;
    unsigned short dayofweek;
    unsigned short day;
    unsigned short hour;
    unsigned short minute;
    unsigned short second;
    unsigned short milliseconds;
};

enum video_standard
{
    STANDARD_NONE = 0,
    STANDARD_NTSC,
    STANDARD_PAL 
};               

enum picture_format {
    ENC_QCIF =2,
    ENC_CIF = 1,
    ENC_2CIF =4,
    ENC_DCIF =0,
    ENC_4CIF =3,
};

struct MENU_COPY_PARAM
{
    int       index;        /*主辅口选择*/
    int       bFullScreen;  /*是否全屏显示*/
    unsigned int x,y,w,h;   /*非全屏显示时指定显示窗口位置,左上起点，宽和高*/
    int       res[2];
};

/****************************菜单ALPHA*****************************/
#define SDK_ALPHA_MENU_BACK11       0		/*菜单背景对比为1:1*/
#define SDK_ALPHA_MENU_BACK31       1		/*菜单背景对比为3:1*/
#define SDK_ALPHA_MENU_BACK13       2		/*菜单背景对比为1:3*/
#define SDK_ALPHA_MENU_BACK_NONE    7		/*不透明*/
struct DISP_PARAM
{
    int		bEnable;    /*菜单开关*/
    int		menuAlpha;  /*菜单与背景图象对比度*/
    unsigned int x;     /*非全屏显示时指定显示窗口位置*/
    unsigned int y;
    unsigned int w;
    unsigned int h;
};
struct SDK_MENU_DISP_PARAM
{
    int	index;                                              /*主辅口选择*/
    int	bFilter;                                            /*是否滤波*/
    int	bFullScreen;                                        /*是否全屏显示*/
    struct DISP_PARAM FullScreenDispParam;                  /*全屏显示时指定显示窗口参数*/
    struct DISP_PARAM DispParam[SDK_MAX_MENU_DISP_NUM];     /*非全屏显示时指定显示窗口参数*/
    int		res[2];
};

struct SDK_MENU_REFRESH_PARAM
{
    int	index;              /*主辅口选择*/
    int	bFullScreenRefresh; /*是否全屏刷新*/
    int	x;                  /*非全屏刷新时指定刷新区域*/
    int	y;
    int	w;
    int	h;
};

struct SDK_MENU_BUF_PARAM
{
    int		index;              /*获取主辅口选择，有应用层传入给SDK库*/
    unsigned char * pMenuY;     /*这几个参数有SDK库填充*/
    unsigned char * pMenuU;
    unsigned char * pMenuV;
    unsigned int	MenuBufW;   /*菜单缓冲区宽度为1024*/
    unsigned int	MenuBufH;   /*缓冲区高度为768*/
};


struct SDK_POS
{
    unsigned int	x;
    unsigned int	y;
};

struct SDK_MOUSE_POS
{
    unsigned int index;/*主辅口选择*/
    struct SDK_POS pos;
};

struct LOGO_DISP_PARAM
{
    unsigned int	bTranslucent;   /*是否半透明*/
    unsigned int x,y;
    unsigned int w;                 /*不大于256，且为16的整数倍*/
    unsigned int h;                 /*不大于128，且为8的整数倍*/
    unsigned int flash;             /*闪烁控制。例：flash＝(2<<8)|1 表示Logo显示2秒，停止1秒*/
    unsigned int res[2];
};


typedef struct
{
    int channel;
    int size;
    int frameType;
    unsigned int standardTime;/* TS/PS标准时间,单位为90kHz时钟,取其高32位,即45kHz时钟 */
    struct SYSTEMTIME absTime;/*绝对时钟*/
    void * pBuf;
}CALLBACK_DATA;

//**************************
// API定义
//**************************

//获取SDK版本信息
int GetSDKVersion(struct version_info* info);
//系统初始化
int InitSystem();
int FiniSystem();

//获取板子相关信息
int GetBoardInfo(struct board_info* info);

//看门狗
int StartWatchdog(unsigned int timer);
int StopWatchdog();

//切换FLASH和HDISK
int EnableATA(void);
int EnableFLASH(void);

//初始化485设备
int InitRS485(int fd,int baudrate, int data, int stop, int parity, int flowcontrol);

//报警输入输出
int GetAlarmIn(unsigned int* alarm_in_status);
int SetAlarmOut(unsigned int alarm_out_status);
int GetAlarmOut(unsigned int * out_status);


int SetOSDDisplayMode(int channel, int brightness, int translucent, int param, int line_count,unsigned short**format);


/*
 @osd_type
#define OSD_TYPE_ENGYYMMDD			0		 XXXX-XX-XX 年月日 
#define OSD_TYPE_ENGMMDDYY			1		 XX-XX-XXXX 月日年 
#define OSD_TYPE_CHNYYMMDD			2		 XXXX年XX月XX日 
#define OSD_TYPE_CHNMMDDYY			3		 XX月XX日XXXX年 
#define OSD_TYPE_ENGDDMMYY			4		 XX-XX-XXXX 日月年 
#define OSD_TYPE_CHNDDMMYY			5		 XX日XX月XXXX年 

 @osd_attr:
 0 不显示OSD
 1 透明，闪烁
 2 透明，不闪烁
 3 不透明，闪烁
 4 不透明，不闪烁
 */
int EnableOSD(int channel, int enable);
int SetDSPDateTime(int channel, struct SYSTEMTIME* now);

/*
 * 屏幕遮挡
 * 矩形的宽度为16对齐,高度为8对齐
 */
int SetupMask(int channel, struct RECT* rc, int count);
int StopMask(int channel);

//移动侦测
int SetMotionDetection(int channel, int grade, int fast_fps, int low_fps, struct RECT* rc, int count);
int EnalbeMotionDetection(int channel, int enable);
int GetMotionDetectionMask(int channel, unsigned int* mask);

/**
 @int channel 通道号1-n
 @int quality 图像质量 0 做好 ；1较好；2一般
 @int mode    图象模式 0 cif；1 qcif；2 d1
 @unsigned char* image
 @unsigned int* size
 */
int GetJPEGImage(int channel, int quality,int mode, unsigned char* image,unsigned int* size);
//int GetOriginalImage(int channel, unsigned char* image, unsigned int size);

int SetVideoParam(int channel, int brightness,int contrast, int saturation, int hue);
int GetVideoParam(int channel, enum video_standard* standard, int* brightness, int*contrast, int*saturation,int* hue);

int SetStreamType(int channel,int chan_type, int type);
int GetStreamType(int channel,int chan_type,int*type);


/*
 *@ 量化系数12-30
 */
int SetDefaultQuant(int channle, int chan_type,int quality);

//设置编码帧结构,帧率
int SetIBPMode(int channel,  int chan_type,int key_frame_inter, int b_frames, int p_frames,int frame_rate);

//设置码流的最大比特率和码流控制模式
int SetBitrateControl(int channel,int chan_type, unsigned int max_bps,int brc );


//设置编码的分辨率格式
int SetEncoderPictureFormat(int channel, int chan_type,enum picture_format format);

//起动编码器
int StartCodec(int channel, int chan_type);
int StopCodec(int channel, int chan_type);

//强制I帧
int CaptureIFrame(int channle, int chan_type);

//获取统计信息
int GetFramesStatistics(int channel,int chan_types,struct frame_statistics* statistics);

/*
 @signal 1 have signal;0 no siganl 
 */
int GetVideoSignal(int channel, int * signal);

int SetMaskAlarm(int channel, int enable, struct RECT* rc);

/*
 @mask 1 mask ;0 no mask 
 */
int GetVideoMask(int channel, int* mask);

typedef void (*STREAM_DATA_CALLBACK)(CALLBACK_DATA CallBackData,void* context);
int RegisterStreamDataCallback(STREAM_DATA_CALLBACK func,void* context);

//语音对讲
int OpenVoiceChannel();
int CloseVoiceChannel();
int ReadVoiceData (void* buf, int size);
int WriteVoiceData(void* buf, int size);


//蜂鸣器
void Beep(unsigned int mode);

int IsDVS_A(void);
int GetDeviceType(unsigned int * device_type);

#define MAIN_CHANNEL 1 
#define SUB_CHANNEL  0 

int SetOutputFormat(int format);

int SetDisplayParams(int index, int screen);

int ClrDisplayParams(int index,int channel);

int SetDisplayOrder(int index, unsigned char* order);

int SetPreviewScreen(int index, int channel, int mode);

int SetPreviewAudio(int index, int channel, int mode);

int SetDecodeScreen(int index, int channel, int mode);

int SetDecodeAudio(int index, int channel, int mode);

int StartDecode(int index, int channel, int mode, void * file_header);

int StopDecode(int handle);

int SetDecodeSpeed(int handle, int speed);

int SetDecodeVolume(int handle, int volume);

int DecodeNextFrame(int handle);

int InputAvData(int handle, void* buf, int size);

int SetMenuDispParam(struct SDK_MENU_DISP_PARAM * pMenuParam);

int	RefreshMenuDisp(struct SDK_MENU_REFRESH_PARAM * pRefreshParam);

int	GetMenuBufParam(struct SDK_MENU_BUF_PARAM * pMenuBufParam);

int DisplayMouseEnable(int VoIndex,int bEnable);

int InitMouse(unsigned char * pMouseData);

int SetMousePos(struct SDK_MOUSE_POS MousePos);

int GetDecodeStatistics(int handle,struct dec_statistics *DecStatistics);

#ifdef __cplusplus
}
#endif

#endif
