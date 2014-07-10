/**********************************************************************************************
Copyright         2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         streamRecord.c
Author:           fangjiancai
Version :         1.0
Date:             07-12-20
Description:      record strream
Version:    	  1.0
Function List:	  StreamRec(),netLocalRecord()
History:      	
    <author>      <time>   <version >   <desc>
    fangjiancai  08-01-07  1.0          
 ***********************************************************************************************/
 #include "include_all.h"

/*******************************************************************************I
//Function:       StreamRec()
//Description:    ����¼��,�����ִ���ֻ¼���һͨ�����Ҹ�ͨ����
                  ����¼�񶼷���ͬһ���ļ��У�Ϊ�˼򵥣�ÿ�ζ�������ļ��Ŀ�ʼ��д
		  ���е�bFirst�����Ǽ��ĳ��ѭ���Ƿ��ǵ���¼��ĵ�һ��ѭ������Ϊ��һ��
                  ѭ��Ҫ���ļ�
//Input:          ��
//Output:         ��
//Return:         ��
********************************************************************************/
void StreamRec()
 {
    char            *bufAddr, *bufAddr1;
    char            fileName[32];
    int             chnl=0;
    int             wlen;
    int             recFd=-1;
    CHNLNETBUF      *pChnlBufTmp =NULL;
    CHNLPARA	    *pChnlParaTmp;
    UINT32         /* len,*/readIdx=0,writeIdx=0;
    UINT32          TotalLen,TotalLen1;
    BOOL            bFirst= TRUE;
    HIKVISION_MEDIA_FILE_HEADER  MediaFileHead;
    
    while(1)
    {
	 /*���¼��ʼ*/
	 if(bRecordStart[chnl])
	 {         
	      if(bFirst)/*����ô�ѭ���Ǹô�¼����״�ѭ��*/
              {   
		   /*������ڲ���ATA MODE*/
                   if(!IsATAMode)
                   {
                        umount("/davinci/");
                        EnableATA();
                        mount("/dev/hda1","/mnt/hda1","vfat",32768,NULL);
                        IsATAMode = TRUE;
                   }
                   pChnlParaTmp = &chnlPara[chnl];
                   pChnlBufTmp = &(pChnlParaTmp->recBuffer);
                   sprintf(fileName,"/mnt/hda1/recfilechannel%d.mp4",chnl);
                   //sprintf(fileName,"/davinci/record1.mp4");
		   //remove(fileName);
                   recFd = open(fileName,O_RDWR|O_CREAT,0777);
                   printf("record fileName=%s\n",fileName);
                   CaptureIFrame(chnl,MAIN_CHANNEL);
                   // len = lseek(recFd,0,SEEK_END);
                   // if(len<sizeof(HIKVISION_MEDIA_FILE_HEADER))
                   {
                        /*���ļ��Ŀ�ʼ��д��������ǰ��¼���ļ�*/
			lseek(recFd,0,SEEK_SET);
                        memcpy(&MediaFileHead,&(chnlPara[chnl].mainVideoHeader),sizeof(HIKVISION_MEDIA_FILE_HEADER));
                        wlen=write(recFd,&MediaFileHead,sizeof(HIKVISION_MEDIA_FILE_HEADER));
		        printf("wlen=%d,recFd=%d\n",wlen,recFd);
		        if(wlen!=sizeof(HIKVISION_MEDIA_FILE_HEADER))
		        {
			        printf("write error,wlen=%d\n",wlen);
				break;
			}

                   }
                   bFirst= FALSE;
		   /*Ҫ��д��һ��I֡��IFrameIdxָ��¼�񻺳��������µ�I֡*/
                   readIdx = pChnlParaTmp->IFrameIdx;
                   writeIdx = pChnlBufTmp->writePos;
              }
	      //  printf("writeidx=%d,readinx=%d\n",writeIdx,readIdx);
               if(writeIdx > MEDIA_BUFF_SIZE)
               {
                   continue;
               }
              if(writeIdx>=readIdx)
              {
                   TotalLen = writeIdx-readIdx;
                   TotalLen1=0;
                   bufAddr = pChnlBufTmp->netBufBase+readIdx;
                   if(TotalLen>0)
                   {
                        write(recFd,bufAddr,TotalLen);
                   }
              }
              else
              {
                   TotalLen = MEDIA_BUFF_SIZE - readIdx;
                   TotalLen1 = writeIdx;
                   bufAddr =pChnlBufTmp->netBufBase+readIdx;
                   bufAddr1 = pChnlBufTmp->netBufBase;
                   if(TotalLen)
                   {
                        write(recFd,bufAddr,TotalLen);
                   }
                   if(TotalLen1)
                   {
                        write(recFd,bufAddr1,TotalLen1);
                   }
              }
              
              readIdx = writeIdx;
              writeIdx = pChnlBufTmp->writePos;
	      usleep(1);
	 }
	 /*���¼��ֹͣ��û�п�ʼ¼��*/
	 else
	 {
	      bFirst=TRUE;
	      /*���¼��ֹͣ����رմ򿪵��ļ�*/
              if(recFd>0)
              {
	           close(recFd);
		   recFd = -1;
	      }
	      usleep(10000);
	 }
    }
}


/********************************************************************************
//Function:         netLocalRecord()
//Description:      �����ͻ��˷����ı���¼������ 
//Input:            �׽ӿ�������-netfd
                    �������ָ��-cmd
                    �ͻ��˵�ַ-clientAddr
//Output:           �� 
//Return:           ��
********************************************************************************/ 
int netLocalRecord(int netfd,char * cmd,int clientAddr)
{
    struct
    {
         NETCMD_HEADER    header;
         int              bstartrec;/*0:stop recording,1:start to record*/
         int              channel;
    }LocalRecordCmd;
    //int ret;
    close(netfd);
    memcpy((char *)&LocalRecordCmd,cmd,sizeof(LocalRecordCmd));
    LocalRecordCmd.bstartrec= ntohl(LocalRecordCmd.bstartrec);
    LocalRecordCmd.channel = ntohl(LocalRecordCmd.channel);
    /*�����ʼ¼��*/
    if(LocalRecordCmd.bstartrec)
    {
         printf("start local record in channel %d\n",LocalRecordCmd.channel);
	 /*��ȫ�ֱ�����Ϊ��*/
     	 bRecordStart[LocalRecordCmd.channel]=TRUE;
    }
    else
    {
         printf("stop local record\n");
         bRecordStart[LocalRecordCmd.channel]=FALSE;
    }
    return OK;
}


/*******************************************************************************I
//Function:       MotStreamRec()
//Description:   �����ƶ����¼��
//Input:          ��
//Output:         ��
//Return:         ��
********************************************************************************/
void MotStreamRec()
 {
    char                 *bufAddr, *bufAddr1,time_str[16];
    char                  fileName[64];
    int                     chnl=0;
    int                     wlen;
    int                     recFd=-1;
    time_t               currTime;
    struct tm          ct;
    CHNLNETBUF      *pChnlBufTmp=NULL;
    CHNLPARA	    *pChnlParaTmp;
    UINT32         /* len,*/readIdx[channelnum],writeIdx[channelnum];
    UINT32          TotalLen,TotalLen1;
   // BOOL            bFirst= TRUE;
    HIKVISION_MEDIA_FILE_HEADER  MediaFileHead;
    
    while(1)
    {
         currTime = time(NULL);
         for(chnl =0;chnl <channelnum;chnl++)
         {
              /*��⵽�ƶ�����Ҫ¼��*/
              if(bMotRecordStart[chnl]&&(currTime<MotRecordStartTime[chnl]+30))
              {
                      /*���¼��û�п�ʼ*/
                     if(MotRecFd[chnl]<0)
                     {
                           /*�������Ӳ��ģʽ�����л���Ӳ��*/
                           if(!IsATAMode)
                           {
                               umount("/davinci/");
                               EnableATA();
                               mount("/dev/hda1","/mnt/hda1","vfat",32768,NULL);
                               IsATAMode = TRUE;
                            }
                            pChnlParaTmp = &chnlPara[chnl];
                            pChnlBufTmp = &(pChnlParaTmp->recBuffer);
				/*��¼���ļ�����*/
				gmtime_r(&currTime, &ct);
	                     sprintf(time_str, "%04d%02d%02d%02d%02d%02d", ct.tm_year+1900, ct.tm_mon+1, ct.tm_mday,ct.tm_hour, ct.tm_min, ct.tm_sec);
                            sprintf(fileName,"/mnt/hda1/motrec%d_%s.mp4",chnl,time_str);
				MotRecFd[chnl]= open(fileName,O_RDWR|O_CREAT,0777);
                            printf("record fileName=%s\n",fileName);
                            CaptureIFrame(chnl,MAIN_CHANNEL);
				memcpy(&MediaFileHead,&(chnlPara[chnl].mainVideoHeader),sizeof(HIKVISION_MEDIA_FILE_HEADER));
                            wlen=write(MotRecFd[chnl],&MediaFileHead,sizeof(HIKVISION_MEDIA_FILE_HEADER));
		              printf("wlen=%d,recFd=%d\n",wlen,recFd);
		              if(wlen!=sizeof(HIKVISION_MEDIA_FILE_HEADER))
		              {
			            printf("write error,wlen=%d\n",wlen);
				     break;
			        }
				readIdx[chnl] = pChnlParaTmp->IFrameIdx;
                            writeIdx[chnl] = pChnlBufTmp->writePos;
                     }
			else/*¼���Ѿ���ʼ*/
			{
			      if(writeIdx[chnl] > MEDIA_BUFF_SIZE)
                           {
                                   continue;
                            }
                           if(writeIdx[chnl]>=readIdx[chnl])
                           {
                                   TotalLen = writeIdx[chnl]-readIdx[chnl];
                                   TotalLen1=0;
					//printf("write len =%d\n",TotalLen);
                                   bufAddr = pChnlBufTmp->netBufBase+readIdx[chnl];
                                   if(TotalLen>0)
                                   {
                                           write(MotRecFd[chnl],bufAddr,TotalLen);
                                   }
                             }
                             else
                             {
                                    TotalLen = MEDIA_BUFF_SIZE - readIdx[chnl];
                                    TotalLen1 = writeIdx[chnl];
                                    bufAddr =pChnlBufTmp->netBufBase+readIdx[chnl];
                                    bufAddr1 = pChnlBufTmp->netBufBase;
					 //printf("write len =%d\n",TotalLen+TotalLen1);
                                    if(TotalLen)
                                    {
                                        write(MotRecFd[chnl],bufAddr,TotalLen);
                                     }
                                    if(TotalLen1)
                                    {
                                         write(MotRecFd[chnl],bufAddr1,TotalLen1);
                                     }
                              }
              
                              readIdx[chnl] = writeIdx[chnl];
                              writeIdx[chnl] = pChnlBufTmp->writePos;
	                       usleep(1);
			}
              }
              else if(bMotRecordStart[chnl])/*����ʱ���ѵ�*/
              {
                     printf("close file\n");
                     close(MotRecFd[chnl]);
                     bMotRecordStart[chnl]= FALSE;
			MotRecFd[chnl]=-1;
              }
         }
	 usleep(10);
    }
}
