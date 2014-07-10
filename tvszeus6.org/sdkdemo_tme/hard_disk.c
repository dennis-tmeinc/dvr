/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:        Hard_disk.c
Author:          fangjiancai  
Version :        1.0      
Date:            07-12-28
Description:     硬盘和flash的切换
Version:         // 版本信息
Function List:	NetSwitchHdMode()
History:      	
    <author>     <time>   <version >   <desc>
    fangjiancai  07-12-19  1.0          处理和硬盘相关的操作
***********************************************************************************************/

#include "include_all.h"
#include "netinet/tcp.h"

/********************************************************************************
//Function:         NetSwitchHdMode()
//Description:    进行硬盘和flash之间的切换，收到的命令的mode为0是
                         进行ATA->FLASH的转换，mode为1时进行FLASH->ATA的转换
//Input:             套接口描述字-netfd
                         命令缓冲区指针-cmd
                         客户端地址-clientAddr
//Output:           无
//Return:           成功返回0，失败返回-1
********************************************************************************/
int  netSwitchHdMode(int netfd,char * cmd,int clientAddr)
{
     struct
     {
          NETCMD_HEADER header;
          int                        mode;/*0:harddisk->flash,1:flash->harddisk*/
     }switchhdcmd;
     int ret;

     memcpy((char *)&switchhdcmd,cmd,sizeof(switchhdcmd));
     switchhdcmd.mode = ntohl(switchhdcmd.mode);
	 
     if(switchhdcmd.mode)
     {
        ret = EnableATA();
            /*拷贝/davinci目录下的文件到home下*/
          /* 
           ret=copyFile("/davinci/HZK16.bin", "/home/HZK16.bin");
           if(ret==ERROR)
           {
                printf("copy HZK16.bin fail\n");
                return ret;
           }
	    umount("/davinci/");
          
           ret = EnableATA();
           if(ret==0)
           {
                //拷贝home目录下的文件到/mnt/hda1目录下
                printf("switch successful !now is ATA mode\n");
                mount("/dev/hda1","/mnt/hda1","vfat",32768,NULL);

               ret=copyFile("/home/HZK16.bin", "/mnt/hda1/HZK16.bin");
               if(ret==ERROR)
               {
                    printf("copy HZK16.bin fail\n");
                    return ret;
               }
			   
               remove("/home/HZK16.bin");
           }
           else
           {
               ret=-1;
               printf("switch fail,now is also in FLASH mode\n");
           }
		   */
     }
     else
     {
        ret = EnableFLASH();
     /*
           umount("/mnt/hda1");
           ret = EnableFLASH();
           if(ret==0)
           {
                //mount("");
                remove("/davinci/sdkCfg.bin");
                printf("switch successful!now is in FLASH mode\n");
                
           }
           else
           {
                ret = -1;
                printf("switch fail,now is also in ATA mode\n");
           }
           */
     }
     close(netfd); 
     return ret;
     
}

