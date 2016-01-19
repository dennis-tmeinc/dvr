

/*
  BIG CLOUD
  Udp Tunnelling progrom

  net.cpp

    network interface

*/

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>

#include "net.h"

// ready for receive
int net_rrdy( int s, int usdelay)
{
	struct pollfd fds;
	fds.fd = s ;
	fds.events = POLLIN ;
	fds.revents = 0 ;
	return poll( &fds, 1, usdelay/1000 );
}

// ready for send
int net_srdy( int s, int usdelay)
{
	struct pollfd fds;
	fds.fd = s ;
	fds.events = POLLOUT ;
	fds.revents = 0 ;
	return poll( &fds, 1, usdelay/1000 );
}

// get multiple ip resolv 
//    return number of addresses
int net_maddr(const char *netname, struct sockad * saddrs, int maxaddr, int family )
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *pres;
    int    i ;
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = family ;
    hints.ai_socktype = SOCK_STREAM ;
    if( netname==NULL ) {
        hints.ai_flags = AI_PASSIVE ;
    }
    res = NULL;
    if (getaddrinfo(netname, "80", &hints, &res) != 0) {
        return 0;
    }
    pres = res ;
    for( i=0; pres && i<maxaddr; i++ ) {
		memcpy( &(saddrs[i]), pres->ai_addr, pres->ai_addrlen );
		saddrs[i].len = pres->ai_addrlen ;
		pres = pres -> ai_next ;
	}
    
    freeaddrinfo(res);
    return i;
}

// get sockad from string address
int net_addr(struct sockad *addr, const char *netname, int port, int family )
{
    struct addrinfo hints;
    struct addrinfo *res;
    char service[20] ;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family ;
    if( netname==NULL ) {
        hints.ai_flags = AI_PASSIVE ;
    }
    sprintf(service, "%u", port);
    res = NULL;
    if (getaddrinfo(netname, service, &hints, &res) == 0) {
		memcpy(addr, res->ai_addr, res->ai_addrlen);
		addr->len = res->ai_addrlen;
		freeaddrinfo(res);
        return 1;
    }
    return 0;
}

// return host name and port number
char * net_name(struct sockad *sad, char * host, int hostlen )
{
    if( getnameinfo( SADDR(*sad), sad->len, host, hostlen, NULL, 0, NI_NUMERICHOST )==0 ) {
		return host ;
    }
    return NULL;
}

int net_bind(int socket, int port, char * host)
{
    struct sockad sad ;
    net_addr(&sad, host, port);
    return bind(socket, SADDR(sad), sad.len) ;
}

int net_connect(int socket, const char * host, int port )
{
    struct sockad sad ;
    if( net_addr(&sad, host, port) ) {
        return connect(socket, SADDR(sad), sad.len) == 0 ;
    }
    return 0;
}

uint32 net_myip(struct sockad *addr)
{
    struct sockad sad ;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if( net_connect(s, "8.8.8.8", 80) ) {
        sad.len = sizeof( sad.s ) ;
        getsockname( s, SADDR(sad), &sad.len ) ;
        if( addr != NULL ) {
            *addr = sad ;
        }
    }
    close(s);
    return IP4_ADDR(sad);
}

int net_available(int s)
{
	int av = 0 ;
	ioctl(s, FIONREAD, &av);
	return av ;
}

int net_send(int s, void * packet, int psize )
{
   	return send(s, packet, psize, 0 );
}

int net_recv(int s, void * packet, int psize )
{
    return recv( s, packet, psize, 0 );
}

int net_sendto(int s, void * packet, int psize, struct sockad * sad)
{
	if( sad!=NULL ){
    	return sendto(s, packet, psize, 0, SADDR(*sad), sad->len);
    }
    else {
    	return send(s, packet, psize, 0 );
    }
}

int net_recvfrom(int s, void * packet, int psize, struct sockad * sad)
{
    if( sad!=NULL ) {
        sad->len = sizeof(sad->s) ;
        return recvfrom( s, packet, psize, 0, SADDR(*sad), &(sad->len) );
    }
    else {
        return recvfrom( s, packet, psize, 0, NULL, NULL );
    }
}
