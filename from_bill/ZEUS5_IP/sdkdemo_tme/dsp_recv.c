/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:      Dsp_recv.c
Author:        fangjiancai
Version :      1.0
Date:          07-12-19
Description:   process the DSP callback,and write it to the buffer for netsend or record
Version:    	// 版本信息
Function List:	DspCallback(),
History:      	
    <author>     <time>     <version >   <desc>
    fangjiancai  07/12/19    1.0         
***********************************************************************************************/

#include "include_all.h"
//static void writeToMediaBuf ( int chnl, void *data, int size, BOOL bMainStream );
/********************************************************************************
//Function:       Dsp data call back function.
//Description:   write the data to buffer
//Input:           DSP回调的数据缓冲区-*data，
                       数据大小-size，通道-chnl，
                       数据流类型-bMainstream
//Output:         无
//Return:         无
********************************************************************************/
void DspCallback ( int channel, void* buf, int size, int frame_type,void* context )
{
	//MEDIADATA	*pMediaDataTmp;
	int 		len1/*, len2,linknums*/;
	int		chnl=channel;
	CHNLNETBUF	*pChnlBufTmp;
	pthread_mutex_t	*mutexLock;
	//pthread_cond_t	cond=PTHREAD_COND_INITIALIZER;
	CONNECTCFG *pslot;/*
	if(frame_type==2)
	{
	     printf("frame_type_audio\n");
	}*/
#if 1
	switch ( frame_type )
	{
              case FRAME_TYPE_HEADER:		//file header
                   memcpy ( &(chnlPara[channel-1].mainVideoHeader), buf, size);
                   break;
              case FRAME_TYPE_AUDIO:		//audio
              case FRAME_TYPE_VIDEO_I:		//video I frame
              case FRAME_TYPE_VIDEO_P:		//video P frame
              //printf("frame_type_video=%d\n",frame_type);
                   if ( chnlPara[channel-1].linknums>0)
                   {     
                        pChnlBufTmp = &(chnlPara[chnl-1].netBuffer);
                        mutexLock = &chnlPara[chnl-1].write_mutexSem;
                        len1 = MEDIA_BUFF_SIZE - pChnlBufTmp->writePos;
                        if ( size > len1 )
                        {	
                             memcpy ( pChnlBufTmp->netBufBase + pChnlBufTmp->writePos, buf, len1 );
                             memcpy ( pChnlBufTmp->netBufBase, buf+len1, size - len1 );
                        }
                        else
                        {	
                             memcpy ( pChnlBufTmp->netBufBase + pChnlBufTmp->writePos, buf, size);
                        }
                             pthread_mutex_lock(mutexLock);
                             pChnlBufTmp->writePos += size;
                             pChnlBufTmp->writePos %= MEDIA_BUFF_SIZE; 
                             pChnlBufTmp->Totalwrites +=size;
                             pthread_mutex_unlock(mutexLock);
				/*
				 pslot = chnlPara[chnl-1].NetConnect;
	                      for(linknums = 0;linknums<chnlPara[chnl-1].linknums;linknums++)
	                      {	 
	                             pthread_cond_signal(&(pslot->cond));
			               pslot = pslot->next;
	                      }
	                      */
                   }
				   
                   if(frame_type==FRAME_TYPE_VIDEO_I)
                   {
                   	  chnlPara[chnl-1].IFrameIdx=chnlPara[chnl-1].netBuffer.writePos;
                   }
                   pChnlBufTmp = &(chnlPara[chnl-1].recBuffer);
                   len1 = MEDIA_BUFF_SIZE - pChnlBufTmp->writePos;
                   if ( size > len1 )
                   {	
                         memcpy ( pChnlBufTmp->netBufBase + pChnlBufTmp->writePos, buf, len1 );
                         memcpy ( pChnlBufTmp->netBufBase, buf+len1, size - len1 );
                   }
                   else
                   {	
                         memcpy ( pChnlBufTmp->netBufBase + pChnlBufTmp->writePos, buf, size);
                   }
                   pthread_mutex_lock(mutexLock);
                   pChnlBufTmp->writePos += size;
                   pChnlBufTmp->writePos %= MEDIA_BUFF_SIZE; 
                   pChnlBufTmp->Totalwrites +=size;
                   pthread_mutex_unlock(mutexLock);
                   break;
              case FRAME_TYPE_VIDEO_BP:	//video BP, BBP frame
                   break;
              case FRAME_TYPE_SUB_HEADER:	//sub file header
                   memcpy ( &(chnlPara[channel-1].subVideoHeader), buf, size);
                   break;
              case FRAME_TYPE_VIDEO_SUB_I:	//sub video I frame
			
              case FRAME_TYPE_VIDEO_SUB_P:	//sub video P frame
                  // if (chnlPara[channel-1].sublinknums>0)
                   //{
                        pChnlBufTmp = &(chnlPara[chnl-1].netBufferSub );
                        mutexLock = &chnlPara[chnl-1].subwrite_mutexSem;
                        len1 = MEDIA_BUFF_SIZE - pChnlBufTmp->writePos;
                        if ( size > len1 )
                        {	
                             memcpy ( pChnlBufTmp->netBufBase + pChnlBufTmp->writePos, buf, len1 );
                             memcpy ( pChnlBufTmp->netBufBase, buf+len1, size - len1 );
                        }
                        else
                        {	
                             memcpy ( pChnlBufTmp->netBufBase + pChnlBufTmp->writePos, buf, size);
                        }
                             pthread_mutex_lock(mutexLock);
                             pChnlBufTmp->writePos += size;
                             pChnlBufTmp->writePos %= MEDIA_BUFF_SIZE; 
                             pChnlBufTmp->Totalwrites +=size;
                             pthread_mutex_unlock(mutexLock);
				/*
		               pslot = chnlPara[chnl-1].SubNetConnect;
		               for(linknums=0;linknums<chnlPara[chnl-1].sublinknums;linknums++)
		               {
		                           pthread_cond_signal(&(pslot->cond));
			                    pslot = pslot->next;
		               }
		               */
                  // }
                   break;	
              case FRAME_TYPE_VIDEO_SUB_BP:	//sub video BP,BBP frame
                   printf ( "sub video BP frame back.\n" );
                   break;
              case FRAME_TYPE_MD_RESULT:		//move detect result
                   MotdetStatus |= (1<<(chnl-1));
                   //printf ( "#############move detect result back. size = %d\n", size );
                   break;
              case FRAME_TYPE_ORIGINAL_IMG:	//original image get
                   printf ( "original image back.\n" );
                   break;
              case FRAME_TYPE_JPEG_IMG:		//Jpeg data get
                   break;
              default:
                   break;
        }
		
	return;
#endif
}

void copyLongs(char *srcAddr, char *dstAddr, int bytes)
{
	UINT32 *srcStart = (UINT32 *)srcAddr;

	UINT32 *srcEnd = srcAddr + (bytes>>2);

	UINT32 *dstStart = (UINT32 *)dstAddr;

	char *addr = (char *)(srcEnd+4);
	char *addr1 = dstAddr + (bytes>>2)+4;
	int i = 0;

	while(srcStart<=srcEnd)
	{
		*dstStart++ = *srcStart++;
	}

	bytes %= 4;
	if ( bytes )
	{
              for ( i = 0;i < bytes; i++ )
	      {
		      *addr1 = *addr++;
	      }
	}
		
}


/********************************************************************************
//Function:
//Input:
//Output:
//Return:
//Description:
********************************************************************************/
int destroyMediaData ( MEDIADATA *mediaData )
{
	free ( mediaData->dataBuf );
	mediaData->dataBuf = NULL;
	return OK;
}

