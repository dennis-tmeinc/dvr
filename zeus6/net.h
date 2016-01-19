/*
   header file for net.cpp
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "bigcloud.h"

#ifndef __NET_H__
#define __NET_H__

struct sockad {
    union {
        struct sockaddr saddr ;
        struct sockaddr_in saddr_in ;
        struct sockaddr_in6 saddr_in6 ;
    } s ;
    socklen_t len;
};

#define SADDR(ad) ((struct sockaddr *)&ad)

// ( uint16 )
#define IP4_FAMILY(ad)  (((struct sockaddr_in *)(&(ad)))->sin_family) 
#define SOCK_FAMILY(ad) (IP4_FAMILY((ad))) 

// ( uint16 )
#define IP4_PORT(ad)    (((struct sockaddr_in *)(&(ad)))->sin_port)
// ( uint32 )
#define IP4_ADDR(ad)    (((struct sockaddr_in *)(&(ad)))->sin_addr.s_addr)

// components of ip packets
#define IP4_PROTOCOL(ip)  (*((uint8 *)(ip)+9 ))
#define IP4_SADDR(ip)  (*(uint32 *)( (uint8 *)(ip)+12 ))
#define IP4_DADDR(ip)  (*(uint32 *)( (uint8 *)(ip)+16 ))
#define IP4_SPORT(ip)  (*(uint16 *)( (uint8 *)(ip)+20 ))
#define IP4_DPORT(ip)  (*(uint16 *)( (uint8 *)(ip)+22 ))

#define TCP_FLAG(ip)  (*((uint8 *)(ip)+33 ))
#define TCP_FLAG_SYN(ip)  (TCP_FLAG(ip)&2)
#define TCP_FLAG_FIN(ip)  (TCP_FLAG(ip)&1)


int net_rrdy( int s, int usdelay=0);
int net_srdy( int s, int usdelay=0);
// return host name and port number
int net_addr(struct sockad *addr, const char *netname, int port=80, int family=AF_INET );
// get multiple ip resolv
//    return number of addresses
int net_maddr(const char *netname, struct sockad * saddrs, int naddr=1, int family = AF_INET );
char * net_name(struct sockad *sad, char * host, int hostlen );
int net_bind(int socket, int port, char * host=NULL);
int net_connect(int socket, const char * host, int port=80 ) ;
uint32 net_myip(struct sockad *addr=NULL);
int net_available(int s);
int net_sendto(int s, void * packet, int psize, struct sockad * sad);
int net_recvfrom(int s, void * packet, int psize, struct sockad * sad);

int net_send(int s, void * packet, int psize );
int net_recv(int s, void * packet, int psize );

#endif  // __NET_H__
