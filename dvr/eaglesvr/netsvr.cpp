
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "eaglesvr.h"

int multicast_en;				// multicast enabled?
struct sockad multicast_addr;	// multicast address

static int serverfd;

#ifdef SUPPORT_UNIXSOCKET
static int unixfd;
#endif

static int net_run;
static int net_port;

static pthread_mutex_t net_mutex;
static void net_lock()
{
    pthread_mutex_lock(&net_mutex);
}

static void net_unlock()
{
    pthread_mutex_unlock(&net_mutex);
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
    if( net_run == 1 ) {
        dvrsvr *pconn;
        net_lock();
        pconn = dvrsvr::head();
        while (pconn != NULL) {
            sends += pconn->onframe(pframe);
            pconn = pconn->next();
        }
        net_unlock();
    }
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

void net_close(int sockfd)
{
    closesocket(sockfd);
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

int net_listen_unix( char * socketname )
{
    struct sockaddr_un local  ;
    int sockfd, len ;

    if( (sockfd = socket(AF_UNIX, SOCK_STREAM, 0))==-1 ) {
        return -1 ;
    }

    unlink( socketname ) ;
    local.sun_family = AF_UNIX ;
    strcpy( local.sun_path, socketname ) ;
    len = sizeof( local ) ;
    if( bind (sockfd, (struct sockaddr *)&local, len) == -1 ) {
        // can't bind
        closesocket( sockfd ) ;
        return -1 ;
    }

    if( listen( sockfd, 5)==-1 ) {
        // can't listen
        closesocket( sockfd ) ;
        return -1 ;
    }

    return sockfd ;
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

int net_wait(int timeout_ms)
{
    struct timeval timeout ;
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    int fd;
    int flag ;
    dvrsvr *pconn;
    dvrsvr *pconn_nx;

    // setup select()
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    net_lock();
    pconn = dvrsvr::head();
    while (pconn != NULL) {
        pconn_nx = pconn->next();
        if (pconn->isdown()) {
            delete pconn;
        } else {
            fd = pconn->socket() ;
            FD_SET(fd, &readfds);
            if (pconn->hasfifo()) {
                FD_SET(fd, &writefds);
            }
            FD_SET(fd, &exceptfds);
        }
        pconn = pconn_nx;
    }
    net_unlock();
    FD_SET(serverfd, &readfds);
#ifdef SUPPORT_UNIXSOCKET
    FD_SET(unixfd, &readfds);
#endif

    timeout.tv_sec = timeout_ms/1000 ;
    timeout.tv_usec = (timeout_ms%1000)*1000 ;
    if (select(FD_SETSIZE, &readfds, &writefds, &exceptfds, &timeout) > 0) {
        // check listensocket
        if (FD_ISSET(serverfd, &readfds)) {
            fd = accept(serverfd, NULL, NULL);
            if (fd > 0) {
//                flag=fcntl(fd, F_GETFL, NULL);
//                fcntl(fd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
                // turn on TCP_NODELAY, to increase performance
                flag = 1 ;
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                net_lock();
                pconn =	new dvrsvr(fd);
                net_unlock();
            }
        }
#ifdef SUPPORT_UNIXSOCKET
        else if (FD_ISSET(unixfd, &readfds)) {
            fd = accept(unixfd, NULL, NULL);
            if (fd > 0) {
                net_lock();
                pconn =	new dvrsvr(fd);
                net_unlock();
            }
        }
#endif
        else {
            pconn = dvrsvr::head();
            while ( pconn != NULL ) {
                net_lock();
                fd = pconn->socket();
                if (FD_ISSET(fd, &exceptfds)) {
                    pconn->down();
                }
                else if (FD_ISSET(fd, &writefds)) {
                    pconn->write();
                }
                else if (FD_ISSET(fd, &readfds)) {
                    pconn->read();
                }
                pconn = pconn->next();
                net_unlock();
            }
        }
        return 1 ;
    }
    return 0 ;
}


// initialize network
void net_init()
{
    // initial mutex
    pthread_mutex_init(&net_mutex, NULL);

    net_port = EAGLESVRPORT ;

#ifdef SUPPORT_UNIXSOCKET
    unixfd = net_listen_unix("/var/dvr/eaglesvr") ;
#endif

    serverfd = net_listen(net_port, SOCK_STREAM);
    if (serverfd == -1) {
        dvr_log("Can not initialize network!");
        return;
    }

    net_run = 1;

    // setup netdbg host
    /*
    netdbg_on = dvrconfig.getvalueint("debug", "dvrsvr");
    if( netdbg_on ) {
        v=dvrconfig.getvalue("debug","host");
        iv=dvrconfig.getvalueint("debug", "port");
        net_addr( v, iv, &netdbg_destaddr );
    }
*/

    dvr_log("Network initialized.");
}

void net_uninit()
{
    net_run = 0 ;
    net_lock();
    while (dvrsvr::head() != NULL) {
        delete dvrsvr::head();
    }
    net_unlock();

    closesocket(serverfd);
    serverfd=0;

#ifdef SUPPORT_UNIXSOCKET
    closesocket(unixfd);
    unixfd=0;
#endif

    dvr_log("Network uninitialized.");
    pthread_mutex_destroy(&net_mutex);
}
