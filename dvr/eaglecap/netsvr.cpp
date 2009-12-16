
#include <errno.h>
#include "dvr.h"
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <net/if.h>

int multicast_en;				// multicast enabled?
struct sockad multicast_addr;	// multicast address

static int net_fifos;
static int serverfd;
static struct sockad loopback;
static pthread_t net_threadid;
static int net_run;
static int net_port;
int     net_livefifosize=100000 ;
int    net_active = 0 ;

// trigger a new network wait cycle
void net_trigger()
{
    if( net_threadid ) {
        pthread_kill( SIGPOLL, net_threadid );
    }
}

// wait for socket ready to send (timeout in micro-seconds)
int net_sendok(int fd, int tout)
{
    struct timeval timeout ;
    timeout.tv_sec = tout/1000000 ;
    timeout.tv_usec = tout%1000000 ;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd + 1, NULL, &fds, NULL, &timeout) > 0) {
        return FD_ISSET(fd, &fds);
    } else {
        return 0;
    }
}

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

int net_onframe(cap_frame * pframe)
{
    int sends = 0;
    dvrsvr *pconn;
    
    // see if we can use multicast
    //    if( multicast_en ) {
    //        sendto( ) ;
    //    }
    
    dvr_lock();
    pconn = dvrsvr::head();
    while (pconn != NULL) {
        sends += pconn->onframe(pframe);
        pconn = pconn->next();
    }
    dvr_unlock();
    return sends;
}

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
        dvr_log("Error:netsvr:net_addr!");
        return -1;
    }
    addr->addrlen = res->ai_addrlen;
    memcpy(&addr->addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return 0;
}

int net_connect(char *netname, int port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    int sockfd;
    int sockflags ;
    int conn_res ;
    char service[20];
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    sprintf(service, "%d", port);
    res = NULL;
    if (getaddrinfo(netname, service, &hints, &res) != 0) {
//        dvr_log("Error:netsvr:net_connect!");
        return -1;
    }
    if (res == NULL) {
//        dvr_log("Error:netsvr:net_connect!");
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
            // Set non-blocking 
            sockflags = fcntl( sockfd, F_GETFL, NULL) ;
            fcntl(sockfd, F_SETFL, sockflags|O_NONBLOCK ) ;
            conn_res = connect(sockfd, res->ai_addr, res->ai_addrlen) ;
            if( conn_res==0 ) {
                fcntl( sockfd, F_SETFL, sockflags );
                break;
            }
            else if( conn_res < 0 && errno == EINPROGRESS) { 
                // waiting 1.5 seconds
                if( net_sendok( sockfd, 1500000 )>0 ) {
                    fcntl( sockfd, F_SETFL, sockflags );
                    break;
                }
            }

            closesocket(sockfd);
            sockfd = -1;
           
        }
        res = res->ai_next;
    }
    
    freeaddrinfo(ressave);
    
    if (sockfd == -1) {
//        dvr_log("Error:netsvr:net_connect!");
        return -1;
    }
    
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

// clean recv buffer
void net_clean(int sockfd)
{
    char buf[2048] ;
    while( net_recvok(sockfd, 0) ) {
        if( recv(sockfd, buf, 2048, 0)<=0 ) {
            break;				// error
        }
    }
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

int net_listen(int port, int socktype)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    int sockfd;
    int val;
    char service[20];
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = socktype;
    
    sprintf(service, "%d", port);
    
    res = NULL;
    if (getaddrinfo(NULL, service, &hints, &res) != 0) {
        dvr_log("Error:netsvr:net_listen!");
        return -1;
    }
    if (res == NULL) {
        dvr_log("Error:netsvr:net_listen!");
        return -1;
    }
    ressave = res;
    
    /*
     Try open socket with each address getaddrinfo returned,
     until getting a valid listening socket.
     */
    sockfd = -1;
    while (res) {
        sockfd = socket(res->ai_family,
                        res->ai_socktype, res->ai_protocol);
        
        if (sockfd != -1) {
            val = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val,
                       sizeof(val));
            if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0)
                break;
            closesocket(sockfd);
            sockfd = -1;
        }
        res = res->ai_next;
    }
    
    freeaddrinfo(ressave);
    
    if (sockfd == -1) {
        dvr_log("Error:netsvr:net_listen!");
        return -1;
    }
    if (socktype == SOCK_STREAM) {
        listen(sockfd, 10);
    }
    
    return sockfd;
}

void *net_thread(void *param)
{
    struct timeval timeout = { 0, 0 };
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    int fd;
    int fifos;					// total sockets with fifo pending
    int flag ;
    dvrsvr *pconn;
    dvrsvr *pconn1;
    
    time_t t1, t2 ;
    int tdiff ;
    
    while (net_run == 1) {		// running?
        
        // setup select()
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        
        dvr_lock();
        fifos = 0;
        pconn = dvrsvr::head();
        while (pconn != NULL) {
            pconn1 = pconn->next();
            if (pconn->isclose()) {
                delete pconn;
            } else {
                FD_SET(pconn->socket(), &readfds);
                if (pconn->isfifo()) {
                    FD_SET(pconn->socket(), &writefds);
                    fifos++;
                }
                FD_SET(pconn->socket(), &exceptfds);
            }
            pconn = pconn1;
        }
        net_fifos = fifos;
        dvr_unlock();

        FD_SET(serverfd, &readfds);
        
        timeout.tv_sec = 2;		// 2 second time out
        timeout.tv_usec = 0 ;
        
        if (select(FD_SETSIZE, &readfds, &writefds, &exceptfds, &timeout) > 0) {
            net_fifos = 1 ;
            // check listensocket
            if (FD_ISSET(serverfd, &readfds)) {
                fd = accept(serverfd, NULL, NULL);
                if (fd > 0) {
                    flag=fcntl(fd, F_GETFL, NULL);
                    fcntl(fd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
                    flag = 1 ;
                    // turn on TCP_NODELAY, to increase performance
                    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                    dvr_lock();
                    pconn =	new dvrsvr(fd);
                    dvr_unlock();
                    continue ;
                }
            }
            net_active=5 ;

            t1=time(NULL);
            
            pconn = dvrsvr::head();
            while ( pconn != NULL ) {
                fd = pconn->socket();
                if( fd>0 ) {
                    if (FD_ISSET(fd, &exceptfds)) {
                        pconn->close();
                    }
                    else {
                        if (FD_ISSET(fd, &readfds)) {
                            dvr_lock();

                    // turn on TCP_NODELAY, to increase performance
//                            flag=1 ;
//                    setsockopt(fd, IPPROTO_TCP, TCP_CORK, (char *) &flag, sizeof(int));

                            while( pconn->read() ) ;

//                            flag=0 ;
//                    setsockopt(fd, IPPROTO_TCP, TCP_CORK, (char *) &flag, sizeof(int));

//                            flag=1 ;
                            // turn on TCP_NODELAY, to increase performance
//                    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                            
                            dvr_unlock ();
                        }
                        if (pconn->isfifo()) {
                            dvr_lock();
                            while( pconn->write() ) ;
                            dvr_unlock ();
                        }
                    }
                }
                pconn = pconn->next();
            }
            
            t2=time(NULL);
            tdiff=(int)t2-(int)t1 ;
            if( tdiff>1 ) {
                tdiff=0 ;
            }
        }
    }
    
    while (dvrsvr::head() != NULL)
        delete dvrsvr::head();
 
    return NULL;
}

// initialize network
void net_init()
{
    net_run = 0;				// assume not running
    net_port = 15121 ;

    net_livefifosize=10000000 ;
    
    serverfd = net_listen(net_port, SOCK_STREAM);
    if (serverfd == -1) {
        return;
    }
    
    net_addr(NULL, net_port, &loopback);	// get loopback addr

    net_run = 1;
    pthread_create(&net_threadid, NULL, net_thread, NULL);
}

void net_uninit()
{
    if( net_run!=0) {
        net_run = 0;				// stop net_thread.
        net_trigger();
        pthread_join(net_threadid, NULL);
        net_threadid = 0 ;
        closesocket(serverfd);
    }
}
