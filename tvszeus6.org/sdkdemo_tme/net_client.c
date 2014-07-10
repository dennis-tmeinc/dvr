/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:        Net_client.c 
Author:          fangjiancai  
Version :        1.0      
Date:            07-12-19
Description:     accept the request of client and read the date from netbuffer,send it to client
Version:         // 版本信息
Function List:	 tcpSend(),tcpRecv(),processClientRequest(),netServerTask(),sendNetReturnVal(),netClientPreview()
                 netSendVideo(),netVideoSendCtrl()
History:      	
    <author>     <time>   <version >   <desc>
    fangjiancai  07-12-19  1.0          接受客户端的请求，解析客户端的命令，发送数据
***********************************************************************************************/


#include "include_all.h"
#include "netinet/tcp.h"

int netGetImage ( int netfd, char *clientCmd, int clientAddr );
int netSendVideo ( int netfd, int chnl, int streamType, int index );
int netVideoSendCtrl ( int netfd, int chnl, int streamType, int index );


/********************************************************************************
//Function: tcpSend
//Description:send date to netfd,the size is nSize
//Input:     socketfd,the address of buffer,the date's size
//Output:
//Return:   
********************************************************************************/
int tcpSend ( int netfd, void *vptr, size_t nSize )
{
	int totalLen, sendLen;
	struct timeval select_timeout;
	fd_set wset;
	char *startAddr;
	int ret;
	startAddr = vptr;
	totalLen = nSize;
	while(totalLen>0)
	{	
		select_timeout.tv_sec = NETWORK_TIMEOUT;	/* 10 second */
		select_timeout.tv_usec = 0;
		FD_ZERO(&wset);				/* wait socket writable */
		FD_SET(netfd, &wset);
		if((ret = select(netfd+1, NULL, &wset, NULL, &select_timeout)) <= 0)
		{
			if(ret==0)		/* timeout */
			{
				continue;
			}
			else
			{
				return ERROR;
			}
		}

		if(FD_ISSET(netfd, &wset))
		{
			#ifdef SLOWSEND
			if ( totalLen > LOW_BANDWIDTH )
			{
				sendLen = send(netfd, startAddr, LOW_BANDWIDTH, 0);
			}
			else
			{
				sendLen = send(netfd, startAddr, totalLen, 0);
			}
			#else
				sendLen = send(netfd, startAddr, totalLen, 0);
			#endif
			totalLen -= sendLen;
			startAddr += sendLen;
		}
	}
	return OK;
}


/********************************************************************************
//Function:       tcpRecv()
//Description:   Receive appointed number bytes from tcp protocol.
//Input:            无
//Output:          无
//Return:         ERROR for error,OK for right 
********************************************************************************/
int tcpRecv ( int netfd, void *buf, int size )
{
	int nLeft;
	int nRead;
	char *ptr;
	struct timeval  select_timeout;
	fd_set rset;

	ptr = (char *)buf;
	nLeft = size;
	while (nLeft > 0)
	{
		select_timeout.tv_sec = 5;
		select_timeout.tv_usec = 0;
		FD_ZERO(&rset);
		FD_SET( netfd, &rset );
		if(select(netfd+1, &rset, NULL, NULL, &select_timeout) <= 0)
		{
			return ERROR;
		}
		if ( FD_ISSET ( netfd, &rset ) )
		{
			if((nRead = recv(netfd, ptr, nLeft, 0)) < 0)
			{
				if(errno == EINTR)
				{
					nRead = 0;
				}
				else
				{
					return ERROR;
				}
			}
			else if ( nRead == 0 )
			{
				break;
			}
			nLeft -= nRead;
			ptr   += nRead;
		}

       }
	return ( size - nLeft );
}

/********************************************************************************
//Function:        processClientRequest()
//Description:    When client connected,it deal with the request.
//Input:             套接字-netfd，客户ip地址-clientAddr
//Output:           无
//Return:           ERROR for error,OK for right 
********************************************************************************/
void processClientRequest ( int netfd, int clientAddr )
{
	char clientCmd[MAX_CLIENT_CMDLEN];
	struct timeval select_timeout;
	fd_set rset;
	int nRead, nCmdLen;
	
	NETCMD_HEADER netCmdHeader;
	pthread_detach(pthread_self());

	printf ( "processClientRequest() running, netfd = %d\n", netfd);
	bzero ( clientCmd, MAX_CLIENT_CMDLEN );
	select_timeout.tv_sec = 5;
	select_timeout.tv_usec = 0;
	FD_ZERO ( &rset );
	FD_SET ( netfd, &rset );
	if ( select ( netfd+1, &rset, NULL, NULL, &select_timeout ) > 0 )
	{
		if ( FD_ISSET ( netfd, &rset ) )
		{
			if ( recv ( netfd, &nCmdLen, 4, 0 ) != 4  )	//read command length first
			{
				printf ( "processClientRequest net read error.\n" );
				close ( netfd );
				return;
			}
			memcpy ( (char *)&clientCmd, &nCmdLen, 4 );
			nCmdLen = ntohl ( nCmdLen );
			//printf ( "nCmdLen= %d., nCmdLen= %x.\n", nCmdLen, nCmdLen );
			nRead = nCmdLen - 4;
			if ( recv ( netfd, clientCmd+4, nRead, 0 ) != nRead )
			{
				printf ( "processClientRequest net read length error.\n" );
				close ( netfd );
				return;
			}
			memcpy ( ( char * )&netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
			netCmdHeader.nCmdType = ntohl ( netCmdHeader.nCmdType );
			printf("cmdtype=%d      %x\n",netCmdHeader.nCmdType,netCmdHeader.nCmdType);
			switch ( netCmdHeader.nCmdType )
			{
				case NETCMD_GET_IMAGE:				//passed test.
					printf ( "##################Net get image.\n" );
					netGetImage ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_PREVIEW:					//passed test.
					printf ( "##################Net preview.\n" );
					netClientPreview ( netfd, clientCmd);
					break;
				case NETCMD_VOICETALK:				//passed test.
					printf ( "##################Net voice talk.\n" );
					netVoiceTalkTaskRun ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_OSDCFG:
					printf ( "##################Net set Osd Char.\n" );
					netSetOsdPara ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_STREAMCFG:
					printf ( "##################Net set video parameter.\n" );
					netSetStreamPara ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_STREAMCFG_GET:
					printf ( "##################Net get video parameter.\n" );
					netGetStreamPara ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_VIDEOCOLOR:
					printf ( "##################Net set video color.\n" );
					netSetVideoColor ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_VIDEOCOLOR_GET:
					printf ( "##################Net get video color.\n" );
					netGetVideoColor ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_DSPTIME:
					printf ( "##################Net set Dsp time.\n" );
					netSetDspTime ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_232ModeCFG:
					printf ( "##################Net set serial 232 mode.\n" );
					netSetSerialMode ( netfd, clientCmd, clientAddr );
					break;
				case NETCMD_232CFG:
					printf ( "##################Net set serial 232.\n" );
					netSetSerialPara ( netfd, clientCmd, clientAddr, TRUE );
					break;
				case NETCMD_485CFG:
					printf ( "##################Net set serial 485.\n" );
					netSetSerialPara ( netfd, clientCmd, clientAddr, FALSE );
					break;
				case NETCMD_MASKSET_AREA:
					printf("###################Net set mask area\n");
					netSetMaskArea(netfd,clientCmd);
					break;
				case NETCMD_MASKCMD_CANCEL:
					printf("###################cancel mask area\n");
					netCancelMaskArea(netfd,clientCmd);
					break;
				case NETCMD_ALARM_SET:
					printf("########Net set Alarm##############\n");
					netSetAlarm(netfd,clientCmd);
					break;
				case NETCMD_ALARMPARA_GET:
					printf("##########Net get Alarmpara##########\n");
					netGetAlarm(netfd,clientCmd);
					break;
				case NETCMD_SWITCHHDCMD:
					printf("##########switch harddisk mode#######\n");
					netSwitchHdMode(netfd,clientCmd,clientAddr);
					break;
				case NETCMD_LOCA_RECORD:
					printf("#########LOCAL RECORD############\n");
					netLocalRecord(netfd,clientCmd,clientAddr);
					break;
				#ifdef DVR_DEMO
				case NETCMD_PLAYBACK:
					printf("#########PLAY BACK###############\n");
					netPlayBack(netfd,clientCmd,clientAddr);
					break;
				case NETCMD_SET_PREVIEWORDER:
					printf("#########SET PREVIEW ORDER#########\n");
					netSetPreviewOrder(netfd,clientCmd,clientAddr);
					break;
				case NETCMD_PLAYBACK_NEXTFRAME:
					printf("#########SWITCH TO NEXT FRAME#######\n");
					netPlayBackNextframe(netfd,clientCmd,clientAddr);
					break;
				case NETCMD_PLAYBACK_SPEEDDOWN:
					printf("#########speed down###############\n");
					netPlayBackSpeedDown(netfd, clientCmd, clientAddr);
					break;
				case NETCMD_PLAYBACK_SPEEDUP:
					printf("#########speed up ################\n");
					netPlayBackSpeedUp(netfd, clientCmd, clientAddr);
					break;
				case NETCMD_SET_OSDDISPLAY:
					printf("#################set osddisplay.#########\n");
					netSetOSDDisplay(netfd,clientCmd,clientAddr);
					break;
				#endif	
				default:
					printf("THE COMMAND IS UNKNOW!\n");
					break;
			}			
		}
	}
	pthread_testcancel ();
	sleep ( 1 );
	return;
}


/********************************************************************************
//Function:         netServerTask()
//Description:     Wait for client connect,then create new thread deal with the connection.
//Input:              无
//Output:            无
//Return:            ERROR for error,OK for right 

********************************************************************************/
void netServerTask ( void )
{
	 int serverFd, clientFd;
	 struct sockaddr_in serverAddr, clientAddr;
	 int sockAddrSize = sizeof ( struct sockaddr_in );
	 int addrReuse = 1;
	 int addr,link;
	 struct linger tcpLinger;

	 printf ( "####################netServerTask() running###############.\n" );

	 for(link=0;link<GLOBAL_MAX_LINK;link++)
	{
	       connectPara[link].next = NULL;
		connectPara[link].readIndex = 0;
		connectPara[link].Totalreads = 0;
		connectPara[link].streamfd = -1;
		connectPara[link].bMaster = -1;

		pthread_mutex_init(&(connectPara[link].mutex),NULL);
		pthread_cond_init(&(connectPara[link].cond),NULL);
	}
	TotalLinks = 0;
	if ( ( serverFd = socket ( AF_INET, SOCK_STREAM, 0 ) ) == ERROR )
		return;

	if(setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (char*)&addrReuse, sizeof(int)) == ERROR)
		return;
	bzero ( (char *)&serverAddr, sockAddrSize );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons ( 7000 );
	serverAddr.sin_addr.s_addr = htonl ( INADDR_ANY );
	bind(serverFd, (struct sockaddr *) &serverAddr, sockAddrSize);
	listen ( serverFd, 20 );
	while ( 1 )
	{
		printf ( "wait for net client connect.\n" );
		if((clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &sockAddrSize)) <0)	
		{
			continue;
		}
		//printf("clientfd=%d,errorno=%d\n",clientFd,errno);
		printf ( "accept next net client.\n" );
		addr = clientAddr.sin_addr.s_addr;

		if ( startUpTask ( 110, 8192, (ENTRYPOINT)processClientRequest, clientFd, addr, 0, 0, 0 ) != 0)
		{
			printf ( "task run error.\n" );
			tcpLinger.l_onoff = 1;
			tcpLinger.l_linger =0;
			setsockopt(clientFd, SOL_SOCKET, SO_LINGER, (char*)&tcpLinger, sizeof(struct linger));
			close(clientFd);
			break;
		}
	}
	close ( serverFd );
	return;
}


/********************************************************************************
//Function:        sendNetReturnVal()
//Description:    send the return to client
//Input:             套接字-netfd，发送给客户信息-returnVal
//Output:           无
//Return:           成功返回发送的字节数，失败返回-1

********************************************************************************/
int sendNetReturnVal ( int netfd, UINT32 returnVal )
{
	NET_RETURN_HEADER netRetValHead;
	int sendLen;
	sendLen = sizeof ( NET_RETURN_HEADER );
	bzero ( (char *)&netRetValHead, sendLen);
	netRetValHead.length = htonl ( sendLen );
	netRetValHead.returnVal = htonl ( returnVal );
	return send ( netfd, &netRetValHead, sendLen, 0 );
}


/********************************************************************************
//Function:       netClientPreview()
//Description:   当客户有预览请求时，调用该函数，创建两个任务
                        netSendVideo()和netVideoSendCtrl()
//Input:            套接字-netfd，命令缓冲区指针-clientCmd
//Output:          无
//Return:          成功返回0，失败返回-1

********************************************************************************/
int netClientPreview ( int netfd, char *clientCmd)
{

	int i, index=0;
	int		chnl, len, streamType;
	HIKVISION_MEDIA_FILE_HEADER		*mediaHeader;
	CONNECTCFG *cnntcfg;
	len = sizeof ( NETCMD_HEADER );
	chnl = (int)clientCmd[len];
	streamType = (int)clientCmd[len+1];

	printf ( "streamType = %d.chnl=%d\n", streamType,chnl );
	if ( streamType == MAIN_CHANNEL )
	{    
		if ((chnlPara[chnl-1].linknums+chnlPara[chnl-1].sublinknums) >= MAX_LINK_NUM )
		{
			printf ( "too much links at main stream channel.\n" );
			return ERROR;
		}
		
		mediaHeader = &(chnlPara[chnl-1].mainVideoHeader);
		send(netfd, mediaHeader, sizeof (HIKVISION_MEDIA_FILE_HEADER), 0);

		CaptureIFrame(chnl, MAIN_CHANNEL);
		
		for(i=0;i<GLOBAL_MAX_LINK;i++)
		{
			  if(connectPara[i].streamfd==-1)
			  {
			         index = i;
			  }
			  break;
		}
		cnntcfg = &connectPara[index];
		TotalLinks++;
		cnntcfg->bMaster = TRUE;
		cnntcfg->readIndex = chnlPara[chnl-1].netBuffer.writePos;
		cnntcfg->Totalreads = chnlPara[chnl-1].netBuffer.Totalwrites;
		cnntcfg->streamfd = netfd;
		ConnectAdd(chnl, cnntcfg);
	}
	else if ( streamType == SUB_CHANNEL )
	{
	       if(chnlPara[chnl-1].sublinknums==0)
	       {
	             /*子码流是在连接时开始编码的*/
	             StartCodec(chnl,SUB_CHANNEL);
	       }
		CaptureIFrame (chnl, SUB_CHANNEL);
		
		if ((chnlPara[chnl-1].linknums+chnlPara[chnl-1].sublinknums) >= MAX_LINK_NUM )
		{
			printf ( "Too much links at sub stream channel.\n" );
			return ERROR;
		}
              
		for(i=0;i<GLOBAL_MAX_LINK;i++)
		{
			  if(connectPara[i].streamfd==-1)
			  {
			         index = i;
			  }
			  break;
		}
		
		cnntcfg = &connectPara[index];
		TotalLinks++;
		cnntcfg->bMaster = FALSE;
		cnntcfg->readIndex = chnlPara[chnl-1].netBufferSub.writePos;
		cnntcfg->Totalreads = chnlPara[chnl-1].netBufferSub.Totalwrites;
		cnntcfg->streamfd = netfd;
		ConnectAdd(chnl, cnntcfg);
		usleep(1000);
		mediaHeader = &(chnlPara[chnl-1].subVideoHeader);
		send ( netfd, mediaHeader, sizeof (HIKVISION_MEDIA_FILE_HEADER), 0);
	}

	startUpTask (180,8192, (ENTRYPOINT)netSendVideo, netfd, chnl,streamType,index,0 );
	startUpTask (110, 8192, (ENTRYPOINT)netVideoSendCtrl, netfd, chnl,streamType,index,0 );
	return OK;
}

#define min(a,b)                (a > b ? b : a)


/********************************************************************************
//Function:       netSendVideo()
//Description:   从缓冲区取出数据，发送给客户端
//Input:            套接字-netfd，预览通道-chnl，预览类型-StreamType 主码流预览/子码流预览
                        该预览连接索引-index
//Output:          无
//Return:          预览结束返回0。
********************************************************************************/
int netSendVideo ( int netfd, int chnl, int streamType, int index )
{
	char *bufAddr, *bufAddr1;
	//char *sendBuf;
	int totalLen, totalLen1,/* sendLen, ret, */sendBufLen;
	int len1 = 0, len2 = 0;
	int sendpos;
	int writepos;
	//int retcode;
	int noDelay = 1;
	CHNLNETBUF *pChnlBufTmp=NULL;
	CHNLPARA	*pChnlParaTmp;
	CONNECTCFG *cnntcfg;
	pthread_mutex_t	*mutexLock=NULL;
	//pthread_cond_t     *rcond;
	//BOOL *netCloseFlg;
	//int sendBufSize = 8192;
	//unsigned long nTotaoLen = 0;
	struct frame_statistics stat;
	pthread_detach(pthread_self());
	//struct timeval now;
	//struct timespec timeout;
		
	sendBufLen = 8192;
	pChnlParaTmp = &chnlPara[chnl-1];
	cnntcfg = &connectPara[index];
	printf ( "###############Net send stream start.\n" );
	if ( streamType == MAIN_CHANNEL )
	{
	     
		pChnlBufTmp = &(pChnlParaTmp->netBuffer);
		mutexLock = &pChnlParaTmp->write_mutexSem;
	}
	else if ( streamType == SUB_CHANNEL )
	{
		pChnlBufTmp = &(pChnlParaTmp->netBufferSub);
		mutexLock = &pChnlParaTmp->subwrite_mutexSem;
	}

	setsockopt ( netfd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof ( noDelay ) );
	
	while (1)
	{
		if(cnntcfg->streamfd==-1)
		{
                      close ( netfd );
	               printf ( "NET client connect closed.\n" );
	               printf ( "Quit the netSendVideo().\n" );
			 break;
		}
	
		pthread_mutex_lock(mutexLock);
		writepos=pChnlBufTmp->writePos;
		pthread_mutex_unlock(mutexLock);
		sendpos = cnntcfg->readIndex;
		
	       if ( writepos>=sendpos)
	       {			 
			     len1= writepos -sendpos;
			     len2 = 0;
			     bufAddr=pChnlBufTmp->netBufBase+sendpos;
			     totalLen = len1;
		             if ( totalLen > 0 )
		             {
				   tcpSend ( netfd, bufAddr, totalLen );
		             }
			     cnntcfg->readIndex+=len1;
			     cnntcfg->Totalreads +=totalLen;
               }
	        else
	        {
			     len1=MEDIA_BUFF_SIZE-sendpos;
			     len2 =  writepos ;
			     bufAddr=pChnlBufTmp->netBufBase+sendpos;
			     totalLen = len1;
			     bufAddr1 = pChnlBufTmp->netBufBase;
			     totalLen1 = len2;
		             if ( totalLen > 0 )
			     {
				 tcpSend ( netfd, bufAddr, totalLen );
		             }
			     if ( totalLen1 > 0 )
			     {
			 	  tcpSend ( netfd, bufAddr1, totalLen1 );
			     }
			     cnntcfg->readIndex = len2;
			     cnntcfg->Totalreads +=totalLen;
			     cnntcfg->Totalreads +=totalLen1;
		 }
		 len1 = len2 = 0;
		 usleep ( 1 );
	}
	
	GetFramesStatistics ( chnl, MAIN_CHANNEL, &stat );
	printf ( "lost=%d, bps = %d, audio = %d, overflow=%d, video = %d.\n", 
	stat.frames_lost, stat.cur_bps, stat.audio_frames, stat.queue_overflow, stat.video_frames);
	return OK;
}


/********************************************************************************
//Function:       netVideoSendCtrl()
//Description:   When client at preview,it recv client data exchang command and stop preview command.
//Input:            套接字-netfd，预览通道-chnl，预览类型-StreamType 主码流预览/子码流预览
                        该预览连接索引-index
//Output:          无
//Return:          结束返回0
********************************************************************************/
int netVideoSendCtrl ( int netfd, int chnl, int streamType, int index )
{
	struct timeval select_timeout;
	fd_set rset;
	NETCMD_HEADER netCmdHeader;
	CONNECTCFG *cnntcfg;
	pthread_detach(pthread_self());

	printf ( "###############Net send stream  control start.\n" );
	cnntcfg = &connectPara[index];
	while ( 1 )
	{
		select_timeout.tv_sec = 20;
		select_timeout.tv_usec = 0;
		FD_ZERO ( &rset );
		FD_SET ( netfd, &rset );
		if ( select ( netfd+1, &rset, NULL, NULL, &select_timeout ) > 0 )
		{
			if ( FD_ISSET ( netfd, &rset ) )
			{
				if ( tcpRecv ( netfd, &netCmdHeader, sizeof ( netCmdHeader ) ) != sizeof ( netCmdHeader ) )
				{
					printf ( "netVideoSendCtrl () read error.\n" );
					break;
				}
				else
				{
					netCmdHeader.nCmdType = ntohl ( netCmdHeader.nCmdType );
					if ( netCmdHeader.nCmdType == NETCMD_MEDIADATA ) 
					{
						    continue;
					}
					else if ( netCmdHeader.nCmdType == NETCMD_PREVIEW_STOP )
					{
						printf ( "################Preview stoped.\n" );
						if(!(cnntcfg->bMaster)&&(chnlPara[chnl-1].sublinknums==1))
						{
							  StopCodec(chnl,SUB_CHANNEL);
						}
						ConnectDelete(chnl,cnntcfg);
						close ( netfd );
						break;
					 }
				}
			}
		}
		else
		{
			printf ( "netVideoSendCtrl() select error.\n" );
			break;
		}
	}

       printf ( "netVideoSendCtrl(), streamType = %d.\n", streamType );
	printf("streamfd=%d in sendctrl\n",cnntcfg->streamfd);
	printf ( "Quit the netVideoSendCtrl (). \n" );
	return OK;
}


