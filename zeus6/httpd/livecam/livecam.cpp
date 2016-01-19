
#include "../../cfg.h"
#include "../../dvrsvr/dvr.h"

// #define NETDBG

int net_addr(char *netname, int port, struct sockad *addr);

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

void dvr_livestream()
{
    int channel=0 ;
    char * qv ;
    int streamfd ;
    int started=0 ;

    streamfd = net_connect("127.0.0.1", 15114) ;
    if( streamfd <= 0 ) {
        return ;
    }

    qv = getenv("VAR_G_channel") ;
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
    printf( "HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\n\r\n" );

    dvr_livestream();

    return 0;
}

