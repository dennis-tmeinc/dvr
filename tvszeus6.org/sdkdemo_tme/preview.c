/**********************************************************************************************
Copyright         2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:         streamRecord.c
Author:           fangjiancai
Version :         1.0
Date:             07-12-20
Description:      和设备端预览相关的函数
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
//Description:      从客户端设置屏幕窗口的显示顺序，如可以让第二通道显示在第一个窗口的位置
//                  
//Input:            套接口描述字-netfd
                    命令缓冲区指针-cmd
                    客户端地址-clientAddr
//Output:           无 
//Return:           无 
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
     /*先关掉正在预览的各个通道*/
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
     /*将设置的预览通道顺序保存在配置结构中*/
     for(i=0;i<16;i++)
     {
     	 pDevCfgPara->VideoOut.PreviewOrder[i]=PreviewOrder.order[i];
     }
     /*调用设置预览顺序的SDK，*/
     SetDisplayOrder(0, &(pDevCfgPara->VideoOut.PreviewOrder[0]));
     /*写配置文件*/
     writeDevCfg(pDevCfgPara);
 }


#endif
