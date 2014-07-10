/**********************************************************************************************
Copyright         2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         streamRecord.c
Author:           fangjiancai
Version :         1.0
Date:             07-12-20
Description:      ���豸��Ԥ����صĺ���
Version:    	  1.0
Function List:	  netSetPreviewOrder()
History:      	
    <author>      <time>   <version >   <desc>
    fangjiancai  08-01-07  1.0          
 ***********************************************************************************************/
 #include "include_all.h"

#ifdef DVR_DEMO
/********************************************************************************
//Function:         netSetPreviewOrder()
//Description:      �ӿͻ���������Ļ���ڵ���ʾ˳��������õڶ�ͨ����ʾ�ڵ�һ�����ڵ�λ��
//                  
//Input:            �׽ӿ�������-netfd
                    �������ָ��-cmd
                    �ͻ��˵�ַ-clientAddr
//Output:           �� 
//Return:           �� 
********************************************************************************/ 
void netSetPreviewOrder(int netfd,char * cmd,int clientAddr)
 {
     int i,channel,ret;
     struct
     {
         NETCMD_HEADER header;
         UINT8 order[16];
     }PreviewOrder;
     close(netfd);
     memcpy(&PreviewOrder,cmd,sizeof(PreviewOrder));/**/
     /*�ȹص�����Ԥ���ĸ���ͨ��*/
     for ( i = 0; i < pDevCfgPara->VideoOut.ScreenNum; i++ ) 
     {
	 channel = pDevCfgPara->VideoOut.PreviewOrder[i];
         if(channel!=0xff)
         {
	     ret=SetPreviewScreen(MAIN_OUTPUT,channel+1,0);
	     printf("ret=%d\n",ret);
         }
     }
     ClrDisplayParams(0,0);
     /*�����õ�Ԥ��ͨ��˳�򱣴������ýṹ��*/
     for(i=0;i<16;i++)
     {
     	 pDevCfgPara->VideoOut.PreviewOrder[i]=PreviewOrder.order[i];
     }
     /*��������Ԥ��˳���SDK��*/
     SetDisplayOrder(0, &(pDevCfgPara->VideoOut.PreviewOrder[0]));
     /*д�����ļ�*/
     writeDevCfg(pDevCfgPara);
 }


#endif
