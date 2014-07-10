
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "../cfg.h"
#include "../dvrsvr/dvr.h"

int net_addr(char *netname, int port, struct sockad *addr)
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
        return 0;
    }
    addr->addrlen = res->ai_addrlen;
    memcpy(&addr->addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return 1;
}

// send dvrsvr a message use DGRAM
// argv1: message
// argv2: port, default 15111
int main( int argc, char * argv[])
{
	struct sockad sad ;
	int port = DVRPORT ;
	char * data = "ofid" ;
	
	if( argc>2 ) {
		port = atoi( argv[2] ) ;
	}
	
	if( argc>1 ) {
		data = argv[1] ;
	}
	
	if( net_addr( "127.0.0.1", port, &sad ) ) {
		int s = socket( AF_INET, SOCK_DGRAM, 0 ) ;
		sendto( s, data, strlen(data), 0, &sad.addr, sad.addrlen );
		close( s );
	}
	return 0 ;
}
