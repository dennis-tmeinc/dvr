
#include <errno.h>
#include "dvr.h"
#include <netinet/ip.h>
#include <netinet/in.h>

static int serverfd;
static int msgfd;
static struct sockad loopback;
static pthread_t net_threadid;
static int net_run;
static int net_port;
static string net_multicast_addr ;
static int noreclive=1 ;
static int net_fifos = 0;

int    net_active = 0 ;
int    powerNum = 0;

static pthread_mutex_t net_mutex;
void net_lock()
{
    pthread_mutex_lock(&net_mutex);
}

void net_unlock()
{
    pthread_mutex_unlock(&net_mutex);
}

// trigger network thread jump out of select()
void net_trigger()
{
	// this make select() on net_thread return right away
	net_lock();
	if( net_fifos == 0 )
		sendto(msgfd, "T", 1, 0, &loopback.addr, loopback.addrlen);
	net_unlock();
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
    
    net_lock();
    pconn = dvrsvr::head;
    while (pconn != NULL) {
        sends += pconn->onframe(pframe) ;
        pconn = pconn->m_next ;
    }
    net_unlock();
    
    if( sends>0 ) {
		if( noreclive ) {
			rec_pause = 50 ;            // temperary pause recording, for 5 seconds
		}
		net_trigger();
	}
    return sends;
}

// received UDP message
void net_message()
{
    struct sockad from;
    int msgsize;
    unsigned long req ;
	
    from.addrlen = sizeof(from) - sizeof(from.addrlen);
    msgsize = recvfrom(msgfd, &req, sizeof(req), 0, &(from.addr), &(from.addrlen));
	if ( msgsize >= (int)sizeof(req) ) {
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
		else if( req == 'OHCE' ) {		// echo
			sendto(msgfd, &req, sizeof(req), 0, &(from.addr), from.addrlen);
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

// return peer ip address, for tvs key check fix (ply266.dll)
int net_peer(int sockfd)
{
	struct sockad peeraddr ;
	peeraddr.addrlen = sizeof(peeraddr)-sizeof(peeraddr.addrlen);
	getpeername( sockfd, &(peeraddr.addr), &(peeraddr.addrlen) );
	return ((sockaddr_in *)&peeraddr.addr)->sin_addr.s_addr ;	
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
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    sprintf(service, "%d", port);
    res = NULL;
    if (getaddrinfo(netname, service, &hints, &res) != 0) {
        dvr_log("getaddrinfo error:netname:$s,service:%s",netname,service);
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
        dvr_log("Error:netsvr:net_connect!");
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
   // printf("inside net_recv\n");
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

// join a multicast group
int net_join( int fd, char * maddr )
{
    struct ip_mreqn mreq;
	memset( &mreq, 0, sizeof(mreq));
	
    mreq.imr_multiaddr.s_addr=inet_addr(maddr);
    mreq.imr_address.s_addr=htonl(INADDR_ANY);
    
    if (setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		perror("IP_ADD_MEMBERSHIP");
	    return 0 ;
    }
    return 1 ; 
}

// drop a multicast group
int net_drop( int fd, char * maddr )
{
    struct ip_mreqn mreq;
	memset( &mreq, 0, sizeof(mreq));
	
    mreq.imr_multiaddr.s_addr=inet_addr(maddr);
    mreq.imr_address.s_addr=htonl(INADDR_ANY);

    if (setsockopt(fd,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		perror("IP_DROP_MEMBERSHIP");
	    return 0 ;
    }
    return 1 ; 
}

void *net_thread(void *param)
{
    struct timeval timeout ;
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    int fd;
    int flag ;
    dvrsvr *pconn;
    dvrsvr *pconn1;
    
    net_lock();
    while ( net_run ) {		// running?
        // setup select()
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        
        FD_SET(serverfd, &readfds);
        FD_SET(msgfd, &readfds);        
        
        pconn1 = NULL ;					// prev conn
        pconn = dvrsvr::head ;
		while ( pconn != NULL ) {
			if (pconn->isclose()) {
				if( pconn == dvrsvr::head ) {
					dvrsvr::head = pconn->m_next ;
					delete pconn ;
					pconn = dvrsvr::head ;					
				}
				else {
					pconn1->m_next = pconn->m_next ;
					delete pconn ;
					pconn = pconn1->m_next ;
				}
			} 
			else {
				if (pconn->hasfifo()) {
					FD_SET(pconn->socket(), &writefds);
					net_fifos ++ ;
				}
				else {
					// do read until output buffer is clean
					FD_SET(pconn->socket(), &readfds);
				}
				FD_SET(pconn->socket(), &exceptfds);
				pconn1 = pconn ;
				pconn = pconn1->m_next ;
			}
		}

		timeout.tv_sec = 10;
        timeout.tv_usec = 0 ;
                
		net_unlock();
		flag = select(FD_SETSIZE, &readfds, &writefds, &exceptfds, &timeout );
		net_lock();
		net_fifos = 0 ;
		
		if( flag > 0 ) {
			
            if (FD_ISSET(msgfd, &readfds)) {
                net_message();
            }
            pconn = dvrsvr::head ;
            while( net_run == 1 && pconn != NULL ) {
				fd = pconn->socket();
				if( fd>0 ) {
					if (FD_ISSET(fd, &exceptfds)) {
						pconn->close();
					}
					else if(FD_ISSET(fd, &writefds)) {
						pconn->write() ;
					}
					else if (FD_ISSET(fd, &readfds)) {
						pconn->read() ;
					}
				}
				pconn = pconn->m_next ;
            }

            // check new connection
            if (FD_ISSET(serverfd, &readfds)) {
                fd = accept(serverfd, NULL, NULL);
                if (fd > 0) {
                    
                    flag=fcntl(fd, F_GETFL, NULL);
                    fcntl(fd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
                    
                    flag = 1 ;
                    // turn on TCP_NODELAY, to increase performance
                    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                    pconn =	new dvrsvr(fd);

					// add to connection list
					pconn->m_next = dvrsvr::head  ;
					dvrsvr::head = pconn ;
                }
            }
            
            net_active=5 ;            		// 5 seconds for active delay
        }
        else {		// time out , or error!
			if( net_active > 0 ) {
				net_active-- ;
			}
			else {
				while( dvrsvr::head != NULL ) {
					pconn = dvrsvr::head->m_next ;
					delete dvrsvr::head ;
					dvrsvr::head = pconn ;
				}			
			
#if defined (TVS_APP) || defined (PWII_APP)
				dvr_logkey( 0, NULL );
#endif
			}

		}

    }
    
    while ( dvrsvr::head != NULL) {
		pconn = dvrsvr::head->m_next ;
        delete dvrsvr::head ;
        dvrsvr::head = pconn ;
	}

	net_unlock();
	
    return NULL;
}

// initialize network
void net_init()
{
	// init network mutex
	net_mutex=mutex_init ;
	
    config dvrconfig(dvrconfigfile);
    net_run = 0;				// assume not running
    net_port = dvrconfig.getvalueint("network", "port");
    if (net_port == 0)
        net_port = DVRPORT;
    
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

	// enable multicast
    net_multicast_addr = dvrconfig.getvalue("network", "multicast_addr");
    if( net_multicast_addr.isempty() ) {
		net_multicast_addr = "228.229.230.231" ;
	}
	net_join( msgfd, (char *)net_multicast_addr );
    
    // get localhost loopback addr
    net_addr("127.0.0.1", net_port, &loopback);	
    
    pthread_attr_t attr;
    size_t stacksize = 0 ;
    pthread_attr_init(&attr);
    
    net_run = 1;
    pthread_create(&net_threadid, &attr, net_thread, NULL);
    
    dvr_log("Network initialized.");
}

void net_uninit()
{
    if( net_run!=0) {
        net_run = 0;				// stop net_thread.
        net_trigger();
        pthread_join(net_threadid, NULL);
        net_threadid = NULL ;
        closesocket(serverfd);
        net_drop( msgfd, (char *)net_multicast_addr );
        closesocket(msgfd);
        dvr_log("Network uninitialized.");
    }
    pthread_mutex_destroy(&net_mutex);    
}
