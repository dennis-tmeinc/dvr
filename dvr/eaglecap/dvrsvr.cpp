
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
    m_recvbuf = NULL;
    m_recvlen = 0;
    m_recvloc = 0;
    m_req.reqcode = 0;
    m_req.reqsize = 0;

    m_conntype = CONN_NORMAL;
   
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
    if (m_sockfd > 0) {
        ::closesocket(m_sockfd);
        m_sockfd = -1;
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
	int res = 0 ;
    if( m_sockfd <= 0 ){
        return res ;
    }
    return res ;
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
    if( isclose() ) {
        return 0;
    }
    if (m_conntype == CONN_REALTIME && m_connchannel == pframe->channel) {
        Send(pframe->framedata, pframe->framesize);
        return 1;
    }
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
        case REQSERVERNAME:
        case REQHOSTNAME:
            DvrServerName();
            break;
        case REQGETCHANNELSETUP:
            GetChannelSetup();
            break;
        case REQSETCHANNELSETUP:
            SetChannelSetup();
            break;
        case REQGETCHANNELSTATE:
            GetChannelState();
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

#define DVRSTREAMHANDLE(p)  (((int)(void *)(p))&0x3fffffff)

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

