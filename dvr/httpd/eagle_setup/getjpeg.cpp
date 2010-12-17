
//#include "../../cfg.h"
#include "../../dvrsvr/dvr.h"

// #define NETDBG

#ifdef NETDBG

// debugging procedures

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
        return -1;
    }
    addr->addrlen = res->ai_addrlen;
    memcpy(&addr->addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return 0;
}

int msgfd ;

// send out UDP message use msgfd
int net_sendmsg( char * dest, int port, const void * msg, int msgsize )
{
    struct sockad destaddr ;
	if( msgfd==0 ) {
		msgfd = socket( AF_INET, SOCK_DGRAM, 0 ) ;
	}
    net_addr(dest, port, &destaddr);
    return (int)sendto( msgfd, msg, (size_t)msgsize, 0, &(destaddr.addr), destaddr.addrlen );
}

void net_dprint( char * fmt, ... ) 
{
    char msg[1024] ;
    va_list ap ;
    va_start( ap, fmt );
    vsprintf(msg, fmt, ap );
    net_sendmsg( "192.168.152.61", 15333, msg, strlen(msg) );
    va_end( ap );
}
#endif

// wait for socket ready to read (timeout in micro-seconds)
int net_recvok(int fd, int tout)
{
    struct timeval timeout ;
    timeout.tv_sec = tout/1000000 ;
    timeout.tv_usec = tout%1000000 ;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(fd, &fds);
    } else {
        return 0;
    }
}

int net_connect(const char *netname, int port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    int sockfd;
    char service[20];
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    sprintf(service, "%d", port);
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
            close(sockfd);
            sockfd = -1;
        }
        res = res->ai_next;
    }
    
    freeaddrinfo(ressave);
    
    return sockfd;
}

// send all data
// return 
//       0: failed
//       other: success
int net_send(int sockfd, void * data, int datasize)
{
    int s ;
    char * cbuf=(char *)data ;
    while( (s = send(sockfd, cbuf, datasize, 0))>=0 ) {
        datasize-=s ;
        if( datasize==0 ) {
            return 1;			// success
        }
        cbuf+=s ;
    }
    return 0;
}

// receive all data
// return 
//       0: failed (time out)
//       other: success
int net_recv(int sockfd, void * data, int datasize)
{
    int r ;
    char * cbuf=(char *)data ;
    while( net_recvok(sockfd, 20000000) ) {
        r = recv(sockfd, cbuf, datasize, 0);
        if( r<=0 ) {
            break;				// error
        }
        datasize-=r ;
        if( datasize==0 ) {
            return 1;			// success
        }
        cbuf+=r ;
    }
    return 0;
}

//  function from getquery
int decode(const char * in, char * out, int osize );
char * getquery( const char * qname );

void dvr_getjpeg()
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    int sockfd ;
    int s, r ;
    FILE * portfile ;
    int port ;
    char * qv ;
    int ch, jpeg_quality, jpeg_pic ;
    char buf[2000] ;
    
    port=15112 ;
    portfile = fopen("dvrsvr_port", "r");
    if( portfile ) {
        fscanf(portfile, "%d", &port);
        fclose(portfile);
    }
	
    sockfd = net_connect("127.0.0.1", port);
    if( sockfd>0 ) {

        ch = 0 ;
        qv = getquery("channel") ;
        if( qv ) {
            sscanf(qv, "%d", &ch);
        }

        jpeg_quality = 1  ;        //0-best, 1-better, 2-average
        qv = getquery("quality");
        if( qv ) {
            if( strcasecmp( qv, "best" )==0 ) {
                jpeg_quality=0 ;
            }
            else if( strcasecmp( qv, "better" )==0 ) {
                jpeg_quality=1 ;
            }
            else {
                jpeg_quality=2 ;
            }
        }

        jpeg_pic = 2  ;           // 0-cif , 1-qcif, 2-4cif
        qv = getquery("picture");
        if( qv ) {
            if( strcasecmp( qv, "cif" )==0 ) {
                jpeg_pic=0 ;
            }
            else if( strcasecmp( qv, "qcif" )==0 ) {
                jpeg_pic=1 ;
            }
            else {
                jpeg_pic=2 ;
            }
        }
        
        
        req.reqcode=REQ2GETJPEG ;
        req.data=(ch&0xff)|( (jpeg_quality&0xff)<<8)|((jpeg_pic&0xff)<<16) ;
        req.reqsize=0 ;
        if( net_send(sockfd, &req, sizeof(req)) ) {
            if( net_recv(sockfd, &ans, sizeof(ans))) {
                if( ans.anscode==ANS2JPEG && ans.anssize>0 ) {
                    s=ans.anssize ;
                    // output data
                    while( s>0 && (r=recv(sockfd, buf, sizeof(buf),0) )>0 ){
                        fwrite( buf, 1, r, stdout);
                        s-=r ;
                    }
                }
            }
        }
        close( sockfd );
    }
}

// return 0: for keep alive
int main()
{
#ifdef NETDBG
	net_dprint( "dvrstatus started.\n"); 
#endif

    // print headers
    printf( "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nCache-Control: no-store\r\n\r\n" );
    fflush(stdout);
    dvr_getjpeg();

#ifdef NETDBG
	net_dprint( "dvrstatus ended.\n"); 
#endif
		
	return 1;
}

