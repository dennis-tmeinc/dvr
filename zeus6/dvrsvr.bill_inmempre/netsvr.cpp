
#include <errno.h>
#include "dvr.h"
#include <netinet/ip.h>

int multicast_en;	// multicast enabled?
struct sockad multicast_addr;	// multicast address

static int net_fifos;
static int serverfd;
static int tdserverfd;
int msgfd;
static struct sockad loopback;
static pthread_t net_threadid;
static pthread_t td_threadid;

static int net_run;
static int net_port;
static int net_multicast;
int     net_livefifosize=100000 ;
static int noreclive=1 ;
int    net_active = 0 ;
int    net_liveview=0;
int    m_keycheck=0;

int    powerNum=0;
static int  tdserver_port=46500;
static string td_serverip;
// trigger a new network wait cycle
void net_trigger()
{
    if (net_fifos == 0) {
        // this make select() on net_thread return right away
        sendto(msgfd, "TRIG", 4, 0, &loopback.addr, loopback.addrlen);
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
#if 0    
    if(pframe->channel==0){
      struct dvrtime dvrt ;
      time_now(&dvrt);
      printf("===second:%d mini:%d\n",dvrt.second,dvrt.milliseconds);
    }
#endif    
    dvr_lock();
    pconn = dvrsvr::head();
    while (pconn != NULL) {
        sends += pconn->onframe(pframe);
        pconn = pconn->next();
    }
    dvr_unlock();   
    
    if( noreclive && sends>0 ) {
        rec_pause = 50 ;            // temperary pause recording, for 5 seconds
    }
    if(sends>0){
        net_liveview = 5;      
    }
 
    return sends;
}

// received UDP message
void net_message()
{
    struct sockad from;
    void *msgbuf;
    int msgsize;
    unsigned long req = 0;
    //msgbuf = mem_alloc(MAXMSGSIZE);	// maximum messg size should be less then 32K
    msgbuf=malloc(MAXMSGSIZE);
    from.addrlen = sizeof(from) - sizeof(from.addrlen);
   // printf("check message-----");
    msgsize =
        recvfrom(msgfd, msgbuf, MAXMSGSIZE, 0, &(from.addr),
                 &(from.addrlen));
   // printf("msgsize:%d\n",msgsize);
    if ( msgsize >= (int)sizeof(unsigned long) ) {
        req = *(unsigned long *) msgbuf;
	//printf("req:%x\n",req);
    }
    if (req == REQDVREX) {
      //  printf("request DVREX\n");
        req = DVRSVREX;
        sendto(msgfd, &req, sizeof(req), 0, &(from.addr), from.addrlen);
    } 
    else if (req == REQSHUTDOWN) {
       // printf("request quit\n");
        app_state = APPQUIT;
    }
    else if (req == REQDVRTIME) {
       // printf("request dvrtime\n");
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
    else if (req == REQMSG ) {
       // printf("request msg\n");
        msg_onmsg(msgbuf, msgsize, &from);
    }
   // mem_free(msgbuf);
   free(msgbuf);
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
       // dvr_log("Error:netsvr:net_connect!");
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

int tcp_recv(int sockfd, void * data, int datasize)
{
    int r ;
    char * cbuf=(char *)data ;
   // printf("inside net_recv\n");
    while( net_recvok(sockfd, 5000000) ) {
        r = recv(sockfd, cbuf, datasize, 0);
        if( r<=0 ) {
            return -1;				// error
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

int DefaultReq(int sockfd)
{
    struct dvr_ans ans ;
    ans.anscode = ANSERROR;
    ans.anssize = 0;
    ans.data = 0;
    if(net_send(sockfd, &ans, sizeof(ans)))
      return 1;
    return 0;
}

int GetSystemSetup(int sockfd)
{
    struct dvr_ans ans ;
    struct system_stru sys ;
    if( dvr_getsystemsetup(&sys) ) {
      
        ans.anscode = ANSSYSTEMSETUP;
        ans.anssize = sizeof( struct system_stru );
        ans.data = 0;
	if(net_send(sockfd, &ans, sizeof(ans))){
	    if(net_send(sockfd, &sys, sizeof(sys))){
	       return 1; 
	    }	    
	} 
	return 0; 
    }
    else {
        dvr_log("dvr_getsystemsetup failed");
        if(DefaultReq (sockfd))
	  return 1;
	return 0;
    }
}

int GetVersion(int sockfd)
{
    int version ;
    struct dvr_ans ans;
    ans.anscode = ANSGETVERSION;
    ans.anssize = 4*sizeof(int);
    ans.data = 0;
    if(net_send(sockfd, &ans, sizeof(ans))){
      version = VERSION0 ;
      if(net_send(sockfd,&version, sizeof(version))){
	version = VERSION1 ;
	if(net_send(sockfd,&version, sizeof(version))){
	    version = VERSION2 ;
	    if(net_send(sockfd,&version, sizeof(version))){
		version = VERSION3 ;
		if(net_send(sockfd,&version, sizeof(version)))
		  return 1;	    
	    }
	}
      }
    }
    return 0;
}

int GetChannelSetup(int sockfd, int channel)
{
    struct DvrChannel_attr dattr ;
    struct dvr_ans ans ;
    if( dvr_getchannelsetup(channel, &dattr, sizeof(dattr)) ) {
        ans.anscode = ANSCHANNELSETUP;
        ans.anssize = sizeof( struct DvrChannel_attr ); //-sizeof(int)*4;
        ans.data = channel;
	//dvr_log("inside GetChannelSetup:channel:%d anscode:%d size:%d",ans.data,ans.anscode,ans.anssize);
        if(net_send(sockfd, &ans, sizeof(ans))){
           if(net_send(sockfd, &dattr, sizeof(dattr))) {   //-sizeof(int)*4)){	   
	     return 1;
	  }
	}
	return 0;
    }
    else {
        dvr_log("dvr_getchannelsetup failed");
        if(DefaultReq(sockfd))
	  return 1;
	return 0;
    }
}

int Req2GetLocalTime(int sockfd)
{
    struct dvr_ans ans ;
    struct dvrtime dvrt ;
    try{
	time_now(&dvrt);
      // printf("re-second:%d mini:%d\n",dvrt.second,dvrt.milliseconds); 
	ans.anscode = ANS2TIME;
	ans.anssize = sizeof( struct dvrtime );
	ans.data = 0 ;
	if(net_send(sockfd,&ans, sizeof(ans))){
	   if(net_send(sockfd, &dvrt, sizeof(dvrt)))
	     return 1;
	}
    }
    catch(...){
      printf("error in Req2GetLocalTime\n");
    }
    return 0;
}
int ReqRelayLive(int sockfd,int sessionId)
{
    struct dvr_ans ans ;
    ans.anscode=REQRELAYLIVE;
    ans.data=sessionId;
    ans.anssize=0;
    if(net_send(sockfd,&ans, sizeof(ans)))
      return 1;
    return 0;
};

int ReqRelayPlayback(int sockfd,int sessionId)
{
    struct dvr_ans ans ;
    ans.anscode=REQRELAYPB;
    ans.data=sessionId;
    ans.anssize=0;
    if(net_send(sockfd,&ans, sizeof(ans)))
      return 1;
    return 0;
};

int ReqCheckKey(int sockfd,char* pData,int keylen)
{
    struct key_data * key ;
    struct dvr_ans ans ;
    if( pData && keylen>=(int)sizeof( struct key_data ) ) {

        key = (struct key_data *)pData ;
        // check key block ;
        if( strcmp( g_mfid, key->manufacturerid )==0 &&
            memcmp( g_filekey, key->videokey, 256 )==0 ) 
        {
	    //dvr_log("m_keycheck is ok");
            m_keycheck=1;
            ans.data = 0;
            ans.anssize=0;
            ans.anscode = ANSOK ;
	    net_send(sockfd,&ans, sizeof(ans));

            // do connection log
            dvr_logkey( 1, key ) ;
           
            return 1;
        }
    }
    DefaultReq(sockfd);
    m_keycheck=0;
    return 0;
}

void SendOpenRelayLive(int sessionId)
{
     char tdserver_ip[128];
     int fd;
     int i;
     int flag ;
     dvrsvr *pconn; 
     strncpy( tdserver_ip, td_serverip.getstring(), 127 );
     for(i=0;i<5;++i){
	fd=net_connect(tdserver_ip,tdserver_port);     
	if(fd>0){
	      flag=fcntl(fd, F_GETFL, NULL);
	      fcntl(fd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
	      flag = 1 ;
	      // turn on TCP_NODELAY, to increase performance
	      setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	      flag = 1 ;
	      setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));
	      flag = 3; //10 ;
	      setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *) &flag, sizeof(int));
	      flag = 30; //60 ;
	      setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (char *) &flag, sizeof(int));
	      flag = 10;//60 ;
	      setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *) &flag, sizeof(int));	 
	      struct dvr_ans ans ;
	      ans.anscode=OPENRELAYLIVE;
	      ans.data=sessionId;
	      ans.anssize=0;
	      if(net_send(fd,&ans, sizeof(ans))){ 
		  dvr_lock();
		  pconn =  new dvrsvr(fd);
		  if(m_keycheck)
		    pconn->setkeycheck();
		  dvr_unlock();
		  break;
	      } else {
		 close(fd);
		 fd=-1;
	      }
	} else {
	  dvr_log("Inside SendOpenRelayLive:connect TD server failed"); 
	}
	sleep(1);
     }
};

void SendOpenRelayPlayback(int sessionId)
{
    char tdserver_ip[128];
     int fd;
     int flag ;
     int i;
     dvrsvr *pconn; 
     strncpy( tdserver_ip, td_serverip.getstring(), 127 );
     for(i=0;i<5;++i){
	fd=net_connect(tdserver_ip,tdserver_port);
	if(fd>0){
	      flag=fcntl(fd, F_GETFL, NULL);
	      fcntl(fd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
	      flag = 1 ;
	      // turn on TCP_NODELAY, to increase performance
	      setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	      flag = 1 ;
	      setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));
	      flag = 3; //10 ;
	      setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *) &flag, sizeof(int));
	      flag = 30; //60 ;
	      setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (char *) &flag, sizeof(int));
	      flag = 10;//60 ;
	      setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *) &flag, sizeof(int));	 
	      struct dvr_ans ans ;
	      ans.anscode=OPENRELAYPB;
	      ans.data=sessionId;
	      ans.anssize=0;
	      if(net_send(fd,&ans, sizeof(ans))){  
		  dvr_lock();
		  pconn =  new dvrsvr(fd);
		  if(m_keycheck)
		    pconn->setkeycheck();
		  dvr_unlock();		  
		  break;
	      } else {
		 close(fd);
		 fd=-1;
	      }
	}  else {
	  dvr_log("Inside SendOPenRelayPlayback:connect TD server failed");        
	}
	sleep(1);
     }
};

void *td_thread(void *param)
{
    struct timeval timeout = { 0, 0 };  
    char tdserver_ip[128];
    char phonenumber[128];
    char *pData=NULL;
    int size_recv;
    int flag ; 
    int ret;
    fd_set readfds; 
    fd_set exceptfds;
    FILE* fp=NULL;
    struct timeval time1, time2,time3 ;
    struct timeval keychecktime;
    int timediff=0;
    tdserverfd=-1;
    struct message_header message_pack;
    strncpy( tdserver_ip, td_serverip.getstring(), 127 );
    dvr_log("inside td_thread");
    gettimeofday(&time3, NULL); 
    gettimeofday(&time1, NULL); 
    gettimeofday(&keychecktime, NULL); 
    while(net_run==1){
	  gettimeofday(&time2, NULL);
	  timediff=time2.tv_sec-time1.tv_sec;      
          if(timediff>20){
	    break; 
	  }
	  sleep(4);
    }   
    fp=fopen("/var/dvr/modeminfo","r"); 
    if(fp){
	fscanf(fp,"%s\n",phonenumber);
	// fgets(phonenumber,128,fp);
	fclose(fp);
	//dvr_log("phone number:%s",phonenumber);
	//tdserverfd= net_connect("192.168.1.145",tdserver_port);
	tdserverfd= net_connect(tdserver_ip,tdserver_port);
	if(tdserverfd>0){	
  
	    flag=fcntl(tdserverfd, F_GETFL, NULL);
	    fcntl(tdserverfd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
		    
	    flag = 1 ;
	    setsockopt(tdserverfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    
	    flag = 1 ;
	    setsockopt(tdserverfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));			      
	    flag = 3; //10 ;
	    setsockopt(tdserverfd, SOL_TCP, TCP_KEEPCNT, (char *) &flag, sizeof(int));
	    flag = 30; //60 ;
	    setsockopt(tdserverfd, SOL_TCP, TCP_KEEPIDLE, (char *) &flag, sizeof(int));
	    flag = 10;//60 ;
	    setsockopt(tdserverfd, SOL_TCP, TCP_KEEPINTVL, (char *) &flag, sizeof(int));		      
	}	     		  
    } else {
	dvr_log("no modeminfo file"); 
    }      
    while(net_run==1){
	  if(tdserverfd<0){
	      sleep(3);
	      //start to connect TD server
	      fp=fopen("/var/dvr/modeminfo","r"); 
	      if(fp){
		  fscanf(fp,"%s\n",phonenumber);
		  fclose(fp);
		 // dvr_log("phone number:%s",phonenumber);
		  //tdserverfd= net_connect("192.168.1.145",tdserver_port);
		  tdserverfd= net_connect(tdserver_ip,tdserver_port);
		  if(tdserverfd>0){	
	    
		      flag=fcntl(tdserverfd, F_GETFL, NULL);
		      fcntl(tdserverfd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
			      
		      flag = 1 ;
		      setsockopt(tdserverfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	      
		      flag = 1 ;
		      setsockopt(tdserverfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));			      
		      flag = 3; //10 ;
		      setsockopt(tdserverfd, SOL_TCP, TCP_KEEPCNT, (char *) &flag, sizeof(int));
		      flag = 30; //60 ;
		      setsockopt(tdserverfd, SOL_TCP, TCP_KEEPIDLE, (char *) &flag, sizeof(int));
		      flag = 10;//60 ;
		      setsockopt(tdserverfd, SOL_TCP, TCP_KEEPINTVL, (char *) &flag, sizeof(int));		      
		  }	     		  
	      } 
 
              gettimeofday(&time1, NULL); 	      
	      while(net_run==1&&tdserverfd<0){
		  gettimeofday(&time2, NULL);
		  timediff=time2.tv_sec-time1.tv_sec;
		  if(timediff<120){
		    sleep(10);
		    continue; 
		  }
		  gettimeofday(&time1, NULL); 
		  fp=fopen("/var/dvr/modeminfo","r"); 
		  if(fp){
		      fscanf(fp,"%s\n",phonenumber);
		      fclose(fp);
		     // dvr_log("phone number:%s",phonenumber);
		  } else {
		     // dvr_log("no modeminfo file");
		      continue; 
		  }
		 // tdserverfd= net_connect("192.168.1.106",tdserver_port);
		  //tdserverfd= net_connect("192.168.1.145",tdserver_port);
		  tdserverfd= net_connect(tdserver_ip,tdserver_port);
		  if(tdserverfd>0){
		    
		    
		      flag=fcntl(tdserverfd, F_GETFL, NULL);
                      fcntl(tdserverfd, F_SETFL, flag|O_NONBLOCK);		// work on non-block mode
		      
		      flag = 1 ;
		      setsockopt(tdserverfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
		      
		      flag = 1 ;
		      setsockopt(tdserverfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));	      
		      flag = 3; //10 ;
		      setsockopt(tdserverfd, SOL_TCP, TCP_KEEPCNT, (char *) &flag, sizeof(int));
		      flag = 30; //60 ;
		      setsockopt(tdserverfd, SOL_TCP, TCP_KEEPIDLE, (char *) &flag, sizeof(int));
		      flag = 10;//60 ;
		      setsockopt(tdserverfd, SOL_TCP, TCP_KEEPINTVL, (char *) &flag, sizeof(int));			      
		      break; 
		  }
	      }
	  }	
	  dvr_log("dvr start send 450 command to TD");
	  //send 450 command for boot up
	  while(tdserverfd>0&&net_run==1){
	      memset(&message_pack,0,sizeof(struct message_header));
	      message_pack.command=REQRELAYOPEN;
	      message_pack.data=100;
	      message_pack.ext_data_size=strlen(phonenumber);
	  //    dvr_log("ext size:%d",message_pack.ext_data_size);
	      if(net_send(tdserverfd, &message_pack, sizeof(struct message_header))){
		      if(net_send(tdserverfd, &phonenumber,strlen(phonenumber))){
			  if(net_recv(tdserverfd,&message_pack,sizeof(struct message_header))){
			      if(message_pack.command==REQRELAYOPEN){
				  dvr_log("REQRELAYOPEN ack received:%d %d %d",message_pack.command,message_pack.data,message_pack.ext_data_size);
				  break;
			      }
			  } else {
			    continue;
			  }
		      } else {
			  close(tdserverfd);
			  tdserverfd=-1;			
		      }		
	      } else {
		  close(tdserverfd);
		  tdserverfd=-1;
	      }
	  } //while(tdserverfd>0)
	  if(tdserverfd<0){
	    continue;
	  }   
	  	
	  dvr_log("dvr td session start to receive message");
	  gettimeofday(&time1, NULL); 
	  while(tdserverfd>0 &&net_run==1){	
	      memset(&message_pack,0,sizeof(struct message_header));
	      ret=tcp_recv(tdserverfd,&message_pack,sizeof(struct message_header));
	      if(ret>0){
		   // dvr_log("TD message:%d %d %d received",message_pack.command,message_pack.data,message_pack.ext_data_size);		
		    if(message_pack.ext_data_size>0){	
		        pData=(char *) malloc(message_pack.ext_data_size);
			net_recv(tdserverfd,pData,message_pack.ext_data_size);
		    }
		    switch(message_pack.command){
		      case REQGETSYSTEMSETUP:
			//dvr_log("REQGETSYSTEMSETUP");
			if(!GetSystemSetup(tdserverfd)){
			  close(tdserverfd);
			  tdserverfd=-1;
			}
			break;
		      case REQGETVERSION:
			//dvr_log("REQGETVERSION received");
			if(!GetVersion(tdserverfd)){
			  close(tdserverfd);
			  tdserverfd=-1;			  
			}
			break;
		      case REQGETCHANNELSETUP:
			//dvr_log("REQGETCHANNELSETUP received");
			if(!GetChannelSetup(tdserverfd, message_pack.data)){
			  close(tdserverfd);
			  tdserverfd=-1;			  
			}
			break;
		      case REQ2GETLOCALTIME:
			//dvr_log("REQ2GETLOCALTIME received");
			if(!Req2GetLocalTime(tdserverfd)){
			  close(tdserverfd);
			  tdserverfd=-1;			  
			}
			break;
		      case REQRELAYLIVE:
			//dvr_log("REQRELAYLIVE received");
			if(ReqRelayLive(tdserverfd,message_pack.data)){
			    SendOpenRelayLive(message_pack.data);
			} else {
			   close(tdserverfd);
			   tdserverfd=-1;
			}

			break;
		      case REQRELAYPB:
			//dvr_log("REQRELAYPB received");
			if(ReqRelayPlayback(tdserverfd,message_pack.data)){
			   SendOpenRelayPlayback(message_pack.data);
			} else {
			  close(tdserverfd);
			  tdserverfd=-1;
			}

			break;
		      case REQCHECKKEY:
			//dvr_log("REQCHECKKEY received");
			if(!ReqCheckKey(tdserverfd,pData,message_pack.ext_data_size)){
			   close(tdserverfd);
			   tdserverfd=-1;			  
			}
			gettimeofday(&keychecktime, NULL); 
			break;
		      default:
			if(!DefaultReq(tdserverfd)){
			  close(tdserverfd);
			  tdserverfd=-1;
			}
			break;					
		    }	
		    if(pData){
		      free(pData);
		      pData=NULL;
		    }
	      } else if(ret<0){
		    close(tdserverfd);
		    tdserverfd=-1;
		    dvr_log("TCP socket was closed");
	      }
	      gettimeofday(&time2, NULL); 
	      timediff=time2.tv_sec-time1.tv_sec;
	      if(timediff>300){
		  gettimeofday(&time1, NULL);		
		  memset(&message_pack,0,sizeof(struct message_header));
		  message_pack.command=REQKEEPALIVE;
		  message_pack.data=100;
		  message_pack.ext_data_size=0;
		  if(tdserverfd>0){
		      if(!net_send(tdserverfd, &message_pack, sizeof(struct message_header))){
			close(tdserverfd);
			tdserverfd=-1;
			dvr_log("REQKEEPALIVE sent to TD failed");
		      }
		  }
	      } //if(timediff>600){
	      timediff=time2.tv_sec-time3.tv_sec;
	      if(timediff>30){
		  gettimeofday(&time3, NULL);
		  if(tdserverfd>0){
		      fp=fopen("/var/dvr/modeminfo","r"); 
		      if(fp){
			fclose(fp); 
		      } else {
			close(tdserverfd); 
			tdserverfd=-1;
			dvr_log("modem is not available");
		      }
		  }	 
	      }
	      timediff=time2.tv_sec-keychecktime.tv_sec;
	      if(timediff>10){
		m_keycheck=0;
	      }
	  }	  
    } //while(net_run==1)
    if(tdserverfd>0){
	close(tdserverfd); 
	tdserverfd=-1;   
	dvr_log("TD connect is closed");
    }
   
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
    int  mpowerstart=0;
    struct timeval tv;
    
    while (net_run == 1) {		// running?
        gettimeofday(&tv,NULL);
        if(powerNum>0){
          //turn the hard driver power on
          turndiskon_power=1;
          if(tv.tv_sec-mpowerstart>0){
             powerNum--;
             mpowerstart=tv.tv_sec;
          }
         // dvr_log("powerNum:%d",powerNum);
        } else {
           if(turndiskon_power==1){
              //turn hard driver power off
              turndiskon_power=2;
           }
           mpowerstart=tv.tv_sec;
        }       
        msg_clean();			// clearn dvr_msg

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
        FD_SET(msgfd, &readfds);
        
        timeout.tv_sec = 0;		// 2 second time out
        timeout.tv_usec = 30000 ;
        
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
                    flag = 1 ;
                    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));
                    flag = 3; //10 ;
                    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (char *) &flag, sizeof(int));
                    flag = 30; //60 ;
                    setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (char *) &flag, sizeof(int));
                    flag = 10;//60 ;
                    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (char *) &flag, sizeof(int));
		    
                    dvr_lock();
                    pconn =	new dvrsvr(fd);
                    dvr_unlock();
                    continue ;
                }
            }

            if (FD_ISSET(msgfd, &readfds)) {
                net_message();
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
    config dvrconfig(dvrconfigfile);
    net_run = 0;				// assume not running
    net_port = dvrconfig.getvalueint("network", "port");
    if (net_port == 0)
        net_port = DVRPORT;
    net_multicast = dvrconfig.getvalueint("network", "multicast");
    
    net_livefifosize=dvrconfig.getvalueint("network", "livefifo");
    if( net_livefifosize<10000 ) {
        net_livefifosize=10000 ;
    }
    if( net_livefifosize>10000000 ) {
        net_livefifosize=10000000 ;
    }
    
    noreclive =  dvrconfig.getvalueint("system", "noreclive");    
    //add the td server;
    td_serverip=dvrconfig.getvalue("network", "td_url");
    
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
    
    net_addr(NULL, net_port, &loopback);	// get loopback addr
    tdserverfd=-1;
    multicast_en = 0;			// default no multicast
    net_addr("228.229.210.211", 15113, &multicast_addr);
    // fixed multicast address for now
    net_run = 1;
    pthread_create(&net_threadid, NULL, net_thread, NULL);
    pthread_create(&td_threadid, NULL, td_thread, NULL);    
    dvr_log("Network initialized.");
}

void net_uninit()
{
    if( net_run!=0) {
        net_run = 0;				// stop net_thread.
        net_trigger();
        pthread_join(net_threadid, NULL);
	pthread_join(td_threadid,NULL);
        closesocket(serverfd);
        closesocket(msgfd);
	if(tdserverfd>0){
	      close(tdserverfd); 
	      tdserverfd=-1;     
	}	
        dvr_log("Network uninitialized.");
    }
}
