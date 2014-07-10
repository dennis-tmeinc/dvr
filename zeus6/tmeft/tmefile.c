
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <unistd.h>
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

char *filename ;

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
			return 0 ;
		}
		totalsend+=sendsize ;
	}
	return totalsend ;
}

// return 0 : send file, 1: recv file, -1: error
int send_cmd( int fd, char cmd, char * filename )
{
    int rcmd ;
    char cmdblk[512] ;

    if( cmd=='p' || cmd=='P' || cmd=='W' || cmd=='w' )
	    rcmd = 0 ;
	else if( cmd=='g' || cmd=='G' || cmd=='r' || cmd=='W' )
	    rcmd = 1 ;
	else 
        return -1 ;

    memset( cmdblk, 0, sizeof(cmdblk) );
    sprintf( cmdblk, "%d %s", rcmd, filename);
	if( send_all( fd, cmdblk, sizeof(cmdblk) )==sizeof(cmdblk) ) {
        return rcmd ;
    }
    else {
        return -1 ;
    }
}

// return 0 : send file, 1: null file, -1: error
int recv_cmd( int fd )
{
    int rcmd ;
    char cmdblk[512] ;

    rcmd=-1 ;
    if( recv_all( fd, cmdblk, sizeof(cmdblk) )==sizeof(cmdblk) ) {
        sscanf( cmdblk, "%d", &rcmd );
        return rcmd ;
    }
    else {
        return -1 ;
    }
}

int recv_file( int sockfd, char * filename )
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
            printf( "Error at recv_file.\n");
            break;
		}
		if( recvsize==0 ) {
		    printf("\nFile finished!\n");
			break;
		}
		fwrite( fbuf, 1, recvsize, fd );
		totalrecv+=recvsize ;
		printf("\r %d bytes received.", totalrecv );
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

int connect_server( char * server, int port )
{
    struct addrinfo hints, *res, *ressave;
    int sockfd;
    char service[20] ;
	
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = PF_UNSPEC ;
    hints.ai_socktype = SOCK_STREAM ;

	res = NULL ;
	sprintf( service, "%d", port );
    if( getaddrinfo(server, service, &hints, &res)!=0 ) {
		fprintf(stderr, "getaddrinfo error!\n");
		return -1 ;
    }
    if( res==NULL ) {
	fprintf(stderr, "getaddrinfo error!\n");
	return -1 ;
    }

    ressave = res;

    sockfd=-1;
    while (res) {
        sockfd = socket(res->ai_family,
                        res->ai_socktype,
                        res->ai_protocol);

        if ( sockfd != -1 ) {
            if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
                break;

            close(sockfd);
            sockfd=-1;
        }
        res=res->ai_next;
    }

    freeaddrinfo(ressave);

    return sockfd;
}

int main(int argc, char *argv[])
{
    int sockfd;  
 	int port ;
	int cmd ;

    if (argc < 4) {
        printf( "usage: %s p|g filename host [port]\n", argv[0]);
		return 1 ;
    }
	
	filename=argv[1] ;
	
	if( argc>4 ) {
	    port=atoi( argv[4] );
	}
	else {
		port=FILEPORT ;
	}

#ifdef WIN32
    winsock_init();
#endif 

	sockfd = connect_server( argv[3], port );
	if( sockfd == -1 ) {
		fprintf( stderr, "Connect error!\n");
		return 1 ;
	}
	
	cmd = send_cmd( sockfd, *argv[1], argv[2] );
	
	if( cmd==0 ) {
		send_file( sockfd, argv[2] );
	}
	else if( cmd==1) {
        if( recv_cmd( sockfd )==0 ) {
	        recv_file( sockfd, argv[2] );
        }
	}
	else {
	    printf("Wrong command!\n");
		closesocket(sockfd);
		return 2 ;
	}
	closesocket(sockfd);
    
#ifdef WIN32
	WSACleanup();
#endif
    return 0;
}

