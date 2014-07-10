#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#ifndef PACKED
#define PACKED  __attribute__((__packed__))
#endif

struct my_message {
    char msg[80] ;
} ;

struct my_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct my_message data;
} PACKED;


uint16_t inet_checksum(void *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 *         beginning at location "addr".
	 */
	int32_t sum = 0;
	uint16_t *source = (uint16_t *) addr;

	while (count > 1)  {
		/*  This is the inner loop */
		sum += *source++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if (count > 0) {
		/* Make sure that the left-over byte is added correctly both
		 * with little and big endian hosts */
		uint16_t tmp = 0;
		*(uint8_t*)&tmp = *(uint8_t*)source;
		sum += tmp;
	}
	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}


/* Construct a ip/udp header for a packet, send packet */
int send_raw_packet(struct my_message *payload,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port)
{
	struct sockaddr_ll dest;
	struct my_packet packet;
	int fd;
	int result = -1;
	const char *msg;

	int IP_UPD_SIZE = sizeof(packet);
	int UPD_SIZE    = IP_UPD_SIZE - sizeof(packet.ip);

	fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	if (fd < 0) {
		msg = "socket(%s)";
		goto ret_msg;
	}

	memset(&dest, 0, sizeof(dest));
	memset(&packet, 0, sizeof(packet));
	packet.data = *payload; /* struct copy */

	dest.sll_family = AF_PACKET;
	dest.sll_protocol = htons(ETH_P_IP);
	dest.sll_ifindex = if_nametoindex("eth0");
	dest.sll_halen = 6;
 	memset(dest.sll_addr, 0xff, 6) ;        // ether net bcast addr
	if (bind(fd, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
		msg = "bind(%s)";
		goto ret_close;
	}

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source_ip;
	packet.ip.daddr = dest_ip;
	packet.udp.source = htons(source_port);
	packet.udp.dest = htons(dest_port);
	/* size, excluding IP header: */
	packet.udp.len = htons(UPD_SIZE);
	/* for UDP checksumming, ip.len is set to UDP packet len */
	packet.ip.tot_len = packet.udp.len;
	packet.udp.check = inet_checksum(&packet, IP_UPD_SIZE);
	/* but for sending, it is set to IP packet len */
	packet.ip.tot_len = htons(IP_UPD_SIZE);
	packet.ip.ihl = sizeof(packet.ip) >> 2;
	packet.ip.version = IPVERSION;
	packet.ip.ttl = IPDEFTTL;
	packet.ip.check = inet_checksum(&packet.ip, sizeof(packet.ip));

	/* Currently we send full-sized DHCP packets (zero padded).
	 * If you need to change this: last byte of the packet is
	 * packet.data.options[end_option(packet.data.options)]
	 */
	result = sendto(fd, &packet, IP_UPD_SIZE, 0,
				(struct sockaddr *) &dest, sizeof(dest));
	msg = "sendto";
 ret_close:
	close(fd);
	if (result < 0) {
 ret_msg:
            printf("PACKET:%s\n", msg);
	}
	return result;
}


int main(int argc, char * argv[])
{
    struct my_message mymsg ;	
   
    if( argc>1 ) {
	strcpy( mymsg.msg, argv[1] );
    }
    else {
        strcpy( mymsg.msg, "Hello!");
    } 
    send_raw_packet(&mymsg, INADDR_ANY, 15115, INADDR_BROADCAST, 15114 );
    
    return 0;
}
