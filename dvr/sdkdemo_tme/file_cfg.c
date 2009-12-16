/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         File_cfg.c
Author:           fangjiancai
Version :         1.0
Date:             07-12-19
Description:   read the config file ,if error and set the default configfile and write it for save
Version:    	// 版本信息
Function List:	readDevCfg(),writeDevCfg(),setDefaultDevCfg()
History:      	
    <author>       <time>   <version >   <desc>
    fangjiancai   07-12-19   1.0        move from dvs_demo
***********************************************************************************************/

#include "include_all.h"


/********************************************************************************
Function:        readDevCfg()
Description:    read the /davinci/sdkCfg.bin to struct config
Input:            the struct of config
Output:          无
Return:          ERROR for error,OK for right
********************************************************************************/
int readDevCfg ( DEVCFG *pDevCfg )
{
	int fd;
	int fileSize;
	printf ( "read sdk configuration.\n" );
	if ( pDevCfg == NULL ) 
	{
		return ERROR;
	}
	
	if((fd = open(CFG_FILENAME, O_RDWR, 0)) == ERROR)
	{
		printf ( "read sdk cfg, open error.\n" );
		return ERROR;
	}
	
	memset((char *)pDevCfg, 0, sizeof(DEVCFG));
	fileSize = lseek(fd, 0, SEEK_END);
	if(fileSize>sizeof(DEVCFG))
	{
		close(fd);
		remove(CFG_FILENAME);
		printf ( "read sdk cfg, file size error.\n" );
		return ERROR;
	}

	lseek(fd, 0, SEEK_SET);
	if(read(fd, (char *)pDevCfg, fileSize) != fileSize)
	{
		close(fd);
		printf ( "read sdk cfg, read error.\n" );
		remove(CFG_FILENAME);
		memset((char *)pDevCfg, 0, sizeof(DEVCFG));
		return ERROR;
	}
	close ( fd );
	printf ( "read the configuration file successful.\n" );
	return OK;
}


/********************************************************************************
Function:        writeDevCfg()
Description:    write the config to /davinci/sdkCfg.bin
Input:             the struct of config
Output:          无
Return:          ERROR for error ,OK for right
********************************************************************************/
int writeDevCfg ( DEVCFG *pDevCfg )
{
	int fd;
	int len = sizeof ( DEVCFG );

	semMTake ( &globalMSem );
	printf ( "write sdk configuration.\n" );
	
	if((fd = open(CFG_FILENAME, O_RDWR|O_CREAT, 0)) == ERROR)
	{
	       printf("open /davinci/sdkcfg.bin error\n");
		semMGive( &globalMSem );
		return ERROR;
	}
	if(write(fd, (char *)pDevCfg, len) != len)
	{
	       printf("write to sdkcfg.bin error\n");
		close(fd);
		semMGive( &globalMSem );
		return ERROR;
	}
	close(fd);
	semMGive ( &globalMSem );
	printf ( "write sdk configuration successful.\n" );
	return OK;
}

/********************************************************************************
Function:        setDefaultDevCfg()
Description:    when read the config file error or was the first excuse ,will set default config
Input:            the struct of config
Output:          无
Return:          ERROR for error,OK for right
********************************************************************************/
int setDefaultDevCfg ( DEVCFG *pDevCfg )
{
	int i,chnl;
	//struct RECT	zeroRect = { 0,0,0,0 }, allRect = {0, 0,256,256};

	printf ( "##################set sdk default configuration.##############\n" );
	pDevCfg->nDevType = 0;
	//videoout
	#ifdef DVR_DEMO
	for(i=0;i<16;i++)
	{
		pDevCfg->VideoOut.PreviewOrder[i]=i;
	}
	pDevCfg->VideoOut.ScreenNum = channelnum;
	#endif
	//osd option, for SetOSD() setting
	pDevCfg->osdPara.dispWeek = TRUE;
	pDevCfg->osdPara.osdAttrib = OSD_TRANS_WINK;
	pDevCfg->osdPara.osdType = OSD_TYPE_ENGYYMMDD;
	pDevCfg->osdPara.upLeft.x = 0;
	pDevCfg->osdPara.upLeft.y = 0;//(2<<8) | 2
	//video color option, for SetVideoParam() setting
	pDevCfg->videoColor.brightness = 128;
	pDevCfg->videoColor.saturation = 128;
	pDevCfg->videoColor.hue = 120;
	pDevCfg->videoColor.contrast = 128;
	//stream type option, for SetStreamType() setting
	for ( chnl= 0;chnl<channelnum;chnl++ )
	{
		pDevCfg->streamType[chnl].mainStreamType = STREAM_TYPE_AVSYN;
		pDevCfg->streamType[chnl].subStreamType = STREAM_TYPE_AVSYN;

		//main stream compression setting.
		pDevCfg->compressPara[chnl].streamAttr = MODE_VBR;
		pDevCfg->compressPara[chnl].intervalFrameI = 50;
		pDevCfg->compressPara[chnl].BFrameNum = 2;
		pDevCfg->compressPara[chnl].videoFrameRate = 25;//全帧率
		pDevCfg->compressPara[chnl].maxVideoBitrate = 2048*1024;
		pDevCfg->compressPara[chnl].quality = 10;
		pDevCfg->compressPara[chnl].picFormat = ENC_CIF;

		//sub stream compression setting.
		pDevCfg->compressParaSub[chnl].streamAttr = MODE_VBR;
		pDevCfg->compressParaSub[chnl].intervalFrameI = 100;
		pDevCfg->compressParaSub[chnl].BFrameNum = 2;
		pDevCfg->compressParaSub[chnl].videoFrameRate = 20;//全帧率
		pDevCfg->compressParaSub[chnl].maxVideoBitrate = 512*1024;
		pDevCfg->compressParaSub[chnl].quality = 10;
		pDevCfg->compressParaSub[chnl].picFormat = ENC_QCIF;

		pDevCfg->bChnlStart[chnl] = TRUE;
		pDevCfg->bSubChnlStart[chnl] = TRUE;

		//set default hidearea config
		pDevCfg->HideArea[chnl].nAreaNum=0;
		for(i=0;i<MAX_MASK_REGION;i++)
		{
			pDevCfg->HideArea[chnl].rect[i].bottom=0;
			pDevCfg->HideArea[chnl].rect[i].left=0;
			pDevCfg->HideArea[chnl].rect[i].right=0;
			pDevCfg->HideArea[chnl].rect[i].top=0;
		}

		//setting default mask alarm config
		pDevCfg->MaskCfg[chnl].bMaskHandle = 0;
		pDevCfg->MaskCfg[chnl].MaskAlarmOutTrig = 0;
		pDevCfg->MaskCfg[chnl].MaskHandleType = 0;

		//setting default detect config
		pDevCfg->MotdetCfg[chnl].bMotHandle = 0;
		pDevCfg->MotdetCfg[chnl].MotAlarmOutTrig = 0;
		pDevCfg->MotdetCfg[chnl].MotHandleType = 0;
		pDevCfg->MotdetCfg[chnl].MoveDetect.delay = 0;
		pDevCfg->MotdetCfg[chnl].MoveDetect.fastInterval = 3;
		pDevCfg->MotdetCfg[chnl].MoveDetect.detectNum = 0;
		pDevCfg->MotdetCfg[chnl].MoveDetect.level = 1;
		pDevCfg->MotdetCfg[chnl].MoveDetect.slowInterval = 12;
		for(i=0;i<pDevCfg->MotdetCfg[chnl].MoveDetect.detectNum;i++)
		{
			pDevCfg->MotdetCfg[chnl].MoveDetect.moveRect[i].bottom = 0;
			pDevCfg->MotdetCfg[chnl].MoveDetect.moveRect[i].top = 0;
			pDevCfg->MotdetCfg[chnl].MoveDetect.moveRect[i].left = 0;
			pDevCfg->MotdetCfg[chnl].MoveDetect.moveRect[i].right=0;
		}

		//setting default nosignal config
		pDevCfg->NoSignalCfg[chnl].bNoSigHandle = 0;
		pDevCfg->NoSignalCfg[chnl].NoSigAlarmOutTrig = 0;
		pDevCfg->NoSignalCfg[chnl].NoSigHandleType = 0;

	}

	for(i=0;i<4;i++)
	{
		pDevCfg->AlarmInCfg[i].Sensortype = 0;
		pDevCfg->AlarmInCfg[i].bAlarmInHandle = 0;
		pDevCfg->AlarmInCfg[i].AlarmInHandleType = 0;
		pDevCfg->AlarmInCfg[i].AlarmInAlarmOutTrig = 0;
	}

	pDevCfg->netPara.alarmSendAddr=0;
	pDevCfg->netPara.alarmSendPort=0;

	return writeDevCfg ( pDevCfg );
}

/********************************************************************************
//Function:
//Input:
//Output:
//Return:
//Description:
********************************************************************************/
int getDevCfgParam ( DEVCFG *pDevCfg )
{
	if ( readDevCfg ( pDevCfg ) == ERROR )
	{
		if ( setDefaultDevCfg ( pDevCfg ) == ERROR ) return ERROR;
	}
	return OK;
}

/***********************************************************************************
Function:           copyFile()
Description:       拷贝文件(将src指定的文件拷贝到dst指定的位置
Input:                源文件位置-src，目标文件位置-dst
Output:              无
Return:              成功返回0，失败返回-1
***********************************************************************************/

int copyFile(char *src,char *dst)
{
    int fd_src,fd_dst;
    int filesize,nread;
    char buf[2048];
    filesize=0;
    nread=0;
    if(src==NULL||dst==NULL)
    {
         return ERROR;	
    }
    fd_src = open(src,O_RDONLY);
    fd_dst = open(dst,O_CREAT|O_WRONLY);
	
    if ( fd_src == -1 || fd_dst == -1 ) 
    {
         close(fd_src);
         close(fd_dst);
         return ERROR;
    }

    while((nread = read(fd_src,buf,2048))>0)
    {
         if(write(fd_dst,buf,nread)!=nread)
         {
              close(fd_src);
              close(fd_dst);
              return ERROR;
         }
         filesize +=nread;
    }
    if(nread<0)
    {
         close(fd_src);
         close(fd_dst);
         return ERROR;
    }
    close(fd_src);
    close(fd_dst);
    return OK;
}
