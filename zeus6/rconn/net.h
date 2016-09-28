/*
   header file for net.cpp
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>

#ifndef __NET_H__
#define __NET_H__

struct sockad {
    union {
        struct sockaddr saddr ;
        struct sockaddr_in saddr_in ;
        struct sockaddr_in6 saddr_in6 ;
        struct sockaddr_un saddr_un;
    } s ;
    socklen_t len;
};

#ifndef uint8
#define uint8 uint8_t
#endif

#ifndef uint16
#define uint16 uint16_t
#endif

#ifndef uint32
#define uint32 uint32_t
#endif


#define SADDR(ad) ((struct sockaddr *)&(ad))

#define SET_ADDRLEN(ad) ( ad.len = sizeof(ad.s) )

// ( uint16 )
#define SOCK_FAMILY(ad) (((struct sockaddr *)(&(ad)))->sa_family) 

// ( uint16 )
#define IP4_FAMILY(ad)  (((struct sockaddr_in *)(&(ad)))->sin_family) 

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
int net_addr(struct sockad *addr, const char *netname=NULL, int port=80, int family=AF_INET );
char * net_name(struct sockad *sad, char * host, int hostlen );
int net_port( struct sockad *sad );
int net_bind(int socket, int port, char * host=NULL);
int net_connect( const char * host, int port=80 );
int net_listen( int port);
unsigned int net_myip(struct sockad *addr=NULL);
int net_available(int s);
int net_sendto(int s, void * packet, int psize, struct sockad * sad);
int net_recvfrom(int s, void * packet, int psize, struct sockad * sad);

int net_send(int s, void * packet, int psize );
int net_sendx(int s, void * packet, int psize );
int net_sendall( int s, char * data, int dsize );
int net_recv(int s, void * packet, int psize );
int net_recvx(int s, void * packet, int psize );

#endif  // __NET_H__
