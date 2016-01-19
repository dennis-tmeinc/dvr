
#include <errno.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <net/if.h>

#include "dvr.h"

static int serverfd;
int msgfd;
static struct sockad loopback;
static pthread_t net_threadid;
static int net_run;
static int net_port;
// for debugging (net_dprint)
static int netdbg_on ;
static struct sockad netdbg_destaddr ;

int    net_livefifosize=100000 ;
static int noreclive=1 ;
int    net_active = 0 ;
int    net_activetime ;

static pthread_mutex_t net_mutex;
static void net_lock()
{
    pthread_mutex_lock(&net_mutex);
}

static void net_unlock()
{
    pthread_mutex_unlock(&net_mutex);
}

// trigger a new network wait cycle
void net_trigger()
{
    sendto(msgfd, "", 1, 0, &(loopback.addr), loopback.addrlen);
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
    fd_set fdsr;
    FD_ZERO(&fdsr);
    FD_SET(fd, &fdsr);
    if (select(fd + 1, &fdsr, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(fd, &fdsr);
    } else {
        return 0;
    }
}

int net_onframe(cap_frame * pframe)
{
    int sends = 0;
    dvrsvr *pconn;

    if( net_run == 1 ) {
        net_lock();
        pconn = dvrsvr::head();
        while (pconn != NULL) {
            sends += pconn->onframe(pframe);
            pconn = pconn->next();
        }

        if( noreclive && sends>0 ) {
            rec_pause = 5 ;         // temperary pause recording, for 5 seconds
        }
        net_unlock();
    }
    return sends;
}

// received UDP message
void net_message()
{
    struct sockad from;
    int msgsize;
    unsigned long req ;
    static char msgbuf[MAXMSGSIZE] ;
    from.addrlen = sizeof(from) - sizeof(from.addrlen);
    msgsize = recvfrom(msgfd, msgbuf, MAXMSGSIZE, 0, &(from.addr),
                 &(from.addrlen));
    if ( msgsize >= (int)sizeof(unsigned long) ) {
        req = *(unsigned long *) msgbuf;
        if (req == REQDVREX) {
            req = DVRSVREX;
            sendto(msgfd, &req, sizeof(req), 0, &(from.addr), from.addrlen);
        }
        else if (req == REQSHUTDOWN) {
            app_state = APPQUIT;
        }
        else if (req == REQDVRTIME) {
            struct dvrtimesync dts ;
            char * tz ;
            dts.tag = DVRSVRTIME ;
            time_utctime (&dts.dvrt) ;
            tz = getenv("TZ");
            if( tz && strlen(tz)<128 ) {
                strcpy( dts.tz, tz );
            }
            else {
                dts.tz[0]=0 ;
            }
            sendto(msgfd, &dts, sizeof(dts), 0, &(from.addr), from.addrlen);
        }
        else if ( memcmp( msgbuf, "ofid", 4 )==0 ) {
			extern void dvr_usbkey_plugin() ;
			dvr_usbkey_plugin();
        }
        else if (strncmp(msgbuf, "iamserver", 9)==0) {
            // received response from a smartserver

            // detecting which interface the packet coming from.
            char          buf[1024];
            struct ifconf ifc;
            struct ifreq *ifr;
            int           nInterfaces;
            int           i;

            /* Query available interfaces. */
            memset( buf, 0, sizeof(buf));
            memset( &ifc, 0, sizeof(ifc));
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = buf;
            if(ioctl(msgfd, SIOCGIFCONF, &ifc) < 0)
            {
                return ;
            }

            /* Iterate through the list of interfaces. */
            ifr         = ifc.ifc_req;
            nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
            for(i = 0; i < nInterfaces; i++)
            {
                struct ifreq *item = &ifr[i];
                struct ifreq ifrmask ;
                ifrmask = *item ;

                // get mask address
                if(ioctl(msgfd, SIOCGIFNETMASK, &ifrmask) >= 0) {
                    if( ((((struct sockaddr_in *)&ifrmask.ifr_netmask)->sin_addr).s_addr & (((struct sockaddr_in *)&from.addr)->sin_addr).s_addr ) ==
                       ((((struct sockaddr_in *)&ifrmask.ifr_netmask)->sin_addr).s_addr & (((struct sockaddr_in *)&item->ifr_addr)->sin_addr).s_addr ) )
                    {
                        dio_smartserveron( item->ifr_name );
                        return ;
                    }
                }
            }
            dio_smartserveron("unknown");
        }
    }
}

int net_addr(char *netname, int port, struct sockad *addr)
{
    struct addrinfo hints;
    struct addrinfo *res;
    char service[20];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if(netname==NULL) {
        hints.ai_flags |= AI_PASSIVE ;
    }
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

int net_name(struct sockad *sad, char *netname, int netnamelen )
{
    return getnameinfo( &(sad->addr), sad->addrlen, netname, netnamelen, NULL, 0, NI_NUMERICHOST );
}

// change port number of sad
int net_setport(struct sockad *sad, int port)
{
    char hostnamex[64] ;
    net_name( sad, hostnamex, sizeof(hostnamex) );
    return net_addr(hostnamex, port, sad) ;
}

// compare ip address, return turn if two addr is the same (ip)
int net_cmpaddr(struct sockad *sad1, struct sockad *sad2)
{
    char hostname1[64] ;
    char hostname2[64] ;
    net_name( sad1, hostname1, sizeof(hostname1) );
    net_name( sad2, hostname2, sizeof(hostname2) );
    return strcmp( hostname1, hostname2 ) ;
}

// send out UDP message use msgfd
int net_sendmsg( char * dest, int port, const void * msg, int msgsize )
{
    struct sockad destaddr ;
    net_addr(dest, port, &destaddr);
    return (int)sendto( msgfd, msg, (size_t)msgsize, 0, &(destaddr.addr), destaddr.addrlen );
}

void net_dprint( char * fmt, ... )
{
    if( netdbg_on && msgfd>0 ) {
        char msg[1024] ;
        strcpy(msg,"dvrsvr:");
        va_list ap ;
        va_start( ap, fmt );
        vsprintf(&msg[7], fmt, ap );
        va_end( ap );
        sendto( msgfd, msg, strlen(msg), 0, &(netdbg_destaddr.addr), netdbg_destaddr.addrlen );
    }
}

// broadcast on specified interface
//    to detect smartserver
//      "rausb0", 49954, "lookingforsmartserver", 21 );
int net_broadcast(char * interface, int port, void * msg, int msgsize )
{
    // find the broadcast address of given interface
//    "rausb0", 49954, smartsvrmsg, strlen(smartsvrmsg) );
    struct ifreq ifr ;
    memset( &ifr, 0, sizeof(ifr));
    strcpy( ifr.ifr_name, interface );
    if( ioctl( msgfd, SIOCGIFBRDADDR, &ifr)>=0 ) {
        // address ready
        char brname[40] ;
        getnameinfo( &(ifr.ifr_broadaddr), sizeof(ifr.ifr_broadaddr), brname, sizeof(brname), NULL, 0, NI_NUMERICHOST );
        return net_sendmsg( brname, port, msg, msgsize );
    }
    return 0 ;
}

char net_smartserver[128] ;
int  net_smartserverport ;
static char smartserver_detection[]="lookingforsmartserver" ;

int net_detectsmartserver()
{
    // find the broadcast address of given interface
    if( net_smartserver[0]>' ' ) {
        net_sendmsg( net_smartserver,  net_smartserverport, smartserver_detection, strlen(smartserver_detection) ) ;
    }
    else {
        // "rausb0", 49954, "lookingforsmartserver", 21 );
        char          buf[1024];
        struct ifconf ifc;
        struct ifreq *ifr;
        int           nInterfaces;
        int           i;

        /* Query available interfaces. */
        memset( buf, 0, sizeof(buf));
        memset( &ifc, 0, sizeof(ifc));
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        if(ioctl(msgfd, SIOCGIFCONF, &ifc) < 0)
        {
            return 0;
        }

        /* Iterate through the list of interfaces. */
        ifr         = ifc.ifc_req;
        nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
        for(i = 0; i < nInterfaces; i++)
        {
            struct ifreq *item = &ifr[i];

            //    "rausb0", 49954, "lookingforsmartserver", 21 );
            /* Get the broadcast address (added by Eric) */
            if(ioctl(msgfd, SIOCGIFBRDADDR, item) >= 0) {
                net_sendmsg( inet_ntoa(((struct sockaddr_in *)&item->ifr_broadaddr)->sin_addr),
                            net_smartserverport, smartserver_detection, strlen(smartserver_detection) ) ;
            }
        }
    }
    return 1;
}

// connect to local unix socket
int net_connect_unix(const char *netname)
{
    int sockfd, len;
    struct sockaddr_un remote ;

    sockfd = socket( AF_UNIX, SOCK_STREAM, 0 ) ;
    if( sockfd == -1 ) {
        return -1 ;
    }

    remote.sun_family = AF_UNIX ;
    strcpy( remote.sun_path, netname ) ;
    len = sizeof( remote ) ;
    if( connect(sockfd, (struct sockaddr *)&remote, len)==-1 ) {
        closesocket(sockfd) ;
        return -1 ;
    }

    return sockfd ;
}


int net_connect(const char *netname, int port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    int sockfd;
    int sockflags ;
    int conn_res ;
    char service[20];

    if( *netname=='/') {
        return net_connect_unix( netname ) ;
    }

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
    char buf[256] ;
    while( net_recvok(sockfd, 0) ) {
        if( recv(sockfd, buf, sizeof(buf), 0)<=0 ) {
            break;				// error
        }
    }
}

// receive all data
// return
//       0: failed (time out)
//       other: success
int net_recv(int sockfd, void * data, int datasize, int ustimeout)
{
    int tr = 0 ;
    char * cbuf=(char *)data ;
    while( tr<datasize && net_recvok(sockfd, ustimeout) ) {
        int r = recv( sockfd, cbuf+tr, datasize-tr, 0 ) ;
        if( r>0 && r<=datasize ) {
            tr+= r ;
        }
        else {
            return 0 ;
        }
    }
    return tr ;
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

// join a multicast group
int net_join( int fd, char * maddr )
{
    struct ip_mreq mreq;
	memset( &mreq, 0, sizeof(mreq));
	
    mreq.imr_multiaddr.s_addr=inet_addr(maddr);
    mreq.imr_interface.s_addr=htonl(INADDR_ANY);
    if (setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		perror("setsockopt");
	    return 0 ;
    }
    return 1 ; 
}

void *net_thread(void *param)
{
    struct timeval timeout = { 0, 0 };
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    int sres ;
    int fd;
    int flag ;
    dvrsvr *pconn;
    dvrsvr *pconn1;

    net_lock();
    while (net_run == 1) {		// running?

        // setup select()
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);

        pconn = dvrsvr::head();
        while (pconn != NULL) {
            pconn1 = pconn->next();
            if (pconn->isdown()) {
                delete pconn;
            } else {
                FD_SET(pconn->socket(), &readfds);
                if (pconn->isfifo()) {
                    FD_SET(pconn->socket(), &writefds);
                }
                FD_SET(pconn->socket(), &exceptfds);
            }
            pconn = pconn1;
        }

        FD_SET(serverfd, &readfds);
        FD_SET(msgfd, &readfds);
        timeout.tv_sec = 10 ;
        timeout.tv_usec = 0 ;
        net_unlock();
        sres = select(FD_SETSIZE, &readfds, &writefds, &exceptfds, &timeout) ;
        net_lock();
        if (sres > 0) {
            net_active=1 ;
            net_activetime = g_timetick ;

            // check listensocket
            if (FD_ISSET(serverfd, &readfds)) {
                fd = accept(serverfd, NULL, NULL);
                if (fd > 0) {
                    flag=fcntl(fd, F_GETFL, NULL);
                    fcntl(fd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
                    flag = 1 ;
                    // turn on TCP_NODELAY, to increase performance
                    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

                    // turn on KEEPALIVE
                    flag = 1 ;
                    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));
                    flag = 10 ;
                    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *) &flag, sizeof(int));
                    flag = 60 ;
                    setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (char *) &flag, sizeof(int));
                    flag = 60 ;
                    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *) &flag, sizeof(int));

                    pconn =	new dvrsvr(fd);
                    continue ;
                }
            }
            if (FD_ISSET(msgfd, &readfds)) {
                net_message();
            }

            pconn = dvrsvr::head();
            while ( pconn != NULL ) {
                fd = pconn->socket();
                if( fd>0 ) {
                    if (FD_ISSET(fd, &exceptfds)) {
                        pconn->down();
                    }
                    else {
                        if (FD_ISSET(fd, &readfds)) {
                            while( pconn->read() ) ;
                        }
                        pconn->write();
                    }
                }
                pconn = pconn->next();
            }
        }
        else if( sres == 0 ) {
            // time out
            if( net_active ) {
                if( (g_timetick-net_activetime)>(15*60*1000) ) {            // 15 minutes time out for network activity
                    while (dvrsvr::head() != NULL) {
                        delete dvrsvr::head();
                    }
                }
                if( dvrsvr::head() == NULL ) {
                    net_active = 0 ;
#if defined (TVS_APP) || defined (PWII_APP)
                    dvr_logkey( 0, NULL );
#endif
                }
            }
        }
    }
    net_active = 0 ;
    while (dvrsvr::head() != NULL) {
        delete dvrsvr::head();
    }
    net_unlock();
    return NULL;
}

// initialize network
void net_init(config &dvrconfig)
{
    string v ;
    int   iv ;

    // initial mutex
    pthread_mutex_init(&net_mutex, NULL);
    net_run = 0;				// assume not running
    net_port = dvrconfig.getvalueint("network", "port");
    if (net_port == 0)
        net_port = DVRPORT;

    net_livefifosize=dvrconfig.getvalueint("network", "livefifo");
    if( net_livefifosize<10000 ) {
        net_livefifosize=10000 ;
    }
    if( net_livefifosize>10000000 ) {
        net_livefifosize=10000000 ;
    }

    noreclive =  dvrconfig.getvalueint("system", "noreclive");

    serverfd = net_listen(net_port, SOCK_STREAM);
    if (serverfd == -1) {
        dvr_log("Can not initialize network!");
        return;
    }
    
    msgfd = net_listen(net_port, SOCK_DGRAM);
    if (msgfd == -1) {
        closesocket(serverfd);
        serverfd = -1;
        return;
    }

    // make msgfd broadcast capable
    int broadcast=1 ;
    setsockopt(msgfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    if( dvrconfig.getvalueint("network", "multicast_en") ) {
        string v ;
        v = dvrconfig.getvalue("network", "multicast_addr");
        if( v.length()>0 ) {
			net_join( msgfd, (char *)v );
		}
		else {
			net_join( msgfd, "228.229.230.231" ) ;
        }
    }

    net_addr(NULL, net_port, &loopback);	// get loopback addr

    v = dvrconfig.getvalue("network", "smartserver");
    if( v.length()>0 ) {
        char * p ;
        strncpy( net_smartserver, v, sizeof(net_smartserver) );
        p = strchr( net_smartserver, ':' );
        if( p ) {
            sscanf( p+1, "%d", &net_smartserverport );
            *p=0 ;
        }
        else {
            net_smartserverport = 49954 ;
        }
    }
    else {
        net_smartserver[0]=0 ;
        net_smartserverport=49954 ;
    }

    net_active = 0 ;
    net_run = 1;
    pthread_create(&net_threadid, NULL, net_thread, NULL);

    // setup netdbg host
    netdbg_on = dvrconfig.getvalueint("debug", "dvrsvr");
    if( netdbg_on ) {
        v=dvrconfig.getvalue("debug","host");
        iv=dvrconfig.getvalueint("debug", "port");
        net_addr( v, iv, &netdbg_destaddr );
    }

    dvr_log("Network initialized.");
}

void net_uninit()
{
    net_run = 0;				// stop net_thread.
    net_trigger();
    pthread_join(net_threadid, NULL);
    net_threadid = (pthread_t)0 ;
    closesocket(serverfd);
    serverfd=0;
    closesocket(msgfd);
    msgfd=0;
    dvr_log("Network uninitialized.");
    pthread_mutex_destroy(&net_mutex);
}
