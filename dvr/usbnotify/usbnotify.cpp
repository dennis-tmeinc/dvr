
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include "../dvrsvr/dvr.h"

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
    while( net_recvok(sockfd, 5000000) ) {
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

/* available environment varialbes:

    USBROOT : usb disk device mounted directory, etc: /dvr/var/xdisk/sda1
    EXEDIR  : autorun executable path, etc: /dvr/var/xdisk/sda1/510
*/

int main()
{
    struct usbkey_plugin_st {
        char device[8] ;       // "sda", "sdb" etc.
        char mountpoint[128] ;
    } key_plugin ;
    
    // get usb key device name from environment $USBROOT
    char * usbroot ;
    char * p ;
    usbroot = getenv( "USBROOT" );
    if( usbroot == NULL ) {
        //printf( "No $USBROOT!\n");
        return 0 ;
    }
    p = strrchr( usbroot, '/' );
    if( p ) {
        key_plugin.device[0]= *(p+1);
        key_plugin.device[1]= *(p+2);
        key_plugin.device[2]= *(p+3);
        key_plugin.device[3]= 0;
        strncpy( key_plugin.mountpoint, usbroot, 128 );

        int dvrsocket = net_connect("127.0.0.1", 15111);
        if( dvrsocket>0 ) {
            struct dvr_req req ;
            struct dvr_ans ans ;
            req.reqcode=REQUSBKEYPLUGIN ;
            req.data=0 ;
            req.reqsize=sizeof(key_plugin) ;
            if( net_send(dvrsocket, &req, sizeof(req)) ) {
                net_send(dvrsocket, &key_plugin, sizeof(key_plugin));
                if( net_recv(dvrsocket, &ans, sizeof(ans))) {
                    if( ans.anscode==ANSOK ) {
                        printf("Key event sent!\n");
                    }
                }
            }
            close( dvrsocket );
        }
    }
    return 0 ;
}
