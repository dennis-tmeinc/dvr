
#include "netdbg.h"

struct sockad {
    struct sockaddr addr;
    char padding[128];
    socklen_t addrlen;
};

int netdbg_on ;
static struct sockad netdbg_destaddr ;
static int netdbg_msgfd=0 ;

static int net_addr(char *netname, int port, struct sockad *addr)
{
    struct addrinfo hints;
    struct addrinfo *res;
    char service[20];
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    sprintf(service, "%d", port);
    res = NULL;
    if (getaddrinfo(netname, service, &hints, &res) != 0) {
        return -1;
    }
    addr->addrlen = res->ai_addrlen;
    memcpy(&addr->addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return 0;
}

void netdbg_init( char * ip, int port ) 
{
    net_addr(ip, port, &netdbg_destaddr);
    netdbg_msgfd=socket(AF_INET, SOCK_DGRAM, 0 );
    if( netdbg_msgfd ) {
        netdbg_on = 1;
    }
}

void netdbg_finish()
{
    if( netdbg_on ) {
        close(netdbg_msgfd);
        netdbg_on=0 ;
    }
}

static int netdbg_showapp = 0 ;

void netdbg_print( char * fmt, ... ) 
{
    char msg[1024] ;
    va_list ap ;
    int h=0 ;
    if( netdbg_on ) {
        if( netdbg_showapp ) {
            strcpy(msg,"glog:");
            h=5;
        }
        va_start( ap, fmt );
        vsprintf(&msg[h], fmt, ap );
        sendto( netdbg_msgfd, msg, strlen(msg), 0, &(netdbg_destaddr.addr), netdbg_destaddr.addrlen );
        va_end( ap );
    }
}

void netdbg_dump( void * data, int size )
{
    if( netdbg_on ) {
        int i ;
        netdbg_showapp = 0 ;
        unsigned char * cbuf = (unsigned char *)data ;
        for( i=0; i<size ; i++ ) {
            netdbg_print("%02x ", (int)cbuf[i] );
        }
        netdbg_showapp = 1 ;
    }
}
