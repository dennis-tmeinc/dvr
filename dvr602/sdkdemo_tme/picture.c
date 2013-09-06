/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:    Picture.c
Author:        Version :          Date:   
Description:   deal wite the picture
Version:    	// �汾��Ϣ
Function List:	netGetImage()
History:      	
    <author>  <time>   <version >   <desc>
***********************************************************************************************/


#include "include_all.h"
/********************************************************************************
Function:      netGetImage()
Description:  Client get jpeg picture from Dsp.
Input:          �׽���-netfd���ͻ��������ָ��-clentcmd���ͻ���ַ-clentaddr
Output:        ��
Return:        �ɹ����ط��͸��ͻ���ͼƬ��С��ʧ�ܷ���-1 

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

