
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dvr.h"
#include "draw.h"

dvrsvr *dvrsvr::m_head = NULL;

// KEY support
int net_keyactive ;

dvrsvr::dvrsvr(int fd)
{
    pthread_mutex_init(&m_mutex, NULL);

    m_sockfd = fd;
    m_shutdown = 0 ;
    m_fifo = NULL;
    m_recvbuf = NULL;
    m_recvlen = 0;
    m_recvloc = 0;
    m_req.reqcode = 0;
    m_req.reqsize = 0;

    m_playback = NULL ;     // playback

    m_conntype = CONN_NORMAL;

    m_fifosize=0;

    // add into svr queue
    m_next = m_head;
    m_head = this;

    // key check
    m_keycheck= 0 ;
}

dvrsvr::~dvrsvr()
{
    lock();

    // remove from dvr queue
    if (m_head == this) {
        m_head = m_next;
    } else {
        dvrsvr *walk = m_head;
        while (walk->m_next != NULL) {
            if (walk->m_next == this) {
                walk->m_next = this->m_next;
                break;
            }
            walk = walk->m_next;
        }
    }

    m_shutdown=0 ;

    if( m_sockfd>0) {
        ::closesocket(m_sockfd);
    }

    cleanfifo();
    if( m_playback!=NULL ) {
        delete m_playback ;
    }
    if( m_recvbuf ) {
        delete m_recvbuf  ;
        m_recvbuf=NULL ;
    }
    unlock();

    pthread_mutex_destroy(&m_mutex);
}

// return 0: no data, 1: may have more data
int dvrsvr::read()
{
    int n;
    char *preq;
    if( isdown() ) {
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
            down() ;            // indicate socket is down
            return 0 ;
        }
        else if( n<0 ) {
            if( errno!=EWOULDBLOCK ) {
                down() ;        // indicate socket is down
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
            m_recvbuf = new char [m_req.reqsize];
            if (m_recvbuf == NULL) {
                // no enough memory
                down();             // indicate socket is down
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
            down();             // indicate socket is down
            return 0 ;
        }
        else if( n<0 ) {
            if( errno!=EWOULDBLOCK ) {		// error!
                down();         // mark socket down
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
        delete m_recvbuf;
        m_recvbuf = NULL;
    }
    m_recvlen = m_recvloc = 0 ;
    m_req.reqcode = 0;
    m_req.reqsize = 0;
    return 0 ;                      // don't continue
}

void dvrsvr::cleanfifo()
{
    net_fifo * nfifo ;
    while( m_fifo ) {
        if (m_fifo->buf != NULL) {
            mem_free(m_fifo->buf);
        }
        nfifo=m_fifo->next ;
        delete m_fifo ;
        m_fifo = nfifo;
    }
    m_fifosize=0;
}

// send out data from fifo
// return 0: no more data
//        1: more data in fifo
//       -1: error
int dvrsvr::write()
{
    int res=0;
    int n;
    net_fifo * nfifo ;

    if( isdown() ) {
        return 0;
    }

    lock();
    while( m_fifo ) {
        if (m_fifo->buf == NULL) {
            nfifo=m_fifo->next ;
            delete m_fifo ;
            m_fifo = nfifo;
        }
        else if(m_fifo->loc >= m_fifo->bufsize) {
            nfifo=m_fifo->next ;
            mem_free(m_fifo->buf);
            delete m_fifo ;
            m_fifo = nfifo;
        }
        else {
            n =::send(m_sockfd,
                &(m_fifo->buf[m_fifo->loc]),
                m_fifo->bufsize - m_fifo->loc, 0);
            if( n<0 ) {
                if(errno == EWOULDBLOCK ) {
                    res = 1 ;
                    break ;
                }
                else {
                    res = -1 ;          // other error!
                    down();             // mark socket down
                    break;
                }
            }
            m_fifosize -= n ;
            m_fifo->loc += n;
        }
    }
    if( m_fifo==NULL ) {
        m_fifosize=0;
        res=0 ;
    }
    unlock();
    return res ;
}

void dvrsvr::send_fifo(char *buf, int bufsize, int loc)
{
    net_fifo *nfifo;

    nfifo = new net_fifo ;
    nfifo->buf = (char *)mem_addref( buf, bufsize );
    nfifo->bufsize = bufsize;
    nfifo->loc = loc;
    nfifo->next = NULL;

    if (m_fifo == NULL) {           // current fifo is empty
        m_fifo = m_fifotail = nfifo;
        m_fifosize = bufsize ;
        net_trigger();
    } else {
        m_fifotail->next = nfifo ;
        m_fifotail = nfifo ;
        m_fifosize += bufsize ;
    }
}

void dvrsvr::Send(void *buf, int bufsize)
{
    if( isdown() ) {
        return ;
    }

    lock();
    if ( m_fifo ) {
        send_fifo((char *)buf, bufsize);
    }
    else {
        int loc = 0 ;
        int n ;
        while( loc<bufsize ) {
            n = ::send(m_sockfd, ((char *)buf)+loc, bufsize-loc, 0);
            if( n<0 ) {
                if( errno==EWOULDBLOCK ) {
                    send_fifo(((char *)buf), bufsize, loc);
                }
                else {
                    // network error!
                    down();         // mark socket down
                }
                break ;
            }
            loc+=n ;
        }
    }
    unlock();
}

int dvrsvr::onframe(cap_frame * pframe)
{
    struct dvr_ans ans ;

    if( isdown() ) {
        return 0;
    }
    if( m_connchannel == pframe->channel ) {
        if (m_conntype == CONN_REALTIME ) {
            if ( pframe->frametype == FRAMETYPE_KEYVIDEO &&	// for keyframes
                m_fifosize > (net_livefifosize+pframe->framesize) )
            {
                cleanfifo();
            }
            Send(pframe->framedata, pframe->framesize);
            return 1;
        }
        else if (m_conntype == CONN_LIVESTREAM ) {
            if (pframe->frametype == FRAMETYPE_KEYVIDEO ) {	// for keyframes
                if( pframe->framesize > net_livefifosize ) {
                    net_livefifosize = pframe->framesize+10000 ;
                }
                m_fifodrop=0;
            }
            if( m_fifosize > net_livefifosize ) {
                m_fifodrop=1;
            }
            if( m_fifodrop ) {
                return 0 ;
            }
            ans.anscode = ANSSTREAMDATA ;
            ans.anssize = pframe->framesize ;
            ans.data = pframe->frametype ;
            Send(&ans, sizeof(struct dvr_ans));
            Send(pframe->framedata, pframe->framesize);
            return 1;
        }
        else if (m_conntype == CONN_JPEG && pframe->frametype==FRAMETYPE_JPEG ) {
            ans.anscode = ANS2JPEG ;
            ans.anssize = pframe->framesize ;
            ans.data = pframe->frametype ;
            Send(&ans, sizeof(struct dvr_ans));
            Send(pframe->framedata, pframe->framesize);
            return 1;
        }
        //	else if( m_live ) {
        //		m_live->onframe( pframe );
        //	}
    }
    return 0;
}

void dvrsvr::onrequest()
{
    switch (m_req.reqcode) {
    case REQOK:
        AnsOk();
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
    case REQHOSTNAME:
        HostName();
        break;
    case REQGETCHANNELSTATE:
        GetChannelState();
        break;
    case REQGETVERSION:
        GetVersion();
        break;
    case REQPTZ:
        ReqPTZ();
        break;
    case REQSTREAMOPEN:
        ReqStreamOpen();
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
    case REQSTREAMFILEHEADER:
        ReqStreamFileHeader();
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
        ReqStreamGetData();
        break;
    case REQ2STREAMGETDATAEX:
        Req2StreamGetDataEx();
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
    case REQSENDDATA:
        ReqSendData();
        break;
    case REQGETDATA:
        ReqGetData();
        break;
    case REQCHECKKEY:
        ReqCheckKey();
        break;
    case REQUSBKEYPLUGIN:
        ReqUsbkeyPlugin();
        break;
    case REQ2GETSETUPPAGE:
        Req2GetSetupPage();
        break;
    case REQ2GETSTREAMBYTES:
        Req2GetStreamBytes();
        break;
    case REQ2KEYPAD:
        Req2Keypad();
        break;
    case REQ2PANELLIGHTS:
        Req2PanelLights();
        break;
    case REQ2GETJPEG:
        Req2GetJPEG();
        break;
    case REQECHO:
        ReqEcho();
        break;
    default:
        AnsError();
        break;
    }
}

void dvrsvr::AnsOk()
{
    struct dvr_ans ans ;
    ans.anscode=ANSOK ;
    ans.data=0;
    ans.anssize=0;
    Send(&ans, sizeof(ans));
}

void dvrsvr::AnsError()
{
    struct dvr_ans ans ;
    ans.anscode = ANSERROR;
    ans.anssize = 0;
    ans.data = 0;
    Send(&ans, sizeof(ans));
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
}

void dvrsvr::ReqRealTime()
{
    struct dvr_ans ans ;
    if( !g_keycheck || m_keycheck!=0 ) {
        if (m_req.data >= 0 && m_req.data < cap_channels) {
            ans.anscode = ANSREALTIMEHEADER;
            ans.data = m_req.data;
            ans.anssize = sizeof(struct hd_file);
            Send(&ans, sizeof(ans));
            Send(cap_fileheader(m_req.data), sizeof(struct hd_file));
            m_conntype = CONN_REALTIME;
            m_connchannel = m_req.data;
            return ;
        }
    }
    AnsError();
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

    if( !g_keycheck || m_keycheck!=0 ) {
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
        return ;
    }
    AnsError();
}

void dvrsvr::HostName()
{
    struct dvr_ans ans;
    ans.anscode = ANSSERVERNAME;
    ans.anssize = g_servername.length()+1;
    ans.data = 0;
    Send( &ans, sizeof(ans));
    Send( (char *)g_servername, ans.anssize );
}

void dvrsvr::DvrServerName()
{
    HostName();
}

void dvrsvr::GetSystemSetup()
{
    struct dvr_ans ans ;
    struct system_stru sys ;
    memset( &sys, 0, sizeof(sys));
    if( dvr_getsystemsetup(&sys) ) {
        ans.anscode = ANSSYSTEMSETUP;
        ans.anssize = sizeof( struct system_stru );
        ans.data = 0;
        Send( &ans, sizeof(ans) );
        Send( &sys, sizeof(sys) );
    }
    else {
        AnsError ();
    }
}

void dvrsvr::SetSystemSetup()
{
    struct system_stru * psys ;
    if( m_recvlen>=(int)sizeof(struct system_stru) ) {
        psys=(struct system_stru *)m_recvbuf ;
        if( dvr_setsystemsetup( (struct system_stru *)m_recvbuf ) ) {
            AnsOk();
            return ;
        }
    }
    AnsError();
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
        AnsError();
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
    AnsError();
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
        if( cap_channel[i]!=NULL ) {
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
    AnsError();
}


#define DVRSTREAMHANDLE(p)  (((int)(void *)(p))&0x3fffffff)

//
//  m_req.data is stream channel
void dvrsvr::ReqStreamOpen()
{
    net_dprint( "ReqStreamOpen, channel %d\n", m_req.data );
    struct dvr_ans ans ;
    if( g_keycheck && m_keycheck==0 ) {
        AnsError();
        return ;
    }
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
            return ;
        }
    }
    AnsError();
}

// Open live stream (2nd vesion)
//  m_req.data is stream channel
void dvrsvr::ReqOpenLive()
{
    struct dvr_ans ans ;
    int hlen ;

    if( g_keycheck && m_keycheck==0 ) {
        AnsError();
        return ;
    }

    if( m_req.data>=0 && m_req.data<cap_channels ) {
        ans.anscode = ANSSTREAMOPEN;
        ans.anssize = 0;
        ans.data = 0 ;
        Send( &ans, sizeof(ans) );
        m_conntype = CONN_LIVESTREAM;
        m_connchannel = m_req.data;
        hlen = cap_channel[m_connchannel]->getheaderlen();
        if( hlen>0 ) {
            ans.anscode = ANSSTREAMDATA;
            ans.anssize = hlen ;
            ans.data = FRAMETYPE_264FILEHEADER ;
            Send( &ans, sizeof(ans));
            Send( cap_channel[m_req.data]->getheader(), hlen );
        }
        return ;
    }
    AnsError();
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
    AnsError();
}

void dvrsvr::ReqStreamSeek()
{
    struct dvr_ans ans ;
    if( m_req.data == DVRSTREAMHANDLE(m_playback) &&
        m_playback &&
        m_recvlen >= (int)sizeof(struct dvrtime) )
    {
        ans.anscode = ANSOK;
        ans.anssize = 0;
        ans.data = m_playback->seek((struct dvrtime *) m_recvbuf) ;
#if defined(EAGLE34) || defined(EAGLE32)
        Send( &ans, sizeof(ans));
#else
        // EAGLE368
        ans.anssize = m_playback->fileheadersize() ;
        Send( &ans, sizeof(ans)) ;
        if( ans.anssize>0 ) {
            char * header = new char [ans.anssize];
            m_playback->readfileheader(header, ans.anssize);
            Send( header, ans.anssize ) ;
            delete header ;
        }
#endif
    }
    else {
        AnsError();
    }
}

void dvrsvr::ReqStreamFileHeader()
{
    struct dvr_ans ans ;
    if( m_req.data == DVRSTREAMHANDLE(m_playback) )
    {
        ans.anssize = m_playback->fileheadersize() ;
        if( ans.anssize > 0 ) {
            ans.anscode = ANSSTREAMFILEHEADER;
            ans.data = 0 ;
            char * header = new char [ans.anssize];
            m_playback->readfileheader(header, ans.anssize);
            Send( &ans, sizeof(ans)) ;
            Send( header, ans.anssize ) ;
            delete header ;
            return ;
        }
    }
    AnsError();
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

void dvrsvr::ReqStreamGetData()
{
    void * pbuf=NULL ;
    int  getsize=0 ;
    int  frametype ;

    if( m_playback && m_req.data == DVRSTREAMHANDLE(m_playback) )
    {
        m_playback->getstreamdata( &pbuf, &getsize, &frametype);
        if( getsize>0  && pbuf ) {
            struct dvr_ans ans ;
            ans.anscode = ANSSTREAMDATA;
            ans.anssize = getsize;
            ans.data = frametype;
            Send( &ans, sizeof( struct dvr_ans ) );
            Send( pbuf, ans.anssize );
            if( m_playback ) {
                m_playback->preread();
            }
            return ;
        }
    }
    AnsError();
}

// Requested by Harrison. (2010-09-30)
void dvrsvr::Req2StreamGetDataEx()
{
    void * pbuf=NULL ;
    int  getsize=0 ;
    int  frametype ;
    struct dvrtime streamtime ;

    if( m_req.data == DVRSTREAMHANDLE(m_playback) &&
       m_playback )
    {
        m_playback->getstreamdata( &pbuf, &getsize, &frametype);
        if( getsize>0  && pbuf ) {
            struct dvr_ans ans ;
            ans.anscode = ANS2STREAMDATAEX;
            ans.anssize = getsize+sizeof(streamtime);
            ans.data = frametype;
            Send( &ans, sizeof( struct dvr_ans ) );
            m_playback->getstreamtime(&streamtime);
            Send( &streamtime, sizeof(streamtime) );
            Send( pbuf, ans.anssize );
            if( m_playback ) {
                m_playback->preread();
            }
            return ;
        }
    }
    AnsError();
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
            return ;
        }
    }
    AnsError();
}

void dvrsvr::ReqStreamDayInfo()
{
    struct dvrtime * pday ;
    struct dvr_ans ans ;
    int i ;

    array <struct dayinfoitem> dayinfo ;

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
        if( ans.anssize>0 ) {
            for( i=0; i<dayinfo.size(); i++ ) {
                Send( &dayinfo[i], sizeof(struct dayinfoitem));
            }
        }
    }
    else {
        AnsError();
    }
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
        return ;
    }
    AnsError();
}

void dvrsvr::ReqStreamDayList()
{
    array <int> daylist ;
    int   daylistsize ;
    struct dvr_ans ans ;
    int i;

    if( m_req.data == DVRSTREAMHANDLE(m_playback) &&
       m_playback!=NULL )
    {
        daylistsize = m_playback->getdaylist(daylist);
        ans.anscode = ANSSTREAMDAYLIST ;
        ans.data = daylistsize ;
        ans.anssize = daylistsize * sizeof(int) ;
        Send( &ans, sizeof(ans));
        if( ans.anssize>0 ) {
            for(i=0; i<daylistsize; i++ ) {
                Send( &daylist[i], sizeof(int));
            }
        }
        return ;
    }
    AnsError();
}

void dvrsvr::ReqLockInfo()
{
    struct dvrtime * pday ;
    struct dvr_ans ans ;
    int i;
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
        if( ans.anssize>0 ) {
            for( i=0; i<dayinfo.size(); i++ ) {
                Send( &dayinfo[i], sizeof(struct dayinfoitem));
            }
        }
        return ;
    }
    AnsError();
}

void dvrsvr::ReqUnlockFile()
{
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
    AnsError();
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
        AnsError();
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
        AnsError();
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

void dvrsvr::Req2GetZoneInfo()
{
    struct dvr_ans ans ;
    char * zoneinfobuf ;
    int i;
    array <string> zoneinfo ;
    string s ;
    const char * p ;

    config_enum enumkey ;
    config dvrconfig(CFG_FILE);
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
            zoneinfobuf=(char *)tzi;
            while( *zoneinfobuf != ' ' &&
                *zoneinfobuf != '\t' &&
                *zoneinfobuf != 0 )
            {
                zoneinfobuf++ ;
            }
            s.setbufsize( strlen(p)+strlen(zoneinfobuf)+10 ) ;
            sprintf( (char *)s, "%s -%s", p, zoneinfobuf );
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
            Send((char *)zoneinfo[i], zoneinfo[i].length());
            Send((void *)"\n", 1);
        }
        Send((void *)"\0", 1);		// send null char
        return ;
    }
    AnsError();
}

void dvrsvr::Req2SetTimeZone()
{
    struct dvr_ans ans ;
    if( m_recvbuf==NULL || m_recvlen<3 ) {
        AnsError();
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
    AnsError();
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
        AnsError();
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
    pid_t childpid ;
    childpid=fork();
    if( childpid==0 ) {
        chdir(WWWROOT);
        execl("cgi/getsetup", "cgi/getsetup", NULL );
        exit(0);
    }
    else if( childpid>0 ) {
        waitpid( childpid, NULL, 0 );
    }
}

void dvrsvr::Req2GetSetupPage()
{
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
    AnsError();

    return ;
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
    AnsError();
}

void dvrsvr::Req2Keypad()
{
#if defined (PWII_APP)
    struct dvr_ans ans ;
    ans.data = 0 ;
    ans.anssize=0;
    ans.anscode = ANSOK ;
    Send( &ans, sizeof(ans));
    screen_key( (int)(m_req.data&0xff), (int)(m_req.data>>8)&1 );
    return ;
#else
    AnsError();
#endif
}

void dvrsvr::Req2PanelLights()
{
#if defined (PWII_APP)
    struct dvr_ans ans ;
    if( m_req.data >=0 && m_req.data < cap_channels ) {
        ans.data = cap_channel[m_req.data]->streambytes() ;
        ans.anssize=0;
        ans.anscode = ANS2STREAMBYTES ;
        Send( &ans, sizeof(ans));
        return ;
    }
#endif
    AnsError();
}

void dvrsvr::Req2GetJPEG()
{
    int ch, jpeg_quality, jpeg_pic ;
    ch = m_req.data & 0xff  ;
    jpeg_quality = (m_req.data>>8) & 0xff  ;        //0-best, 1-better, 2-average
    jpeg_pic = (m_req.data>>16) & 0xff  ;           // 0-cif , 1-qcif, 2-4cif
    if( ch >=0 && ch < cap_channels ) {
        // problem is JPEG capture doesn't work if called from here, JPEG capturing only works in main thread
        // so this call is just a signal to trigger jpeg capture
        m_conntype = CONN_JPEG ;
        m_connchannel = ch ;
        if( cap_channel[ch]->captureJPEG(jpeg_quality, jpeg_pic) ) {
            // set connection type to JPEG
            return ;
        }
    }
    AnsError();
}

void dvrsvr::ReqSendData()
{
    AnsError();
}

void dvrsvr::ReqGetData()
{
    AnsError();
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
            dvr_log("Key check failed!");
        }
    }
#endif
    AnsError();
}

#ifdef PWII_APP

// Generate USB id string like: Ven_SanDisk&Prod_Cruzer_Mini&Rev_0.1\20041100531daa107a09

// Ven   :  /sys/block/sda/device/vendor
// Prod  :  /sys/block/sda/device/model
// Rev   :  /sys/block/sda/device/rev
// serial:  cd -P /sys/block/sda/device ; ../../../../serial
//          /sys/block/sda/device/../../../../serial

// return 0 : failed, >0: success
int getUSBkeyserialid(char * device, char * serialid)
{
    char sysfilename[256] ;
    FILE * sysfile ;
    int r ;
    char vendor[64] ;
    char product[64] ;
    char rev[64] ;
    char serial[64] ;

    // get vendor
    vendor[0]=0 ;
    sprintf( sysfilename, "/sys/block/%s/device/vendor", device );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(vendor, 64, sysfile );
        fclose( sysfile );
    }
    else {
        return 0 ;
    }
    str_trimtail( vendor );

    // get product
    product[0] = 0 ;
    sprintf( sysfilename, "/sys/block/%s/device/model", device );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(product, 64, sysfile );
        fclose( sysfile );
    }
    else {
        return 0 ;
    }
    str_trimtail( product );

    // get rev
    rev[0] = 0 ;
    sprintf( sysfilename, "/sys/block/%s/device/rev", device );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(rev, 64, sysfile );
        fclose( sysfile );
    }
    else {
        return 0 ;
    }
    str_trimtail( rev );

    // get serial no
    serial[0]=0 ;
    sprintf( sysfilename, "/sys/block/%s/device/../../../../serial", device );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(serial, 64, sysfile );
        fclose( sysfile );
    }
    else {
        return 0 ;
    }
    str_trimtail( serial );

    sprintf( sysfilename, "Ven_%s&Prod_%s&Rev_%s\\%s", vendor, product, rev, serial ) ;

    r = 0 ;
    while( sysfilename[r] ) {
        if( sysfilename[r]==' ' ) {
            serialid[r] = '_' ;
        }
        else {
            serialid[r] = sysfilename[r] ;
        }
        r++ ;
    }
    serialid[r]=0 ;
    return r ;
}

static void strgetline( char * source, char * dest )
{
    char * d = dest ;
    while( *source ) {
        if( *source=='\r' || *source=='\n' ) break ;
        if( *source<=' ' ) {
            source++ ;
            continue ;
        }
        *d++=*source++ ;
    }
    *d=0 ;
    str_trimtail( dest );
}

// check police id key
int PoliceIDCheck( char * iddata, int idsize, char * serialid )
{
    extern void md5_checksum( char * md5checksum, unsigned char * data, int datasize) ;
    char md5checksum[16] ;
    int checksize ;
    struct RC4_seed rc4seed ;
    unsigned char k256[256] ;
    struct key_data * keydata ;

    key_256( serialid, k256 ) ;
    RC4_KSA( &rc4seed, k256 );
    RC4_crypt( (unsigned char *)iddata, idsize, &rc4seed ) ;
    keydata = (struct key_data *) iddata ;

    checksize = keydata->size ;
    if( checksize<0 || checksize>(idsize-(int)sizeof(keydata->checksum)) ) {
        checksize=0;
    }
    md5_checksum( md5checksum, (unsigned char *)(&(keydata->size)), checksize );
    if( memcmp( md5checksum, keydata->checksum, 16 )==0 &&
        strcmp( keydata->usbkeyserialid, serialid )== 0 )
    {
        // key file data verified ok!

        // is it a police key and has correct MF ID ?
        if( strcmp( g_mfid, keydata->manufacturerid )==0 &&
            keydata->usbid[0]=='P' &&
            keydata->usbid[1]=='L' )
        {
            // search for "Officer ID" or "Contact Name" from key info part
            char * info ;
            char * idstr ;
            char officerid[128] ;
            officerid[0]=0 ;
            info = (char *)(&(keydata->size))+keydata->keyinfo_start ;
            if( (idstr=strcasestr( info, "Officer ID:" )) ) {
                strgetline(idstr+11, officerid);
            }
            else if( (idstr=strcasestr( info, "Contact Name:" )) ) {
                strgetline(idstr+13, officerid);
            }
            if( officerid[0] ) {
                // select a new offer ID.
                strcpy( g_policeid, officerid ) ;

                // generate new id list
                array <string> idlist ;
                FILE * fid ;
                int ididx = 0 ;
                idlist[ididx++] = g_policeid ;
                fid=fopen(g_policeidlistfile, "r");
                if( fid ) {
                    while( fgets(officerid, 100, fid) ) {
                        str_trimtail(officerid);
                        if( strlen(officerid)<=0 ) continue ;
                        if( strcmp(officerid, g_policeid)==0 ) continue ;
                        idlist[ididx++]=officerid ;
                    }
                    fclose(fid);
                }

                // writing new id list to file
                fid=fopen(g_policeidlistfile, "w");
                if( fid ) {
                    for( ididx=0; ididx<idlist.size(); ididx++) {
                        fprintf(fid, "%s\n", (char *)idlist[ididx] );
                    }
                    fclose( fid ) ;
                }

                // let screen display new policeid
                dvr_log( "New Police ID detected : %s", g_policeid );
                screen_menu(1);
                return 1 ;
            }
        }
    }
    return 0 ;
}

#endif      // PWII_APP

struct usbkey_plugin_st {
    char device[8] ;       // "sda", "sdb" etc.
    char mountpoint[128] ;
} ;


// put usb key plug in process here for convieniece
void dvr_usbkey_plugin()
{
#ifdef PWII_APP
	int rb ;
	char officerid[128] ;
    FILE * fpw = fopen("/var/dvr/pwofficerid", "r" );
    if( fpw ) {
		rb = fread( officerid, 1, 120, fpw );
		fclose( fpw );
		if( rb > 0 ) {
			officerid[rb]=0 ;	// null terminate

            // select a new offer ID.
			strcpy( g_policeid, officerid ) ;

			// generate new id list
			array <string> idlist ;
			FILE * fid ;
			int ididx = 0 ;
			idlist[ididx++] = g_policeid ;
			fid=fopen(g_policeidlistfile, "r");
			if( fid ) {
				while( fgets(officerid, 100, fid) ) {
					str_trimtail(officerid);
					if( strlen(officerid)<=0 ) continue ;
					if( strcmp(officerid, g_policeid)==0 ) continue ;
					idlist[ididx++]=officerid ;
				}
				fclose(fid);
			}

			// writing new id list to file
			fid=fopen(g_policeidlistfile, "w");
			if( fid ) {
				for( ididx=0; ididx<idlist.size(); ididx++) {
					fprintf(fid, "%s\n", (char *)idlist[ididx] );
				}
				fclose( fid ) ;
			}

			// let screen display new policeid
			dvr_log( "New Police ID detected : %s", g_policeid );
			screen_menu(1);
		}
	}
#endif
}

void dvrsvr::ReqUsbkeyPlugin()
{
    int res = 0 ;
#ifdef PWII_APP
    struct usbkey_plugin_st * usbkeyplugin ;
    struct dvr_ans ans ;
#endif
    if( m_recvbuf && m_recvlen>=(int)sizeof( struct usbkey_plugin_st ) ) {
#ifdef PWII_APP
        // check pwii police ID key
        usbkeyplugin = (struct usbkey_plugin_st *)m_recvbuf ;
        // get device serial ID
        char serialid[256] ;
        if( getUSBkeyserialid(usbkeyplugin->device, serialid) ) {
            FILE * tvskeyfile ;
            char tvskeyfilename[256] ;
            sprintf( tvskeyfilename, "%s/tvs/tvskey.dat", usbkeyplugin->mountpoint );
            tvskeyfile = fopen( tvskeyfilename, "rb" );
            if( tvskeyfile ) {
                fseek( tvskeyfile, 0, SEEK_END ) ;
                int fsize = ftell( tvskeyfile );
                char * keydata = new char [fsize+20] ;
                if( keydata ) {
                    fseek( tvskeyfile, 0, SEEK_SET ) ;
                    fread( keydata, 1, fsize, tvskeyfile ) ;
                    keydata[fsize]=0 ;
                    if( PoliceIDCheck( keydata, fsize, serialid ) ) {
                        ans.data = 0;
                        ans.anssize=0;
                        ans.anscode = ANSOK ;
                        Send( &ans, sizeof(ans));
                        res = 1 ;
                    }
                    delete keydata ;
                }
                fclose( tvskeyfile ) ;
            }
        }
#endif
    }
    if( res == 0 ) {
        AnsError();
    }
}

// client side support


// open live stream from remote dvr
int dvr_openlive(int sockfd, int channel)
{
    struct dvr_req req ;
    struct dvr_ans ans ;

    req.reqcode=REQOPENLIVE ;
    req.data=channel ;
    req.reqsize=0;
    net_clean(sockfd);
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


// SCREEN service

int dvr_screen_setmode(int sockfd, int videostd, int screennum)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQSCREEENSETMODE ;
    req.data=screennum ;
    req.reqsize=sizeof(videostd) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, &videostd, sizeof(videostd));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

// stop play this channel
int dvr_screen_stop(int sockfd, int channel)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQSCREENSTOP ;
    req.data=channel ;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_screen_live(int sockfd, int channel)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQSCREEENLIVE ;
    req.data=channel ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

// speed=4: normal speed
int dvr_screen_play(int sockfd, int channel, int speed)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQSCREENPLAY ;
    req.data=channel ;
    req.reqsize=sizeof(speed) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, &speed, sizeof(speed));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

// input play back data
int dvr_screen_playinput(int sockfd, int channel, void * inputbuf, int inputsize)
{
    struct dvr_req req ;
    //	struct dvr_ans ans ;
    req.reqcode=REQSRCEENINPUT ;
    req.data=channel ;
    req.reqsize=inputsize;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, inputbuf, inputsize );
    // mask off for transmit performance
/*
    if( net_recv(sockfd, &ans, sizeof(ans))) {
      if( ans.anscode==ANSOK ) {
        return TRUE ;
      }
    }
    return FALSE ;
*/
    return TRUE;
}

// turn on/off audio channel
int dvr_screen_audio(int sockfd, int channel, int onoff)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQSCREENAUDIO ;
    req.data=channel ;
    req.reqsize=sizeof(onoff) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, &onoff, sizeof(onoff));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

// get screen width
int dvr_screen_width(int sockfd)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQGETSCREENWIDTH ;
    req.data=0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSGETSCREENWIDTH ) {
            return ans.data ;
        }
    }
    return FALSE ;
}

// get screen height
int dvr_screen_height(int sockfd)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQGETSCREENHEIGHT ;
    req.data=0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSGETSCREENHEIGHT ) {
            return ans.data ;
        }
    }
    return FALSE ;
}

// set draw clip area
int dvr_draw_setarea(int sockfd, int x, int y, int w, int h)
{
    int param[4] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWSETAREA ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    param[0]=x ;
    param[1]=y ;
    param[2]=w ;
    param[3]=h ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, &param, req.reqsize );
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_refresh(int sockfd)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWREFRESH ;
    req.data=0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_setcolor(int sockfd, UINT32 color)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWSETCOLOR ;
    req.data=color ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

UINT32 dvr_draw_getcolor(int sockfd)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWGETCOLOR ;
    req.data=0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSDRAWGETCOLOR ) {
            return ans.data ;
        }
    }
    return FALSE ;
}


int dvr_draw_setpixelmode(int sockfd, int pixelmode)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWSETPIXELMODE ;
    req.data=pixelmode ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}


int dvr_draw_getpixelmode(int sockfd)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWGETPIXELMODE ;
    req.data=0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSDRAWGETPIXELMODE ) {
            return ans.data ;
        }
    }
    return FALSE ;
}

int dvr_draw_putpixel(int sockfd, int x, int y, UINT32 color)
{
    int param[2] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=x ;
    param[1]=y ;
    req.reqcode=REQDRAWPUTPIXEL ;
    req.data=color ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

UINT32 dvr_draw_getpixel(int sockfd, int x, int y)
{
    int param[2] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=x ;
    param[1]=y ;
    req.reqcode=REQDRAWGETPIXEL ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSDRAWGETPIXEL ) {
            return ans.data ;
        }
    }
    return FALSE ;
}

UINT32 dvr_draw_line(int sockfd, int x1, int y1, int x2, int y2 )
{
    int param[4] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=x1 ;
    param[1]=y1 ;
    param[2]=x2 ;
    param[3]=y2 ;
    req.reqcode=REQDRAWLINE ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_rect(int sockfd, int x, int y, int w, int h )
{
    int param[4] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=x ;
    param[1]=y ;
    param[2]=w ;
    param[3]=h ;
    req.reqcode=REQDRAWRECT ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_fillrect(int sockfd, int x, int y, int w, int h)
{
    int param[4] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=x ;
    param[1]=y ;
    param[2]=w ;
    param[3]=h ;
    req.reqcode=REQDRAWFILL ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_circle(int sockfd, int cx, int cy, int r)
{
    int param[3] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=cx ;
    param[1]=cy ;
    param[2]=r ;
    req.reqcode=REQDRAWCIRCLE ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_fillcircle(int sockfd, int cx, int cy, int r)
{
    int param[3] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=cx ;
    param[1]=cy ;
    param[2]=r ;
    req.reqcode=REQDRAWFILLCIRCLE ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_bitmap(int sockfd, struct BITMAP * bmp, int dx, int dy, int sx, int sy, int w, int h )
{
    int param[6] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=dx ;
    param[1]=dy ;
    param[2]=sx ;
    param[3]=sy ;
    param[4]=w ;
    param[5]=h ;
    req.reqcode=REQDRAWBITMAP ;
    req.data=0 ;
    req.reqsize=sizeof(param)+sizeof(struct BITMAP)+(bmp->height * bmp->bytes_per_line) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, sizeof(param));
    net_send(sockfd, bmp, sizeof(struct BITMAP));
    net_send(sockfd, bmp->bits, bmp->height * bmp->bytes_per_line );
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_stretchbitmap(int sockfd, struct BITMAP * bmp, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh )
{
    int param[8] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=dx ;
    param[1]=dy ;
    param[2]=dw ;
    param[3]=dh ;
    param[4]=sx ;
    param[5]=sy ;
    param[6]=sw ;
    param[7]=sh ;
    req.reqcode=REQDRAWSTRETCHBITMAP ;
    req.data=0 ;
    req.reqsize=sizeof(param)+sizeof(struct BITMAP)+(bmp->height * bmp->bytes_per_line) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, sizeof(param));
    net_send(sockfd, bmp, sizeof(struct BITMAP));
    net_send(sockfd, bmp->bits, bmp->height * bmp->bytes_per_line );
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_readbitmap(int sockfd, struct BITMAP * bmp, int x, int y, int w, int h )
{
    int param[4] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    param[0]=x ;
    param[1]=y ;
    param[2]=w ;
    param[3]=h ;
    req.reqcode=REQDRAWREADBITMAP ;
    req.data=0 ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, param, sizeof(param));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSDRAWREADBITMAP ) {
            int bitsize ;
            net_recv(sockfd, bmp, sizeof(struct BITMAP));
            bmp->bits = NULL ;
            bitsize = ans.anssize - sizeof(struct BITMAP) ;
            if( bitsize>0 ) {
                bmp->bits = (UINT8 *)malloc(bitsize) ;
                net_recv(sockfd, bmp->bits, bitsize );
                return TRUE ;
            }
        }
    }
    return FALSE ;
}

int dvr_draw_setfont(int sockfd, struct BITMAP * font)
{
    dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWSETFONT ;
    req.data=(int)(long)font ;
    net_clean(sockfd);
    if( font==NULL ) {
        req.reqsize=0 ;
        net_send(sockfd, &req, sizeof(req));
    }
    else {
        req.reqsize=sizeof(struct BITMAP)+(font->height * font->bytes_per_line) ;
        net_send(sockfd, &req, sizeof(req));
        net_send(sockfd, font, sizeof(struct BITMAP));
        net_send(sockfd, font->bits, font->height * font->bytes_per_line );
    }
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_text( int sockfd, int dx, int dy, char * text)
{
    int param[2] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWTEXT ;
    req.data=strlen(text) ;
    req.reqsize=sizeof(param)+req.data+1 ;
    param[0]=dx ;
    param[1]=dy ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, &param, sizeof(param));
    net_send(sockfd, text, req.data+1 );
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

int dvr_draw_textex( int sockfd, int dx, int dy, int fontw, int fonth, char * text)
{
    int param[4] ;
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQDRAWTEXTEX ;
    req.data=strlen(text) ;
    req.reqsize=sizeof(param)+req.data+1 ;
    param[0]=dx ;
    param[1]=dy ;
    param[2]=fontw ;
    param[3]=fonth ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd, &param, sizeof(param));
    net_send(sockfd, text, req.data+1 );
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSOK ) {
            return TRUE ;
        }
    }
    return FALSE ;
}

/*
int dvr_draw_refresh(int sockfd)
{
 struct dvr_req req ;
 struct dvr_ans ans ;
 req.reqcode=REQDRAWREFRESH ;
    req.data=0 ;
    req.reqsize=0;
    net_clean(sockfd);
 net_send(sockfd, &req, sizeof(req));
 if( net_recv(sockfd, &ans, sizeof(ans))) {
  if( ans.anscode==ANSOK ) {
   return TRUE ;
  }
 }
 return FALSE ;
}

// show area (for eagle34, may remove in the fucture)
int dvr_draw_show(int sockfd, int id, int x, int y, int w, int h )
{
 int param[4] ;
 struct dvr_req req ;
 struct dvr_ans ans ;
 param[0]=x ;
 param[1]=y ;
 param[2]=w ;
 param[3]=h ;
 req.reqcode=REQDRAWSHOW ;
    req.data=id ;
    req.reqsize=sizeof(param) ;
    net_clean(sockfd);
 net_send(sockfd, &req, sizeof(req));
 net_send(sockfd, param, sizeof(param));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
  if( ans.anscode==ANSOK ) {
   return TRUE ;
  }
 }
 return FALSE ;
}

// hide area (for eagle34, may remove in the fucture)
int dvr_draw_hide(int sockfd, int id )
{
 struct dvr_req req ;
 struct dvr_ans ans ;
 req.reqcode=REQDRAWHIDE ;
    req.data=id ;
    req.reqsize=0 ;
    net_clean(sockfd);
 net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
  if( ans.anscode==ANSOK ) {
   return TRUE ;
  }
 }
 return FALSE ;
}
*/

// Jpeg Capture
//  channel: capture card channel (0-3)
//  quality: 0=best, 1=better, 2=average
//  pic:     0=cif, 1=qcif, 2=4cif
int dvr_jpeg_capture(int sockfd, struct cap_frame *capframe, int channel, int quality, int pic)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQ2GETJPEG ;
    req.data = (channel&0xff)|((quality&0xff)<<8)|((pic&0xff)<<16) ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANS2JPEG ) {
            capframe->channel = channel ;
            capframe->framesize = ans.anssize ;
            capframe->frametype = ans.data ;
            capframe->framedata = (char *)mem_alloc( capframe->framesize );
            if( capframe->framedata ) {
                net_recv(sockfd, capframe->framedata, capframe->framesize);
                return TRUE ;
            }
        }
    }
    return FALSE ;
}

// Start local capture ( for eagle368 )
int dvr_startcapture(int sockfd )
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQ5STARTCAPTURE ;
    req.data = 0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        return ( ans.anscode==ANSOK ) ;
    }
    return FALSE ;
}

// Stop local capture ( for eagle368 )
int dvr_stopcapture(int sockfd )
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQ5STOPCAPTURE ;
    req.data = 0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        return ( ans.anscode==ANSOK ) ;
    }
    return FALSE ;
}

// Init local shared memory
int dvr_shm_init(int sockfd, char * shmfile )
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQINITSHM ;
    req.data = 0 ;
    req.reqsize=strlen(shmfile)+1 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    net_send(sockfd,shmfile,req.reqsize);
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        return ( ans.anscode==ANSOK ) ;
    }
    return FALSE ;
}


// close local shared memory
int dvr_shm_close(int sockfd)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    req.reqcode=REQCLOSESHM ;
    req.data = 0 ;
    req.reqsize=0 ;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        return ( ans.anscode==ANSOK ) ;
    }
    return FALSE ;
}

// open live stream from local svr, over shared memory
int dvr_openliveshm(int sockfd, int channel)
{
    struct dvr_req req ;
    struct dvr_ans ans ;

    req.reqcode=REQOPENLIVESHM ;
    req.data=channel ;
    req.reqsize=0;
    net_clean(sockfd);
    net_send(sockfd, &req, sizeof(req));
    if( net_recv(sockfd, &ans, sizeof(ans))) {
        if( ans.anscode==ANSSTREAMOPEN ) {
            return 1 ;
        }
    }
    return 0 ;
}

