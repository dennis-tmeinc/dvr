/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:        Audio.c 
Author:          fangjiancai  
Version :        1.0      
Date:            07-12-18
Description:     process audio
Version:    	// 版本信息
Function List:	netvoiceTalkTaskrun(),voiceTalkToNet(),voiceTalkFromNet()
History:      	
    <author>  <time>   <version >   <desc>
***********************************************************************************************/


#include "include_all.h"

static BOOL	bVoiceTalkClose = TRUE;

void voiceTalkToNet ( int netfd );
void voiceTalkFromNet ( int netfd );

/********************************************************************************
Function:         netVoiceTalkTaskRun()
Description:      create two task to process the voice
Input:            套接字-netfd，命令缓冲区指针-clientCmd，客户ip地址-clientAddr
Output:          
Return:           ERROR for error,OK for right
********************************************************************************/
int netVoiceTalkTaskRun( int netfd, char *clientCmd, int clientAddr )
{
	struct
	{
    	     NETCMD_HEADER netCmdHeader;
    	     int cmdtype;
	}voicecmd;
	int ret = ERROR;

	printf ( "###Enter netVoiceTalkRun().\n" );
	memcpy ( &voicecmd, clientCmd, sizeof ( voicecmd) );

	voicecmd.netCmdHeader.nTotalLen = ntohl ( voicecmd.netCmdHeader.nTotalLen );
	printf ( " netCmdHeader.nTotalLen = %x.\n ",  voicecmd.netCmdHeader.nTotalLen);
	
	voicecmd.cmdtype=ntohl(voicecmd.cmdtype);
	printf ( "###cmdType = %d.\n", voicecmd.cmdtype );
	pthread_mutex_lock ( &voiceMutex );

	if (voicecmd.cmdtype == 1 )
	{
             ret=SetPreviewAudio(0,1, 0);
	     if(ret==0)
	     {
	          printf("close preview audio successful\n");
             }
	     if ( ( ret = OpenVoiceChannel () ) == OK )
	     {
                  printf ( "###OpenVoiceChannel().\n" );
                  //start voice talk task.
                  bVoiceTalkClose = FALSE;
                  //printf ( "#####Run voice 2 task.\n" );
                  startUpTask (150, 8192, ( ENTRYPOINT )voiceTalkToNet, netfd,0,0,0,0 );
                  //startUpTask (150, 8192, ( ENTRYPOINT )voiceTalkFromNet, netfd,0,0,0,0 );
             }
             else if ( ret == -1 )
             {
                  printf ( "####OpenVoiceChannel (), The voice talk started.\n" );
                  return ERROR;
             }
        }
	else if (voicecmd.cmdtype == 0 )
	{
             ret=SetPreviewAudio(0,1, 1);
	     if(ret==0)
	     {
	          printf("open preview audio successful\n");
             }
             bVoiceTalkClose = TRUE;
             if ( ( ret = CloseVoiceChannel () ) == OK )
             {
                  //close voice talk task.
                  printf ( "####closeVoiceChannel().\n" );
                  close ( netfd );
             }
             else if ( ret == -2 )
             {
                  printf ( "####CloseVoiceChannel (), The voice talk have not started.\n" );
             }
	}
	else 
	{
             return ERROR;
	}
	pthread_mutex_unlock ( &voiceMutex );
	return OK;
}


/********************************************************************************
Function:         voiceTalkToNet()
Description:      read voice data(the DSP processed) from voice buffer and send it to client 
Input:            语音对讲套接字-netfd
Output:           无
Return:           无
********************************************************************************/
void voiceTalkToNet ( int netfd )
{
	char 	buf[320];
	int 	len;

	printf ( "####voiceTalkToNet () start.\n" );
	while ( !bVoiceTalkClose )
	{
	      len=0;
	      len = ReadVoiceData (buf, sizeof ( buf ));
	      printf("len=%d\n",len);
	      if ( len )
              {
		   printf ( "voice talk to net, data len = %d.\n", len );
                   tcpSend ( netfd, buf, len );
                   WriteVoiceData(buf,len);
              }
              usleep (100000);
	}
        if ( netfd != ERROR ) 
        {
              close ( netfd );
        }
	return;
}


/********************************************************************************
Function:           voiceTalkFromNet()
Description:        receive the data form socketfd,and write it to buffer then DSP can process
Input:              语音对讲套接字-netfd
Output:             无
Return:             无
********************************************************************************/
void voiceTalkFromNet ( int netfd )
{
	char	buf[320];
	int	len;
	fd_set	rset;
	struct  timeval	select_timeout;

	printf ( "####voiceTalkFromNet ().\n" );
	while ( !bVoiceTalkClose )
	{
              select_timeout.tv_sec = NETWORK_TIMEOUT;
              select_timeout.tv_usec = 0;
              FD_ZERO ( &rset );
              FD_SET ( netfd, &rset );
              if (select ( netfd+1, &rset, NULL, NULL, &select_timeout ) <= 0 ) 
              {
                   return;
	      }
              else
              {
                   if ( FD_ISSET ( netfd, &rset ) )
                   {
                        len = recv ( netfd, buf, 80, 0 );
                        printf ( "voice talk frome net ,len = %d.\n", len );
                        if ( len > 0 )
		        {
                             WriteVoiceData (buf, len);
		        }
                        usleep ( 1 );
                   }
              }
	}
	if ( netfd != ERROR ) close ( netfd );
	return;

}
