/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:       Alarm.c 
Author:         方建才     
Version :       1.0  
Date:           2007-12-20
Description:    process alarm.
Function List:	NetSetAlarm(),NetGetAlarm(),AlarmCtrl(),BeepAlarm()
History:      	
    <author>  <time>   <version >   <desc>
   fangjiancai 07/12/20 1.0         add alarmCtrl() and netSetAlarm()
***********************************************************************************************/

#include "include_all.h"


/********************************************************************************
Function:   BeepAlarm()
Input:      NULL
Output:     NULL
Return:     NULL
Calls:      Beep()
Called by:  AlarmCtrl()
Description:make sound of the beep
********************************************************************************/
void BeepAlarm(void)
{
        Beep(0);
	sleep(1);
	Beep(1);
	sleep(1);
	Beep(0);
	sleep(1);
	Beep(1);
	sleep(1);
	Beep(0);
	sleep(1);
	Beep(1);

}


/********************************************************************************
Function:
Input:
Output:
Return:
Description:
********************************************************************************/
/*int maskAlarm ( void )
{
	int nMask;
	if ( GetVideoMask( 1, &nMask ) )
	{
		  return OK;
	}
}

*/
/********************************************************************************
Function:         netSetAlarm()
Input:            socketfd,and a pointer of command buffer
Output:            
Return:           ERROR for error,OK for right
Calls:            SetMotionDetection()
                  EnalbeMotionDetection()
                  SetMaskAlarm()
                  writeDevCfg()
Called by:        processClientRequest()
Description:      set the device's configer file 
********************************************************************************/
int  netSetAlarm(int netfd,char *cmd)
{
        ALARM_CMDPARA alarmparacmd;
	int chnl,i;

	close(netfd);
	memcpy(&alarmparacmd,cmd+NETCMD_HEADER_LEN,sizeof(alarmparacmd));
	chnl=ntohl(alarmparacmd.chnl);

        /*设置报警输入配置：报警器类型，处理方式*/
	for(i=0;i<alarmInnum;i++)
	{
	    pDevCfgPara->AlarmInCfg[i].Sensortype=ntohl(alarmparacmd.AlarmInCfg[i].Sensortype);
	    pDevCfgPara->AlarmInCfg[i].AlarmInAlarmOutTrig=ntohl(alarmparacmd.AlarmInCfg[i].AlarmInAlarmOutTrig);
	    pDevCfgPara->AlarmInCfg[i].AlarmInHandleType=ntohl(alarmparacmd.AlarmInCfg[i].AlarmInHandleType);
	}
	
	pDevCfgPara->MaskCfg[chnl-1].bMaskHandle=htonl(alarmparacmd.MaskCfg.bMaskHandle);
	printf("bmaskHandle=%d\n",pDevCfgPara->MaskCfg[chnl-1].bMaskHandle);
        /*如果设置了遮挡报警*/
	if(pDevCfgPara->MaskCfg[chnl-1].bMaskHandle)
	{
            /*将设置信息存如配置结构*/
	    pDevCfgPara->MaskCfg[chnl-1].MaskAlarmOutTrig=htonl(alarmparacmd.MaskCfg.MaskAlarmOutTrig);
	    pDevCfgPara->MaskCfg[chnl-1].MaskHandleType=htonl(alarmparacmd.MaskCfg.MaskHandleType);
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.bottom=htonl(alarmparacmd.MaskCfg.MaskRect.bottom);
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.left = htonl(alarmparacmd.MaskCfg.MaskRect.left);
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.right = htonl(alarmparacmd.MaskCfg.MaskRect.right);
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.top = htonl(alarmparacmd.MaskCfg.MaskRect.top);

	    printf("handletype=%d,maskalarmouttrig=%d,bottom=%d,letf=%d,right=%d,top=%d\n",
	           pDevCfgPara->MaskCfg[chnl-1].MaskHandleType,
                   pDevCfgPara->MaskCfg[chnl-1].MaskAlarmOutTrig,
	           pDevCfgPara->MaskCfg[chnl-1].MaskRect.bottom,
	           pDevCfgPara->MaskCfg[chnl-1].MaskRect.left,
	           pDevCfgPara->MaskCfg[chnl-1].MaskRect.right,
	           pDevCfgPara->MaskCfg[chnl-1].MaskRect.top
		  );
	}
	else/*没有设置遮挡报警，将和遮挡报警相关的结构清零*/
	{
	    pDevCfgPara->MaskCfg[chnl-1].MaskAlarmOutTrig=0;
	    pDevCfgPara->MaskCfg[chnl-1].MaskHandleType=0;
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.bottom=0;
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.left=0;
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.right=0;
	    pDevCfgPara->MaskCfg[chnl-1].MaskRect.top=0;
	}

	pDevCfgPara->MotdetCfg[chnl-1].bMotHandle = ntohl(alarmparacmd.MotdetCfg.bMotHandle);
        /*设置移动侦测且侦测区域没有达到极限*/
	if(pDevCfgPara->MotdetCfg[chnl-1].bMotHandle&&pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum<MAX_MASK_REGION)
	{
            /*将配置信息保存在结构中*/
	    pDevCfgPara->MotdetCfg[chnl-1].MotAlarmOutTrig = ntohl(alarmparacmd.MotdetCfg.MotAlarmOutTrig);
	    pDevCfgPara->MotdetCfg[chnl-1].MotHandleType = ntohl(alarmparacmd.MotdetCfg.MotHandleType);

	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum].bottom =\
						              ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].bottom);

	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum].left =\
							      ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].left);

	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum].right =\
							      ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].right);

	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum].top =\
							      ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].top);
            pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum=1;
	}
	/*取消移动侦测*/
	else if(pDevCfgPara->MotdetCfg[chnl-1].bMotHandle==0)
	{
            /*将相应的结构清零*/
	    pDevCfgPara->MotdetCfg[chnl-1].MotAlarmOutTrig=0;
	    pDevCfgPara->MotdetCfg[chnl-1].MotHandleType=0;
            pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum=0;
	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].bottom=0;
	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].left=0;
	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].right=0;
	    pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].top=0;
	}
	else
	{
	    printf("too much area\n");
	}

	pDevCfgPara->NoSignalCfg[chnl-1].bNoSigHandle=ntohl(alarmparacmd.NoSignalCfg.bNoSigHandle);
        /*设置信号丢失报警*/
	if(pDevCfgPara->NoSignalCfg[chnl-1].bNoSigHandle)
	{
	    pDevCfgPara->NoSignalCfg[chnl-1].NoSigAlarmOutTrig=ntohl(alarmparacmd.NoSignalCfg.NoSigAlarmOutTrig);
	    pDevCfgPara->NoSignalCfg[chnl-1].NoSigHandleType = ntohl(alarmparacmd.NoSignalCfg.NoSigHandleType);
	}
	else
	{
	    pDevCfgPara->NoSignalCfg[chnl-1].NoSigAlarmOutTrig=0;
	    pDevCfgPara->NoSignalCfg[chnl-1].NoSigHandleType=0;
	}
	/*启动设置，使设置生效，并写回配置文件*/
	SetMotionDetection(chnl,
	                   pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.level,
	                   pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.fastInterval,
	                   pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.slowInterval,
	                   pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect,
		            pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum
	                   );
	EnalbeMotionDetection(chnl,pDevCfgPara->MotdetCfg[chnl-1].bMotHandle);
	SetMaskAlarm(chnl,pDevCfgPara->MaskCfg[chnl-1].bMaskHandle,&(pDevCfgPara->MaskCfg[chnl-1].MaskRect));
	writeDevCfg(pDevCfgPara);
	/*//dubug
	printf("alarminalarmoutTrig=%d\n\
	           alarmInHandleType=%d\n\
	           bAlarmInHandle=%d\n\
	           SensorType=%d\n\
	           MaskAlarmOutTrig=%d\n\
	           MaskHandleType=%d\n\
	           bMaskHandle=%d\n\
	           MaskRect=(%d,%d,%d,%d)\n\
	           MotAlarmOutTrig=%d\n\
	           MotHandleType=%d\n\
	           bMotHandle=%d\n\
	           moveRect=(%d,%d,%d,%d)\n\
	           NosigAlarmouttrig=%d\n\
	           NoSigHandleType=%d\n\
	           bNoSigHandle=%d\n",
	           ntohl(alarmparacmd.AlarmInCfg[0].AlarmInAlarmOutTrig),
	            ntohl(alarmparacmd.AlarmInCfg[0].AlarmInHandleType),
	            ntohl(alarmparacmd.AlarmInCfg[0].bAlarmInHandle),
	            ntohl(alarmparacmd.AlarmInCfg[0].Sensortype),
	            ntohl(alarmparacmd.MaskCfg.MaskAlarmOutTrig),
	            ntohl(alarmparacmd.MaskCfg.MaskHandleType),
	            ntohl(alarmparacmd.MaskCfg.bMaskHandle),
	            ntohl(alarmparacmd.MaskCfg.MaskRect.bottom),
	            ntohl(alarmparacmd.MaskCfg.MaskRect.left),
	            ntohl(alarmparacmd.MaskCfg.MaskRect.right),
	            ntohl(alarmparacmd.MaskCfg.MaskRect.top),
	            ntohl(alarmparacmd.MotdetCfg.MotAlarmOutTrig),
	            ntohl(alarmparacmd.MotdetCfg.MotHandleType),
	            ntohl(alarmparacmd.MotdetCfg.bMotHandle),
	            ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].bottom),
	            ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].left),
	            ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].right),
	            ntohl(alarmparacmd.MotdetCfg.MoveDetect.moveRect[0].top),
	            ntohl(alarmparacmd.NoSignalCfg.NoSigAlarmOutTrig),
	            ntohl(alarmparacmd.NoSignalCfg.NoSigHandleType),
	            ntohl(alarmparacmd.NoSignalCfg.bNoSigHandle));
	*/
	return OK;
}

/***********************************************************************************************
Function:           netGetAlarm()
Description:       get the device's configer file about the alarm 
Input:               socketfd,and a pointer of command buffer
Output:            
Return:             ERROR for error,OK for right
***********************************************************************************************/
int netGetAlarm(int netfd,char *cmd)
{
    struct
    {
	     NETCMD_HEADER header;
            int   channel;
    }NetGetAlarmParacmd;
    ALARM_CMDPARA   NetGetAlarmPara;
    HIDEAREACFG        HideArea;
    int chnl,i;
    memcpy((char *)&NetGetAlarmParacmd,cmd,sizeof(NetGetAlarmParacmd));
    chnl = ntohl(NetGetAlarmParacmd.channel);
    NetGetAlarmPara.AlarmInCfg[0].Sensortype = htonl(pDevCfgPara->AlarmInCfg[0].Sensortype);
    NetGetAlarmPara.AlarmInCfg[0].bAlarmInHandle=htonl(pDevCfgPara->AlarmInCfg[0].bAlarmInHandle);
    NetGetAlarmPara.AlarmInCfg[0].AlarmInHandleType=htonl(pDevCfgPara->AlarmInCfg[0].AlarmInHandleType);
    NetGetAlarmPara.AlarmInCfg[0].AlarmInAlarmOutTrig=htonl(pDevCfgPara->AlarmInCfg[0].AlarmInAlarmOutTrig);
	
    NetGetAlarmPara.AlarmInCfg[1].Sensortype = htonl(pDevCfgPara->AlarmInCfg[1].Sensortype);
    NetGetAlarmPara.AlarmInCfg[1].bAlarmInHandle=htonl(pDevCfgPara->AlarmInCfg[1].bAlarmInHandle);
    NetGetAlarmPara.AlarmInCfg[1].AlarmInHandleType=htonl(pDevCfgPara->AlarmInCfg[1].AlarmInHandleType);
    NetGetAlarmPara.AlarmInCfg[1].AlarmInAlarmOutTrig=htonl(pDevCfgPara->AlarmInCfg[1].AlarmInAlarmOutTrig);

    NetGetAlarmPara.AlarmInCfg[2].Sensortype = htonl(pDevCfgPara->AlarmInCfg[2].Sensortype);
    NetGetAlarmPara.AlarmInCfg[2].bAlarmInHandle=htonl(pDevCfgPara->AlarmInCfg[2].bAlarmInHandle);
    NetGetAlarmPara.AlarmInCfg[2].AlarmInHandleType=htonl(pDevCfgPara->AlarmInCfg[2].AlarmInHandleType);
    NetGetAlarmPara.AlarmInCfg[2].AlarmInAlarmOutTrig=htonl(pDevCfgPara->AlarmInCfg[2].AlarmInAlarmOutTrig);

    NetGetAlarmPara.AlarmInCfg[3].Sensortype = htonl(pDevCfgPara->AlarmInCfg[3].Sensortype);
    NetGetAlarmPara.AlarmInCfg[3].bAlarmInHandle=htonl(pDevCfgPara->AlarmInCfg[3].bAlarmInHandle);
    NetGetAlarmPara.AlarmInCfg[3].AlarmInHandleType=htonl(pDevCfgPara->AlarmInCfg[3].AlarmInHandleType);
    NetGetAlarmPara.AlarmInCfg[3].AlarmInAlarmOutTrig=htonl(pDevCfgPara->AlarmInCfg[3].AlarmInAlarmOutTrig);

    NetGetAlarmPara.MaskCfg.bMaskHandle = htonl(pDevCfgPara->MaskCfg[chnl-1].bMaskHandle);
    NetGetAlarmPara.MaskCfg.MaskAlarmOutTrig=htonl(pDevCfgPara->MaskCfg[chnl-1].MaskAlarmOutTrig);
    NetGetAlarmPara.MaskCfg.MaskHandleType=htonl(pDevCfgPara->MaskCfg[chnl-1].MaskHandleType);
    NetGetAlarmPara.MaskCfg.MaskRect.bottom=htonl(pDevCfgPara->MaskCfg[chnl-1].MaskRect.bottom);
    NetGetAlarmPara.MaskCfg.MaskRect.left = htonl(pDevCfgPara->MaskCfg[chnl-1].MaskRect.left);
    NetGetAlarmPara.MaskCfg.MaskRect.right=htonl(pDevCfgPara->MaskCfg[chnl-1].MaskRect.right);
    NetGetAlarmPara.MaskCfg.MaskRect.top=htonl(pDevCfgPara->MaskCfg[chnl-1].MaskRect.top);

    NetGetAlarmPara.MotdetCfg.bMotHandle=htonl(pDevCfgPara->MotdetCfg[chnl-1].bMotHandle);
    NetGetAlarmPara.MotdetCfg.MotAlarmOutTrig=htonl(pDevCfgPara->MotdetCfg[chnl-1].MotAlarmOutTrig);
    NetGetAlarmPara.MotdetCfg.MotHandleType = htonl(pDevCfgPara->MotdetCfg[chnl-1].MotHandleType);
    NetGetAlarmPara.MotdetCfg.MoveDetect.delay=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.delay);
    NetGetAlarmPara.MotdetCfg.MoveDetect.detectNum=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.detectNum);
    NetGetAlarmPara.MotdetCfg.MoveDetect.fastInterval=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.fastInterval);
    NetGetAlarmPara.MotdetCfg.MoveDetect.level=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.level);
    NetGetAlarmPara.MotdetCfg.MoveDetect.slowInterval=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.slowInterval);
	/*只回送给客户端该通道的第一个移动侦测区域*/
    NetGetAlarmPara.MotdetCfg.MoveDetect.moveRect[0].bottom=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].bottom);
    NetGetAlarmPara.MotdetCfg.MoveDetect.moveRect[0].left=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].left);
    NetGetAlarmPara.MotdetCfg.MoveDetect.moveRect[0].right=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].right);
    NetGetAlarmPara.MotdetCfg.MoveDetect.moveRect[0].top=htonl(pDevCfgPara->MotdetCfg[chnl-1].MoveDetect.moveRect[0].top);

    NetGetAlarmPara.NoSignalCfg.bNoSigHandle=htonl(pDevCfgPara->NoSignalCfg[chnl-1].bNoSigHandle);
    NetGetAlarmPara.NoSignalCfg.NoSigAlarmOutTrig=htonl(pDevCfgPara->NoSignalCfg[chnl-1].NoSigAlarmOutTrig);
    NetGetAlarmPara.NoSignalCfg.NoSigHandleType=htonl(pDevCfgPara->NoSignalCfg[chnl-1].NoSigHandleType);

    /*发送屏幕遮挡区域*/
    HideArea.nAreaNum = pDevCfgPara->HideArea[chnl-1].nAreaNum;

    for(i=0;i<MAX_MASK_REGION;i++)
    {
         HideArea.rect[i].bottom= htonl(pDevCfgPara->HideArea[chnl-1].rect[i].bottom);
         HideArea.rect[i].left = htonl(pDevCfgPara->HideArea[chnl-1].rect[i].left);
         HideArea.rect[i].right = htonl(pDevCfgPara->HideArea[chnl-1].rect[i].right);
         HideArea.rect[i].top = htonl(pDevCfgPara->HideArea[chnl-1].rect[i].top);
    }
	/*发送数据*/
    tcpSend(netfd, &NetGetAlarmPara, sizeof(NetGetAlarmPara));
    tcpSend(netfd,&HideArea,sizeof(HIDEAREACFG));
    close(netfd);
    return OK;
		
	   #if 0
	ALARMCFG alarmcfg;
	memcpy(&chnl,cmd+NETCMD_HEADER_LEN,sizeof(chnl));
	chnl=ntohl(chnl);
	alarmcfg.bMaskAlarm=htonl(pDevCfgPara->alarmPara[chnl-1].bMaskAlarm);
	alarmcfg.bMoveDetect=htonl(pDevCfgPara->alarmPara[chnl-1].bMoveDetect);
	alarmcfg.bNoSignal =htonl(pDevCfgPara->alarmPara[chnl-1].bNoSignal);
	for(i=0;i<pDevCfgPara->alarmPara[chnl-1].maskAlarmArea.nAreaNum;i++)
	{
		alarmcfg.maskAlarmArea.rect[i].bottom=htonl(pDevCfgPara->alarmPara[chnl-1].maskAlarmArea.rect[i].bottom);
		alarmcfg.maskAlarmArea.rect[i].left=htonl(pDevCfgPara->alarmPara[chnl-1].maskAlarmArea.rect[i].left);
		alarmcfg.maskAlarmArea.rect[i].right=htonl(pDevCfgPara->alarmPara[chnl-1].maskAlarmArea.rect[i].right);
		alarmcfg.maskAlarmArea.rect[i].top=htonl(pDevCfgPara->alarmPara[chnl-1].maskAlarmArea.rect[i].top);
	}
	alarmcfg.maskAlarmArea.nAreaNum=htonl(pDevCfgPara->alarmPara[chnl-1].maskAlarmArea.nAreaNum);
	for(i=0;i<pDevCfgPara->alarmPara[chnl-1].moveDetectArea.detectNum;i++)
	{
		alarmcfg.moveDetectArea.moveRect[i].bottom = htonl(pDevCfgPara->alarmPara[chnl-1].moveDetectArea.moveRect[i].bottom);
		alarmcfg.moveDetectArea.moveRect[i].left= htonl(pDevCfgPara->alarmPara[chnl-1].moveDetectArea.moveRect[i].left);
		alarmcfg.moveDetectArea.moveRect[i].right= htonl(pDevCfgPara->alarmPara[chnl-1].moveDetectArea.moveRect[i].right);
		alarmcfg.moveDetectArea.moveRect[i].top= htonl(pDevCfgPara->alarmPara[chnl-1].moveDetectArea.moveRect[i].top);
		
	}
	alarmcfg.moveDetectArea.detectNum=htonl(pDevCfgPara->alarmPara[chnl-1].moveDetectArea.detectNum);
	tcpSend( netfd, &alarmcfg, sizeof (alarmcfg) );
	close ( netfd );
	return OK;
	#endif
}


/*******************************************************************************************
Function:           AlarmCtrl()
Input:              NULL
Output:             NULL
Return:           
Calls:              GetAlarmIn()
                    GetVideoMask()
                    GetVideoSignal()
                    BeepAlarm()
Called by           main()
Description:        deal with the device's alarm 
*******************************************************************************************/
void AlarnInCtrl(void)
{
    UINT32              sensorType;
    //UINT32              ADD_ALARM_CHANS[MAX_ALARM_TYPE];/*新增的某类型报警的通道*/
   // UINT32              DEL_ALARM_CHANS[MAX_ALARM_TYPE];/*需要删除的报警，有报警-》无报警*/
    unsigned int        alarmInStatus;
    unsigned int        alarmOutStatus;
    unsigned int        oldalarmOutStatus;
    unsigned int        TrigAlarmOut[alarmOutnum];
    UINT32              oldalarmStatus[MAX_ALARM_TYPE];
    //UINT32              chans;
    UINT32              CurrNeedBeep = 0;
    UINT32              NeedBeep=0;
    BOOL                bHaveSignal, bMask;
    int                 i,ret,chnl,alarmoutchnl;
    time_t             currTime;
	 
    
    MotdetStatus = 0;
    oldalarmOutStatus = 0;
    while(1)
    {
        sensorType = 0;
        alarmInStatus = 0;
        alarmOutStatus = 0;
        CurrNeedBeep = 0;
        currTime =time(NULL);
        memcpy((char *)&oldalarmStatus[0],(char *)&CurrAlarmStatus[0],sizeof(UINT32)*MAX_ALARM_TYPE);
        GetAlarmIn(&alarmInStatus);
        
        for(i=0;i<alarmInnum;i++)
        {
             sensorType |=((pDevCfgPara->AlarmInCfg[i].Sensortype&0x1)<<i);
        }
        for(i=0;i<MAX_ALARM_TYPE;i++)
        {
             CurrAlarmStatus[i] = 0;
        }
		
        CurrAlarmStatus[MOTDET_ALARM] = MotdetStatus;
        MotdetStatus = 0;
		
        for(i=0;i<alarmOutnum;i++)
        {
             TrigAlarmOut[i]=0;
        }
        CurrAlarmStatus[ALARMIN_ALARM] = sensorType&alarmInStatus;
        for(i=0;i<channelnum;i++)
        {
            /*检测是否有遮挡*/
             if(pDevCfgPara->MaskCfg[i].bMaskHandle)
             {
                 ret = GetVideoMask(i+1, (int *)&bMask);
                 if ( ret == ERROR )
                 {
                      printf("GetVideoMask() fail\n");
                    //return ERROR;
                 }
		/*发生遮挡*/
                 if ( bMask)
                 {
                    CurrAlarmStatus[MASK_ALARM] |=(0x1<<i);
                 }
             }
            
             /*进行视频丢失处理*/
             if(pDevCfgPara->NoSignalCfg[i].bNoSigHandle)
             {
                ret = GetVideoSignal(i+1,( int *)&bHaveSignal);
                if(!bHaveSignal)
                {
                    printf("no signal\n");
                    CurrAlarmStatus[NOSIGNAL_ALARM] |=(0x1<<i);
                }
             }
            
        }
	
        /*开始处理报警i=1是NOSIGNAL-ALARM，2 ALARMIN-ALARM，3 MASK-ALARM ,4 MOTDET-ALARM*/
        for(i=1;i<MAX_ALARM_TYPE;i++)
        {                 

             for(chnl=0;chnl<channelnum;chnl++)
              { 
                 if(CurrAlarmStatus[i]&(1<<chnl))
                 {
                      switch(i)
                      {
                           case NOSIGNAL_ALARM:
                                 if(pDevCfgPara->NoSignalCfg[chnl].NoSigHandleType&SOUND_WARN)
                                 {
                                      CurrNeedBeep |=(1<<chnl);
                                 }
                                 if(pDevCfgPara->NoSignalCfg[chnl].NoSigHandleType&TRIGGER_ALARMOUT)
                                 {
                                      for(alarmoutchnl=0;alarmoutchnl<alarmOutnum;alarmoutchnl++)
                                      {
                                           if(pDevCfgPara->NoSignalCfg[chnl].NoSigAlarmOutTrig&(1<<alarmoutchnl))
                                           {
                                                TrigAlarmOut[alarmoutchnl] |=(1<<chnl);
                                           }
                                      }
                                 }
                                 break;
                           case ALARMIN_ALARM:
                                 if(chnl>alarmInnum-1)
                                 {
                                      break;
                                 }
                                 if(pDevCfgPara->AlarmInCfg[chnl].AlarmInHandleType&SOUND_WARN)
                                 {
                                      CurrNeedBeep |=(1<<chnl);
                                 }
                                 if(pDevCfgPara->AlarmInCfg[chnl].AlarmInHandleType&TRIGGER_ALARMOUT)
                                 {
                                      for(alarmoutchnl=0;alarmoutchnl<alarmOutnum;alarmoutchnl++)
                                      {
                                           if(pDevCfgPara->AlarmInCfg[chnl].AlarmInAlarmOutTrig&(1<<alarmoutchnl))
                                           {
                                                TrigAlarmOut[alarmoutchnl] |=(1<<chnl);
                                           }
                                      }
                                 }
                                 break;
                            case MASK_ALARM:
                                 if(pDevCfgPara->MaskCfg[chnl].MaskHandleType&SOUND_WARN)
                                 {
                                      CurrNeedBeep |=(1<<chnl);
                                 }
                                 if(pDevCfgPara->MaskCfg[chnl].MaskHandleType&TRIGGER_ALARMOUT)
                                 {
                                      for(alarmoutchnl=0;alarmoutchnl<alarmOutnum;alarmoutchnl++)
                                      {
                                           if(pDevCfgPara->MaskCfg[chnl].MaskAlarmOutTrig&(1<<alarmoutchnl))
                                           {
                                                TrigAlarmOut[alarmoutchnl] |=(1<<chnl);
                                           }
                                      }
                                 }
                                 break;
                            case MOTDET_ALARM:
				     printf("handleType=%d\n",pDevCfgPara->MotdetCfg[chnl].MotHandleType);
                                 if(pDevCfgPara->MotdetCfg[chnl].MotHandleType&SOUND_WARN)
                                 {
                                      CurrNeedBeep |=(1<<chnl);
                                 }
                                 if(pDevCfgPara->MotdetCfg[chnl].MotHandleType&TRIGGER_ALARMOUT)
                                 {
                                      for(alarmoutchnl=0;alarmoutchnl<alarmOutnum;alarmoutchnl++)
                                      {
                                           if(pDevCfgPara->MotdetCfg[chnl].MotAlarmOutTrig&(1<<alarmoutchnl))
                                           {
                                                TrigAlarmOut[alarmoutchnl] |=(1<<chnl);
                                           }
                                      }
                                 }
                                 if(pDevCfgPara->MotdetCfg[chnl].MotHandleType&TRIGGER_RECORD)
                                 {
                                       printf("Trigger_record\n");
                                       bMotRecordStart[chnl] = TRUE;
                                       MotRecordStartTime[chnl] = currTime;
                                 }
                                 //else
                                 //{
                                 //      bMotRecordStart[chnl] = FALSE;
                                // }
                                 break;
                            default:
                                 break;
                       }
                  }
             }
        }/*end of for*/
	NeedBeep = CurrNeedBeep;// 
	if(NeedBeep)
	{
		BeepAlarm();
	       printf("NeedBeep=%d\n",NeedBeep);
	}
	for(i=0;i<alarmOutnum;i++)
	{
             // printf("alarmout %d=%d\n",i,TrigAlarmOut[i]);
              if(TrigAlarmOut[i])
              {
                   alarmOutStatus |=(1<<i);
              }
	}
	//printf("alarmoutstatus=%d\noldalarmoutstatus=%d\n",alarmOutStatus,oldalarmOutStatus);
	if(alarmOutStatus !=oldalarmOutStatus)
	{
	     SetAlarmOut(alarmOutStatus);
	     oldalarmOutStatus = alarmOutStatus;
	}
	usleep(1000);
    }/*end of while(1)*/
}/*end*/



