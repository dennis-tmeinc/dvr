/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:        Hard_disk.c
Author:          fangjiancai  
Version :        1.0      
Date:            07-12-28
Description:     Ӳ�̺�flash���л�
Version:         // �汾��Ϣ
Function List:	NetSwitchHdMode()
History:      	
    <author>     <time>   <version >   <desc>
    fangjiancai  07-12-19  1.0          �����Ӳ����صĲ���
***********************************************************************************************/

#include "include_all.h"
#include "netinet/tcp.h"

/********************************************************************************
//Function:         NetSwitchHdMode()
//Description:    ����Ӳ�̺�flash֮����л����յ��������modeΪ0��
                         ����ATA->FLASH��ת����modeΪ1ʱ����FLASH->ATA��ת��
//Input:             �׽ӿ�������-netfd
                         �������ָ��-cmd
                         �ͻ��˵�ַ-clientAddr
//Output:           ��
//Return:           �ɹ�����0��ʧ�ܷ���-1
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
            /*����/davinciĿ¼�µ��ļ���home��*/
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
                //����homeĿ¼�µ��ļ���/mnt/hda1Ŀ¼��
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

