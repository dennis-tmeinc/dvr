/**********************************************************************************************
Copyright 2007-2008 Hikvision Digital Technology Co., Ltd.   
FileName:   Serial.c
Author:        Version :          Date: 
Description:  function for serial port
Version:    	// 版本信息
Function List:	
History:      	
    <author>  <time>   <version >   <desc>
***********************************************************************************************/


#include "include_all.h"
#include <termios.h>

int redef232Mode ( int netfd, char *clientCmd, int clientAddr )
{
	int mode;

	mode = ( int )clientCmd[sizeof ( NETCMD_HEADER )];

	if ( mode == pDevCfgPara->bSerialMode ) return OK;
	pDevCfgPara->bSerialMode = clientCmd[sizeof ( NETCMD_HEADER )];

	return writeDevCfg ( pDevCfgPara );
}

//TRUE--console,FALSE--trans mode.
int set232Mode ( int fd, BOOL mode )
{
	struct termios opt;
	tcgetattr(fd,&opt);
	if ( mode )
	{
		opt.c_lflag |= ICANON | ECHO | ECHOE | ISIG;
		opt.c_iflag |= ICRNL;
		opt.c_oflag |= OPOST;
	}
	else
	{
		opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* input */
		opt.c_iflag &= ~ICRNL;	/* 不将 CR(0D) 映射成 NR(0A) */
		opt.c_oflag &= ~OPOST;	/* output */
	}
	return tcsetattr(fd,TCSANOW,&opt);
}

int setSerialBaud ( int fd, int rate )
{
	struct termios opt;
	tcgetattr(fd,&opt);
	cfsetispeed(&opt,rate);
	cfsetospeed(&opt,rate);
	return tcsetattr(fd,TCSANOW,&opt);
}

int setSerialOtherPara ( int fd, int databits,int stopbits, int parity )
{
	struct termios opt;
	tcgetattr(fd, &opt);

	opt.c_cflag &= ~CSIZE;
	opt.c_cflag |= databits;
	
	switch( stopbits )
	{
		case STOPB0:
			opt.c_cflag &= ~CSTOPB;
			break;
		case STOPB1:
			opt.c_cflag |= CSTOPB;
			break;
		default:
			return -1;
			break;
	}
	
	switch ( parity )
	{
		case NOPARITY:
			opt.c_cflag &= ~PARENB;
			opt.c_iflag &= ~INPCK;
			break;
		case ODDPARITY:
			opt.c_cflag |= (PARENB|PARODD);
			opt.c_iflag |= INPCK;
			break;
		case EVENPARITY:
			opt.c_cflag |= PARENB;
			opt.c_cflag &= ~PARODD;
			opt.c_iflag |= INPCK;
			break;
		default:
			return -1;
			break;
	}

	return tcsetattr(fd,TCSANOW,&opt);
}

int setSerialFlowCtrl ( int fd, int control, int enable )
{
	struct termios opt;
	tcgetattr(fd, &opt);

	switch( control )
	{
		case 1:
			if ( enable )
				opt.c_iflag |= (IXON | IXOFF | IXANY);
			else
				opt.c_iflag &=~(IXON | IXOFF | IXANY);
			break;
		case 2:
			if ( enable )
				opt.c_cflag |= CRTSCTS;
			else
				opt.c_cflag &=~CRTSCTS;
			break;
		case 0:
			opt.c_iflag &=~(IXON | IXOFF | IXANY);
			opt.c_cflag &=~CRTSCTS;
			break;
		default:
			break;
	}

	return tcsetattr(fd,TCSANOW,&opt);
}

int setSerialPara ( SERIALPARA serialPara, BOOL serialType )
{
	int fd;
	//struct termios	opt;

	if ( serialType )	//serial 232 setup.
	{
		fd = open ( "/dev/ttyS0", O_RDWR, 0 );
		if ( fd == ERROR ) return ERROR;
		set232Mode ( fd, pDevCfgPara->bSerialMode );
		setSerialBaud ( fd, serialPara.baudrate );
		setSerialOtherPara ( fd, serialPara.dataBit, serialPara.stopBit, serialPara.parity );
		setSerialFlowCtrl ( fd, serialPara.flowCtrl, TRUE );
	}
	else	//serial 485 setup.
	{
		fd = open ( "/dev/ttyS1", O_RDWR, 0 );
		if ( fd == ERROR ) return ERROR;
			InitRS485( fd, serialPara.baudrate, serialPara.dataBit,
			serialPara.stopBit, serialPara.parity, serialPara.flowCtrl );
	}
	close ( fd );
	return OK;
}
/********************************************************************************
//Function:Client set serial port parameter:dataBit/stopBit/parity/flowControl/baudrate.
//Input:
//Output:
//Return:
//Description:
********************************************************************************/
int netSetSerialPara ( int netfd, char *clientCmd, int clientAddr, int serialType )
{
	SERIALPARA		serialPara;
	NETCMD_HEADER	netCmdHeader;

	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != ( sizeof ( NETCMD_HEADER) + sizeof ( SERIALPARA ) ) )
		return ERROR;

	memcpy ( &serialPara,  clientCmd + sizeof ( NETCMD_HEADER ),  sizeof ( SERIALPARA ));
	return setSerialPara ( serialPara, serialType );
}
/********************************************************************************
//Function:Client set serial port work mode:console or trans mode.
//Input:
//Output:
//Return:
//Description:
********************************************************************************/
int netSetSerialMode ( int netfd, char *clientCmd, int clientAddr )
{
	int mode;
	NETCMD_HEADER	netCmdHeader;

	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != ( sizeof ( NETCMD_HEADER) + 1 ) )
		return ERROR;

	mode = (int)clientCmd[sizeof (NETCMD_HEADER)];
	pDevCfgPara->bSerialMode = mode;
	return writeDevCfg ( pDevCfgPara );
}


/********************************************************************************

********************************************************************************/
void test485port()
{
	int fd,ret,i;
	char buf[128];
	
	fd = open("/dev/ttyS1", O_RDWR, 0);
	ret=InitRS485(fd,BAUD9600,DATAB8, STOPB0,NOPARITY,NOCTRL);
	
	if(ret!=0)
	{
		printf("init rs485 fail!\n");
	}
	else
	{
	       printf("init rs485 successful!\n");
	}
	
	for(i=0;i<128;i++)
	{
		buf[i]=i;
		printf("%d   \n",buf[i]);
	}
	
      for(;;)
	{
	       printf("begin write to 485!\n");
		i=write(fd,buf,120);
		
		if(i<120)
		{
			printf("write error!\n");
		}
		
		printf("end of this time of write!\n");
		sleep(1);
	}

}

