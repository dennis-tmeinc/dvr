/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         playback.c
Author:           fangjiancai
Version :         1.0
Date:             07-12-20
Description:      处理客户端发送过来的回放命令和执行回放操作
Version:    	  1.0
Function List:	  netPlayBack(),PlayBack()
History:      	
    <author>      <time>   <version >   <desc>
    fangjiancai  08-01-08  1.0          
 ***********************************************************************************************/
#include "include_all.h"
int g_Speed = 4;

#ifdef DVR_DEMO
/********************************************************************************
//Function:       PlayBack_Write()
//Description:    进行回放，解码缓冲区中的数据
//Input:          无
//Output:         无
//Return:         无
********************************************************************************/
void PlayBack_Write()
{
    struct dec_statistics DecStatistics;
    while(1)
    { 
         if(!(bPlayBackStart[0]))
         {
              break;
         }
         if(PlayBackStruct.TotalWrite>=PlayBackStruct.TotalRead)/**/
         {
         //printf("waiting in write to dsp\n");
               usleep(1);
              continue;
         }
         /*将数据写入DSP进行解码*/  
        InputAvData(1,PlayBackStruct.PlayBackBuf[PlayBackStruct.WriteIdx].databuf,PlayBackStruct.PlayBackBuf[PlayBackStruct.WriteIdx].readlen);
	 PlayBackStruct.WriteIdx +=1;
	 PlayBackStruct.WriteIdx %=3;
        PlayBackStruct.TotalWrite +=1;
	GetDecodeStatistics(1,&DecStatistics);
	//printf("%d\n",DecStatistics.dec_sec);
	 //usleep(10);
    }
}

/********************************************************************************
//Function:       PlayBack()
//Description:    进行回放,从硬盘中读数据到缓冲区
//Input:          通道号
//Output:         无
//Return:         无
********************************************************************************/
void  PlayBack(int channel)
{
    int           ret;
    int           nSize,/*len, */fileFd,i;
    int           chnl=channel;
    //FILE        * fileFd1;
    const int     headLen = sizeof(HIKVISION_MEDIA_FILE_HEADER);
    char          fileName[32];
    HIKVISION_MEDIA_FILE_HEADER FileHead;
    sprintf(fileName,"/mnt/hda1/recfilechannel%d.mp4",chnl);
   //sprintf(fileName,"/davinci/Xzhangjing.mp4");
   //sprintf(fileName,"/davinci/record.mp4");
   //sprintf(fileName,"/mnt/hda1/Xzhangjing.mp4");
    while(1)
    {
         if(bPlayBackStart[chnl]) 
         { 
	      /*关掉各通道的预览*/
              for ( i = 0; i < pDevCfgPara->VideoOut.ScreenNum; i++ ) 
	      {
		   SetPreviewScreen(MAIN_OUTPUT,i+1,0);
	      }
	      /*如果但前不是在ATA Mode，则转化为ATA Mode*/
              if(!IsATAMode)
              {
                   EnableATA();
	           mount("/dev/hda1","/mnt/hda1","vfat",32768,NULL);
		   IsATAMode = TRUE;
	      }/**/
                  
	      // for decoder
	      ClrDisplayParams(MAIN_OUTPUT,0);
	      /*从新分配资源，主口，单画面*/
	      SetDisplayParams(MAIN_OUTPUT,1);
	      /*打开解码通道*/
	      SetDecodeScreen(MAIN_OUTPUT,1,1);
	      /*初始化回放缓冲区*/
              for(i=0;i<3;i++)
              {
                   PlayBackStruct.PlayBackBuf[i].readlen=0;
              }
              PlayBackStruct.ReadIdx=0;
              PlayBackStruct.WriteIdx = 0;
              PlayBackStruct.TotalRead = 0;
              PlayBackStruct.TotalWrite = 0;

	      fileFd=open(fileName,O_RDONLY);
	      /*打开文件错误，将从新回到预览状态*/
              if(fileFd==-1)
              {
                  printf("open fail,Maybe the file was delete\n");
                  goto NEXT;
              }
	      printf("fileFd=%d\n",fileFd);
	      /*先读出头部，开始解码*/
	      nSize = read( fileFd, &FileHead,headLen);
             ret=SetDecFileForStatistics(1,fileName);
			 printf("totalsec=%d\n",ret);
	      /*如果读出的头部的大小错误，从新返回到预览状态*/
	      if(nSize!=headLen)
	      {
                   printf("read head error!nSize=%d,headlen=%d\n",nSize,headLen);
		   goto NEXT;
	      }
	      ret=StartDecode(MAIN_OUTPUT,1,1,&FileHead);
              if(ret==0)
              {
                   printf("StartDecode fail, ret=%d\n",ret);
              }
	      /*设置回放速度*/    
	      ret= SetDecodeSpeed(1,g_Speed);
	      if(ret==0)
	      {
		   printf("set speed OK\n");
	      }
	      /*关闭预览声音*/
	      ret=SetPreviewAudio(0,1,0);
              if(0==ret)
              {
                   printf("close preview audio successful\n");
              }
	      /*打开音频解码*/
	      SetDecodeAudio(MAIN_OUTPUT,0,1);
	      /*设置音频解码音量*/
	      SetDecodeVolume(1,10);
	      /*开启一个向DSP中写数据的任务*/
              startUpTask(180,8192,(ENTRYPOINT)PlayBack_Write,0,0,0,0,0);
	      while(1)
	      {
                   /*命令退出回放*/
		   if(!bPlayBackStart[chnl])
		   {
		        break;
		   }
		   /*缓冲区已经满*/
		   if((PlayBackStruct.TotalRead-PlayBackStruct.TotalWrite)>2)
		   {//printf("was waiting\n");
		        usleep(1);
                        continue;
		   }
                   PlayBackStruct.PlayBackBuf[PlayBackStruct.ReadIdx].readlen=read(fileFd,PlayBackStruct.PlayBackBuf[PlayBackStruct.ReadIdx].databuf,8*1024);
		   /*退出回放,读错误或回放文件读完*/
		   if(!(PlayBackStruct.PlayBackBuf[PlayBackStruct.ReadIdx].readlen>0))
		   {
		        break;
		   }
			   
                   PlayBackStruct.ReadIdx +=1;
                   PlayBackStruct.ReadIdx %=3;
                   PlayBackStruct.TotalRead +=1;
                   //usleep(1);
	      }
              close(fileFd);
         NEXT:
	      bPlayBackStart[chnl]=FALSE;
	      StopDecode(1);
              /*切换到预览*/
	      //SetOutputFormat(MAIN_OUTPUT,VIDEOS_PAL);
              ClrDisplayParams(MAIN_OUTPUT,0);
	      // for preview
	      // demo 4 screen preview
	      SetDisplayParams(MAIN_OUTPUT,pDevCfgPara->VideoOut.ScreenNum);

	      for ( i = 0; i < pDevCfgPara->VideoOut.ScreenNum; i++ ) 
	      {
	           if(pDevCfgPara->VideoOut.PreviewOrder[i]!=0xff)
	           {
		        SetPreviewScreen(MAIN_OUTPUT,pDevCfgPara->VideoOut.PreviewOrder[i]+1,1);
	           }
	      }
	      /*打开预览声音*/
              SetPreviewAudio(0,1,1);
		      
         }	
	 usleep(1);
		
    }
    
}


 /********************************************************************************
//Function:             netPlayBack()
//Description:          解析客户端的回放命令
//Input:                套接口描述字-netfd
                        命令缓冲区指针-cmd
                        客户端地址-clientAddr
//Output:               无 
//Return:               无
********************************************************************************/ 
int netPlayBack(int netfd,char * cmd,int clientAddr)
{
    struct
    {
         NETCMD_HEADER header;
         int                        bStartPlayBack;/*0:stop play,1:start to play*/
         int                        channel;
    }PlayBackCmd;
    //int ret;
    close(netfd);
    memcpy((char *)&PlayBackCmd,cmd,sizeof(PlayBackCmd));
    PlayBackCmd.bStartPlayBack= ntohl(PlayBackCmd.bStartPlayBack);
    PlayBackCmd.channel = ntohl(PlayBackCmd.channel);
    /*打开回放*/
    if(PlayBackCmd.bStartPlayBack)
    {
         printf("start play back\n");
        g_Speed =4;
     	 bPlayBackStart[PlayBackCmd.channel]=TRUE;
    }
    /*关闭回放*/
    else
    {
         printf("stop play back\n");
         bPlayBackStart[PlayBackCmd.channel]=FALSE;
    }
    return OK;
}


 /********************************************************************************
//Function:         netPlayBackNextframe()
//Description:     
//                  
//Input:            套接口描述字-netfd
                    命令缓冲区指针-cmd
                    客户端地址-clientAddr
//Output:           无 
//Return:           无 
********************************************************************************/
void netPlayBackNextframe(int netfd,char *cmd,int clientAddr)
{
    int ret;
    NETCMD_HEADER header;
    close(netfd);
    memcpy(&header,cmd,sizeof(header));
    g_Speed = 8;
    ret= SetDecodeSpeed(1,g_Speed);
    if(ret ==0)
    {
         printf("set speed OK\n");
    }
    ret = DecodeNextFrame(1);
    if(ret==0)
    {
         printf("set decode next frame OK\n");
    }
}

  /********************************************************************************
//Function:         netPlayBackSpeedDown()
//Description:     
//                  
//Input:            套接口描述字-netfd
                    命令缓冲区指针-cmd
                    客户端地址-clientAddr
//Output:           无 
//Return:           无 
********************************************************************************/
void netPlayBackSpeedDown(int netfd,char *cmd,int clientAddr)
{
    int ret;
    NETCMD_HEADER header;
    close(netfd);
    memcpy(&header,cmd,sizeof(header));
    if(g_Speed>=8)
    {
         printf("the speed have arrived min\n");
    }
    else
    {
         g_Speed++;
         printf("speed =%d\n",g_Speed);
         ret = SetDecodeSpeed(1,g_Speed);
         if(ret ==0)
         {
              printf("SetDecodeSpeed() successful\n");
         }
         else
         {
              printf("SetDecodeSpeed() fail\n");
         }
    }
    
}


 /********************************************************************************
//Function:         netPlayBackSpeedUp()
//Description:     
//                  
//Input:            套接口描述字-netfd
                    命令缓冲区指针-cmd
                    客户端地址-clientAddr
//Output:           无 
//Return:           无 
********************************************************************************/
void netPlayBackSpeedUp(int netfd,char *cmd,int clientAddr)
{
    int ret;
    NETCMD_HEADER header;
    close(netfd);
    memcpy(&header,cmd,sizeof(header));
    if(g_Speed<=0)
    {
         printf("the speed have arrived max\n");
    }
    else
    {
         g_Speed--;
         printf("speed =%d\n",g_Speed);
         ret = SetDecodeSpeed(1,g_Speed);
         if(ret ==0)
         {
              printf("SetDecodeSpeed() successful\n");
         }
         else
         {
              printf("SetDecodeSpeed() fail\n");
         }
    }
    
}


#endif
 
