

#include "dvr.h"

struct dvr_msg {
    unsigned long code;
    int session;	// answering package will use same session number
    int seqno;		// sequence number of the session start from 0
};

class msg_svr * msg_head ;

class msg_svr : public dvrsvr {
    int m_session ;
    int seqno ;
    struct sockad m_peer ;
    struct timeval m_tv ;       // timer
    public:
        msg_svr(int session, struct sockad * from) : dvrsvr(msgfd)
     {
         m_session=session ;
         memcpy(&m_peer, from, sizeof(m_peer));
     };
        
        virtual ~msg_svr()
     {
         msg_svr * walk ;
         
         if (msg_head == this) {
             msg_head = (msg_svr *)m_next;
         } else {
             walk = msg_head;
             while (walk->m_next != NULL) {
                 if ((msg_svr *)walk->m_next == this) {
                     walk->m_next = this->m_next;
                     break;
                 }
                 walk = (msg_svr *)walk->m_next;
             }
         }
     }
        
        void queue(){
            m_next = msg_head;
            m_head = this;
        }
        
        int checksvr(int session, struct sockad *from) {
            if( session==m_session &&
               from->addrlen == m_peer.addrlen &&
               memcmp( from, &m_peer, m_peer.addrlen )==0 ) {
                   gettimeofday( &m_tv, NULL );
                   return 1;
               }
            else {
                return 0;
            }	
        }
        int onmsg(void *msgbuf, int msgsize )
     {
         struct dvr_msg * dmsg ;
         struct dvr_req * preq ;
         m_recvbuf = (char *)msgbuf ;
         dmsg = (struct dvr_msg *)m_recvbuf ;
         m_recvbuf += sizeof(struct dvr_msg);
         preq = (struct dvr_req *)m_recvbuf ;
         m_recvbuf += sizeof(struct dvr_req);
         m_recvlen = msgsize-sizeof(struct dvr_req)-sizeof(struct dvr_msg);
         if( dmsg->seqno!=0 ) 
             return 1 ;				// not supported yet!!!
         if( m_recvlen < preq->reqsize ) {
             return 1 ;				// package not finish, but not support yet!!
         }
         
         // do dvr service
         onrequest();
         
         m_recvbuf=NULL;
         m_recvlen=0;		
         m_req.reqcode = 0;
         m_req.reqsize = 0;
         return 0;
     }	
        virtual void Send(void * buf, int bufsize)
     {
         int msgsize = bufsize+sizeof( struct dvr_msg );
         char * sendbuf = (char *)mem_alloc( msgsize );
         struct dvr_msg * pmsg = (struct dvr_msg *) sendbuf ;
         pmsg->code = 1 ;
         pmsg->session = m_session ;
         pmsg->seqno = ++seqno;
         memcpy( sendbuf+sizeof(struct dvr_msg), buf, bufsize );
         sendto( msgfd, (void *)sendbuf, msgsize, 0, &(m_peer.addr), m_peer.addrlen);
         mem_free( sendbuf );
     }
        int clean(){
            return (1) ;
        }
};

msg_svr * findmsg(int session, struct sockad * from)
{
    msg_svr * msg =msg_head ;
    while(msg!=NULL) {
        if( msg->checksvr( session, from ) )
            return msg ;
        msg = (msg_svr *)msg->next();
    }
    return NULL ;
}

int msg_onmsg(void *msgbuf, int msgsize, struct sockad *from)
{
    msg_svr * msg ;
    struct dvr_msg * dmsg;
    dmsg = (struct dvr_msg *) msgbuf;
    if( dmsg->code != DVRMSG || msgsize<=(int)sizeof(struct dvr_msg)) 
        return 1;
    msg=findmsg( dmsg->session, from );
    if( msg==NULL ) {
        msg=new msg_svr(dmsg->session, from);
        msg->queue();
    }
    return msg->onmsg(msgbuf, msgsize);
}

void msg_clean()
{
    msg_svr * walk ;
    msg_svr * n ;
    
    walk = msg_head ;
    while( walk!=NULL ) {
        n=(msg_svr *)walk->next() ;
        if( walk->clean() )
            delete walk ;
        walk=n ;
    }
}

void msg_init()
{
    msg_head=NULL ;
}

void msg_uninit()
{
    while(msg_head!=NULL) {
        delete msg_head ;
    }
}
