/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         main.c
Author:           fangjiancai
Version :         1.0
Date:             07-12-20
Description:   begin to execute the program
Version:    	// 版本信息
Function List:	main(),initChnlEncode()
History:      	
    <author>      <time>   <version >   <desc>
    fangjiancai  07-12-20  1.0          moved from dvs_demo
 ***********************************************************************************************/

#include "include_all.h"

extern int getDevCfgParam ( DEVCFG *pDevCfg );

unsigned int      wdTime = 5;
int                    VIDEO_STANDARD;
struct board_info BOARDINFO;
DEVCFG            devCfgParam;
DEVCFG	         *pDevCfgPara = &devCfgParam;
CONNECTCFG        connectPara[GLOBAL_MAX_LINK];
CONNECTCFG       *pConnectPara = &connectPara;
CHNLPARA	  chnlPara[MAX_CHNL_NUM];
unsigned long     readIndex;
unsigned int      alarmInnum;
unsigned int      alarmOutnum;
UINT32            CurrAlarmStatus[MAX_ALARM_TYPE];
/*如果某类型的报警的某一位为1，说明该通道有报警发生,例如，
CurrAlarmStatus[1]的第0位为1，说明第0通道有类型为NOSIGNAL_ALARM报警*/
UINT8             channelnum;
BOOL	          bWriteFlg;
UINT16            TotalLinks;
UINT32            MotdetStatus;
pthread_mutex_t   voiceMutex;
PLAY_BACK         PlayBackStruct;
BOOL              IsATAMode;
BOOL              bRecordStart[MAX_CHNL_NUM];
BOOL              bMotRecordStart[MAX_CHNL_NUM];
int                   MotRecFd[MAX_CHNL_NUM];
time_t             MotRecordStartTime[MAX_CHNL_NUM];

unsigned short   DeviceName[]={ 16,16,'D','E','V','I','C','E','_','_','N','A','M','E','_','D','A','V','I','N','C',0};
unsigned short   DateTime[] = { 16,200,/*_OSD_YEAR4*/0x9000,0x2d,/* _OSD_MONTH3*/0x9002,0x2d,/*_OSD_DAY*/0x9004,0x20,/* _OSD_WEEK3*/0x9005,0x20,/*_OSD_HOUR24*/0x9007,0x3a,/*_OSD_MINUTE*/0x9009,0x3a,/*_OSD_SECOND*/0x900a,0x20,0};
unsigned short   ChnlName[MAX_CHNL_NUM][32]={{16,416,'C','H','A','N','N','E','L','_','_','0','0',0},
	                                                                 {16,416,'C','H','A','N','N','E','L','_','_','0','1',0},
	                                                                 {16,416,'C','H','A','N','N','E','L','_','_','0','2',0},
	                                                                 {16,416,'C','H','A','N','N','E','L','_','_','0','3',0}};

#ifdef DVR_DEMO
BOOL              bPlayBackStart[MAX_CHNL_NUM];
#endif
extern unsigned short *osd_buffer;

/********************************************************************************
Function:        initChnlEncode()
Description:     初始化各个通道，并给通道分配缓冲区。
Input  :         无
Output:          无
Return:          ERROR for error and OK for right
********************************************************************************/
int initChnlEncode ( )
{
	int       i;
	CHNLPARA *pChnlParaTmp;
	
	for ( i = 0; i </* MAX_CHNL_NUM*/channelnum; i++ )
	{
		pChnlParaTmp = &chnlPara[i];
		pthread_mutex_init (&(pChnlParaTmp->write_mutexSem), NULL);
		pthread_mutex_init (&(pChnlParaTmp->subwrite_mutexSem), NULL);
		pChnlParaTmp->netBuffer.netBufSize = MEDIA_BUFF_SIZE;
		pChnlParaTmp->netBuffer.readPos = pChnlParaTmp->netBuffer.writePos = 0;
		pChnlParaTmp->netBufferSub.netBufSize = MEDIA_BUFF_SIZE;
		pChnlParaTmp->netBufferSub.readPos = pChnlParaTmp->netBufferSub.writePos = 0;
		pChnlParaTmp->recBuffer.netBufSize = MEDIA_BUFF_SIZE;
		pChnlParaTmp->recBuffer.readPos = pChnlParaTmp->recBuffer.writePos = 0;
		pChnlParaTmp->recBuffer.netBufBase = memalign ( 0x10, MEDIA_BUFF_SIZE );
		pChnlParaTmp->linknums = pChnlParaTmp->sublinknums =0;
		pChnlParaTmp->NetConnect = pChnlParaTmp->SubNetConnect=NULL;
		pChnlParaTmp->netBuffer.Totalwrites = 0;
		if ( pDevCfgPara->bChnlStart[i] )
		{
			printf ( "initChnlEncode, i = %d.\n", i+1 );
			pChnlParaTmp->netBuffer.netBufBase = memalign ( 0x10, MEDIA_BUFF_SIZE );
			
			if ( pChnlParaTmp->netBuffer.netBufBase == NULL ) 
			{
			       printf("memalign error!\n");
			       return ERROR;
			}
		}
		if ( pDevCfgPara->bSubChnlStart[i] )
		{
			pChnlParaTmp->netBufferSub.netBufBase = memalign ( 0x10, MEDIA_BUFF_SIZE );
			if ( pChnlParaTmp->netBufferSub.netBufBase == NULL ) 
			{
				return ERROR;
			}
		}
		bRecordStart[i]=FALSE;
		bMotRecordStart[i] = FALSE;
		MotRecordStartTime[i] = 0;
		MotRecFd[i] = -1;
		#ifdef DVR_DEMO
		bPlayBackStart[i]=FALSE;
		#endif
	}
	return OK;
}

/********************************************************************************
Function:       exitCall()
Description:    when eixt the system ,it can free the resources
Input:          无
Output:         无
Return:         无
********************************************************************************/
void exitCall ()
{
	int i;
	CHNLPARA	*pChnlParaTmp;
	for ( i = 0; i < MAX_CHNL_NUM; i++ )
	{
		pChnlParaTmp = &chnlPara[i];
		pthread_mutex_destroy ( &(pChnlParaTmp->write_mutexSem ) );
		pthread_mutex_destroy ( &(pChnlParaTmp->subwrite_mutexSem ) );
		
		if ( pDevCfgPara->bChnlStart[i] || pChnlParaTmp->netBuffer.netBufBase != NULL )
		{
			free ( pChnlParaTmp->netBuffer.netBufBase );
		}
		
		if ( pDevCfgPara->bSubChnlStart[i] || pChnlParaTmp->netBufferSub.netBufBase != NULL )
		{
			free ( pChnlParaTmp->netBufferSub.netBufBase );
		}
		
		pChnlParaTmp->netBuffer.netBufBase = NULL;
		pChnlParaTmp->netBufferSub.netBufBase = NULL;
	}
	pthread_mutex_destroy ( &voiceMutex );

	semMDestroy ( &globalMSem );

	StopWatchdog ();

	FiniSystem ();
	return;
}

/*********************************************************************************
Function :        main()
Description:      initial the board ,create netserverTask and AlarmCtrl task  
Input :           无
Output:           无
Return:           错误返回-1，正确返回0
*********************************************************************************/

int main(int argc, char* argv[] )
{
	int     ret,i/*,retdata*/;
	struct version_info  ver;
	struct board_info    BOARDINFO;
	//char   jpeg[200*1024];
	//int    maxEncChan = 0;
	int    channel;
	//pthread_t pid;
	printf ( "system initial.\n" );
	atexit ( exitCall );
	pthread_mutex_init ( &voiceMutex, NULL );
	#ifdef DVR_DEMO
	VIDEO_STANDARD = VIDEOS_PAL;
	video_osd_setup(0);
	#endif
	//SetEncodeMode(1);
	ret = InitSystem();
	if(ret==-1)
	{
		printf("InitSystem fail\n");
	}
	else
	{
		printf("InitSystem successful\n");
	}
	setOsdTransparency(0,TRUE,0,0,0,0);
	if(0!=ret)
	{
		printf("initsytem error!\n");
	}
	
	GetSDKVersion(&ver);
	printf(" SDK Version: V %d.%d build %d\n", MAJ_VER(ver.version),MIN_VER(ver.version), ver.build);
	
	GetBoardInfo(&BOARDINFO);
	alarmInnum = BOARDINFO.alarm_in;
	alarmOutnum = BOARDINFO.alarm_out;
	channelnum=BOARDINFO.enc_chan/*2*/;
	printf("BOARDINFO.enc_chan=%d\n",BOARDINFO.enc_chan);
	printf("memsize=%x,flashsize =%x,alarmInNum=%d\n",BOARDINFO.mem_size,BOARDINFO.flash_size,BOARDINFO.alarm_in);
	IsATAMode = FALSE;
	printf("This board has %d enc_channel\n",channelnum);
	
	if ( getDevCfgParam ( pDevCfgPara ) == ERROR ) 
	{
		 return ERROR;
	}

	RegisterStreamDataCallback(DspCallback, NULL);
	printf("Registerstreamdatacallback-------------------------------------\n");

	if ( serVideoPara () == ERROR ) 
	{
	      return ERROR;
	}
	else
	{
	      printf("**********servideopare() successful*************************\n");
	}
	initChnlEncode ();
	#ifdef DVR_DEMO
	SetOutputFormat(VIDEO_STANDARD);
	// for preview
	// demo 4 screen preview
	ret=SetDisplayOrder(MAIN_OUTPUT,&(pDevCfgPara->VideoOut.PreviewOrder[0]));
	if(0!=ret)
	{
	      printf("SetDisplayOrder() Fail\n");
	}
	else
	{
              printf("SetDisplayOrder() successful\n");
	}
	ret=SetDisplayParams(MAIN_OUTPUT,pDevCfgPara->VideoOut.ScreenNum);
	if(0!=ret)
	{
	      printf("SetDisplayParams() Fail\n");
	}
	else
	{
	      printf("SetDisplayParams() successful\n");
	}
	for ( i = 0; i < pDevCfgPara->VideoOut.ScreenNum&&i<channelnum; i++ ) 
	{
	      channel = pDevCfgPara->VideoOut.PreviewOrder[i];
              if(channel!=0xff)
              {
		   ret=SetPreviewScreen(MAIN_OUTPUT,channel+1,1);
		   printf("ret=%d\n",ret);
              }
	}
        ret=SetPreviewAudio(0,1, 1);
	if(ret==0)
	{
	      printf("open Preview Audio successful\n");
	}
	else
	{
	      printf("open Preview Audio fault\n");
	}
	/*画边框*/
	//DrawFrame(osd_buffer,704,100,100,300,300,20,0x7800,0x55);
	/**/
	//FillArea(osd_buffer,720,80,50,550,450,0x0004,TRUE,0x33,0x77,0xFFFF,5);
	/*显示一个rgb888格式的bmp图片*/
	//char filename[32];
	//sprintf(filename,"record.bmp");
	//DrawBMP(filename,osd_buffer,720,80,50,TRUE,0x33,0x77,0xFFFF,3);
	/*画菜单*/
	//DrawMenu(osd_buffer);
	#endif

	Beep(0);
	sleep(1);
	Beep(1);
	
        for(i=0;i<channelnum;i++)
	{
	      ret=StartCodec(i+1,MAIN_CHANNEL);
              if(ret==-1)
	      {
	           printf("startCodec fail\n");
	      }
	      else
	      {
                   printf("startcodec successful\n");
	      }
	      //StartCodec(i+1,SUB_CHANNEL);
	}
	/*
	//报警输入输出测试
       unsigned int alarm_out_status;
	alarm_out_status = 1;
       SetAlarmOut(alarm_out_status);
	 usleep(10);
	 unsigned int *out_status;
	 GetAlarmOut(out_status);
	 printf("alarmout status=%d\n",*out_status);

	 alarm_out_status = 2;
        SetAlarmOut(alarm_out_status);
	 usleep(10);
	 GetAlarmOut(out_status);
	 printf("alarmout status=%d\n",*out_status);
	 unsigned int alarminstatus;
	 GetAlarmIn(&alarminstatus);
	 printf("alarminstatus=%d\n",alarminstatus);
	*/
	startUpTask ( 170, 4096, (ENTRYPOINT)netServerTask, 0,0,0,0,0 );
	startUpTask ( 170, 4096, (ENTRYPOINT)AlarnInCtrl,0, 0, 0,0,0);
	#ifdef DVR_DEMO
	startUpTask(170,8192,(ENTRYPOINT)PlayBack,0,0,0,0,0);
	#endif
	startUpTask(170,8192,(ENTRYPOINT)StreamRec,0,0,0,0,0);
	startUpTask(170,8192,(ENTRYPOINT)MotStreamRec,0,0,0,0,0);
	//StreamRec();
	 
	while ( 1 )
	{
	      pause ();
	}
	
	ret = FiniSystem();

	return OK;
}
