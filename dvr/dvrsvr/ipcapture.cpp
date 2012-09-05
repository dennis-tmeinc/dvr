
#include "dvr.h"

ipeagle32_capture::ipeagle32_capture( int channel )
: capture(channel)
{
    config dvrconfig(CFG_FILE);
    char cameraid[16] ;

    m_sockfd = -1 ;
    m_state = 0 ;
    m_streamthreadid = 0 ;
    m_type=CAP_TYPE_HIKIP;
    m_ipchannel = 0;
    m_enable=0;

    m_timesynctimer = g_timetick + 1000000 ;    // to make sure camera time be update right away
    m_updtimer = 0 ;

    sprintf(cameraid, "camera%d", m_channel+1 );

    m_ip = dvrconfig.getvalue( cameraid, "ip" );
    if( m_ip.length()==0 ) {
        m_ip="127.0.0.1" ;				// default point to localhost
    }
    m_port = dvrconfig.getvalueint( cameraid, "port");
    if( m_port==0 ) {
        m_port = EAGLESVRPORT ;
    }
    m_ipchannel = dvrconfig.getvalueint( cameraid, "channel");

    // init shared memory
    m_shm_enabled = dvrconfig.getvalueint( cameraid, "shm");
    if( m_shm_enabled ) {
        void * shmtest = mem_shm_alloc(20) ;
        if( shmtest ) {             // shm initialized ?
            mem_free( shmtest );
        }
        else {
            // init shared memory
            int shm_size = dvrconfig.getvalueint( "system", "shm_size");
            if( shm_size< (1*1024*1024) ) {         // minimum 1M
                shm_size = (4*1024*1024) ;          // default 4M
            }
            const char * shm_file = dvrconfig.getvalue("system", "shm_file") ;
            if( shm_file == NULL || shm_file[0]==0 ) {
                shm_file = "/tmp/dvrshare" ;
            }

            dvr_log("Init shared memory %s size:%d", shm_file, shm_size);
            mem_shm_init(shm_file, shm_size);
        }
    }

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


void ipeagle32_capture::streamthread_net()
{
    struct cap_frame capframe ;
    int recvok;
    int recvact=g_timetick ;

    capframe.channel=m_channel ;
    int streamfd = 0 ;
    while( m_state ) {
        if( streamfd<=0 ) {
            usleep(100000);           // wait a bit, for EAGLE368 it should wait until all channels setting up
            streamfd = net_connect (m_ip, m_port) ;
            if( streamfd > 0 ) {
                if( dvr_openlive (streamfd, m_ipchannel)<=0 ) {
//                    dvr_log("dvr openlive failed!");
                    closesocket( streamfd ) ;
                    streamfd=0 ;
                }
            }
        }
        if( streamfd<=0 ) {
            continue ;
        }

        // recevie frames
        recvok = net_recvok(streamfd, 100000) ;
        if( recvok>0 ) {
            struct dvr_ans ans ;
            if( net_recv (streamfd, &ans, sizeof(struct dvr_ans))>0 ) {
                if( ans.anscode == ANSSTREAMDATA ) {
                    capframe.framesize = ans.anssize ;
                    capframe.frametype = ans.data ;
                    capframe.framedata=(char *)mem_alloc (capframe.framesize) ;
                    if( capframe.framedata ) {
                        int nr = net_recv( streamfd, capframe.framedata, capframe.framesize ) ;
                        if( nr>0 ) {
                            onframe(&capframe);
                        }
                        else {
                            closesocket( streamfd );
                            streamfd = 0 ;
                        }
                        mem_free(capframe.framedata);
                    }
                    else {
                        dvr_log( "ipcapture:no enought memory!") ;
                        closesocket( streamfd );
                        streamfd = 0 ;
                    }
                }
                else {
                    dvr_log( "ipcapture:error streaming header");
                    closesocket( streamfd );
                    streamfd = 0 ;
                }
            }
            else {
                dvr_log( "ipcapture:error recv ans");
                closesocket( streamfd );
                streamfd = 0 ;
            }
            recvact=g_timetick ;
        }
        else if( recvok==0) {       // recvok timeout
            if( g_timetick-recvact>30000 ) {        // no active for 30 seconds?
//                dvr_log( "ipcapture:socket time out");
                closesocket( streamfd );
                streamfd = 0 ;
                recvact=g_timetick ;
            }
        }
        else {                      // error
            break;
        }
    }
    if( streamfd>0 ) {
        closesocket(streamfd);
    }
}

// get live stream over sh mem
void ipeagle32_capture::streamthread_shm()
{
    int dvr_openliveshm(int sockfd, int channel) ;

    struct cap_frame capframe ;
    int recvok;
    int recvact=g_timetick ;

    capframe.channel=m_channel ;
    int streamfd = 0 ;
    while( m_state ) {
        if( streamfd<=0 ) {
            usleep(100000);           // wait a bit, for EAGLE368 it should wait until all channels setting up
            streamfd = net_connect (m_ip, m_port) ;
            if( streamfd > 0 ) {
                if( dvr_openliveshm (streamfd, m_ipchannel)<=0 ) {
                    dvr_log("dvr openlive failed!");
                    closesocket( streamfd ) ;
                    streamfd=0 ;
                }
            }
        }
        if( streamfd<=0 ) {
            continue ;
        }

        // recevie frames
        recvok = net_recvok(streamfd, 100000) ;
        if( recvok>0 ) {
            struct dvr_ans ans ;
            memset( &ans, 0, sizeof(ans));
            if( net_recv (streamfd, &ans, sizeof(struct dvr_ans))>0 ) {
                if( ans.anscode == ANSSTREAMDATASHM ) {
                    int frameinfo[10] ;
                    if( ans.anssize>0 ) {
                        net_recv(streamfd, frameinfo, ans.anssize ) ;
                    }
                    capframe.framesize = frameinfo[1] ;
                    capframe.frametype = ans.data ;
                    char * framedata =(char *)mem_shm_byindex( frameinfo[0] ) ;
                    if( framedata ) {
                        capframe.framedata = (char *)mem_alloc(capframe.framesize) ;
                        if( capframe.framedata ) {
                            mem_cpy32(capframe.framedata,framedata,capframe.framesize);
                            onframe(&capframe);
                            mem_free(capframe.framedata);
                        }
                        mem_free(framedata);
                    }
                    else {
                        dvr_log( "ipcapture:shared memory error!") ;
                        closesocket( streamfd );
                        streamfd = 0 ;
                    }
                }
                else if( ans.anscode == ANSSTREAMDATA ) {
                    capframe.framesize = ans.anssize ;
                    capframe.frametype = ans.data ;
                    capframe.framedata=(char *)mem_alloc (capframe.framesize) ;
                    if( capframe.framedata ) {
                        int nr = net_recv( streamfd, capframe.framedata, capframe.framesize ) ;
                        if( nr>0 ) {
                            onframe(&capframe);
                        }
                        else {
                            closesocket( streamfd );
                            streamfd = 0 ;
                        }
                        mem_free(capframe.framedata);
                    }
                    else {
                        dvr_log( "ipcapture:no enought memory!") ;
                        closesocket( streamfd );
                        streamfd = 0 ;
                    }
                }
                else {
                    dvr_log( "ipcapture:error streaming header");
                    closesocket( streamfd );
                    streamfd = 0 ;
                }
            }
            else {
                dvr_log( "ipcapture:error recv ans");
                closesocket( streamfd );
                streamfd = 0 ;
            }
            recvact=g_timetick ;
        }
        else if( recvok==0) {       // recvok timeout
            if( g_timetick-recvact>30000 ) {        // no active for 30 seconds?
                dvr_log( "ipcapture:socket time out");
                closesocket( streamfd );
                streamfd = 0 ;
                recvact=g_timetick ;
            }
        }
        else {                      // error
            break;
        }
    }
    if( streamfd>0 ) {
        closesocket(streamfd);
    }
}

// get live stream over sh mem
void ipeagle32_capture::streamthread()
{
    if( m_shm_enabled ) {
        streamthread_shm();
    }
    else {
        streamthread_net();
    }
}

static void * ipeagle32_thread(void *param)
{
    ((ipeagle32_capture *)param) ->streamthread();
    return NULL ;
}

int ipeagle32_capture::channelsetup( int socket )
{
    struct  DvrChannel_attr attr;
    // set ip cam attr
    attr = m_attr ;
    attr.Enable = 1 ;
    attr.RecMode = -1;					// No recording
    dvr_setchannelsetup (socket, m_ipchannel, &attr) ;
}


int ipeagle32_capture::connect()
{
    struct dvrtime dvrt ;
    char * tzenv ;

    m_sockfd = net_connect (m_ip, m_port) ;
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

    channelsetup( m_sockfd ) ;

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

    return 1 ;
}

void ipeagle32_capture::start()
{
    if( m_enable ) {
        if( !m_started ) {
            if( connect() ) {
                m_state=1 ;
                pthread_create(&m_streamthreadid, NULL, ipeagle32_thread, this);
                m_started = 1 ;
            }
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

// periodically called
void ipeagle32_capture::update(int updosd)
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
        if( g_timetick - m_timesynctimer>600000 || g_timetick - m_timesynctimer < 0 ) {
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

// eagle368 specific
int ipeagle32_capture::eagle368_startcapture()
{
    dvr_log("start eagle368!");
    if( m_started && m_sockfd>0 ) {
        dvr_startcapture(m_sockfd);
    }
}

// eagle368 specific
int ipeagle32_capture::eagle368_stopcapture()
{
    dvr_log("stop eagle368!");
    if( m_started && m_sockfd>0 ) {
        dvr_stopcapture(m_sockfd);
    }
}
