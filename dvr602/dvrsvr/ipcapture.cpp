/* --- Changes ---
 *
 * 05/12/2011 by Harrison
 *   1. Added Jpeg support
 *
 */

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
    m_jpegframe.framesize = 0;
    m_jpegframe.framedata = NULL;
}

ipeagle32_capture::~ipeagle32_capture()
{
    stop();
}

void ipeagle32_capture::streamthread()
{
    struct cap_frame capframe ;
    struct hd_frame hdframe ;
    struct hd_subframe subframe ;       // subframes ;
    int  subframes ;                    // number of sub frames
    int i;
    int frametype ;
    int timeout ;
    
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
                    mem_cpy32( capframe.framedata, &hdframe, sizeof(struct hd_frame) );
//                    memcpy( capframe.framedata, &hdframe, sizeof(struct hd_frame) );
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
                                mem_cpy32( capframe.framedata, &subframe, sizeof(subframe) );
//                                memcpy( capframe.framedata, &subframe, sizeof(subframe) );
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
	    m_started = 1;
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
    m_started = 0;
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
#if 0
int ipeagle32_capture::captureJPEG(int quality, int pic) 
{
  struct dvr_req req ;
  struct dvr_ans ans ;
  int jpeg_quality, jpeg_pic ;
  int total = 0, r, res = 0;
  char buf[2048] ;

  int sockfd = net_connect(m_ip.getstring(), m_port);
  if( sockfd>0 ) {    
    jpeg_quality = 1;
    jpeg_pic = 2;
    if ((quality >= 0) && (quality <= 2)) {
      jpeg_quality = quality;
    }
    if ((pic >= 0) && (pic <= 2)) {
      jpeg_pic = pic;
    }
   
    req.reqcode=REQ2GETJPEG ;
    req.data=(m_ipchannel&0xff) | ( (jpeg_quality&0xff)<<8) |
      ((jpeg_pic&0xff)<<16) ;
    req.reqsize=0 ;
    if( net_send(sockfd, &req, sizeof(req)) ) {
      if( net_recv(sockfd, &ans, sizeof(ans))) {
	if( ans.anscode==ANS2JPEG && ans.anssize>0 ) {
	  struct cap_frame capframe;
	  capframe.channel = m_channel ;
	  capframe.framesize = ans.anssize;
	  capframe.frametype = FRAMETYPE_JPEG ;
	  capframe.framedata = (char *) mem_alloc( capframe.framesize );

	  while( total < ans.anssize &&
		 (r=recv(sockfd, buf, sizeof(buf), 0) )>0 ){
	    if( capframe.framedata ) {
	      int to_copy = r;
	      if (to_copy > (ans.anssize - total)) {
		to_copy = ans.anssize - total;
	      }
	      mem_cpy32(capframe.framedata + total, buf, to_copy) ;
	      total += to_copy;
	    }
	  }
	  if (total == ans.anssize) {
	    res = 1;
	    onframe(&capframe);
	    mem_free(capframe.framedata);
	  }
	}
      }
    }
    close( sockfd );
  }
  return res ;
}
#endif
int ipeagle32_capture::captureJPEG(int quality, int pic) 
{
  struct dvr_req req ;
  struct dvr_ans ans ;
  int jpeg_quality, jpeg_pic ;
  int total = 0, r, res = 0;
  char buf[2048], *framedata = NULL;

  int sockfd = net_connect(m_ip.getstring(), m_port);
  if( sockfd>0 ) {    
    jpeg_quality = 1;
    jpeg_pic = 2;
    if ((quality >= 0) && (quality <= 2)) {
      jpeg_quality = quality;
    }
    if ((pic >= 0) && (pic <= 2)) {
      jpeg_pic = pic;
    }
   
    req.reqcode=REQ2GETJPEG ;
    req.data=(m_ipchannel&0xff) | ( (jpeg_quality&0xff)<<8) |
      ((jpeg_pic&0xff)<<16) ;
    req.reqsize=0 ;
    if( net_send(sockfd, &req, sizeof(req)) ) {
      if( net_recv(sockfd, &ans, sizeof(ans))) {
	if( ans.anscode==ANS2JPEG && ans.anssize>0 ) {
	  framedata = (char *) mem_alloc(ans.anssize);
	  while( total < ans.anssize &&
		 (r=recv(sockfd, buf, sizeof(buf), 0) )>0 ){
	    if( framedata ) {
	      int to_copy = r;
	      if (to_copy > (ans.anssize - total)) {
		to_copy = ans.anssize - total;
	      }
	      mem_cpy32(framedata + total, buf, to_copy) ;
	      total += to_copy;
	    }
	  }
	  if (total == ans.anssize && framedata) {
	    m_jpegframe.channel = m_channel ;
	    m_jpegframe.framesize = ans.anssize;
	    m_jpegframe.frametype = FRAMETYPE_JPEG ;
	    m_jpegframe.framedata = framedata;
	    res = 1;
	  }
	}
      }
    }
    close( sockfd );
  }

  if (framedata && !res)
    mem_free(framedata);

  return res ;
}

void ipeagle32_capture::docaptureJPEG()
{
  if (m_jpegframe.framesize > 0 && m_jpegframe.framedata) {
    fprintf(stderr, "docaptureJPEG:%d2\n", m_jpegframe.framesize);
    onframe(&m_jpegframe);
    mem_free(m_jpegframe.framedata);
    m_jpegframe.framesize = 0;
    m_jpegframe.framedata = NULL;
  }
}
