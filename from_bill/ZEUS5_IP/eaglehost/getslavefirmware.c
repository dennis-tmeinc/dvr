
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <net/if.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

// #define __DBG__

#ifdef __DBG__
#define PDBG( str ) 	printf( str )
#else
#define PDBG( str )
#endif

#define HOSTIDSIZE	32

struct eagleip_message {	
	uint32_t	msgsize;		// sizeof eagleip_message
	uint32_t	msgcode; 		// 1: setip, 2: slave ip
	uint32_t	ipaddr ;
	uint32_t	eagleid ;		// board #, 0: host, 1,2,3... for slave board
	uint8_t		hostid[HOSTIDSIZE] ;	// mac id for now
} ;

#define HOST_PORT	(15151)
#define SLAVE_PORT	(15152)

// host id file should be persistant
char * hostidfile="/dav/dvr/hostid" ;

// wait for socket ready to read (timeout in micro-seconds)
int net_recvok(int fd, int tout)
{
	struct timeval timeout ;
	timeout.tv_sec = tout/1000000 ;
	timeout.tv_usec = tout%1000000 ;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
		return FD_ISSET(fd, &fds);
	} else {
		return 0;
	}
}

int sethostip(uint32_t ip)
{
	char cmd[256] ;
	sprintf( cmd, "ifconfig eth0 %d.%d.%d.%d",
		(ip>>24)&0xff,
		(ip>>16)&0xff,
		(ip>>8)&0xff,
		(ip)&0xff );
	return system( cmd );
}

int boardnum=1 ;

int main(int argc, char * argv[] )
{
	int i;
    int res ;
    int fd ;
    struct eagleip_message sendmsg, recvmsg ;	
    struct sockaddr_in addr;
    socklen_t fromlen ;
	FILE * fhd ;
	uint8_t	hostid[HOSTIDSIZE] ;
	int nohostid = 1 ;		// assume no host id recorded
	int boardid = 1 ;		// default board # 1
	int pkg = 0 ;		    // assum no any pkt received ;
	uint32_t ip ;

	if( argc<=1 ) {
		printf("Usage: ipcamd boardnumber idfile\n");
		return 2;
	}

	if( argc>1 ) {
		boardid = atoi(argv[1]) ;
	}

	if( argc>2 ) {
		hostidfile = argv[2] ;
	}

	if( boardid <1 )
		boardid=1 ;
	if( boardid >7 ) 
		boardid=7 ;

	// read recorded hostid
	memset( hostid, 0, sizeof(hostid));
	fhd = fopen(hostidfile, "rb");
	if( fhd ) {
		fread( hostid, 1, sizeof(hostid), fhd );
		fclose( fhd );
	}
	for( i=0; i<HOSTIDSIZE; i++ ) {
		if( hostid[i] ) {
			nohostid = 0 ;		// host id recorded
			break;
		}
	}
	
	// receiver socket
    fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SLAVE_PORT);
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));

	for( i=0; i<100; i++ ) {
		if( net_recvok( fd, 500000 ) ) {
			fromlen = sizeof( addr );
			res = recvfrom( fd, &recvmsg, sizeof(recvmsg), 0, (struct sockaddr *)&addr, &fromlen  );
			if( res==sizeof(recvmsg) &&
				recvmsg.msgsize == sizeof( struct eagleip_message ) &&
				recvmsg.msgcode == 1 ) {
				
				PDBG("Recv host ip.\n");

				pkg=1;
				if( nohostid ||
				    memcmp( hostid, recvmsg.hostid, sizeof(hostid) )==0 ) {
						break;
				}
			}
		}
	}

	if( pkg ) {			// there is packet from host				
		// set my ip address
		ip = recvmsg.ipaddr + boardid ;
		sethostip( ip);

		// responds
		sendmsg=recvmsg ;							// copy from received msg
		sendmsg.msgsize = sizeof( sendmsg );		// sizeof eagleip_message
		sendmsg.msgcode=2 ;							// slave ip
		sendmsg.ipaddr = ip ;
		sendmsg.eagleid = boardid ;

		// responds socket
		sendto( fd, &sendmsg, sizeof(sendmsg), 0, (struct sockaddr *)&addr, sizeof(addr) );

		PDBG("Sent slave ip.\n");

		// record host id
		fhd = fopen( hostidfile, "wb" );
		if( fhd ) {
			fwrite( recvmsg.hostid, 1, sizeof( recvmsg.hostid ), fhd );
			fclose( fhd );
		}

		// set host ip
		fhd = fopen( "/etc/hosts", "w" );
		if( fhd ) {
			fprintf( fhd, "127.0.0.1    localhost\n%d.%d.%d.%d    eaglehost\n",
				(recvmsg.ipaddr>>24)&0xff,
				(recvmsg.ipaddr>>16)&0xff,
				(recvmsg.ipaddr>>8)&0xff,
				(recvmsg.ipaddr)&0xff );
			fclose( fhd );
		}
		res = 0 ;
	}
	else {
		ip = 0xc0a8f701+boardid ;		// 192.168.247.x
		sethostip( ip);
		// set host ip
		fhd = fopen( "/etc/hosts", "w" );
		if( fhd ) {
			fprintf( fhd, "127.0.0.1    localhost\n%d.%d.%d.%d    eaglehost\n",
				192, 168, 247, 1 );		// assume eagle host ip
			fclose( fhd );
		}
		res = 1 ;
	}

	close( fd );
    
    return res;
}

