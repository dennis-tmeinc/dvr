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
//Description:    进行录像,本部分代码只录像第一通道，且该通道的
                  所有录像都放在同一个文件中，为了简单，每次都是向该文件的开始处写
		  其中的bFirst变量是检查某次循环是否是当次录像的第一次循环，因为第一次
                  循环要打开文件
//Input:          无
//Output:         无
//Return:         无
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
	 /*如果录像开始*/
	 if(bRecordStart[chnl])
	 {         
	      if(bFirst)/*如果该次循环是该次录像的首次循环*/
              {   
		   /*如果现在不是ATA MODE*/
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
                        /*从文件的开始处写，覆盖以前的录像文件*/
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
		   /*要先写入一个I帧，IFrameIdx指向录像缓冲区的最新的I帧*/
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
	 /*如果录像停止或还没有开始录像*/
	 else
	 {
	      bFirst=TRUE;
	      /*如果录像停止，则关闭打开的文件*/
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
//Description:      解析客户端发来的本地录像命令 
//Input:            套接口描述字-netfd
                    命令缓冲区指针-cmd
                    客户端地址-clientAddr
//Output:           无 
//Return:           无
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
    /*如果开始录像*/
    if(LocalRecordCmd.bstartrec)
    {
         printf("start local record in channel %d\n",LocalRecordCmd.channel);
	 /*将全局变量置为真*/
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
//Description:   进行移动侦测录像
//Input:          无
//Output:         无
//Return:         无
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
              /*侦测到移动，需要录像，*/
              if(bMotRecordStart[chnl]&&(currTime<MotRecordStartTime[chnl]+30))
              {
                      /*如果录像还没有开始*/
                     if(MotRecFd[chnl]<0)
                     {
                           /*如果不是硬盘模式，先切换到硬盘*/
                           if(!IsATAMode)
                           {
                               umount("/davinci/");
                               EnableATA();
                               mount("/dev/hda1","/mnt/hda1","vfat",32768,NULL);
                               IsATAMode = TRUE;
                            }
                            pChnlParaTmp = &chnlPara[chnl];
                            pChnlBufTmp = &(pChnlParaTmp->recBuffer);
				/*给录像文件命名*/
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
			else/*录像已经开始*/
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
              else if(bMotRecordStart[chnl])/*结束时间已到*/
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
