/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:          Video.c
Author:            fangjiancai
Version :          1.0
Date:   
Description:       some function for set /get video parameter and stream parameter
Version:           // 版本信息
Function List:     servideoPara(),netSetOsdPara(),netSetVideoColor(),netSetMaskArea(),netCancelMaskArea()
                   netGetStreamPara(),netSetStreamPara(),netGetVideoColor()
History:      	
    <author>      <time>   <version >   <desc>
    fangjiancai   07-12-20
***********************************************************************************************/


#include "include_all.h"

#define	 	_OSD_YEAR4		0x9000
#define 		_OSD_YEAR2		0x9001
#define		_OSD_MONTH3	0x9002
#define 		_OSD_MONTH2	0x9003
#define 		_OSD_DAY		0x9004
#define 		_OSD_WEEK3	0x9005
#define 		_OSD_CWEEK1	0x9006
#define 		_OSD_HOUR24	0x9007
#define 		_OSD_HOUR12	0x9008
#define		_OSD_MINUTE	0x9009
#define		_OSD_SECOND	0x900a


/********************************************************************************
Function:       serVideoPara()
Description:   when program start,this function will set the video parameter
Input:           无
Output:         无
Return:        return ERROR for error,OK for right
********************************************************************************/
int serVideoPara ()
{
	struct SYSTEMTIME osdTime;
	struct tm *now;
	time_t 	tmpTime;
	int ret, i;
	//struct RECT	tmpRect[MAX_MOVERECT_NUM];

printf ( "brig=%d, con=%d, sat=%d, hue=%d.\n",  
	pDevCfgPara->videoColor.brightness,
	pDevCfgPara->videoColor.contrast,
	pDevCfgPara->videoColor.saturation,
	pDevCfgPara->videoColor.hue);
for(i=0;i<channelnum;i++)
{
	ret = SetVideoParam(i+1,
		                           pDevCfgPara->videoColor.brightness,
		                           pDevCfgPara->videoColor.contrast,
		                           pDevCfgPara->videoColor.saturation,
		                           pDevCfgPara->videoColor.hue
		                           );	//api:set video color parameter

	if ( ( pDevCfgPara->streamType[i].mainStreamType > 3 )
		|| ( pDevCfgPara->streamType[i].subStreamType < 1 ) )
	{
		return ERROR;
	}
	
       printf ( "####set video param, stream type = %d.\n",  pDevCfgPara->streamType[i].mainStreamType);
	   
	ret = SetStreamType(i+1,MAIN_CHANNEL,pDevCfgPara->streamType[i].mainStreamType);	//api:set stream type
	if ( ret == ERROR )
	{
		printf ( "SetStreamType() error.\n" );
		return ERROR;
	}
	if(ret==0)printf("setstreamtype() successful!\n");

	ret = SetDefaultQuant(i+1,MAIN_CHANNEL,pDevCfgPara->compressPara[i].quality);	//api:set quant
	if ( ret == ERROR )
	{
		printf ( "SetDefaultQuant() error.\n" );
		return ERROR;
	}
	if(0==ret)printf("setdefaultquant() successful!\n");

	ret = SetIBPMode (i+1,
		MAIN_CHANNEL,pDevCfgPara->compressPara[i].intervalFrameI,
		pDevCfgPara->compressPara[i].BFrameNum,
		0,
		pDevCfgPara->compressPara[i].videoFrameRate);	//api:set compression option
	if ( ret == ERROR )
	{
		printf ( "SetIBPMode() error.\n" );
		return ERROR;
	}
	if(0==ret)
	{
		printf("setibpmode() successful!\n");
	}

	ret=SetBitrateControl(i+1,MAIN_CHANNEL, pDevCfgPara->compressPara[i].maxVideoBitrate,pDevCfgPara->compressPara[i].streamAttr);
	
	ret = SetEncoderPictureFormat(i+1,MAIN_CHANNEL,pDevCfgPara->compressPara[i].picFormat);	//api
	if ( ret == ERROR )
	{
		printf ( "SetEncoderPictureFormat() error.\n" );
		return ERROR;
	}


//sub stream setting.
	ret = SetStreamType(i+1,SUB_CHANNEL,pDevCfgPara->streamType[i].subStreamType);	//api:set stream type
	if ( ret == ERROR )
	{
		printf ( "Sub:SetStreamType() error.\n" );
		return ERROR;
	}

	ret = SetDefaultQuant(i+1,SUB_CHANNEL,pDevCfgPara->compressParaSub[i].quality);	//api:set quant
	if ( ret == ERROR )
	{
		printf ( "Sub:SetDefaultQuant() error.\n" );
		return ERROR;
	}

	ret = SetIBPMode (i+1,
		SUB_CHANNEL,pDevCfgPara->compressParaSub[i].intervalFrameI,
		pDevCfgPara->compressParaSub[i].BFrameNum,
		0,
		pDevCfgPara->compressParaSub[i].videoFrameRate);	//api:set compression option
	if ( ret == ERROR )
	{
		printf ( "Sub:SetIBPMode() error.\n" );
		return ERROR;
	}
	ret=SetBitrateControl(i+1,SUB_CHANNEL,
		                             pDevCfgPara->compressParaSub[i].maxVideoBitrate,
		                             pDevCfgPara->compressParaSub[i].streamAttr
		                             );

	ret = SetEncoderPictureFormat(i+1,
	                                                   SUB_CHANNEL,
	                                                   pDevCfgPara->compressParaSub[i].picFormat);	//api
	if ( ret == ERROR )
	{
		printf ( "Sub:SetEncoderPictureFormat() error.\n" );
		return ERROR;
	}
	if(pDevCfgPara->HideArea[i].nAreaNum>0)
	{
	       ret=SetupMask(i+1,
		   	                  pDevCfgPara->HideArea[i].rect, 
		   	                  pDevCfgPara->HideArea[i].nAreaNum
		   	                  );
	}
	if ( ret == ERROR )
	{
		printf ( "SetupMask() error.\n" );
		return ERROR;
	}
	
       SetMaskAlarm(i+1,
		  	         (int)(pDevCfgPara->MaskCfg[i].bMaskHandle),
		  	               &( pDevCfgPara->MaskCfg[i].MaskRect)
		  	          );
	
       if(pDevCfgPara->MotdetCfg[i].MoveDetect.detectNum>0)
       {
	       SetMotionDetection(i+1,
					 	    pDevCfgPara->MotdetCfg[i].MoveDetect.level, 
					 	    pDevCfgPara->MotdetCfg[i].MoveDetect.fastInterval,
					 	    pDevCfgPara->MotdetCfg[i].MoveDetect.slowInterval,
					 	    pDevCfgPara->MotdetCfg[i].MoveDetect.moveRect, 
					 	    pDevCfgPara->MotdetCfg[i].MoveDetect.detectNum
					 	     );
	         EnalbeMotionDetection(i+1,(int)(pDevCfgPara->MotdetCfg[i].bMotHandle));
	}
   }

	time ( &tmpTime );
	now = gmtime ( &tmpTime );
	osdTime.year = now->tm_year+1900;
	osdTime.month = now->tm_mon+1;
	osdTime.day = now->tm_mday;
	osdTime.dayofweek = now->tm_wday;
	osdTime.hour = now->tm_hour;
	osdTime.minute = now->tm_min;
	osdTime.second = now->tm_sec;
	osdTime.milliseconds = 0;

	enum video_standard std;
	int bright, contrast, saturation, hue;
	int top;
	for(i=0;i<channelnum;i++)
	{
	      GetVideoParam ( i+1, &std, &bright, &contrast, &saturation, &hue );
	      if ( std == STANDARD_NTSC )
	      	{
		      top = 414;
	      	}
	      else if ( std == STANDARD_PAL )
	      	{
		      top = 512;
	      	}
	      printf ( "###########std=%d.\n", std );
	      //unsigned short format1[] = { 16,/*top*/16,'D','A','V','I','N','C','I','_','S','D','K','_','D','E','M','O',0};
	      //unsigned short format2[] = { 16,48,_OSD_YEAR4,0x2d, _OSD_MONTH3,0x2d,_OSD_DAY,0x20, 
		///*这两个是汉字|星期|0xd0c7,0xc6da,*/_OSD_WEEK3,0x20,_OSD_HOUR24,0x3a,_OSD_MINUTE,0x3a,_OSD_SECOND,0x20,0};
	      unsigned short* p[3];
	      p[0] =DeviceName;
	      p[1] = DateTime;
             p[2] =ChnlName[i];
	      ret=SetOSDDisplayMode(i+1,256,0,0x1,3,p);
	      if(ret==0)
	      {
	          printf("SetOSDDisplayMode() successful\n");
	      }
	       else
	       {
	          printf("SetOSDDisplayMode() fail\n");
	       }
		EnableOSD(i+1, 1);
	}

	printf( "#################video parameter set successful.#########\n" );
	return OK;
}


/********************************************************************************
Function:    netSetOsdPara()
Input:       socketfd,the address of command's buffer,the clieant's IP address
Output:       
Return:      return ERROR for error,OK for right
Description: set the osd parameter from network
********************************************************************************/
int netSetOsdPara ( int netfd, char *clientCmd, int clientAddr )
{
	OSDCHARPARA osdCharPara;
	unsigned short *format;
	int ret;
	NETCMD_HEADER	netCmdHeader;

	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != ( sizeof ( NETCMD_HEADER) + sizeof ( OSDCHARPARA ) ) )
		{close(netfd);
		return ERROR;
		}
	memcpy ( &osdCharPara, clientCmd+sizeof ( NETCMD_HEADER ), sizeof ( osdCharPara ) );

	format = osdCharPara.osdLine[0];

	ret = SetOSDDisplayMode ( 1,
		osdCharPara.brightness,
		osdCharPara.translucent,
		osdCharPara.otherPara,
		osdCharPara.osdLineNum,
		&format );	//api:set osd display mode

	if ( ret == ERROR )
	{
		printf ( "SetOSDDisplayMode() error.\n" );
		close(netfd);
		return ERROR;
	}
	close(netfd);
	return OK;
}


/********************************************************************************
//Function:        netSetVideoColor()
//Description:   Client set video color parameter from network
//Input:            套接字-netfd，命令缓冲区指针-clientcmd，客户地址-clientaddr
//Output:          无
//Return:         return ERROR for error,OK for right

********************************************************************************/
int netSetVideoColor ( int netfd, char *clientCmd, int clientAddr )
{
	struct{
		int		channel;
		VIDEO_COLOR	videoColor;
	}colorPara;

	NETCMD_HEADER	netCmdHeader;
	int ret;

	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != ( sizeof ( NETCMD_HEADER) + sizeof ( colorPara ) ) )
	{
		close ( netfd );
		return ERROR;
	}
	printf ( "netSetVideoColor(), length is right!\n" );

	memcpy ( &colorPara, &clientCmd[sizeof ( NETCMD_HEADER )], sizeof ( colorPara ) );
	printf ( "channel=%d, bright=%d, contra=%d, satura=%d, hue=%d.\n",  
		ntohl(colorPara.channel),
		colorPara.videoColor.brightness,
		colorPara.videoColor.contrast,
		colorPara.videoColor.saturation,
		colorPara.videoColor.hue);
	ret = SetVideoParam ( ntohl(colorPara.channel), colorPara.videoColor.brightness,
		colorPara.videoColor.contrast, colorPara.videoColor.saturation, colorPara.videoColor.hue );
	printf ( "####SetVideoParam successfully, ret = %d.\n", ret );
	close ( netfd );
	return OK;
}


/********************************************************************************
//Function:        netGetVideoColor()
//Description:    Client get video color parameter 
//Input:            套接口描述字-netfd，命令缓冲区-*clientcmd，客户ip-clientAddr
//Output:          无
//Return:         ERROR for error,OK for right
********************************************************************************/
int netGetVideoColor ( int netfd, char *clientCmd, int clientAddr )
{
	VIDEO_COLOR		videoColor;

	videoColor.brightness = pDevCfgPara->videoColor.brightness;
	videoColor.contrast = pDevCfgPara->videoColor.contrast;
	videoColor.hue = pDevCfgPara->videoColor.hue;
	videoColor.saturation = pDevCfgPara->videoColor.saturation;
	printf ( "bright=%d, contr=%d, hue=%d, satur=%d.\n", 
		videoColor.brightness,
		videoColor.contrast,
		videoColor.hue,
		videoColor.saturation);
	tcpSend( netfd, &videoColor, sizeof ( videoColor ) );
	close ( netfd );
	return OK;
}


/********************************************************************************
//Function:        netSetStreamParam()
//Description:    Client set main and sub stream parameter.
//Input:            套接口描述字-netfd，命令缓冲区-*clientcmd，客户ip-clientAddr
//Output:          无
//Return:          ERROR for error,OK for right

********************************************************************************/
int netSetStreamPara ( int netfd, char *clientCmd, int clientAddr )
{
	struct{
		int					chnl;
		STREAMPARA	mainStreamPara;
		STREAMPARA	subStreamPara;
	}streamPara;

//	STREAMPARA	streamPara;
	NETCMD_HEADER	netCmdHeader;
	int ret = ERROR;
	//int type;

	close ( netfd );
//	StopCodec (1, MAIN_CHANNEL);
//	StopCodec(1, SUB_CHANNEL);
	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != ( sizeof ( NETCMD_HEADER) + sizeof ( streamPara ) ) )
	{
		close ( netfd );
		return ERROR;
	}

//GetStreamType (1, &type, MAIN_CHANNEL);
	//printf ( "stream type = %d.\n",  type);
	printf ( "###stream para length is right.\n" );
	memcpy ( &streamPara, &clientCmd[sizeof ( NETCMD_HEADER )], sizeof ( streamPara ) );

	streamPara.chnl = ntohl( streamPara.chnl );

	streamPara.mainStreamPara.intervalFrameI = ntohl(streamPara.mainStreamPara.intervalFrameI);
	streamPara.mainStreamPara.numFrameB = ntohl(streamPara.mainStreamPara.numFrameB);
	streamPara.mainStreamPara.numFrameP = ntohl(streamPara.mainStreamPara.numFrameP);
	streamPara.mainStreamPara.frameRate = ntohl(streamPara.mainStreamPara.frameRate);
	streamPara.mainStreamPara.maxBpsLimit = ntohl(streamPara.mainStreamPara.maxBpsLimit);
	streamPara.mainStreamPara.streamAttrib = ntohl(streamPara.mainStreamPara.streamAttrib);
	streamPara.mainStreamPara.format = ntohl(streamPara.mainStreamPara.format);
	streamPara.mainStreamPara.streamType = ntohl(streamPara.mainStreamPara.streamType);

	streamPara.subStreamPara.intervalFrameI = ntohl(streamPara.subStreamPara.intervalFrameI);
	streamPara.subStreamPara.numFrameB = ntohl(streamPara.subStreamPara.numFrameB);
	streamPara.subStreamPara.numFrameP = ntohl(streamPara.subStreamPara.numFrameP);
	streamPara.subStreamPara.frameRate = ntohl(streamPara.subStreamPara.frameRate);
	streamPara.subStreamPara.maxBpsLimit = ntohl(streamPara.subStreamPara.maxBpsLimit);
	streamPara.subStreamPara.streamAttrib = ntohl(streamPara.subStreamPara.streamAttrib);
	streamPara.subStreamPara.format = ntohl(streamPara.subStreamPara.format);
	streamPara.subStreamPara.streamType = ntohl(streamPara.subStreamPara.streamType);

	printf ( "channel=%d, IFrame=%d, format=%d, rate=%d, maxBps=%d, Bfram=%d, Pframe=%d, Attrib=%d.\n", 
		streamPara.chnl,
		streamPara.mainStreamPara.intervalFrameI,
		streamPara.mainStreamPara.format,
		streamPara.mainStreamPara.frameRate,
		streamPara.mainStreamPara.maxBpsLimit,
		streamPara.mainStreamPara.numFrameB,
		streamPara.mainStreamPara.numFrameP,
		streamPara.mainStreamPara.streamAttrib);

	printf ( "Sub:channel=%d, IFrame=%d, format=%d, rate=%d, maxBps=%d, Bfram=%d, Pframe=%d, Attrib=%d.\n", 
		streamPara.chnl,
		streamPara.subStreamPara.intervalFrameI,
		streamPara.subStreamPara.format,
		streamPara.subStreamPara.frameRate,
		streamPara.subStreamPara.maxBpsLimit,
		streamPara.subStreamPara.numFrameB,
		streamPara.subStreamPara.numFrameP,
		streamPara.subStreamPara.streamAttrib);

       StopCodec(streamPara.chnl, MAIN_CHANNEL);
	ret = SetStreamType(streamPara.chnl,
		MAIN_CHANNEL,
		streamPara.mainStreamPara.streamType);	//api:set stream type
	if ( ret == ERROR )
	{
		printf ( "SetStreamType() error.\n" );
		return ERROR;
	}
       StartCodec(streamPara.chnl, MAIN_CHANNEL);
	ret = SetIBPMode ( streamPara.chnl, MAIN_CHANNEL,
		streamPara.mainStreamPara.intervalFrameI,
		streamPara.mainStreamPara.numFrameB,
		0,
		streamPara.mainStreamPara.frameRate);
	if ( ret == ERROR )
	{
		printf ( "#####netSetStreamPara(), SetIBPMode() error.\n" );
		return ret;
	}
	ret=SetBitrateControl(streamPara.chnl,MAIN_CHANNEL, streamPara.mainStreamPara.maxBpsLimit,streamPara.mainStreamPara.streamAttrib);

	if ( streamPara.mainStreamPara.format < 0 || streamPara.mainStreamPara.format > 4 )
	{
		printf ( "#####netSetStreamPara(), picture format error.\n" );
		return ERROR;
	}
	ret = SetEncoderPictureFormat ( streamPara.chnl, MAIN_CHANNEL, streamPara.mainStreamPara.format );
	if ( ret == ERROR )
	{
		printf ( "#####netSetStreamPara(), SetEncoderPictureFormat() error.\n" );
		return ret;
	}

	//set sub stream parameter.

       StopCodec(streamPara.chnl, SUB_CHANNEL);
	ret = SetStreamType(streamPara.chnl,
		SUB_CHANNEL,
		streamPara.subStreamPara.streamType);	//api:set stream type
	if ( ret == ERROR )
	{
		printf ( "SetStreamType() error.\n" );
		return ERROR;
	}
      StartCodec(streamPara.chnl, SUB_CHANNEL);
	ret = SetIBPMode ( streamPara.chnl, SUB_CHANNEL, 
		streamPara.subStreamPara.intervalFrameI,
		streamPara.subStreamPara.numFrameB, 
		0,
		streamPara.subStreamPara.frameRate);
	if ( ret == ERROR )
	{
		printf ( "#####netSetStreamPara(), SetIBPMode() error.\n" );
		return ret;
	}
	ret=SetBitrateControl(streamPara.chnl,SUB_CHANNEL,streamPara.subStreamPara.maxBpsLimit,streamPara.subStreamPara.streamAttrib);

	if ( streamPara.subStreamPara.format < 0 || streamPara.subStreamPara.format > 4 )
	{
		printf ( "#####netSetStreamPara(), picture format error.\n" );
		return ERROR;
	}
	ret = SetEncoderPictureFormat ( streamPara.chnl, SUB_CHANNEL , streamPara.subStreamPara.format);
	if ( ret == ERROR )
	{
		printf ( "#####netSetStreamPara(), SetEncoderPictureFormat() error.\n" );
		return ret;
	}

	pDevCfgPara->compressPara[streamPara.chnl-1].BFrameNum = streamPara.mainStreamPara.numFrameB;
	pDevCfgPara->compressPara[streamPara.chnl-1].intervalFrameI = streamPara.mainStreamPara.intervalFrameI;
	pDevCfgPara->compressPara[streamPara.chnl-1].maxVideoBitrate = streamPara.mainStreamPara.maxBpsLimit;
	pDevCfgPara->compressPara[streamPara.chnl-1].picFormat = streamPara.mainStreamPara.format;
	pDevCfgPara->compressPara[streamPara.chnl-1].streamAttr = streamPara.mainStreamPara.streamAttrib;
	pDevCfgPara->compressPara[streamPara.chnl-1].videoFrameRate = streamPara.mainStreamPara.frameRate;

	pDevCfgPara->compressParaSub[streamPara.chnl-1].BFrameNum = streamPara.subStreamPara.numFrameB;
	pDevCfgPara->compressParaSub[streamPara.chnl-1].intervalFrameI = streamPara.subStreamPara.intervalFrameI;
	pDevCfgPara->compressParaSub[streamPara.chnl-1].maxVideoBitrate = streamPara.subStreamPara.maxBpsLimit;
	pDevCfgPara->compressParaSub[streamPara.chnl-1].picFormat = streamPara.subStreamPara.format;
	pDevCfgPara->compressParaSub[streamPara.chnl-1].streamAttr = streamPara.subStreamPara.streamAttrib;
	pDevCfgPara->compressParaSub[streamPara.chnl-1].videoFrameRate = streamPara.subStreamPara.frameRate;

	pDevCfgPara->streamType[streamPara.chnl-1].mainStreamType = streamPara.mainStreamPara.streamType;
	pDevCfgPara->streamType[streamPara.chnl-1].subStreamType = streamPara.subStreamPara.streamType;

	if ( writeDevCfg (pDevCfgPara) == ERROR )
		return ERROR;

	printf ( "#########set stream parameter sucessfully!\n" );

	return OK;
}



/********************************************************************************
Function:         netGetStreamParam()
Description:     Client get main and sub stream parameter.
Input:             套接口描述字-netfd，命令缓冲区-*clientcmd，客户ip-clientAddr
Output:           无
Return:            ERROR for error,OK for right
********************************************************************************/
int netGetStreamPara ( int netfd, char *clientCmd, int clientAddr )
{ 
	struct{
		STREAMPARA	mainStreamPara;
		STREAMPARA	subStreamPara;
	}streamPara;
//	STREAMPARA	streamPara;
	NETCMD_HEADER	netCmdHeader;
	int channel;
      
	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	memcpy(&channel,clientCmd+sizeof(NETCMD_HEADER),sizeof(int));
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != (sizeof ( NETCMD_HEADER)+sizeof(int)) )
		return ERROR;
	//channel = *(clientCmd+sizeof(NETCMD_HEADER));
	channel = ntohl(channel)-1;
	streamPara.mainStreamPara.streamAttrib = htonl(pDevCfgPara->compressPara[channel].streamAttr);
	streamPara.mainStreamPara.intervalFrameI = htonl(pDevCfgPara->compressPara[channel].intervalFrameI);
	streamPara.mainStreamPara.numFrameB = htonl(pDevCfgPara->compressPara[channel].BFrameNum);
	streamPara.mainStreamPara.maxBpsLimit = htonl(pDevCfgPara->compressPara[channel].maxVideoBitrate);
	streamPara.mainStreamPara.frameRate = htonl(pDevCfgPara->compressPara[channel].videoFrameRate);
	streamPara.mainStreamPara.format = htonl(pDevCfgPara->compressPara[channel].picFormat);
	streamPara.mainStreamPara.numFrameP = htonl(3);
	streamPara.mainStreamPara.streamType = htonl(pDevCfgPara->streamType[channel].mainStreamType);

	streamPara.subStreamPara.streamAttrib = htonl(pDevCfgPara->compressParaSub[channel].streamAttr);
	streamPara.subStreamPara.intervalFrameI = htonl(pDevCfgPara->compressParaSub[channel].intervalFrameI);
	streamPara.subStreamPara.numFrameB = htonl(pDevCfgPara->compressParaSub[channel].BFrameNum);
	streamPara.subStreamPara.maxBpsLimit = htonl(pDevCfgPara->compressParaSub[channel].maxVideoBitrate);
	streamPara.subStreamPara.frameRate = htonl(pDevCfgPara->compressParaSub[channel].videoFrameRate);
	streamPara.subStreamPara.format = htonl(pDevCfgPara->compressParaSub[channel].picFormat);
	streamPara.subStreamPara.numFrameP = htonl(3);
	streamPara.subStreamPara.streamType = htonl(pDevCfgPara->streamType[channel].subStreamType);

	printf ( "channel = %d ,sub attrib = %d, main streamAttrib = %d.\n",
		channel+1,
		pDevCfgPara->compressParaSub[channel].streamAttr,
		pDevCfgPara->compressPara[channel].streamAttr);

	tcpSend (netfd, &streamPara, sizeof ( streamPara ));
	close ( netfd );
	return OK;
//	streamPara.mainStreamPara.numFrameP = pDevCfgPara.compressPara.
}


/********************************************************************************
Function:          netSetMaskArea()
Description:     Client set  mask.
Input:              套接口描述字-netfd，命令缓冲区-*clientcmd，客户ip-clientAddr
Output:            无
Return:            ERROR for error,OK for right

********************************************************************************/
int netSetMaskArea(int netfd,char *cliendCmd)
{
     struct RECT maskrect;
     int headlen,channel,ret;
     MASKCMD maskcmd;
     headlen= sizeof ( NETCMD_HEADER );

     memcpy(&maskcmd,cliendCmd+headlen,sizeof(maskcmd));
     channel=(int)maskcmd.channel;
     channel=ntohl(channel);
     maskrect=maskcmd.mastrect;
     maskrect.bottom=ntohl(maskrect.bottom);
     maskrect.left=ntohl(maskrect.left);
     maskrect.right=ntohl(maskrect.right);
     maskrect.top=ntohl(maskrect.top);
     if(pDevCfgPara->HideArea[channel-1].nAreaNum<MAX_MASK_REGION)
     {
	     pDevCfgPara->HideArea[channel-1].rect[pDevCfgPara->HideArea->nAreaNum]=maskrect;
	     pDevCfgPara->HideArea[channel-1].nAreaNum++;
      }
     else
     {
           printf("mask too much area\n");
           return OK;
     }
     printf("channel=%d,left=%d,right=%d,top=%d,bottom=%d\n",channel,maskrect.left,maskrect.right,maskrect.top,maskrect.bottom);
     ret=SetupMask(channel, &maskrect,pDevCfgPara->HideArea[channel-1].nAreaNum);
     if(0==ret)
	  printf("setupmask successful>>>>>>>>>>>>>>\n");
      close(netfd);
      writeDevCfg(pDevCfgPara);
     return OK;

}


/********************************************************************************
Function:        netCancelMaskArea()
Description:    Client Cancel mask
Input:            套接口描述字-netfd，命令缓冲区-*clientcmd
Output:          无
Return:          ERROR for error,OK for right

********************************************************************************/
int netCancelMaskArea(int netfd,char *cliendCmd)
{

    int channel,headlen,ret;
    headlen=sizeof(NETCMD_HEADER);

    memcpy(&channel,cliendCmd+headlen,sizeof(int));
    channel=ntohl(channel);
    if(pDevCfgPara->HideArea[channel-1].nAreaNum==0)
    {
         printf("no area masked in channel %d\n",channel);
    }
    else
    {
         pDevCfgPara->HideArea[channel-1].nAreaNum=0;
         memset((char *)(pDevCfgPara->HideArea[channel-1].rect),0,sizeof(struct RECT)*MAX_MASK_REGION);

         ret=StopMask(channel);
         if(0==ret)
         {
              printf("cancel mask area successful>>>>>>>>>>>>>>>>>>\n");
         }
         else
         {
              printf("cancel mask area fail\n");
         }
         writeDevCfg(pDevCfgPara);
    }			  
    close(netfd);
    return OK;

}

int netSetOSDDisplay( int netfd, char *clientCmd, int clientAddr )
{
    int channel,DispData,DispDevName,DispChnlName;
    int line=0,enable = 1,ret;
    unsigned short* format[3];
    struct
    {
          NETCMD_HEADER header;
          int                        bDispDate;
          int                        bDispDevName;
		  int                        bDispChnlName;
          int                        channel;
    }OSDDISPLAYCMD;

    memcpy(&OSDDISPLAYCMD,clientCmd,sizeof(OSDDISPLAYCMD));
    channel = ntohl(OSDDISPLAYCMD.channel);
    channel = channel -1;
    DispData = ntohl(OSDDISPLAYCMD.bDispDate);
    DispDevName = ntohl(OSDDISPLAYCMD.bDispDevName);
    DispChnlName = ntohl(OSDDISPLAYCMD.bDispChnlName);

    if(DispDevName)
    {
        format[0] = DeviceName;
	 if(DispData)
	 {
	     format[1] = DateTime;
	      if(DispChnlName)
	      	{
	      	    format[2] = ChnlName[channel];
                  line = 3;
	      	}
		else
		 {
		     format[2] = NULL;
                   line =2;
		 }
	 }
	 else if(DispChnlName)
	 {
	     format[1] = ChnlName[channel];
            format[2] =NULL;
	     line = 2;
	 }
	 else
	 {
	     format[1] = NULL;
            format[2] = NULL;
            line =1;
	 }
    }
    else if(DispData)
    {
         format[0] = DateTime;
         if(DispChnlName)
         {
              format[1] = ChnlName[channel];
              format[2] = NULL;
		line =2;
         }
         else
         {
              format[1] = NULL;
              format[2] = NULL;
              line =1;
         }
    }
    else if(DispChnlName)
    {
         format[0] = ChnlName[channel];
         format[1] = NULL;
         format[2] = NULL;
         line =1;
    }
    else
    {
         format[1] = NULL;
         format[2] = NULL;
         format[0] = NULL;
         line =0;
         enable = 0;
    }


     ret=SetOSDDisplayMode(channel+1,256,0,0x1,line,format);
     if(ret==0)
     {
          printf("SetOSDDisplayMode() successful\n");
     }
      else
      {
          printf("SetOSDDisplayMode() fail\n");
       }
	EnableOSD(channel+1, enable);

}
