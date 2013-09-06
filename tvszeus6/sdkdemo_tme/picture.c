/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:    Picture.c
Author:        Version :          Date:   
Description:   deal wite the picture
Version:    	// 版本信息
Function List:	netGetImage()
History:      	
    <author>  <time>   <version >   <desc>
***********************************************************************************************/


#include "include_all.h"
/********************************************************************************
Function:      netGetImage()
Description:  Client get jpeg picture from Dsp.
Input:          套接字-netfd，客户命令缓冲区指针-clentcmd，客户地址-clentaddr
Output:        无
Return:        成功返回发送给客户的图片大小，失败返回-1 

********************************************************************************/
int netGetImage ( int netfd, char *clientCmd, int clientAddr )
{
	char jpeg_data[200*1024];
	int ret;
	int len, quality, mode, chnl;
	NETCMD_HEADER	netCmdHeader;

	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != ( sizeof ( NETCMD_HEADER) + 3 ) )
		return ERROR;

	len = sizeof ( NETCMD_HEADER );
	quality = (int)clientCmd[len];
	mode = (int)clientCmd[len+1];
	chnl = (int)clientCmd[len+2];
	len = sizeof ( jpeg_data );
	printf ( "GetJpegImage().\n" );
//	ret = GetJPEGImage(1, 0,3, jpeg_data,&len);//quality--[0,3],mode--[2--D1,0--CIF,1--QCIF]
	ret = GetJPEGImage(chnl, quality,mode, jpeg_data,&len);//quality--[0,3],mode--[2--D1,0--CIF,1--QCIF]
	printf ( "Jpeg length = %d.\n", len );
	if ( ret == OK )
	{
		return tcpSend ( netfd, jpeg_data, len );
	}
	pthread_testcancel();
	close ( netfd );
	return ERROR;
}

