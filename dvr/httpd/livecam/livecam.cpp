
#include "../../cfg.h"
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

// open live stream from remote dvr
int dvr_openlive(int sockfd, int channel)
{
    struct dvr_req req ;
    struct dvr_ans ans ;

    req.reqcode=REQOPENLIVE ;
    req.data=channel ;
    req.reqsize=0;
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSSTREAMOPEN ) {
            if( ans.anssize>0 ) {
                char * buf = new char [ans.anssize] ;
                net_recv(sockfd, buf, ans.anssize );
                delete buf ;
            }
            return 1 ;
        }
    }
    return 0 ;
}

char * getquery( const char * qname );

void dvr_livestream()
{
    int channel=0 ;
    char * qv ;
    int streamfd ;
    int started=0 ;

    streamfd = net_connect("127.0.0.1", 15159) ;
    if( streamfd <= 0 ) {
        return ;
    }

    qv = getquery("channel") ;
    if( qv ) {
        sscanf(qv, "%d", &channel);
    }

    if( dvr_openlive (streamfd, channel)<=0 ) {
        close( streamfd ) ;
        return ;
    }

    while( net_recvok(streamfd, 5000000)>0 ) {

        struct dvr_ans ans ;
        if( net_recv (streamfd, &ans, sizeof(struct dvr_ans))>0 ) {
            if( ans.anscode == ANSSTREAMDATA ) {
                char framedata[ans.anssize];      // dynamic array
                if( net_recv( streamfd, framedata, ans.anssize )>0 ) {
                    if( ans.data == FRAMETYPE_KEYVIDEO )
                    {
                        fwrite(framedata, 1, ans.anssize, stdout);
                        fflush(stdout);
                        started = 1 ;           // make key frame the first frame to send out
                    }
                    else if( started &&
                             ( ans.data == FRAMETYPE_VIDEO || ans.data == FRAMETYPE_AUDIO ) )
                    {
                        fwrite(framedata, 1, ans.anssize, stdout);
                    }
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }
        else {
            break;
        }
    }

    close(streamfd);
}

// return 0: for keep alive
int main()
{
    // print headers
    printf( "HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nCache-Control: no-store\r\n\r\n" );

    dvr_livestream();
		
    return 0;
}

