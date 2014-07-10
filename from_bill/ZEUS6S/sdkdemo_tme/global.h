/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:    global.h
Author:        Version :          Date:   07-12-15
Description:   
Version:    	// ∞Ê±æ–≈œ¢
Function List:	
History:      	
    <author>  <time>   <version >   <desc>
***********************************************************************************************/


#ifndef __GLOBAL_H__
#define __GLOBAL_H__

extern pthread_mutex_t    globalMSem;

extern DEVCFG                *pDevCfgPara;
extern CONNECTCFG          connectPara[GLOBAL_MAX_LINK];
extern CONNECTCFG         *pConnectPara;
extern pthread_mutex_t	     voiceMutex;
extern UINT8                     channelnum;
extern char                        sendBufMedia[];
extern unsigned long          readIndex;
extern BOOL	                   bWriteFlg;
extern CHNLPARA	            chnlPara[];
extern UINT16                   TotalLinks;
extern  UINT32                  CurrAlarmStatus[];
extern  struct board_info   BOARDINFO;
extern unsigned int            alarmInnum;
extern unsigned int            alarmOutnum;
extern UINT32                  MotdetStatus;
extern PLAY_BACK  PlayBackStruct;
extern BOOL           IsATAMode;
extern int                    VIDEO_STANDARD;

extern BOOL                     bRecordStart[MAX_CHNL_NUM];
extern BOOL              bMotRecordStart[MAX_CHNL_NUM];
extern int                   MotRecFd[MAX_CHNL_NUM];
extern time_t             MotRecordStartTime[MAX_CHNL_NUM];
extern unsigned short DeviceName[];
extern unsigned short DateTime[];
extern unsigned short ChnlName[4][32];
#ifdef DVR_DEMO
extern BOOL    bPlayBackStart[MAX_CHNL_NUM];
#endif
#endif
