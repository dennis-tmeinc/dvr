/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:    Print.c
Author:        Version :          Date:
Description:   printf the board infamation
Version:    	// ∞Ê±æ–≈œ¢
Function List:	printBoardInfo()
History:      	
    <author>  <time>   <version >   <desc>
***********************************************************************************************/

#include "include_all.h"
/********************************************************************************
Function: printBoardInfo()
Input:      the struct of board_info
Output:    the board infamation
Return:
Description:printf the board's infamation
********************************************************************************/
void printBoardInfo ( struct board_info boardInfo )
{
	struct version_info  versionInfo;
	GetSDKVersion (&versionInfo);	//api:get sdk version
	printf ( "#################SDK Version########################.\n" );
	printf ( " version = %d.\n build = %d.\n\n", versionInfo.version, versionInfo.build );
	printf ( "#################Board information####################.\n" );
	printf ( "###Board type = %d.\n", boardInfo.board_type );
	printf ( "###production_date = %s.\n", boardInfo.production_date);
	printf ( "###sn = %s.\n", boardInfo.sn);
	printf ( "###flash_size = %d.\n", boardInfo.flash_size);
	printf ( "###mem_size = %d.\n", boardInfo.mem_size);
	printf ( "###dsp_count = %d.\n", boardInfo.dsp_count);
	printf ( "###enc_chan = %d.\n", boardInfo.enc_chan);
	printf ( "###dec_chan = %d.\n", boardInfo.dec_chan);
	printf ( "###alarm in = %d.\n", boardInfo.alarm_in);
	printf ( "###alarm out = %d.\n", boardInfo.alarm_out);
	printf ( "###disk number = %d.\n", boardInfo.disk_num);
	printf ( "###mac address = %s.\n", boardInfo.mac);
//	printf ( "Board type = %d.\n", boardInfo.board_type );
	return;
}
