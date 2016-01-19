/*
 * sender.c -- multicasts "hello, world!" to a multicast group once a second
 *
 * Antony Courtney,	25/11/94
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <netdb.h>



#define HELLO_PORT 15114
//#define HELLO_GROUP "225.0.0.37"
#define HELLO_GROUP "228.229.230.231"

// ready for receive
int net_rrdy( int s, int usdelay)
{
	struct pollfd fds;
	fds.fd = s ;
	fds.events = POLLIN ;
	fds.revents = 0 ;
	return poll( &fds, 1, usdelay/1000 );
}

void main(int argc, char *argv[])
{
     struct sockaddr_in addr;
     struct sockaddr_in raddr;
     socklen_t rlen ;
     int fd, cnt;
     struct ip_mreq mreq;
     char *message="ECHO Hello, World!";
     char buf[256] ;
     int r ;

     /* create what looks like an ordinary UDP socket */
     if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
	  perror("socket");
	  exit(1);
     }

     /* set up destination address */
     memset(&addr,0,sizeof(addr));
     addr.sin_family=AF_INET;
     addr.sin_addr.s_addr=inet_addr(HELLO_GROUP);
     addr.sin_port=htons(HELLO_PORT);
     
     /* now just sendto() our destination! */
     while (1) {
	  if (sendto(fd,message,strlen(message)+1,0,(struct sockaddr *) &addr,
		     sizeof(addr)) < 0) {
	       perror("sendto");
	       exit(1);
	  }
	  if( net_rrdy( fd, 1000000 ) ) {
		  rlen = sizeof(raddr) ;
		  r=recvfrom(fd, buf, 255, 0, (struct sockaddr *)&raddr, &rlen );
		  if( r>0 ) {
			 
			char host[256] ;
			  getnameinfo( (struct sockaddr *)&raddr, rlen, host, 255, NULL, 0, NI_NUMERICHOST );
				  
			  buf[r]=0 ;
			  printf("RECV: %s \nFrom: %s\n", buf, host );
		  }
		  sleep(1);
		}
     }
}

