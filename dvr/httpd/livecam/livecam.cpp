
#include "../../cfg.h"
#include "../../dvrsvr/dvr.h"

// #define NETDBG

#ifdef NETDBG

// debugging procedures
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

#endif  // NETDBG

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

