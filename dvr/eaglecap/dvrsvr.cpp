
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dvr.h"

#define DVRPORT 15111

char Dvr264Header[40] = 
{
    '\x34', '\x48', '\x4B', '\x48', '\xFE', '\xB3', '\xD0', '\xD6',
    '\x08', '\x03', '\x04', '\x20', '\x00', '\x00', '\x00', '\x00',
    '\x03', '\x10', '\x02', '\x10', '\x01', '\x10', '\x10', '\x00',
    '\x80', '\x3E', '\x00', '\x00', '\x10', '\x02', '\x40', '\x01',
    '\x11', '\x10', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
};


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

    m_live = NULL ;
    
    m_conntype = CONN_NORMAL;
   
    m_fifosize=0;
    m_active=1;

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
    if( m_live!=NULL ) {
        delete m_live ;
        m_live=NULL;
    }
    if( m_recvbuf ) {
        mem_free( m_recvbuf ) ;
        m_recvbuf = NULL;
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
            m_recvbuf = (char *)mem_alloc(m_req.reqsize);
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
        mem_free(m_recvbuf);
        m_recvbuf = NULL;
    }
    m_recvlen = m_recvloc = 0 ;
    m_req.reqcode = 0;
    m_req.reqsize = 0;
    return 0 ;                      // don't continue
}

int dvrsvr::write()
{
	int n;
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
    n =::send(m_sockfd,
              &(m_fifo->buf[m_fifo->loc]),
              m_fifo->bufsize - m_fifo->loc, 0);
	if( n<0 ) {
		if( errno != EWOULDBLOCK ) {
			close();
		}
        return res ;
	}
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
//        memcpy(nfifo->buf, buf, bufsize);
        mem_cpy32(nfifo->buf, buf, bufsize);
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
        n = ::send(m_sockfd, cbuf, cbufsize, 0);
        if( n<0 ) {
            if( errno==EWOULDBLOCK ) {
                usleep(1000);
            }
            else {
                close();							// net work error!
                return ;
            }
        }
        else {
            cbufsize-=n ;
            cbuf+=n ;
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
    if (m_conntype == CONN_REALTIME && m_connchannel == pframe->channel) {
        if (pframe->frametype == FRAMETYPE_KEYVIDEO ) {	// for keyframes
            if(m_fifo != NULL &&
               m_fifosize > (net_livefifosize+pframe->framesize) )
            {
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
#ifdef NETDBG
    printf( "Receive cmd : %d - payload : %d\n", m_req.reqcode, m_recvlen );
#endif    
    
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
        case REQGETCHANNELSETUP:
            GetChannelSetup();
            break;
        case REQSETCHANNELSETUP:
            SetChannelSetup();
            break;
        case REQHOSTNAME:
            HostName();
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
        case REQSTREAMOPENLIVE:
            ReqStreamOpenLive();
            break;
        case REQSTREAMCLOSE:
            ReqStreamClose();
            break;
        case REQSTREAMGETDATA:
            ReqStreamGetData();
            break;
        case REQSTREAMTIME:
            ReqStreamTime();
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
        case REQ2SETTIMEZONE:
            Req2SetTimeZone();
            break;
        case REQ2GETTIMEZONE:
            Req2GetTimeZone();
            break;
        case REQSETHIKOSD:
            ReqSetHikOSD();
            break;
        case REQ2GETCHSTATE:
            Req2GetChState();
            break;
        case REQSENDDATA:
            ReqSendData();
            break;
        case REQGETDATA:
            ReqGetData();
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
    if( dvr_setchannelsetup(m_req.data, pchannel, m_req.reqsize) ) {
        ans.anscode = ANSOK;
        ans.anssize = 0;
        ans.data = 0;
        Send( &ans, sizeof(ans) );
        return ;
    }
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
            cs.rec =  0;                                         // channel recording?
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
    version = DVRVERSION0 ;
    Send(&version, sizeof(version));
    version = DVRVERSION1 ;
    Send(&version, sizeof(version));
    version = DVRVERSION2 ;
    Send(&version, sizeof(version));
    version = DVRVERSION3 ;
    Send(&version, sizeof(version));
}

struct ptz_cmd {
    DWORD command ;
    DWORD param ;
} ;

void dvrsvr::ReqPTZ()
{
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
void dvrsvr::ReqStreamOpenLive()
{
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

void dvrsvr::ReqStreamClose()
{
    struct dvr_ans ans ;
    
    if( m_req.data == DVRSTREAMHANDLE(m_live) ) {
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

void dvrsvr::ReqStreamGetData()
{
    void * pbuf=NULL ;
    int  getsize=0 ;
    int  frametype ;

    if( m_req.data == DVRSTREAMHANDLE(m_live) && 
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
    DefaultReq();
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
    time_now(&dvrt);
    ans.anscode = ANS2TIME;
    ans.anssize = sizeof( struct dvrtime );
    ans.data = 0 ;
    Send( &ans, sizeof(ans));
    Send( &dvrt, sizeof(dvrt));
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

void dvrsvr::ReqSendData()
{
    DefaultReq();
}

void dvrsvr::ReqGetData()
{
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
            return 1 ;
        }
    }
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
int dvr_settimezone(int sockfd, char * tzenv)
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

// open new clip file on remote dvr
int dvr_nfileopen(int sockfd, int channel, struct nfileinfo * nfi)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQNFILEOPEN ;
    req.data=channel ;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSNFILEOPEN && ans.anssize>=(int)sizeof(struct nfileinfo)) {
            if( net_recv(sockfd, nfi, sizeof(struct nfileinfo)) ) {
                return ans.data ;
            }
        }
    }
    return 0 ;
}


// close nfile on remote dvr
// return 
//       1: success
//       0: failed
int dvr_nfileclose(int sockfd, int nfile)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQNFILECLOSE ;
    req.data=nfile ;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return 1 ;
        }
    }
    return 0 ;
}


// read nfile from remote dvr
// return actual readed bytes
int dvr_nfileread(int sockfd, int nfile, void * buf, int size)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    
    req.reqcode=REQNFILEREAD ;
    req.data=nfile ;
    req.reqsize=sizeof(int);
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, &size, sizeof(size));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSNFILEREAD && ans.anssize>0) {
            if( ans.anssize>size ) {
                ans.anssize=size ;
            }
            if( net_recv(sockfd, buf, ans.anssize) ) {
                return ans.anssize ;
            }
        }
    }
    return 0 ;
}
