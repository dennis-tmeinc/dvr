
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

#ifndef PACKED
#define PACKED  __attribute__((__packed__))
#endif

#define HOSTIDSIZE	32

#define INFO_SHOW        1

struct eagleip_message {	
	uint32_t	msgsize;		// sizeof eagleip_message
	uint32_t	msgcode; 		// 1: setip, 2: slave ip, 3: reboot, 4: wrong ip (duplicated ip)
	uint32_t	ipaddr ;
	uint32_t	eagleid ;		// board #, 0: host, 1,2,3... for slave board
	uint8_t		hostid[HOSTIDSIZE] ;	// mac id for now
} ;

struct eagleip_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct eagleip_message data;
} PACKED;

#define HOST_PORT	(15151)
#define SLAVE_PORT	(15152)

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

// compute IP/UDP package check sum
uint16_t inet_checksum(void *addr, int count)
{
	int32_t sum = 0;
	uint16_t *source = (uint16_t *) addr;

	while (count > 1)  {
		sum += *source++;
		count -= 2;
	}

	if (count > 0) {
		uint16_t tmp = 0;
		*(uint8_t*)&tmp = *(uint8_t*)source;
		sum += tmp;
	}

	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}

	return ~sum;
}


/* Construct a ip/udp header for a packet, broadcat packet through interface "eth0" */
int send_eagleip_packet(struct eagleip_message * msg, uint32_t source_ip, int source_port, uint32_t dest_ip, int dest_port )
{
	int res = 0 ;
	struct sockaddr_ll dest;
	struct eagleip_packet packet;
	int fd;

	int ip_size = sizeof(packet);
	int upd_size = ip_size - sizeof(packet.ip);

	fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	if (fd < 0) {
		return 0 ;
	}

	memset(&dest, 0, sizeof(dest));
	memset(&packet, 0, sizeof(packet));
	packet.data = *msg;  	

	dest.sll_family = AF_PACKET;
	dest.sll_protocol = htons(ETH_P_IP);
	dest.sll_ifindex = if_nametoindex("eth0") ;
	dest.sll_halen = 6;
 	memset(dest.sll_addr, 0xff, 6) ;        // ether net bcast addr
	if (bind(fd, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
		close( fd ) ;
		return 0 ;
	}

	packet.ip.protocol = IPPROTO_UDP;
//	packet.ip.saddr = INADDR_ANY;			
	packet.ip.saddr = source_ip ;
	packet.ip.daddr = dest_ip ;
	packet.udp.source = htons(source_port);
	packet.udp.dest = htons(dest_port);
	/* size, excluding IP header: */
	packet.udp.len = htons(upd_size);
	/* for UDP checksumming, ip.len is set to UDP packet len */
	packet.ip.tot_len = packet.udp.len;
	packet.udp.check = inet_checksum(&packet, ip_size);
	/* but for sending, it is set to IP packet len */
	packet.ip.tot_len = htons(ip_size);
	packet.ip.ihl = sizeof(packet.ip) >> 2;
	packet.ip.version = IPVERSION;
	packet.ip.ttl = IPDEFTTL;
	packet.ip.check = inet_checksum(&packet.ip, sizeof(packet.ip));

	/* send ip packet */
	res = sendto(fd, &packet, ip_size, 0,
				(struct sockaddr *) &dest, sizeof(dest));
	close(fd);
	return res ;
}

int	mkrebootmsg( struct eagleip_message * sendmsg )
{
	FILE * fd ;
	memset( sendmsg, 0, sizeof( *sendmsg ) );
	sendmsg->msgsize = sizeof( struct eagleip_message ) ;
	sendmsg->msgcode = 3 ;		// reboot

	// get mac id
	fd = fopen( "/sys/class/net/eth0/address", "r" );
	if( fd ) {
		fread( sendmsg->hostid, 1, sizeof( sendmsg->hostid ), fd );
		fclose( fd );
	}
	return 1 ;
}

// make a broadcast message
int mkipmsg( struct eagleip_message * sendmsg )
{
	FILE * fd ;
	uint32_t ip ;
	unsigned int  randomdelay ;

	memset( sendmsg, 0, sizeof( *sendmsg ) );
	sendmsg->msgsize = sizeof( struct eagleip_message ) ;
	sendmsg->msgcode = 1 ;		// set ip

	// get mac id
	fd = fopen( "/sys/class/net/eth0/address", "r" );
	if( fd ) {
		fread( sendmsg->hostid, 1, sizeof( sendmsg->hostid ), fd );
		fclose( fd );
	}

	fd = fopen( "/dev/urandom", "w" );
	if( fd ) {
		fwrite( sendmsg, 1, sizeof( *sendmsg), fd ) ;		// ?
		fclose( fd );
	}

	fd = fopen( "/dev/urandom", "r" );

	if( fd ) {
		char urbuf[256] ;
		int i ;
		for( i=0; i<256; i++ ) {
			fread( urbuf, 1, 256, fd ) ;	// dummy read
		}

		fread( &randomdelay, sizeof(randomdelay), 1, fd );
		randomdelay %= 100000 ;
		usleep( randomdelay );	// delay random tens mili seconds

		// generate random ip address
		fread( &ip, sizeof(ip), 1, fd );
		fclose( fd );
	}
	else {
		ip = 0xc0010101 ;		// 192.1.1.1
	}
	ip &= 0x1ffffff8 ;			// any c class address
	ip |= 0xc0000001 ;			// host ip addr
	sendmsg->ipaddr = ip ;
	sendmsg->eagleid = 0 ;		// hostid

	return 1 ;
}

int boardnum=2 ;
uint32_t slaveip[8] ;

int sethostip(uint32_t ip)
{
	char cmd[256] ;
	sprintf( cmd, "ifconfig eth0:1 %d.%d.%d.%d",
		(ip>>24)&0xff,
		(ip>>16)&0xff,
		(ip>>8)&0xff,
		(ip)&0xff );
#if INFO_SHOW
        printf("set host ip:%d.%d.%d.%d\n",
		(ip>>24)&0xff,
		(ip>>16)&0xff,
		(ip>>8)&0xff,
		(ip)&0xff );
#endif
	slaveip[0] = ip ;
	return system( cmd );
}

int add_slave( struct eagleip_message * slavemsg)
{
	if( slavemsg->eagleid<=0 || slavemsg->eagleid>=8 ) 
		return 0 ;
	slaveip[ slavemsg->eagleid ] = slavemsg->ipaddr ;
	return 1 ;
}

int check_slave()
{
	int i;
	for( i=1; i<boardnum; i++ ) {
		if( slaveip[i]==0 ) {
			return 0;
		}
	}
	return 1 ;
}

int clear_slave()
{
	int i;
	for( i=1; i<boardnum; i++ ) {
		slaveip[i]=0 ;
	}
	return 1 ;
}

int write_hosts()
{
    int i;
	FILE * hosts ;
	hosts = fopen( "/etc/hosts", "w" );
	if( hosts ) {
		fprintf( hosts, "# hosts generated by ipcamd.\n127.0.0.1    localhost\n" );
        	for( i=1; i<boardnum ; i++ ) {
			if( slaveip[i] == 0 ) {
				slaveip[i] = slaveip[0]+i ;
			}
			fprintf( hosts, "%d.%d.%d.%d	ipcam%d\n", 
				(slaveip[i]>>24)&0xff,
				(slaveip[i]>>16)&0xff,
				(slaveip[i]>>8)&0xff,
				(slaveip[i])&0xff,
				i );
#if INFO_SHOW
                      	printf("%d.%d.%d.%d	ipcam%d\n", 
				(slaveip[i]>>24)&0xff,
				(slaveip[i]>>16)&0xff,
				(slaveip[i]>>8)&0xff,
				(slaveip[i])&0xff,
				i );
#endif
		}
		fclose( hosts );
	}
	return 0;
}

int main(int argc, char * argv[])
{
	int i;
	int res ;
	int fd ;
	struct eagleip_message sendmsg, recvmsg ;	
	struct sockaddr_in addr;
	socklen_t fromlen ;
	int dupip = 0 ;
	
        if(argc<2)
          return;

	if( argc>1 ) {
		boardnum = atoi( argv[1] ) ;
	}
        if(boardnum==1)
           return;
        printf("board num=%d\n",boardnum);

	if( boardnum<1 ) {
		mkrebootmsg(&sendmsg);
		send_eagleip_packet( &sendmsg, 
		INADDR_ANY, HOST_PORT,  INADDR_BROADCAST, 
		SLAVE_PORT );
		return 0;
	}
	if( boardnum>8 ) {
		boardnum=8 ;
	}
	
	mkipmsg(&sendmsg);
	// set host ip
	sethostip(sendmsg.ipaddr);
	
	// receiving socket
	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(HOST_PORT);
	bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	memset( &recvmsg, 0, sizeof(recvmsg) );

	for( i=0; i<120; i++ ) { // 2 minute
#if INFO_SHOW
		printf( "Send ipcam req.\n");
#endif
		if( send_eagleip_packet( &sendmsg, 
			INADDR_ANY,
			HOST_PORT, 
			INADDR_BROADCAST, 
			SLAVE_PORT )==0 ) 
			break;
		while( net_recvok( fd, 1000000 ) ) {
			fromlen = sizeof( addr );
			res = recvfrom( fd, &recvmsg, sizeof(recvmsg), 0, (struct sockaddr *)&addr, &fromlen  );
			if( res==sizeof(recvmsg) &&
				recvmsg.msgsize == sizeof( struct eagleip_message ) &&
				memcmp( recvmsg.hostid, sendmsg.hostid, sizeof( sendmsg.hostid ))==0 ) {
				if( recvmsg.msgcode == 2 ) {    
                                  // received from slave board
#if INFO_SHOW
					printf("Recv ipcam ip.\n");
#endif
					// responds from slave board
					add_slave(&recvmsg);
				}
				else if( recvmsg.msgcode == 4 ) {	
           // (duplicated ip) received from slave board belong to other host
#if INFO_SHOW
                                        printf("receive dup ip\n");
#endif
					dupip=1;
				}
			}
		}
		if( dupip ) {	// ip address conflict
			mkipmsg(&sendmsg); // generate a new host ip address
			// set host ip
			sethostip(sendmsg.ipaddr);
			clear_slave();
			dupip=0;
		}
		else if( check_slave() ) {// if all slave boards detected, 
			break;
		}
	}

    close(fd);

	// make /etc/hosts file
	write_hosts();
    
    return 0;
}
