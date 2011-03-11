
#include "dvr.h"

#define IPCAMERAPORT 15115

ipeagle32_capture::ipeagle32_capture( int channel )
: capture(channel)
{
    config dvrconfig(dvrconfigfile);
    char cameraid[16] ;
    
    m_sockfd = -1 ;
    m_streamfd=-1 ;
    m_state = 0 ;
    m_streamthreadid = 0 ;
    m_type=CAP_TYPE_HIKIP;
    m_ipchannel = 0;
    m_updcounter = 0;
    m_enable=0;
    
    sprintf(cameraid, "camera%d", m_channel+1 );
    
    m_ip = dvrconfig.getvalue( cameraid, "ip" );
    if( m_ip.length()==0 ) {
        return ;							// don't enable this channel without valid ip address
    }
    m_port = dvrconfig.getvalueint( cameraid, "port");
    if( m_port==0 ) {
        m_port = IPCAMERAPORT ;
    }
    m_ipchannel = dvrconfig.getvalueint( cameraid, "ipchannel");
    m_enable = m_attr.Enable ;
}

ipeagle32_capture::~ipeagle32_capture()
{
    stop();
}

/*
void ipeagle32_capture::streamthread()
{
    struct cap_frame capframe ;
    struct hd_frame hdframe ;
    struct hd_subframe subframe ;       // subframes ;
    int  subframes ;                    // number of sub frames
    int i;
    int frametype ;
    int timeout=0 ;
    
    capframe.channel=m_channel ;
    m_streamfd=0 ;
    while( m_state ) {

        if( m_streamfd<=0 ) {
            m_streamfd = net_connect (m_ip.getstring(), m_port) ;
            if( m_streamfd > 0 ) {
                if( dvr_openlive (m_streamfd, m_ipchannel, &m_hd264)<=0 ) {
                    closesocket( m_streamfd ) ;
                    m_streamfd=0 ;
                }
            }
        }
        if( m_streamfd<=0 ) {
            sleep(1);
            continue ;
        }

        // recevie frames
        i = net_recvok(m_streamfd, 100000) ;
        if( i<0 ) {
            break; 
        }
        else if ( i>0 ) {        
            hdframe.flag=0 ;
            if( net_recv (m_streamfd, &hdframe, sizeof(struct hd_frame))>0 ) {
                if( hdframe.flag==1 ) {                                       // .264 frame
                    subframes = HD264_FRAMESUBFRAME(hdframe) - 1 ;
                    frametype =  HD264_FRAMETYPE(hdframe) ;
                    if( frametype==1 ) {                       // audio frame
                        capframe.frametype = FRAMETYPE_AUDIO ;
                    }
                    else if( frametype==4 ) {                  // video frame
                        capframe.frametype = FRAMETYPE_VIDEO ;
                    }
                    else if( frametype==3 && subframes==0 ) {  // key frame
                        capframe.frametype = FRAMETYPE_KEYVIDEO ;               
                    }
                    else {
                        capframe.frametype = FRAMETYPE_UNKNOWN ;
                    }

                    capframe.framesize=sizeof(struct hd_frame)+hdframe.framesize ;
                    capframe.framedata=(char *)mem_alloc (capframe.framesize) ;
                    memcpy( capframe.framedata, &hdframe, sizeof(struct hd_frame) );
                    if( net_recv( m_streamfd, capframe.framedata + sizeof(struct hd_frame), hdframe.framesize )<=0 ) {
                        break;
                    }
                    onframe( &capframe );
                    mem_free( capframe.framedata );			
                    
                    capframe.frametype = FRAMETYPE_UNKNOWN ;
                    
                    if( subframes>0 ) {
                        for( i=0; i<subframes; i++ ) {
                            net_recv( m_streamfd, &subframe, sizeof( struct hd_subframe ) );
                            if( subframe.framesize>0 && subframe.framesize<500000 ) {
                                capframe.framesize = sizeof( struct hd_subframe ) + subframe.framesize ;
                                capframe.framedata = (char *) mem_alloc (capframe.framesize) ;
                                memcpy( capframe.framedata, &subframe, sizeof(subframe) );
                                net_recv( m_streamfd, capframe.framedata+sizeof(subframe), subframe.framesize );
                                onframe(&capframe);
                                mem_free( capframe.framedata );
                            }
                        }
                    }
                    
                    timeout=0 ;
                }
                else {
                    net_clean(m_streamfd);
                }
            }
            else {
                break;                                                          // error or shutdown
            }
        }
        else {
            if( ++timeout> 20 ) {
                closesocket( m_streamfd );
                m_streamfd = 0 ;
            }
        }
    }
    if( m_streamfd>0 ) {
        closesocket(m_streamfd);
        m_streamfd = 0 ;
    }
}
*/

void ipeagle32_capture::streamthread()
{
    struct cap_frame capframe ;
    int i;
    int timeout=0 ;
    
    capframe.channel=m_channel ;
    m_streamfd=0 ;
    while( m_state ) {

        if( m_streamfd<=0 ) {
            usleep(20000);
            m_streamfd = net_connect (m_ip.getstring(), m_port) ;
            if( m_streamfd > 0 ) {
                if( dvr_openlive (m_streamfd, m_ipchannel)<=0 ) {
                    closesocket( m_streamfd ) ;
                    m_streamfd=0 ;
                }
            }
        }
        if( m_streamfd<=0 ) {
            continue ;
        }

        // recevie frames
        i = net_recvok(m_streamfd, 100000) ;
        if( i<0 ) {
            break; 
        }
        else if ( i>0 ) {        
            struct dvr_ans ans ;
            if( net_recv (m_streamfd, &ans, sizeof(struct dvr_ans))>0 ) {
                if( ans.anscode == ANSSTREAMDATA ) {
                    capframe.framesize = ans.anssize ;
                    capframe.frametype = ans.data ;
                    capframe.framedata=(char *)mem_alloc (capframe.framesize) ;
                    if( capframe.framedata ) {
                        if( net_recv( m_streamfd, capframe.framedata, capframe.framesize )>0 ) {
                            onframe(&capframe);
                        }
                        else {
                            net_clean(m_streamfd);
                        }
                        mem_free(capframe.framedata);
                    }
                    else {
                        net_clean(m_streamfd);
                    }
                    timeout=0 ;
                }
            }
            else {
                closesocket( m_streamfd );
                m_streamfd = 0 ;
            }
        }
        else {
            if( ++timeout> 50 ) {
                closesocket( m_streamfd );
                m_streamfd = 0 ;
            }
        }
    }
    if( m_streamfd>0 ) {
        closesocket(m_streamfd);
        m_streamfd = 0 ;
    }
}

static void * ipeagle32_thread(void *param)
{
    ((ipeagle32_capture *)param) ->streamthread();
    return NULL ;
}

int ipeagle32_capture::connect()
{
    struct  DvrChannel_attr attr;
    struct dvrtime dvrt ;
    char * tzenv ;
    
    m_sockfd = net_connect (m_ip.getstring(), m_port) ;
    if( m_sockfd<0 ) {
        return 0;
    }
    // sync time and time zone
    tzenv = getenv("TZ");
    if( tzenv && strlen(tzenv)>0 ) {
        dvr_settimezone(m_sockfd, tzenv) ;
    }
    
    time_utctime( &dvrt );
    dvr_adjtime(m_sockfd, &dvrt) ;
    
    // set ip cam attr
    attr = m_attr ;
    attr.Enable = 1 ;
    attr.RecMode = -1;					// No recording
    
    dvr_setchannelsetup (m_sockfd, m_ipchannel, &attr) ;
    
    updateOSD();
    
    return 1 ;
}

void ipeagle32_capture::start()
{
    if( m_enable ) {
        stop();
        if( connect() ) {
            m_state=1 ;
            pthread_create(&m_streamthreadid, NULL, ipeagle32_thread, this);
            m_started = 1 ;
        }
    }		
}

void ipeagle32_capture::stop()
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

void ipeagle32_capture::setosd( struct hik_osd_type * posd )
{
    if( m_sockfd>0 && m_enable ) {
        dvr_sethikosd(m_sockfd, m_ipchannel, posd);
    }
}

// periodic called (0.125s)
void ipeagle32_capture::update(int updosd)
{
    struct dvrtime dvrt ;
    int chstate ;
    int mot ;
    
    if( !m_enable ) {
        return ;
    }
    
    m_updcounter++ ;
    
    if( m_state==0 ) {
        start();
    }
    
    if( m_sockfd<=0 ) {
        if( m_updcounter%80 == 0 && m_streamfd>0 ) {
            // re-connect
            connect();
        }
        m_signal = 0;
        return ;
    }
    
    if( m_updcounter%2400 == m_ipchannel*1200 ) {
        // sync ip camera time
        time_utctime( &dvrt );
        if( dvr_adjtime(m_sockfd, &dvrt) == 0 ) {	// faile?
            closesocket( m_sockfd );
            m_sockfd = -1 ;
        }
    }
    else if( m_updcounter%9 == 0 ) {
        chstate=dvr_getchstate (m_sockfd, m_ipchannel) ;
        if( chstate == -1 ) {				// failed?
            closesocket( m_sockfd );
            m_sockfd = -1 ;
            m_signal = 0 ;
        }
        else {
            m_signal = chstate & 1 ;
            mot = (chstate&2)!=0 ;
            if( mot!=m_motion ) {
                m_motion= mot ;
                updosd=1 ;
            }
        }
    }
    if( m_state ) {
        capture::update( updosd );
    }
}
