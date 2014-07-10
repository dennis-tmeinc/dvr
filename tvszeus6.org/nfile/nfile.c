#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#define closesocket close

int net_connect(char *netname, char * service)
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *ressave;
	int sockfd;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	res = NULL;
	if (getaddrinfo(netname, service, &hints, &res) != 0) {
		return -1;
	}
	if (res == NULL) {
		return -1;
	}
	ressave = res;

	/*
	   Try open socket with each address getaddrinfo returned,
	   until getting a valid socket.
	 */
	sockfd = -1;
	while (res) {
		sockfd = socket(res->ai_family,
		res->ai_socktype, res->ai_protocol);

		if (sockfd != -1) {
			if( connect(sockfd, res->ai_addr, res->ai_addrlen)==0 ) {
				break;
			}
			closesocket(sockfd);
			sockfd = -1;
		}
		res = res->ai_next;
	}

	freeaddrinfo(ressave);

	if (sockfd == -1) {
		return -1;
	}

	return sockfd;
}


char buf[1024] ;
int main( int argc, char * argv[])
{
    int fd ;
    int r ;
    if( argc!=3 ) {
	return 1 ;
    }
    fd = net_connect( argv[1], argv[2] );
    if( fd<=0 ) {
        return 1 ;
    }
    while( (r=recv( fd, buf, 1024, 0 ))>0 ) {
        fwrite(buf, 1, r, stdout);
    }
    closesocket( fd );
    return 0;
}

