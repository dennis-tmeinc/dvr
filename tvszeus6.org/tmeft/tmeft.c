
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define FILEPORT 3456



#ifdef WIN32
void winsock_init()
{
	WSADATA wsaData ;
	WSAStartup( MAKEWORD(2,2), &wsaData );
}
#else 
#define closesocket close
#endif 

int recv_all( int fd, char * buf, int bufsize )
{
    int recvsize ;
	int totalrecv=0 ;
	while( totalrecv<bufsize ) {
	    recvsize = recv( fd, &buf[totalrecv], bufsize-totalrecv, 0 );
		if( recvsize < 0 ) {
			printf("Error at recv_all.\n");
			return -1 ;
		}
		else if( recvsize==0 ) {
			printf("Connection closed!\n");
		    break;
		}
		totalrecv+=recvsize ;
	}
	return totalrecv ;
}

int send_all( int fd, char * buf, int bufsize )
{
    int sendsize ;
	int totalsend=0 ;
	while( totalsend<bufsize ) {
	    sendsize = send( fd, &buf[totalsend], bufsize-totalsend, 0 );
		if( sendsize < 0 ) {
                    printf("Error at send_all.\n");
			return -1 ;
		}
		else if( sendsize==0 ) {
		    printf("Connection closed!\n");
		    break;
		}
		totalsend+=sendsize ;
	}
	return totalsend ;
}

int receive_cmd( int fd, char * filename )
{
    int recvsize ;
    char cmdblk[512] ;
    int  rcmd ;
	recvsize=recv_all( fd, cmdblk, sizeof(cmdblk) );
	if( recvsize<=0 ) {
//	    perror( "receive" );
		return -1 ;
	}
    rcmd=-1 ;
    sscanf( cmdblk, "%d %s", &rcmd, filename );
    return rcmd ;
}

int send_cmd( int fd, int cmd, char * filename )
{
    int sendsize ;
    char cmdblk[512] ;
    memset( cmdblk, 0, sizeof(cmdblk));
    sprintf( cmdblk, "%d %s", cmd, filename);
    return send_all(fd, cmdblk, sizeof(cmdblk) );
}

int receive_file( int sockfd, char * filename )
{
    int totalrecv ;
	int recvsize ;
    char fbuf[1024] ;
	FILE * fd ;
	
	fd = fopen( filename, "wb" );
	if( fd==NULL ) {
	    printf("File error : %s\n", filename );
		return 2 ;
	}

	totalrecv = 0 ;

	printf("Receiving file : %s \n", filename );
	
	for(;;) {
		recvsize=recv( sockfd, fbuf, sizeof(fbuf), 0 );
		if( recvsize<0 ) {
		    perror( "receive" );
			break;
		}
		if( recvsize==0 ) {
		    printf(" File finished!\n");
			break;
		}
		fwrite( fbuf, 1, recvsize, fd );
		totalrecv+=recvsize ;
		printf("\r %d bytes", totalrecv );
	}
	fclose( fd );
	return totalrecv ;
}

int send_file( int sockfd, char * filename )
{
    int totalsend ;
	int sendsize ;
    char fbuf[1024] ;
	FILE * fd ;

	fd = fopen( filename, "rb" );
	if( fd==NULL ) {
	    printf("File error : %s\n", filename );
		return 2 ;
	}

	totalsend = 0 ;

	printf("Sending file : %s \n", filename );
	
	for(;;) {
	    sendsize = fread( fbuf, 1, sizeof(fbuf), fd );
		if( sendsize<=0 ) {
		    printf(" Done!\n");
		    break;
		}
		send_all( sockfd, fbuf, sendsize );
		totalsend+=sendsize ;
		printf("\r %d bytes ", totalsend );
	}
	fclose( fd );
	return totalsend ;
}

int listen_server( int port ) 
{
	struct addrinfo hints, *res, *ressave;
	int n, sockfd;
	char service[20] ;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags    = AI_PASSIVE ;
    hints.ai_family   = PF_UNSPEC ;
    hints.ai_socktype = SOCK_STREAM ;
	
	sprintf(service, "%d", port);

	res = NULL ;
    if ( getaddrinfo(NULL, service, &hints, &res) != 0 ) {
	    fprintf(stderr, "getaddrinfo error!\n");
		return -1 ;
    }
	if( res == NULL ) {
	    fprintf(stderr, "getaddrinfo error!\n");
		return -1 ;
	}
    ressave=res;

    /*
		Try open socket with each address getaddrinfo returned,
		until getting a valid listening socket.
	*/
    sockfd=-1;
    while (res) {
        sockfd = socket(res->ai_family,
                        res->ai_socktype,
                        res->ai_protocol);

        if ( sockfd != -1 ) {
            if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0)
                break;

            close(sockfd);
            sockfd=-1;
        }
        res = res->ai_next;
    }
	
	freeaddrinfo( ressave );

    if (sockfd == -1) {
        fprintf(stderr,
                "socket error:: could not open socket\n");
        return -1;
    }
    listen(sockfd, 5);

    return sockfd;
}

struct sockad {
	int addrsize ;
	struct sockaddr addr ;
	char padding[128] ;
} ;

char * getnamestr(struct sockad * addr)
{
    static char host[1024] ;
    if( getnameinfo(& addr->addr, addr->addrsize, host, 1024, NULL, 0, 0) !=0 ) {
		return NULL ;
	}
	else 
		return host ;
}

void tmeft( int so ) {
    int status ;
	int cmd ;
    char filename[128]="rcvfile.dat" ;

    if( fork()==0 ) {
		// receiving command
		cmd = receive_cmd( so, filename );
	
		if( cmd==0 ) {	//  receiving file
			receive_file( so, filename ) ;
		}
		else if( cmd==1 ) { // send file
            send_cmd( so, 0, filename);
			send_file( so, filename );
		}

        closesocket( so );
        exit(0);
    }
    while( waitpid((pid_t)-1, &status, WNOHANG ) > 0 ) ; // to release any zoombie children 
    closesocket( so );
}
		

int main(int argc, char * argv[])
{
    int sockfd, newfd ;
	int port ;
    struct sockad client_addr ;

#ifdef WIN32
    winsock_init();
#endif
	
	port = FILEPORT ;
	if( argc>1 ) {
	    port = atoi( argv[1] );
	}

	sockfd = listen_server( port );
	
	if( sockfd == -1 ) {
	    return 1 ;
	}
	
	for(;;) {
		client_addr.addrsize = sizeof( client_addr.addr ) ;
		newfd = accept( sockfd, &client_addr.addr, (socklen_t *)&client_addr.addrsize );
		if( newfd == -1 ) {
			perror( "accept" );
			closesocket( sockfd );
			return 1 ;
		}
		printf("server: got connection from %s\n", getnamestr(&client_addr));
        
        tmeft( newfd ) ;
	}
	
	closesocket( sockfd );

#ifdef WIN32
	WSACleanup();
#endif
	return 0;
}
