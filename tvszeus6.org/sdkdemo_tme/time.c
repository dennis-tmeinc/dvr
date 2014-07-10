#include "include_all.h"
/********************************************************************************
//Function:
//Input:
//Output:
//Return:
//Description:Client set dsp time.
********************************************************************************/
int netSetDspTime ( int netfd, char *clientCmd, int clientAddr )
{
	struct SYSTEMTIME	time;
	NETCMD_HEADER	netCmdHeader;

	memcpy ( &netCmdHeader, clientCmd, sizeof ( NETCMD_HEADER ) );
	netCmdHeader.nTotalLen = ntohl ( netCmdHeader.nTotalLen );
	if ( netCmdHeader.nTotalLen != ( sizeof ( NETCMD_HEADER) + sizeof ( struct SYSTEMTIME ) ) ){
		close(netfd);
		return ERROR;
		}
	memcpy ( &time, &clientCmd[sizeof ( NETCMD_HEADER )], sizeof ( struct SYSTEMTIME ) );
	time.year = ntohs ( time.year );
	time.month = ntohs ( time.month );
	time.dayofweek= ntohs ( time.dayofweek );
	time.day = ntohs ( time.day );
	time.hour = ntohs ( time.hour );
	time.minute= ntohs ( time.minute );
	time.second= ntohs ( time.second);
	time.milliseconds= ntohs ( time.milliseconds);
	printf ( "year=%d,month=%d,dayofweek=%d,day=%d,hour=%d,min=%d,sec=%d.\n ",
		time.year, 
		time.month,
		time.dayofweek,
		time.day,
		time.hour,
		time.minute,
		time.second);
	close ( netfd );
	return SetDSPDateTime ( 1, &time );
}

