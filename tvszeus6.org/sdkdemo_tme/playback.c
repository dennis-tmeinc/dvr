/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         playback.c
Author:           fangjiancai
Version :         1.0
Date:             07-12-20
Description:      ����ͻ��˷��͹����Ļط������ִ�лطŲ���
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
//Description:    ���лطţ����뻺�����е�����
//Input:          ��
//Output:         ��
//Return:         ��
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
         /*������д��DSP���н���*/  
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
//Description:    ���лط�,��Ӳ���ж����ݵ�������
//Input:          ͨ����
//Output:         ��
//Return:         ��
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
	      /*�ص���ͨ����Ԥ��*/
              for ( i = 0; i < pDevCfgPara->VideoOut.ScreenNum; i++ ) 
	      {
		   SetPreviewScreen(MAIN_OUTPUT,i+1,0);
	      }
	      /*�����ǰ������ATA Mode����ת��ΪATA Mode*/
              if(!IsATAMode)
              {
                   EnableATA();
	           mount("/dev/hda1","/mnt/hda1","vfat",32768,NULL);
		   IsATAMode = TRUE;
	      }/**/
                  
	      // for decoder
	      ClrDisplayParams(MAIN_OUTPUT,0);
	      /*���·�����Դ�����ڣ�������*/
	      SetDisplayParams(MAIN_OUTPUT,1);
	      /*�򿪽���ͨ��*/
	      SetDecodeScreen(MAIN_OUTPUT,1,1);
	      /*��ʼ���طŻ�����*/
              for(i=0;i<3;i++)
              {
                   PlayBackStruct.PlayBackBuf[i].readlen=0;
              }
              PlayBackStruct.ReadIdx=0;
              PlayBackStruct.WriteIdx = 0;
              PlayBackStruct.TotalRead = 0;
              PlayBackStruct.TotalWrite = 0;

	      fileFd=open(fileName,O_RDONLY);
	      /*���ļ����󣬽����»ص�Ԥ��״̬*/
              if(fileFd==-1)
              {
                  printf("open fail,Maybe the file was delete\n");
                  goto NEXT;
              }
	      printf("fileFd=%d\n",fileFd);
	      /*�ȶ���ͷ������ʼ����*/
	      nSize = read( fileFd, &FileHead,headLen);
             ret=SetDecFileForStatistics(1,fileName);
			 printf("totalsec=%d\n",ret);
	      /*���������ͷ���Ĵ�С���󣬴��·��ص�Ԥ��״̬*/
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
	      /*���ûط��ٶ�*/    
	      ret= SetDecodeSpeed(1,g_Speed);
	      if(ret==0)
	      {
		   printf("set speed OK\n");
	      }
	      /*�ر�Ԥ������*/
	      ret=SetPreviewAudio(0,1,0);
              if(0==ret)
              {
                   printf("close preview audio successful\n");
              }
	      /*����Ƶ����*/
	      SetDecodeAudio(MAIN_OUTPUT,0,1);
	      /*������Ƶ��������*/
	      SetDecodeVolume(1,10);
	      /*����һ����DSP��д���ݵ�����*/
              startUpTask(180,8192,(ENTRYPOINT)PlayBack_Write,0,0,0,0,0);
	      while(1)
	      {
                   /*�����˳��ط�*/
		   if(!bPlayBackStart[chnl])
		   {
		        break;
		   }
		   /*�������Ѿ���*/
		   if((PlayBackStruct.TotalRead-PlayBackStruct.TotalWrite)>2)
		   {//printf("was waiting\n");
		        usleep(1);
                        continue;
		   }
                   PlayBackStruct.PlayBackBuf[PlayBackStruct.ReadIdx].readlen=read(fileFd,PlayBackStruct.PlayBackBuf[PlayBackStruct.ReadIdx].databuf,8*1024);
		   /*�˳��ط�,�������ط��ļ�����*/
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
              /*�л���Ԥ��*/
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
	      /*��Ԥ������*/
              SetPreviewAudio(0,1,1);
		      
         }	
	 usleep(1);
		
    }
    
}


 /********************************************************************************
//Function:             netPlayBack()
//Description:          �����ͻ��˵Ļط�����
//Input:                �׽ӿ�������-netfd
                        �������ָ��-cmd
                        �ͻ��˵�ַ-clientAddr
//Output:               �� 
//Return:               ��
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
    /*�򿪻ط�*/
    if(PlayBackCmd.bStartPlayBack)
    {
         printf("start play back\n");
        g_Speed =4;
     	 bPlayBackStart[PlayBackCmd.channel]=TRUE;
    }
    /*�رջط�*/
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
//Input:            �׽ӿ�������-netfd
                    �������ָ��-cmd
                    �ͻ��˵�ַ-clientAddr
//Output:           �� 
//Return:           �� 
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
//Input:            �׽ӿ�������-netfd
                    �������ָ��-cmd
                    �ͻ��˵�ַ-clientAddr
//Output:           �� 
//Return:           �� 
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
//Input:            �׽ӿ�������-netfd
                    �������ָ��-cmd
                    �ͻ��˵�ַ-clientAddr
//Output:           �� 
//Return:           �� 
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
 
