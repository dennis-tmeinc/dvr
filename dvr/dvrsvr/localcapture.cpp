
#include "dvr.h"
#include "localcapture.h"

localcapture::localcapture( int channel )
: capture(channel)
{
    config dvrconfig(CFG_FILE);
    char cameraid[16] ;

    m_type=CAP_TYPE_HIKLOCAL;

    m_timesynctimer = g_timetick + 1000000 ;    // to make sure camera time be update right away
    m_updtimer = 0 ;

    sprintf(cameraid, "camera%d", m_channel+1 );
    m_localchannel = dvrconfig.getvalueint( cameraid, "channel");

    m_enable = m_attr.Enable ;
}

localcapture::~localcapture()
{
    stop();
}

void localcapture::start()
{
    if( m_enable ) {
        if( !m_started ) {
            struct  DvrChannel_attr attr;
            // set ip cam attr
            attr = m_attr ;
            attr.Enable = 1 ;
            attr.RecMode = -1;					// No recording mode on ip cam

            eagle_cap_setattr( m_localchannel, &attr );

            eagle_dvr_setchannelsetup (m_sockfd, m_ipchannel, &attr) ;

            if( m_shm_enabled ) {
                void * shmtest = mem_shm_alloc(20) ;
                if( shmtest ) {             // shm initialized ?
                    mem_free( shmtest );
                    //
                    dvr_shm_init( m_sockfd, mem_shm_filename() ) ;
                }
                else {
                    m_shm_enabled = 0 ;
                }
            }

            if( connect() ) {
                m_state=1 ;
                pthread_create(&m_streamthreadid, NULL, ipeagle32_thread, this);
                m_started = 1 ;
            }
        }
    }
}

void localcapture::stop()
{
    m_state = 0;
    if( m_sockfd>0 ) {
        closesocket( m_sockfd ) ;
    }
    m_sockfd = -1 ;
    if( m_streamthreadid ) {
        pthread_join(m_streamthreadid, NULL);
    }
    m_streamthreadid = 0 ;
    m_started = 0 ;
}

void localcapture::setosd( struct hik_osd_type * posd )
{
    if( m_sockfd>0 && m_enable ) {
        dvr_sethikosd(m_sockfd, m_ipchannel, posd);
    }
}

// periodically called
void localcapture::update(int updosd)
{
    if( !m_enable ) {
        return ;
    }

    if( m_state==0 ) {
        start();
    }

    if( m_sockfd<=0 ) {
        connect();
        return ;
    }
    else {
        if( !m_local && (g_timetick - m_timesynctimer>600000 || g_timetick - m_timesynctimer < 0) ) {
            // sync ip camera time
            struct dvrtime dvrt ;
            time_utctime( &dvrt );
            if( dvr_adjtime(m_sockfd, &dvrt) == 0 ) {	// faile?
                closesocket( m_sockfd );
                m_sockfd = -1 ;
                return ;
            }
            m_timesynctimer = g_timetick ;
        }

        if( g_timetick - m_updtimer>500 || g_timetick - m_updtimer < 0 ) {
            // sync camera state
            int chstate ;
            chstate=dvr_getchstate (m_sockfd, m_ipchannel) ;
            if( chstate == -1 ) {				// failed?
                closesocket( m_sockfd );
                m_sockfd = -1 ;
                return ;
            }
            else {
                int motion = (chstate&2)!=0 ;
                if( motion!=m_motion ) {
                    m_motion = motion ;
                    updosd=1 ;
                }
                m_signal = chstate & 1 ;
                m_updtimer = g_timetick ;
            }
        }
    }
    if( m_state ) {
        capture::update( updosd );
    }
}

#ifdef EAGLE368

// eagle368 specific
int localcapture::eagle368_startcapture()
{
    dvr_log("start eagle368!");
    if( m_started && m_sockfd>0 ) {
        dvr_startcapture(m_sockfd);
    }
}

// eagle368 specific
int localcapture::eagle368_stopcapture()
{
    dvr_log("stop eagle368!");
    if( m_started && m_sockfd>0 ) {
        dvr_stopcapture(m_sockfd);
    }
}

#endif
