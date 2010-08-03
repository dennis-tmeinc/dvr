
#include "dvr.h"

#define DAYLENGTH (24*60*60)

live::live( int channel )
{
    m_channel=channel ;
    m_fifo=NULL ;
    m_framebuf=NULL ;
    m_framesize=0 ;
}

live::~live()
{
    net_fifo * pfifo ;
    while( m_fifo ) {
        pfifo=m_fifo ;
        m_fifo=pfifo->next ;
        mem_free( pfifo->buf );
        mem_free( pfifo );
    }
    if( m_framebuf ) {
        mem_free( m_framebuf );
    }
}

void live::getstreamdata(void ** getbuf, int * getsize, int * frametype)
{
    net_fifo * pfifo ;
    if( m_framebuf ) {
        mem_free( m_framebuf );
        m_framebuf=NULL ;
    }
    if( m_fifo ) {
        m_framebuf = m_fifo->buf ;
        m_framesize = m_fifo->bufsize ;
        pfifo=m_fifo ;
        m_fifo=pfifo->next ;
        *getbuf = m_framebuf ;
        *getsize = m_framesize ;
        *frametype = FRAMETYPE_UNKNOWN ;
        mem_free( pfifo );
    }
    else {
        *getsize=0 ;
    }
}

void live::onframe(cap_frame * pframe ) 
{
    net_fifo *pfifo, *tfifo, *nfifo;
    if( m_channel == pframe->channel) {
        if (pframe->frametype == FRAMETYPE_KEYVIDEO) {	// for keyframes
            if (m_fifo != NULL && m_fifo->next != NULL) {
                pfifo = m_fifo->next;
                m_fifo->next = NULL;
                
                while (pfifo != NULL) {
                    tfifo = pfifo->next;
                    mem_free(pfifo->buf);
                    mem_free(pfifo);
                    pfifo = tfifo;
                }
            }
        }
        nfifo = (net_fifo *) mem_alloc(sizeof(net_fifo));
        nfifo->buf = (char *)mem_ref(pframe->framedata, pframe->framesize);
        nfifo->next = NULL;
        nfifo->bufsize = pframe->framesize;
        nfifo->loc = 0;
        if (m_fifo == NULL) {
            m_fifo = nfifo;
        } else {
            pfifo = m_fifo;
            while (pfifo->next != NULL)
                pfifo = pfifo->next;
            pfifo->next = nfifo;
        }
    }
}
