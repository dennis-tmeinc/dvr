
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dvr.h"

#if 0
char Dvr264Header[40] = 
{
    '\x49', '\x4d', '\x4b', '\x48', '\x01', '\x01', '\x01', '\x00',
    '\x02', '\x00', '\x01', '\x00', '\x21', '\x72', '\x01', '\x0e',
    '\x80', '\x3e', '\x00', '\x00', '\x00', '\x7d', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};
#else
char Dvr264Header[40] = 
{
    '\x46', '\x45', '\x4d', '\x54', '\x01', '\x00', '\x01', '\x00',
    '\x01', '\x00', '\x01', '\x00', '\x01', '\x00', '\x01', '\x10',
    '\x40', '\x1f', '\x00', '\x00', '\x80', '\x3e', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};
#endif

//#define OUTPUT_COMMAND

dvrsvr *dvrsvr::m_head = NULL;

dvrsvr::dvrsvr(int fd)
{
    m_sockfd = fd;
    m_fifo = NULL;
    m_recvbuf = NULL;
    m_recvlen = 0;
    m_recvloc = 0;
    m_req.reqcode = 0;
    m_req.reqsize = 0;

    m_playback = NULL ;     // playback 
    m_live = NULL ;
    m_nfilehandle = NULL ;	// new clip file handle. (to support new file copying)
    
    m_conntype = CONN_NORMAL;
    m_keycheck = 0 ;
    
    // awkward fix for ply266.dll, which don't check key for every connections, so check if there are any existing connection from same ip
    int peer = net_peer(m_sockfd);
    dvrsvr * walk ;
    walk = m_head ;
    while( walk ) {
    	 if( walk->m_keycheck && peer == net_peer(walk->m_sockfd) ) {
    	 	m_keycheck = 1 ;
    	 	break;
    	 }
    	 walk=walk->m_next ;
    }
   
    m_fifosize=0;
    m_active=1;
    mIframe=0;
    // add into svr queue
    m_next = m_head;
    m_head = this;
}

dvrsvr::~dvrsvr()
{
    dvrsvr *walk;
    this->close();
    // remove from dvr queue
    if (m_head == this) {
        m_head = m_next;
    } else {
        walk = m_head;
        while (walk->m_next != NULL) {
            if (walk->m_next == this) {
                walk->m_next = this->m_next;
                break;
            }
            walk = walk->m_next;
        }
    }
}

void dvrsvr::close()
{
    net_fifo *pfifo;
    if (m_sockfd > 0) {
        ::closesocket(m_sockfd);
        m_sockfd = -1;
    }
    while (m_fifo != NULL) {
        pfifo = m_fifo->next;
        mem_free(m_fifo->buf);
        mem_free(m_fifo);
        m_fifo = pfifo;
    }
    if( m_playback!=NULL ) {
        delete m_playback ;
        m_playback=NULL;
    }
    if( m_live!=NULL ) {
        delete m_live ;
        m_live=NULL;
    }
    if( m_nfilehandle!=NULL ) {
        fclose( m_nfilehandle );
        m_nfilehandle=NULL ;
    }
}

// return 0: no data, 1: may have more data
int dvrsvr::read()
{
    int n;
    char *preq;
    if( m_sockfd <= 0 ){
        return 0;
    }
    if (m_recvbuf == NULL) {	// receiving req struct
        preq = (char *) &m_req;
        if (m_recvlen == 0) {
            m_recvlen = sizeof(m_req);
            m_recvloc = 0;
        }
        n = ::recv(m_sockfd, &preq[m_recvloc], m_recvlen - m_recvloc, 0);
        if (n == 0) {			// error or connection closed by peer
            close();
	        return 0 ;
        }
	else if( n<0 ) {
		if( errno!=EWOULDBLOCK ) {
			close();
		}
		return 0 ;
	}
        m_recvloc += n;
        if (m_recvloc < m_recvlen)
	        return 1 ;
        
        m_recvlen = 0;
        if (m_req.reqsize > 0x100000 || m_req.reqsize < 0) {	// invalid req
            m_req.reqcode = 0;
            m_req.reqsize = 0;
	        return 0 ;
        }
        else if (m_req.reqsize>0 ){	// wait for extra data
            m_recvbuf = (char *) malloc(m_req.reqsize);
            if (m_recvbuf == NULL) {
                close();		// no enough memory
                return 0 ;
            }
            else {
                m_recvlen = m_req.reqsize;
                m_recvloc = 0;
                return 1;
            }
        }
    } else {
        n = ::recv(m_sockfd, &m_recvbuf[m_recvloc], m_recvlen - m_recvloc, 0);
		if (n == 0) {			// connection closed by peer
		    close();
	        return 0 ;
        }
	else if( n<0 ) {
		if( errno!=EWOULDBLOCK ) {		// error!
			close();
		}
		return 0 ;
	}
        m_recvloc += n;
        if (m_recvloc < m_recvlen)	// buffer not complete 
        {
            return 1 ;
        }
    }
    
    // req completed, process it
    onrequest();

    if (m_recvbuf != NULL) {
        free(m_recvbuf);
        m_recvbuf = NULL;
    }
    m_recvlen = m_recvloc = 0 ;
    m_req.reqcode = 0;
    m_req.reqsize = 0;
    return 0 ;                      // don't continue
}

int dvrsvr::write()
{
    int n=0;
    int res = 0 ;
    net_fifo * nfifo ;
    if( m_sockfd <= 0 ){
        return res ;
    }
    if (m_fifo == NULL) {
	m_fifosize = 0 ;
        return res ;
    }
    if (m_fifo->buf == NULL) {
        nfifo=m_fifo->next ;
        mem_free(m_fifo);
        m_fifo = nfifo;
        return write();
    }
    if (m_fifo->loc >= m_fifo->bufsize) {
        nfifo=m_fifo->next ;
        mem_free(m_fifo->buf);
        mem_free(m_fifo);
        m_fifo = nfifo;
        return write();
    }
    if(m_fifo->loc==0&&m_fifo->bufsize>0){
       // printf("send header\n");
	struct dvr_ans ans ;
	////////////////////////////////////
	ans.anscode = ANSSTREAMDATA;
	ans.anssize = m_fifo->bufsize;
	ans.data = 0 ;
	//Send( &ans, sizeof(ans));	
	n=::send(m_sockfd,&ans,sizeof(ans),0);
	if(n<0){
         //  printf("send n<0\n");
	    if( errno != EWOULDBLOCK ) {
		    close();
	    }
            return res ;	    
	}
	//printf("header sent:%d\n",n);
    }   
    if(m_sockfd>0){
     // printf("sending size:%d loc:%d\n",m_fifo->bufsize,m_fifo->loc);
#if 0     
      if(m_connchannel==0){
	struct dvrtime dvrt ;
	time_now(&dvrt);
	printf("second:%d mini:%d\n",dvrt.second,dvrt.milliseconds);
      }
#endif      
      n =::send(m_sockfd,
		&(m_fifo->buf[m_fifo->loc]),
		m_fifo->bufsize - m_fifo->loc, 0);	
    }
    if( n<0 ) {
            printf("send n<0\n");
	    if( errno != EWOULDBLOCK ) {
		    close();
	    }
            return res ;
    }
//    if(m_fifo)
//      printf("buffer size:%d n:%d\n",m_fifo->bufsize,n);   
    m_active=2 ;
    m_fifosize -= n ;
    m_fifo->loc += n;
    if (m_fifo->loc >= m_fifo->bufsize) {
		nfifo=m_fifo->next ;
		mem_free(m_fifo->buf);
		mem_free(m_fifo);
		m_fifo = nfifo;
		if( m_fifo == NULL ) {
			m_fifosize=0;
		}
		else {
			res = 1 ;
		}
    }

    return res ;
}

void dvrsvr::send_fifo(char *buf, int bufsize)
{
    net_fifo *pfifo;
    net_fifo *nfifo;
    
    if( isclose() ) {
        return ;
    }
    
    nfifo = (net_fifo *) mem_alloc(sizeof(net_fifo));
    if (mem_check(buf)) {
        nfifo->buf = (char *) mem_addref(buf);
    } else {
        nfifo->buf = (char *) mem_alloc(bufsize);
        memcpy(nfifo->buf, buf, bufsize);
        //mem_cpy32(nfifo->buf, buf, bufsize);
    }
    nfifo->next = NULL;
    nfifo->bufsize = bufsize;
    nfifo->loc = 0;
    
    if (m_fifo == NULL) {
        m_fifo = nfifo;
	m_fifosize = bufsize ;
        net_trigger();				// trigger sending
    } else {
        pfifo = m_fifo;
        while (pfifo->next != NULL)
            pfifo = pfifo->next;
        pfifo->next = nfifo;
	m_fifosize += bufsize ;
    }
}


void dvrsvr::Send(void *buf, int bufsize)
{
    int n ;
    char * cbuf = (char *)buf; 
    int cbufsize = bufsize ;
    while(cbufsize>0) {
        if(m_sockfd){
	    n = ::send(m_sockfd, cbuf, cbufsize, 0);
	    if( n<0 ) {
		if( errno==EWOULDBLOCK ) {
		    usleep(1000);
		}
		else {
		    close();		// net work error!
		    return ;
		}
	    }
	    else {
		cbufsize-=n ;
		cbuf+=n ;
	    }
	} else {
	  break; 
	}
    }
//    send_fifo((char *)buf, bufsize);
/*
    char * cbuf = (char *)buf;
    //int n;
    if( isclose() ) {
        return ;
    }
    
    if ( m_fifo || mem_check (buf)) {
        send_fifo(cbuf, bufsize);
    }
    else {
        while( bufsize>0 ) {
            n = ::send(m_sockfd, cbuf, bufsize, 0);
            if( n<0 ) {
                if( errno==EWOULDBLOCK ) {
                    send_fifo(cbuf, bufsize);
                }
                else {
                    close();							// net work error!
                }
                return ;
            }
            cbuf+=n ;
            bufsize-=n ;
        }
    } 
*/
}

int dvrsvr::isclose()
{
    if( m_active<=0 ) {
        close();
    }
    return (m_sockfd <= 0);
}

int dvrsvr::onframe(cap_frame * pframe)
{
    net_fifo *pfifo, *tfifo;
    if( isclose() ) {
        return 0;
    }
    if ((m_conntype == CONN_REALTIME||m_conntype==CONN_LIVESTREAM) && m_connchannel == pframe->channel) {
        if(mIframe>0){
           mIframe--;
           if(pframe->frametype==FRAMETYPE_VIDEO){
	      printf("channel:%d p frame dropped\n",m_connchannel);
             return 0;
	   }
        }
        if (pframe->frametype == FRAMETYPE_KEYVIDEO ) {	// for keyframes
            if(m_fifo != NULL &&
               m_fifosize > (net_livefifosize+pframe->framesize) )
            {
	        printf("channel:%d frame drop\n",m_connchannel);
                pfifo = m_fifo->next;
                m_fifo->next = NULL;
                
                while (pfifo != NULL) {
                    tfifo = pfifo->next;
                    mem_free(pfifo->buf);
                    m_fifosize-=pfifo->bufsize;
                    mem_free(pfifo);
                    pfifo = tfifo;
                }
                if( m_active>0 ) {
                    m_active--;
                }
                mIframe=30;
            }
        }
        send_fifo(pframe->framedata, pframe->framesize);
        return 1;
    }
    //	else if( m_live ) {
    //		m_live->onframe( pframe );
    //	}
    return 0;
}

void dvrsvr::onrequest()
{
#ifdef OUTPUT_COMMAND
    printf( "Receive cmd : %d - payload : %d\n", m_req.reqcode, m_recvlen );
#endif    
 //  dvr_log("Receive cmd : %d - payload : %d", m_req.reqcode, m_recvlen);
 //   if(m_req.reqcode==210){
  //    dvr_log("Receive cmd:get stream day info");
  //  }
    switch (m_req.reqcode) {
        case REQOK:
            ReqOK();
            break ;
        case REQREALTIME:
            ReqRealTime();
            break;
        case REQCHANNELINFO:
            ChannelInfo();
            break;
        case REQSERVERNAME:
            DvrServerName();
            break;
        case REQGETSYSTEMSETUP:
            GetSystemSetup();
            break;
        case REQSETSYSTEMSETUP:
            SetSystemSetup();
            break;
        case REQGETCHANNELSETUP:
            GetChannelSetup();
            break;
        case REQSETCHANNELSETUP:
            SetChannelSetup();
            break;
        case REQKILL:
            ReqKill();
            break;
        case REQSETUPLOAD:
            SetUpload();
            break;
        case REQHOSTNAME:
            HostName();
            break;
        case REQFILERENAME:
        case REQRENAME:
            FileRename();
            break;
        case REQFILE:
        case REQFILECREATE:
            FileCreate();
            break;
        case REQFILEREAD:
            FileRead();
            break;
        case REQFILEWRITE:
            FileWrite();
            break;
        case REQFILECLOSE:
            FileClose();
            break;
        case REQFILESETPOINTER:
            FileSetPointer();
            break;
        case REQFILEGETPOINTER:
            FileGetPointer();
            break;
        case REQFILEGETSIZE:
            FileGetSize();
            break;
        case REQFILESETEOF:
            FileSetEof();
            break;
        case REQFILEDELETE:
            FileDelete();
            break;
        case REQDIRFINDFIRST:
            DirFindFirst();
            break;
        case REQDIRFINDNEXT:
            DirFindNext();
            break;
        case REQDIRFINDCLOSE:
            DirFindClose();
            break;
        case REQGETCHANNELSTATE:
            GetChannelState();
            break;
        case REQSETSYSTEMTIME:
        case REQSETTIME:
            SetDVRSystemTime();
            break;
        case REQGETSYSTEMTIME:
        case REQGETTIME:
            GetDVRSystemTime();
            break;
        case REQSETLOCALTIME:
            SetDVRLocalTime();
            break;
        case REQGETLOCALTIME:
            GetDVRLocalTime();
            break;
        case REQSETTIMEZONE:
            SetDVRTimeZone();
            break;
        case REQGETTIMEZONE:
            GetDVRTimeZone();
            break;
        case REQGETTZIENTRY:
            GetDVRTZIEntry();
            break;
        case REQGETVERSION:
            GetVersion();
            break;
        case REQPTZ:
            ReqPTZ();
            break;
        case REQAUTH:
            ReqAuth();
            break;
        case REQKEY:
            ReqKey();
            break;
        case REQDISCONNECT:
            ReqDisconnect();
            break;
        case REQCHECKID:
            ReqCheckId();
            break;
        case REQLISTID:
            ReqListId();
            break;
        case REQDELETEID:
            ReqDeleteId();
            break;
        case REQADDID:
            ReqAddId();
            break;
        case REQSHAREPASSWD:
            ReqSharePasswd();
            break;
        case REQRUN:
        case REQGETFILE:
        case REQPUTFILE:
        case REQSHAREINFO:
        case REQPLAYBACK:
            DefaultReq();
            break;
        case REQSTREAMOPEN:
	    powerNum=60;
            ReqStreamOpen();
            break;
        case REQSTREAMOPENLIVE:
            ReqStreamOpenLive();
            break;
        case REQOPENLIVE:
            ReqOpenLive();
            break;	    
        case REQSTREAMCLOSE:
            ReqStreamClose();
            break;
        case REQSTREAMSEEK:
            ReqStreamSeek();
            break;
        case REQSTREAMNEXTFRAME:
            ReqNextFrame();
            break;
        case REQSTREAMNEXTKEYFRAME:
            ReqNextKeyFrame();
            break;
        case REQSTREAMPREVKEYFRAME:
            ReqPrevKeyFrame();
            break;
        case REQSTREAMGETDATA:
	    powerNum=60;
            ReqStreamGetData();
            break;
       case REQ2STREAMGETDATAEX:
	    powerNum=60;
            ReqStreamGetDataEx();
            break; 
        case REQSTREAMTIME:
            ReqStreamTime();
            break;
        case REQSTREAMDAYINFO:
            ReqStreamDayInfo();
            break;
        case REQSTREAMMONTHINFO:
            ReqStreamMonthInfo();
            break;
        case REQLOCKINFO:
            ReqLockInfo();
            break;
        case REQUNLOCKFILE:
            ReqUnlockFile();
            break;
        case REQSTREAMDAYLIST:
            ReqStreamDayList ();
            break;
        case REQ2ADJTIME:
            Req2AdjTime();
            break;
        case REQ2SETLOCALTIME:
            Req2SetLocalTime();
            break;
        case REQ2GETLOCALTIME:
            Req2GetLocalTime();
            break;
        case REQ2SETSYSTEMTIME:
            Req2SetSystemTime();
            break;
        case REQ2GETSYSTEMTIME:
            Req2GetSystemTime();
            break;
        case REQ2GETZONEINFO:
            Req2GetZoneInfo();
            break;
        case REQ2SETTIMEZONE:
            Req2SetTimeZone();
            break;
        case REQ2GETTIMEZONE:
            Req2GetTimeZone();
            break;
        case REQSETLED:
            ReqSetled();
            break;
        case REQSETHIKOSD:
            ReqSetHikOSD();
            break;
        case REQ2GETCHSTATE:
            Req2GetChState();
            break;
        case REQ2GETSETUPPAGE:
            Req2GetSetupPage();
            break;

		// TVS/PW
	    case REQCHECKKEY:
	        ReqCheckKey();
	        break;
            
        case REQ2GETSTREAMBYTES:
            Req2GetStreamBytes();
            break;
        case REQECHO:
            ReqEcho();
            break;
        default:
            DefaultReq();
            break;
    }
}

void dvrsvr::ReqOK()
{
    struct dvr_ans ans ;
    ans.anscode=ANSOK ;
    ans.data=0;
    ans.anssize=0;
    Send(&ans, sizeof(ans));
    
#ifdef NETDBG
    printf( "ReqOK\n");
#endif    
    
}

void dvrsvr::ReqEcho()
{
    struct dvr_ans ans ;
    ans.anscode=ANSECHO ;
    ans.data=0;
    ans.anssize=m_recvlen ;
    Send(&ans, sizeof(ans));
    if( m_recvlen>0 ) {
        Send(m_recvbuf, m_recvlen);
    }
    
#ifdef NETDBG
    printf( "ReqEcho\n");
#endif    
    
}

void dvrsvr::ReqRealTime()
{
    struct dvr_ans ans ;
    if (m_req.data >= 0 && m_req.data < MAXCHANNEL) {
        ans.anscode = ANSREALTIMEHEADER;
        ans.data = m_req.data;
        ans.anssize = sizeof(struct hd_file);
        Send(&ans, sizeof(ans));
        Send(Dvr264Header, sizeof(struct hd_file));
        m_conntype = CONN_REALTIME;
        m_connchannel = m_req.data;
    }
    else {
        DefaultReq();
    }
}

struct channel_info {
    int Enable ;
    int Resolution ;
    char CameraName[64] ;
} ;

void dvrsvr::ChannelInfo()
{
    int i;
    struct dvr_ans ans ;
    struct channel_info chinfo ;
    
    ans.anscode = ANSCHANNELDATA ;
    ans.data = cap_channels ;
    ans.anssize = cap_channels * sizeof(struct channel_info);
    Send( &ans, sizeof(ans));
    for( i=0; i<cap_channels; i++ ) {
        memset( &chinfo, 0, sizeof(chinfo));
        chinfo.Enable=cap_channel[i]->enabled() ;
        chinfo.Resolution=0 ;
        strncpy( chinfo.CameraName, cap_channel[i]->getname(), 64);
        Send(&chinfo, sizeof(chinfo));
    }
}

void dvrsvr::HostName()
{
    struct dvr_ans ans;
    ans.anscode = ANSSERVERNAME;
    ans.anssize = strlen(g_hostname)+1;
    ans.data = 0;
    Send( &ans, sizeof(ans));
    Send( g_hostname, ans.anssize );
}

void dvrsvr::DvrServerName()
{
    HostName();
}

void dvrsvr::GetSystemSetup()
{
    struct dvr_ans ans ;
    struct system_stru sys ;
    if( dvr_getsystemsetup(&sys) ) {
        ans.anscode = ANSSYSTEMSETUP;
        ans.anssize = sizeof( struct system_stru );
        ans.data = 0;
        Send( &ans, sizeof(ans) );
        Send( &sys, sizeof(sys) );
    }
    else {
        DefaultReq ();
    }
}

void dvrsvr::SetSystemSetup()
{
    /*
    int sendsize = sizeof(struct dvr_ans);
    char * sendbuf = (char *)mem_alloc( sendsize);
    struct dvr_ans * pans = (struct dvr_ans *)sendbuf ;
    struct system_stru * psys = (struct system_stru *) m_recvbuf ;
    if( dvr_setsystemsetup(psys) ) {
        pans->anscode = ANSOK;
        pans->anssize = 0;
        pans->data = 0;
        Send( sendbuf, sendsize );
        mem_free( sendbuf );
        return ;
    }
    else {
        mem_free( sendbuf ) ;
    }
    */
    DefaultReq();
}

void dvrsvr::GetChannelSetup()
{
    struct DvrChannel_attr dattr ;
    struct dvr_ans ans ;
    if( dvr_getchannelsetup(m_req.data, &dattr, sizeof(dattr)) ) {
        ans.anscode = ANSCHANNELSETUP;
        ans.anssize = sizeof( struct DvrChannel_attr );
        ans.data = m_req.data;
        Send( &ans, sizeof(ans));
        Send( &dattr, sizeof(dattr));
    }
    else {
        DefaultReq();
    }
}

void dvrsvr::SetChannelSetup()
{
    struct dvr_ans ans ;
    struct DvrChannel_attr * pchannel = (struct DvrChannel_attr *) m_recvbuf ;
    dvr_log("SetChannelSetup received");
    if( dvr_setchannelsetup(m_req.data, pchannel, m_req.reqsize) ) {
        ans.anscode = ANSOK;
        ans.anssize = 0;
        ans.data = 0;
        Send( &ans, sizeof(ans) );
        return ;
    }
    DefaultReq();	
}

void dvrsvr::ReqKill()
{
    DefaultReq();
}

void dvrsvr::SetUpload()
{
    DefaultReq();
}

void dvrsvr::FileRename()
{
    DefaultReq();
}

void dvrsvr::FileCreate()
{
    DefaultReq();
}

void dvrsvr::FileRead()
{
    DefaultReq();
}

void dvrsvr::FileWrite()
{
    DefaultReq();
}

void dvrsvr::FileClose()
{
    DefaultReq();
}

void dvrsvr::FileSetPointer()
{
    DefaultReq();
}

void dvrsvr::FileGetPointer()
{
    DefaultReq();
}

void dvrsvr::FileGetSize()
{
    DefaultReq();
}

void dvrsvr::FileSetEof()
{
    DefaultReq();
}

void dvrsvr::FileDelete()
{
    DefaultReq();
}

void dvrsvr::DirFindFirst()
{
    DefaultReq();
}

void dvrsvr::DirFindNext()
{
    DefaultReq();
}

void dvrsvr::DirFindClose()
{
    DefaultReq();
}

void dvrsvr::GetChannelState()
{
    int i ;
    struct channelstate {
        int sig ;
        int rec ;
        int mot ;
    } cs ;
    struct dvr_ans ans ;
    
    ans.anscode = ANSGETCHANNELSTATE ;
    ans.anssize = sizeof( struct channelstate ) * cap_channels ;
    ans.data = cap_channels ;
    Send( &ans, sizeof(ans));
    for( i=0; i<cap_channels; i++ ) {
        if( cap_channel !=NULL && cap_channel[i]!=NULL ) {
            cs.sig = ( cap_channel[i]->getsignal()==0 ) ;                   // signal lost?
            cs.rec =  rec_state(i);                                         // channel recording?
            cs.mot = cap_channel[i]->getmotion() ;                          // motion detections
        }
        else {
            cs.sig = 0 ;
            cs.rec = 0 ;
            cs.mot = 0 ;
        }
        Send(&cs, sizeof(cs));
    }
}

void dvrsvr::SetDVRSystemTime()
{
    DefaultReq();
}

void dvrsvr::GetDVRSystemTime()
{
    DefaultReq();
}

void dvrsvr::SetDVRLocalTime()
{
    DefaultReq();
}

void dvrsvr::GetDVRLocalTime()
{
    DefaultReq();
}

void dvrsvr::SetDVRTimeZone()
{
    DefaultReq();
}

void dvrsvr::GetDVRTimeZone()
{
    DefaultReq();
}

void dvrsvr::GetDVRTZIEntry()
{
    DefaultReq();
}

void dvrsvr::GetVersion()
{
    int version ;
    struct dvr_ans ans;
    ans.anscode = ANSGETVERSION;
    ans.anssize = 4*sizeof(int);
    ans.data = 0;
    Send(&ans, sizeof(ans));
#if USENEWVERSION
    version = VERSION0 ;
    Send(&version, sizeof(version));
    version = VERSION1 ;
    Send(&version, sizeof(version));
    version = VERSION2 ;
    Send(&version, sizeof(version));
    version = VERSION3 ;
    Send(&version, sizeof(version));
#else
    version = DVRVERSION0 ;
    Send(&version, sizeof(version));
    version = DVRVERSION1 ;
    Send(&version, sizeof(version));
    version = DVRVERSION2 ;
    Send(&version, sizeof(version));
    version = DVRVERSION3 ;
    Send(&version, sizeof(version));
#endif    
}

struct ptz_cmd {
    DWORD command ;
    DWORD param ;
} ;

void dvrsvr::ReqPTZ()
{
    struct ptz_cmd * pptz ;
    if( m_req.data>=0 && m_req.data<cap_channels ) {
        pptz = (struct ptz_cmd *)m_recvbuf ;
        if( ptz_msg( m_req.data, pptz->command, pptz->param ) != 0 ) {
            struct dvr_ans ans ;
            ans.anscode = ANSOK;
            ans.anssize = 0;
            ans.data = 0;
            Send( &ans, sizeof(ans));
            return ;
        }
    }
    DefaultReq();
}

void dvrsvr::ReqAuth()
{
    DefaultReq();
}

void dvrsvr::ReqKey()
{
    DefaultReq();
}

void dvrsvr::ReqDisconnect()
{
    DefaultReq();
}

void dvrsvr::ReqCheckId()
{
    DefaultReq();
}

void dvrsvr::ReqListId()
{
    DefaultReq();
}

void dvrsvr::ReqDeleteId()
{
    DefaultReq();
}

void dvrsvr::ReqAddId()
{
    DefaultReq();
}

void dvrsvr::ReqSharePasswd()
{
    DefaultReq();
}

#define DVRSTREAMHANDLE(p)  (((int)(void *)(p))&0x3fffffff)

//
//  m_req.data is stream channel
void dvrsvr::ReqStreamOpen()
{
    if( !m_keycheck && g_keycheck ) {
    		DefaultReq();
    		return ;
    	}

    struct dvr_ans ans ;
    if( m_req.data>=0 && m_req.data<cap_channels ) {
        if( m_playback ) {
            delete m_playback ;
        }
        m_playback=new playback( m_req.data ) ;
        if( m_playback!=NULL ) {
            ans.anscode = ANSSTREAMOPEN;
            ans.anssize = 0;
            ans.data = DVRSTREAMHANDLE(m_playback) ;
            Send( &ans, sizeof(ans) );
            
#ifdef NETDBG
            printf( "Stream Open : %d\n", ans.data);
#endif    
        //    dvr_log("Stream Open : %d", m_req.data);
            return ;
        }
    }
    DefaultReq();
}


//
//  m_req.data is stream channel
void dvrsvr::ReqStreamOpenLive()
{
	    if( !m_keycheck && g_keycheck ) {
    		DefaultReq();
    		return ;
    	}


    struct dvr_ans ans ;
    if( m_req.data>=0 && m_req.data<cap_channels ) {

        if( m_live!=NULL ) {
            delete m_live ;
        }
        m_live=new live( m_req.data ) ;
        if( m_live ) {
            ans.anscode = ANSSTREAMOPEN;
            ans.anssize = 0;
            ans.data = DVRSTREAMHANDLE(m_live) ;
            Send( &ans, sizeof(ans));
            return ;
        }
    }
    DefaultReq();
}

// Open live stream (2nd vesion)
//  m_req.data is stream channel
void dvrsvr::ReqOpenLive()
{

printf("keycheck:%d - g_keycheck: %d\n", m_keycheck, g_keycheck);	
	
	if( !m_keycheck && g_keycheck ) {
   	DefaultReq();
   	return ;
   }

    struct dvr_ans ans ;
    int hlen ;

    if( m_req.data>=0 && m_req.data<cap_channels ) {    
        ans.anscode = ANSSTREAMOPEN;
        ans.data = m_req.data;
        ans.anssize = 0;
        ans.data=0;
        Send(&ans, sizeof(ans));
	////////////////////////////////////
        ans.anscode = ANSSTREAMDATA;
        ans.anssize = 40 ;
        ans.data = FRAMETYPE_264FILEHEADER ;
        Send( &ans, sizeof(ans));	
	     Send(Dvr264Header, 40);
        m_conntype = CONN_LIVESTREAM;
        m_connchannel = m_req.data;
        return ;
    }
    DefaultReq();    
}
void dvrsvr::ReqStreamClose()
{
    struct dvr_ans ans ;
    
    if( m_req.data == DVRSTREAMHANDLE(m_playback) ) {
        ans.anscode = ANSOK;
        ans.anssize = 0;
        ans.data = 0;
        Send( &ans, sizeof(ans) );
        if( m_playback ) {
            delete m_playback ;
            m_playback=NULL ;
        }
        return ;
    }
    else if( m_req.data == DVRSTREAMHANDLE(m_live) ) {
        ans.anscode = ANSOK;
        ans.anssize = 0;
        ans.data = 0;
        Send( &ans, sizeof(ans) );
        if( m_live ) {
            delete m_live ;
            m_live=NULL ;
        }
        return ;
    }
    DefaultReq();
}

void dvrsvr::ReqStreamSeek()
{
    struct dvr_ans ans ;
    char H264header[40];
    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
        m_playback && 
        m_recvlen >= (int)sizeof(struct dvrtime) ) 
    {
      /*
        ans.anscode = ANSOK;
        ans.anssize = 0;
        m_playback->seek((struct dvrtime *) m_recvbuf);
	ans.data = (int)m_playback->getcurrentfileheader();
       
        Send( &ans, sizeof(ans));
        */	
	//send error code : 0 ok, -1 wrong	
        if(m_playback->seek((struct dvrtime *) m_recvbuf)<0){
           ans.anscode = ANSOK;
           ans.anssize = 40;	  
	   ans.data = -1;
	   Send( &ans, sizeof(ans));   
	} else {
           ans.anscode = ANSOK;
           ans.anssize = 40;	 	  
	   ans.data = 0;

	   Send( &ans, sizeof(ans));  
	   //send 40 bytes file header	   
	   m_playback->getfileheader(H264header,sizeof( struct hd_file ));	   	
	} 	  
//	printf("ans.code:%d size=%d data=%d\n",ans.anscode,ans.anssize,ans.data);	
	Send(H264header, 40);
#if 0	
	int i;
	for(i=0;i<40;++i)
	  printf("%x ",H264header[i]);
	printf("\n");
#endif	
#ifdef NETDBG
        printf("Stream seek, %04d-%02d-%02d %02d:%02d:%02d\n", 
               ((struct dvrtime *) m_recvbuf)->year,
               ((struct dvrtime *) m_recvbuf)->month,
               ((struct dvrtime *) m_recvbuf)->day,
               ((struct dvrtime *) m_recvbuf)->hour,
               ((struct dvrtime *) m_recvbuf)->minute,
               ((struct dvrtime *) m_recvbuf)->second ) ;
#endif    
        
    }
    else {
        DefaultReq();
    }
}


void dvrsvr::ReqNextFrame()
{
    struct dvr_ans ans;
    ans.anscode = ANSERROR;
    ans.data = 0;
    ans.anssize = 0;
    Send( &ans, sizeof(ans));
}

void dvrsvr::ReqNextKeyFrame()
{
    struct dvr_ans ans;
    ans.anscode = ANSERROR;
    ans.data = 0;
    ans.anssize = 0;
    Send( &ans, sizeof(ans));
}

void dvrsvr::ReqPrevKeyFrame()
{
    struct dvr_ans ans;
    ans.anscode = ANSERROR;
    ans.data = 0;
    ans.anssize = 0;
    Send( &ans, sizeof(ans));
}

void dvrsvr::ReqStreamGetDataEx()
{
    void * pbuf=NULL ;
    int  getsize=0 ;
    int  frametype ;
   // printf("Request\n");
    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
       m_playback )
    {

      //  printf("get stream\n");
        m_playback->getstreamdataex( &pbuf, &getsize, &frametype);
        if( getsize>0  && pbuf ) {	  
#if 0	      
	    unsigned int mValue;
	    int mIndex=0;
	    int mPesLeng_L;
	    int mPesLeng_H;
	    int mPesLeng;	    
	    unsigned char* pChar=(unsigned char*)pbuf;
            mIndex=3;
            mValue=(unsigned int)pChar[0]<<16 | \
            (unsigned int)pChar[1]<<8 | \
            (unsigned int)pChar[2];
	    while(mIndex<getsize){
	       if(mValue==0x000001){
		  if(pChar[mIndex]==0xe0||pChar[mIndex]==0xc0){
                     printf("mIndex:%d ",mIndex);
		     ++mIndex;  //pes length h
		     mPesLeng_H=pChar[mIndex];
		     ++mIndex;  //pes length low;
		     mPesLeng_L=pChar[mIndex];
		     mPesLeng = mPesLeng_H<<8|mPesLeng_L;
		     ++mIndex; //0x8c
		   //  printf("%x ",pChar[mIndex]);
		     ++mIndex; //0x80;
		   //  printf("%x ",pChar[mIndex]);
		     ++mIndex; //head length
		   //  printf("header length:%d ",pChar[mIndex]);
		     ++mIndex; //PTS 32-30
		    // printf("%x ",pChar[mIndex]);
		     u_int8_t mPTS0=((pChar[mIndex]>>1)&0x07);
		    // printf("mPTS0:%x ",mPTS0);
		     ++mIndex; //PTS 29-22
		  //   printf("%x ",pChar[mIndex]);
		     u_int8_t mPTS1= pChar[mIndex];
		  //   printf("mPTS1:%x ",mPTS1);
		     ++mIndex; //PTS 21-15;
		   //  printf("%x ",pChar[mIndex]);
		     u_int8_t mPTS2=((pChar[mIndex]>>1)&0x7f);
		  //    printf("mPTS2:%x ",mPTS2);
		     ++mIndex; //14-7
		 //    printf("%x ",pChar[mIndex]);
		     u_int8_t mPTS3= pChar[mIndex];
		 //    printf("mPTS3:%x ",mPTS3);
		     ++mIndex; //6-0;
		  //   printf("%x ",pChar[mIndex]);
		     u_int8_t mPTS4= ((pChar[mIndex]>>1)&0x7f);
		  //   printf("mPTS4:%x ",mPTS4);
		     u_int32_t mPTS=((u_int32_t)mPTS0<<30)|
		                    ((u_int32_t)mPTS1<<22)|
		                    ((u_int32_t)mPTS2<<15)|
		                    ((u_int32_t)mPTS3<<7)|
		                    (u_int32_t)mPTS4;
	            
		     if(frametype==FRAMETYPE_VIDEO){
		       dvr_log("Packet size:%d PESLeng:%d Video PES, PTS:%u\n",getsize,mPesLeng,mPTS); 
		     } else {
		       dvr_log("Packet size:%d PESLeng:%d Audio PES,PTS:%u\n",getsize,mPesLeng,mPTS); 
		     }			    
		     break;
		  }		   
	       }
	       mValue=((mValue<<16)>>8)|pChar[mIndex];
	       ++mIndex;
	    }
#endif	  
            struct dvr_ans ans ;
            ans.anscode = ANS2STREAMDATAEX;
            ans.anssize = getsize;
            ans.data = frametype;
            Send( &ans, sizeof( struct dvr_ans ) );
            Send( pbuf, ans.anssize );

#ifdef NETDBG
//            printf( "Stream Data Ex: %d\n", ans.anssize);
#endif                
         //    printf( "Stream Data: %d type:%d\n", ans.anssize,frametype);
         //   dvr_log("Stream Data: size=%d type=%d",ans.anssize,frametype);

            if( m_playback ) {
                m_playback->prereadex();
            }
            return ;
        }
    }        
    else if( m_req.data == DVRSTREAMHANDLE(m_live) && 
       m_live )
    {
        m_live->getstreamdata( &pbuf, &getsize, &frametype);
        if( getsize>=0 && pbuf ) {
            struct dvr_ans ans ;
            ans.anscode = ANSSTREAMDATA ;
            ans.anssize = getsize ;
            ans.data = frametype ;
            Send( &ans, sizeof(ans));
            if( getsize>0 ) {
                Send( pbuf, getsize );
            }
            return ;
        }
    }
    DefaultReq();
}

void dvrsvr::ReqStreamGetData()
{
    void * pbuf=NULL ;
    int  getsize=0 ;
    int  frametype ;

    static struct timeval tm;
    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
       m_playback )
    {

        m_playback->getstreamdata( &pbuf, &getsize, &frametype);
        if( getsize>0  && pbuf ) {
            struct dvr_ans ans ;
            ans.anscode = ANSSTREAMDATA;
            ans.anssize = getsize;
            ans.data = frametype;
            Send( &ans, sizeof( struct dvr_ans ) );
            Send( pbuf, ans.anssize );

#ifdef NETDBG
            printf( "Stream Data: %d\n", ans.anssize);
#endif                
           //  printf( "Stream Data: %d type:%d\n", ans.anssize,frametype);
            //dvr_log("Stream Data: size=%d type=%d",ans.anssize,frametype);

            if( m_playback ) {
                m_playback->preread();
            }
            return ;
        }
    }        
    else if( m_req.data == DVRSTREAMHANDLE(m_live) && 
       m_live )
    {
        m_live->getstreamdata( &pbuf, &getsize, &frametype);
        if( getsize>=0 && pbuf ) {
            struct dvr_ans ans ;
            ans.anscode = ANSSTREAMDATA ;
            ans.anssize = getsize ;
            ans.data = frametype ;
            Send( &ans, sizeof(ans));
            if( getsize>0 ) {
                Send( pbuf, getsize );
            }
            return ;
        }
    }
    DefaultReq();
}

void dvrsvr::ReqStreamTime()
{
    struct dvrtime streamtime ;
    struct dvr_ans ans ;
    
    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
       m_playback )
    {
        if( m_playback->getstreamtime(&streamtime) ) {
            ans.anscode = ANSSTREAMTIME;
            ans.anssize = sizeof(struct dvrtime);
            ans.data = 0;
            Send( &ans, sizeof(ans));
            Send( &streamtime, ans.anssize);
#ifdef NETDBG
        printf("Stream time, %04d-%02d-%02d %02d:%02d:%02d.%03d\n", 
               streamtime.year,
               streamtime.month,
               streamtime.day,
               streamtime.hour,
               streamtime.minute,
               streamtime.second,
               streamtime.milliseconds ) ;
#endif            
            return ;
        }
    }
    DefaultReq();
}

void dvrsvr::ReqStreamDayInfo()
{
    struct dvrtime * pday ;
    struct dvr_ans ans ;
    
    array <struct dayinfoitem> dayinfo ;
 //   dvr_log("RequestStreamDayInfo");
    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
       m_playback!=NULL &&
       m_recvlen>=(int)sizeof(struct dvrtime) )
    {
        pday = (struct dvrtime *)m_recvbuf ;
        m_playback->getdayinfo(dayinfo, pday) ;
        ans.anscode = ANSSTREAMDAYINFO;
        ans.data = dayinfo.size();
        ans.anssize = dayinfo.size()*sizeof(struct dayinfoitem);
        Send( &ans, sizeof(ans));

#ifdef NETDBG
        printf("Stream DayInfo %04d-%02d-%02d\n",
               pday->year, pday->month, pday->day );
#endif  
   //    dvr_log("Stream DayInfo %04d-%02d-%02d",
    //           pday->year, pday->month, pday->day );
        if( ans.anssize>0 ) {
            Send( dayinfo.at(0), ans.anssize);
#if 0
            int x ;
            for( x=0; x<dayinfo.size(); x++ ) {
                dvr_log("%d - %d", dayinfo[x].ontime, dayinfo[x].offtime );
            }
#endif  
        }
        return ;
    }
    DefaultReq();
}

void dvrsvr::ReqStreamMonthInfo()
{
    DWORD monthinfo ;
    struct dvr_ans ans ;
    struct dvrtime * pmonth ;

    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
       m_playback!=NULL &&
       m_recvlen>=(int)sizeof(struct dvrtime) )
    {
        pmonth = (struct dvrtime *)m_recvbuf ;
        monthinfo=m_playback->getmonthinfo(pmonth) ;
        ans.anscode = ANSSTREAMMONTHINFO;
        ans.anssize = 0;
        ans.data = (int)monthinfo;;
        Send(&ans, sizeof(ans));
#ifdef NETDBG
        printf("Stream Month Info %04d-%02d-%02d ",
               pmonth->year, pmonth->month, pmonth->day );
        int x ;
        for( x=0; x<32; x++ ) {
            if( monthinfo&(1<<x) ) {
                printf(" %d", x+1 );
            }
        }
        printf("\n");
#endif          
        return ;
    }
    DefaultReq();
}

void dvrsvr::ReqStreamDayList()
{
    int * daylist ;
    int   daylistsize ;
    struct dvr_ans ans ;

    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
       m_playback!=NULL ) 
    {
        daylistsize = m_playback->getdaylist(&daylist);
        ans.anscode = ANSSTREAMDAYLIST ;
        ans.data = daylistsize ;
        ans.anssize = daylistsize * sizeof(int) ;
        Send( &ans, sizeof(ans));
        if( ans.anssize>0 ) {
            Send( (void *)daylist, ans.anssize );
        }
        return ;
    }
    DefaultReq();
}

void dvrsvr::ReqLockInfo()
{
    struct dvrtime * pday ;
    struct dvr_ans ans ;
    array <struct dayinfoitem> dayinfo ;

    if( m_req.data == DVRSTREAMHANDLE(m_playback) && 
       m_playback!=NULL &&
       m_recvlen>=(int)sizeof(struct dvrtime) )
    {
        
        pday = (struct dvrtime *)m_recvbuf ;
        
        m_playback->getlockinfo(dayinfo, pday) ;
        
        ans.anscode = ANSSTREAMDAYINFO;
        ans.data = dayinfo.size();
        ans.anssize = dayinfo.size()*sizeof(struct dayinfoitem);
        Send( &ans, sizeof(ans));
        
#ifdef NETDBG
        printf("Stream Lock DayInfo %04d-%02d-%02d\n",
               pday->year, pday->month, pday->day );
#endif  
        
        if( ans.anssize>0 ) {
            Send( dayinfo.at(0), ans.anssize);
        }
#ifdef NETDBG
        int x ;
        for( x=0; x<dayinfo.size(); x++ ) {
            printf("%d - %d\n", dayinfo[x].ontime, dayinfo[x].offtime );
        }
#endif  
            
        return ;
    }
    DefaultReq();
}

void dvrsvr::ReqUnlockFile()
{
    rec_index idx ;
    struct dvrtime * tbegin ;
    struct dvrtime * tend ;
    struct dvr_ans ans ;
    
    if( m_recvlen>=(int)sizeof(struct dvrtime) ) {
        tbegin = (struct dvrtime *)m_recvbuf ;
        tend = tbegin+1 ;
        if( disk_unlockfile( tbegin, tend ) ) {
            ans.anscode = ANSOK;
            ans.anssize = 0;
            ans.data = 0;
            Send( &ans, sizeof(ans) );
            return ;
        }
    }
    DefaultReq();
}

void dvrsvr::Req2AdjTime()
{
    struct dvr_ans ans ;
    if( m_recvlen>=(int)sizeof(struct dvrtime) ) {
        time_adjtime((struct dvrtime *)m_recvbuf);
        ans.anscode = ANSOK;
        ans.anssize = 0;
        ans.data = 0;
        Send( &ans, sizeof(ans));
    }
    else {
        DefaultReq();
    }
    return ;
}

void dvrsvr::Req2SetLocalTime()
{
    struct dvr_ans ans ;
    if( m_recvlen>=(int)sizeof(struct dvrtime) ) {
        time_setlocaltime((struct dvrtime *)m_recvbuf);
        ans.anscode = ANSOK;
        ans.anssize = 0;
        ans.data = 0;
        Send( &ans, sizeof(ans));
    }
    else {
        DefaultReq();
    }
}

void dvrsvr::Req2GetLocalTime()
{
    struct dvr_ans ans ;
    struct dvrtime dvrt ;
    try{
    time_now(&dvrt);
   // printf("re-second:%d mini:%d\n",dvrt.second,dvrt.milliseconds); 
    ans.anscode = ANS2TIME;
    ans.anssize = sizeof( struct dvrtime );
    ans.data = 0 ;
    Send( &ans, sizeof(ans));
    Send( &dvrt, sizeof(dvrt));
    }
    catch(...){
      printf("error in Req2GetLocalTime\n");
    }
    return ;
}

void dvrsvr::Req2SetSystemTime()
{
    struct dvr_ans ans ;
    time_setutctime((struct dvrtime *)m_recvbuf);
    ans.anscode = ANSOK;
    ans.anssize = 0;
    ans.data = 0;
    Send( &ans, sizeof(ans));
    return ;
}

void dvrsvr::Req2GetSystemTime()
{
    struct dvr_ans ans ;
    struct dvrtime dvrt ;
    ans.anscode = ANS2TIME;
    ans.anssize = sizeof( struct dvrtime );
    ans.data = 0 ;
    Send( &ans, sizeof(ans));
    time_utctime( &dvrt );
    Send( &dvrt, sizeof(dvrt));
    return ;
}

void dvrsvr::Req2GetZoneInfo()
{
    struct dvr_ans ans ;
    char * zoneinfobuf ;
    int i, zisize;
    array <string> zoneinfo ;
    string s ;
    char * p ;
    readtxtfile ( "/usr/share/zoneinfo/zone.tab", zoneinfo );
    if( zoneinfo.size()>1 ) {
        zoneinfobuf = (char *)mem_alloc(100000);	// 100kbytes
        zisize=0 ;
        for( i=0; i<zoneinfo.size(); i++ ) {
            p=zoneinfo[i].getstring();
            while(*p==' ' ||
                  *p=='\t' ||
                  *p=='\r' ||
                  *p=='\n' ) p++ ;
            if( *p=='#' )	// a comment line
                continue ;
            
            // skip 2 colume
            while( *p!='\t' &&
                  *p!=' ' &&
                  *p!=0 ) p++ ;
            if( *p==0 ) continue ;
            
            while( *p=='\t' ||
                  *p==' ') p++ ;
            
            while( *p!='\t' &&
                  *p!=' ' &&
                  *p!=0 ) p++ ;
            if( *p==0 ) continue ;
            
            while( *p=='\t' ||
                  *p==' ') p++ ;
            if( *p==0 ) continue ;
            
            strcpy( &zoneinfobuf[zisize], p);
            zisize+=strlen(p);
            zoneinfobuf[zisize++]='\n' ;
        }
        zoneinfobuf[zisize++]=0;
        int sendsize = sizeof(struct dvr_ans)+zisize;
        char * sendbuf = (char *)mem_alloc( sendsize);
        char * senddata = sendbuf+sizeof(struct dvr_ans);
        struct dvr_ans * pans = (struct dvr_ans *)sendbuf ;
        memcpy( senddata, zoneinfobuf, zisize);
        mem_free(zoneinfobuf);
        pans->anscode = ANS2ZONEINFO;
        pans->anssize = zisize ;
        pans->data = 0 ;
        Send( sendbuf, sendsize );
        mem_free( sendbuf );
        return ;
    }
    else {
        config_enum enumkey ;
        config dvrconfig(dvrconfigfile);
        string tzi ;
        
        // initialize enumkey
        enumkey.line=0 ;
        while( (p=dvrconfig.enumkey("timezones", &enumkey))!=NULL ) {
            tzi=dvrconfig.getvalue("timezones", p );
            if( tzi.length()<=0 ) {
                s=p ;
                zoneinfo.add(s);
            }
            else {
                zoneinfobuf=tzi.getstring();
                while( *zoneinfobuf != ' ' && 
                      *zoneinfobuf != '\t' &&
                      *zoneinfobuf != 0 ) {
                          zoneinfobuf++ ;
                      }
                s.setbufsize( strlen(p)+strlen(zoneinfobuf)+10 ) ;
                sprintf( s.getstring(), "%s -%s", p, zoneinfobuf );
                zoneinfo.add(s);
            }
        }
        if( zoneinfo.size()>0 ) {
            ans.anscode = ANS2ZONEINFO;
            ans.anssize = 0 ;
            ans.data = 0 ;
            for( i=0; i<zoneinfo.size(); i++ ) {
                ans.anssize+=zoneinfo[i].length()+1 ;
            }
            ans.anssize+=1 ;	// count in null terminate char
            Send(&ans, sizeof(ans));
            for( i=0; i<zoneinfo.size(); i++ ) {
                Send(zoneinfo[i].getstring(), zoneinfo[i].length());
                Send((void *)"\n", 1);
            }
            Send((void *)"\0", 1);		// send null char
            return ;
        }
    }
    DefaultReq();	
}

void dvrsvr::Req2SetTimeZone()
{
    struct dvr_ans ans ;
    if( m_recvbuf==NULL || m_recvlen<3 ) {
        DefaultReq();
        return ;
    }		
    m_recvbuf[m_recvlen-1]=0 ;
    time_settimezone(m_recvbuf);
    
    ans.anscode = ANSOK;
    ans.anssize = 0 ;
    ans.data = 0 ;
    Send( &ans, sizeof(ans));
    return ;
}

void dvrsvr::Req2GetTimeZone()
{
    struct dvr_ans ans ;
    char timezone[256] ;
    
    ans.anscode = ANS2TIMEZONE;
    ans.data = 0 ;
    
    if( time_gettimezone(timezone) ) {
        ans.anssize=strlen(timezone)+1 ;
        Send(&ans, sizeof(ans));
        Send(timezone, ans.anssize);
    }
    else {
        ans.anssize=4 ;
        Send(&ans, sizeof(ans));
        Send( (void *)"UTC", 4 );
    }
    return ;
}

void dvrsvr::ReqSetled()
{
    struct dvr_ans ans ;
    ans.anscode = ANSOK;
    ans.anssize = 0;
    ans.data = 0;
    Send(&ans, sizeof(ans));
    setdio(m_req.data!=0);
}

void dvrsvr::ReqSetHikOSD()
{
    struct dvr_ans ans ;
    if( m_req.data>=0 && m_req.data<cap_channels 
       && m_req.reqsize>=(int)sizeof( hik_osd_type )
       && cap_channel[m_req.data]->type() == CAP_TYPE_HIKLOCAL ) 
    {
        capture * pcap = (capture *)cap_channel[m_req.data] ;
        struct hik_osd_type * posd = (struct hik_osd_type *) m_recvbuf ;
        pcap->setremoteosd();
        pcap->setosd(posd);
        ans.anscode = ANSOK ;
        ans.anssize = 0 ;
        ans.data = 0 ;
        Send( &ans, sizeof(ans));
        return ;
    }
    DefaultReq();
}

void dvrsvr::Req2GetChState()
{
    struct dvr_ans ans ;
    int ch = m_req.data ;
    if( ch>=0 && ch<cap_channels ) {
        ans.anscode = ANS2CHSTATE ;
        ans.anssize = 0 ;
        ans.data = 0 ;
        if( cap_channel[ch]->getsignal() ) {					// bit 0: signal ok.
            ans.data |= 1 ;
        }
        if( cap_channel[ch]->getmotion() ) {					// bit 1: motion detected
            ans.data |= 2 ;
        }
        Send( &ans, sizeof(ans) );
    }
    else {
        DefaultReq();
    }
}


static char * www_genserialno( char * buf, int bufsize )
{
    time_t t ;
    FILE * sfile ;
    int i ;
    unsigned int c ;
    // generate random serialno
    sfile=fopen("/dev/urandom", "r");
    for(i=0;i<(bufsize-1);i++) {
        c=(((unsigned int)fgetc(sfile))*256+(unsigned int)fgetc(sfile))%62 ;
        if( c<10 )
            buf[i]= '0'+ c;
        else if( c<36 )
            buf[i]= 'a' + (c-10) ;
        else if( c<62 )
            buf[i]= 'A' + (c-36) ;
        else
            buf[i]='A' ;
    }
    buf[i]=0;
    fclose(sfile);

	printf("genserial\n");

    sfile=fopen(WWWSERIALFILE, "w");
    time(&t);
    if(sfile) {
        fprintf(sfile, "%u %s", (unsigned int)t, buf);
        fclose(sfile);
    }
    return buf ;
}

// initialize www data for setup page
static void www_setup()
{
	char * getsetup="cgi/getsetup" ;
    pid_t childpid ;
    childpid=fork();
    if( childpid==0 ) {
        chdir(WWWROOT);
        
        execl(getsetup, getsetup, NULL );
        exit(0);
    }
    else if( childpid>0 ) {
        waitpid( childpid, NULL, 0 );
    }
}

void dvrsvr::ReqCheckKey()
{
#if defined (TVS_APP) || defined (PWII_APP)
    struct key_data * key ;
    struct dvr_ans ans ;
    if( m_recvbuf && m_recvlen>=(int)sizeof( struct key_data ) ) {

        key = (struct key_data *)m_recvbuf ;

        // check key block ;
        if( strcmp( g_mfid, key->manufacturerid )==0 &&
            memcmp( g_filekey, key->videokey, 256 )==0 )
        {
            m_keycheck=1;
            ans.data = 0;
            ans.anssize=0;
            ans.anscode = ANSOK ;
            Send( &ans, sizeof(ans));

            // do connection log
            dvr_logkey( 1, key ) ;

            return ;
        }
        else {
        		m_keycheck=0;
            dvr_log("Key check failed!");
        }
    }
#endif
    DefaultReq();
}


void dvrsvr::Req2GetSetupPage()
{
#ifdef TVS_APP
    struct dvr_ans ans ;
    char serno[60] ;
    char pageuri[100] ;
    if( m_keycheck || g_keycheck==0 )
    {
        // prepare setup web pages
        www_genserialno( serno, sizeof(serno) );
        www_setup();

        sprintf( pageuri, "/system.html?ser=%s", serno );

        ans.anscode = ANS2SETUPPAGE ;
        ans.anssize = strlen(pageuri)+1 ;
        ans.data = 0 ;

        Send(&ans, sizeof(ans));
        Send(pageuri, ans.anssize);
        return ;
    }
	DefaultReq();		
#else    
    struct dvr_ans ans ;
    char * pageuri="/system.html";
    
    // prepare setup pages
//    system("preparesetup");
    
    ans.anscode = ANS2SETUPPAGE ;
    ans.anssize = strlen(pageuri)+1 ;
    ans.data = 0 ;
    
    Send(&ans, sizeof(ans));
    Send(pageuri, ans.anssize);
    return ;
#endif    
}

void dvrsvr::Req2GetStreamBytes()
{
    struct dvr_ans ans ;
    if( m_req.data >=0 && m_req.data < cap_channels ) {
        ans.data = cap_channel[m_req.data]->streambytes() ;
        ans.anssize=0;
        ans.anscode = ANS2STREAMBYTES ;
        Send( &ans, sizeof(ans));
        return ;
    }
    DefaultReq();
}


void dvrsvr::DefaultReq()
{
    struct dvr_ans ans ;
    ans.anscode = ANSERROR;
    ans.anssize = 0;
    ans.data = 0;
    Send(&ans, sizeof(ans));
}


// client side support


// open live stream from remote dvr
int dvr_openlive(int sockfd, int channel, struct hd_file * hd264)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQREALTIME ;
    req.data=channel ;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSREALTIMEHEADER && ans.anssize==(int)sizeof(struct hd_file)) {
            if( net_recv(sockfd, hd264, sizeof(struct hd_file)) ) {
                return 1 ;
            }
        }
    }
    return 0 ;
}

// get remote dvr system setup
// return 1:success
//        0:failed
int dvr_getsystemsetup(int sockfd, struct system_stru * psys)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQGETSYSTEMSETUP ;
    req.data=0 ;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSSYSTEMSETUP && ans.anssize>=(int)sizeof(struct system_stru)) {
            if( net_recv(sockfd, psys, sizeof(struct system_stru)) ) {
                return 1 ;
            }
        }
    }
    return 0 ;
}

// set remote dvr system setup
// return 1:success
//        0:failed
int dvr_setsystemsetup(int sockfd, struct system_stru * psys)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQSETSYSTEMSETUP ;
    req.data=0 ;
    req.reqsize=sizeof( struct system_stru );
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, psys, sizeof( struct system_stru ));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            dvr_log("dvr_setsystemsetup ok");
            return 1 ;
        }
    }
    dvr_log("dvr_setsystemsetup failed");
    return 0 ;
}

// get channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_getchannelsetup(int sockfd, int channel, struct DvrChannel_attr * pchannelattr)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQGETCHANNELSETUP ;
    req.data=channel ;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSCHANNELSETUP && ans.anssize>=(int)sizeof(struct DvrChannel_attr)) {
            if( net_recv(sockfd, pchannelattr, sizeof(struct DvrChannel_attr)) ) {
                return 1 ;
            }
        }
    }
    return 0 ;
}

// set channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_setchannelsetup (int sockfd, int channel, struct DvrChannel_attr * pchannelattr)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQSETCHANNELSETUP ;
    req.data=channel ;
    req.reqsize=sizeof(struct DvrChannel_attr);
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, pchannelattr, sizeof( struct DvrChannel_attr ));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return 1 ;
        }
    }
    return 0 ;
}

int dvr_sethikosd(int sockfd, int channel, struct hik_osd_type * posd)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQSETHIKOSD ;
    req.data=channel ;
    req.reqsize=sizeof(struct hik_osd_type);
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, posd, sizeof( struct hik_osd_type ));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return 1 ;
        }
    }
    return 0 ;
}

// set remote dvr time
// return 1: success, 0:failed
int dvr_settime(int sockfd, struct dvrtime * dvrt)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQ2SETLOCALTIME ;
    req.data=0 ;
    req.reqsize=sizeof(struct dvrtime);
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, dvrt, sizeof( struct dvrtime ));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return 1 ;
        }
    }
    return 0 ;
}

// get remote dvr time
// return 1: success, 0:failed
int dvr_gettime(int sockfd, struct dvrtime * dvrt)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQ2GETLOCALTIME;
    req.data=0;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANS2TIME && ans.anssize>=(int)sizeof(struct dvrtime)) {
            if( net_recv(sockfd, dvrt, sizeof(struct dvrtime)) ) {
                return 1 ;
            }
        }
    }
    return 0 ;
}

// get remote dvr time in utc
// return 1: success, 0:failed
int dvr_getutctime(int sockfd, struct dvrtime * dvrt)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQ2GETSYSTEMTIME ;
    req.data=0;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANS2TIME && ans.anssize>=(int)sizeof(struct dvrtime)) {
            if( net_recv(sockfd, dvrt, sizeof(struct dvrtime)) ) {
                return 1 ;
            }
        }
    }
    return 0 ;
}

// get remote dvr time
// return 1: success, 0:failed
int dvr_setutctime(int sockfd, struct dvrtime * dvrt)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQ2SETSYSTEMTIME ;
    req.data=0 ;
    req.reqsize=sizeof(struct dvrtime);
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, dvrt, sizeof( struct dvrtime ));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return 1 ;
        }
    }
    return 0 ;
}

// get remote dvr time
// return 1: success, 0:failed
int dvr_adjtime(int sockfd, struct dvrtime * dvrt)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQ2ADJTIME ;
    req.data=0 ;
    req.reqsize=sizeof(struct dvrtime);
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, dvrt, sizeof( struct dvrtime ));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return 1 ;
        }
    }
    return 0 ;
}

// set remote dvr time zone (for IP cam)
// return 1: success, 0: failed
int     dvr_settimezone(int sockfd, char * tzenv)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQ2SETTIMEZONE ;
    req.data=0 ;
    req.reqsize=strlen(tzenv)+1;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, tzenv, req.reqsize );
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return 1 ;
        }
    }
    return 0 ;
}

// get channel status
//    return value:
//    bit 0: signal lost
//    bit 1: motion detected
//    -1 : error
int dvr_getchstate(int sockfd, int ch)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQ2GETCHSTATE ;
    req.data=ch ;
    req.reqsize=0 ;
    net_clean(sockfd);
    if( net_send(sockfd, &req, sizeof(req)) ) {
        if( net_recv(sockfd, &ans, sizeof(ans))) {
            if( ans.anscode==ANS2CHSTATE ) {
                return ans.data ;
            }
        }
    }
    return -1 ;
}
