
#include "dvr.h"

#define DAYLENGTH (24*60*60)

//#define DEBUG_PLAYBACK
#ifdef DEBUG_PLAYBACK
FILE *fp=NULL;
#endif
static int norecplayback = 1 ;
int net_playarc=0;
playback::playback( int channel )
{
    m_channel=channel ;
    net_playarc=50;
    if( norecplayback ) {
        rec_pause = 850 ;            // temperary pause recording, for 5 seconds
    }    
    disk_getdaylist(m_daylist);         // get data available days
    if( norecplayback ) {
        rec_pause = 100 ;            // temperary pause recording, for 5 seconds
    }     
#ifdef DEBUG_PLAYBACK    
    if(m_channel==0){
       fp=fopen("/mnt/raw/videoPES.264","wb");      
    }
#endif    
    net_playarc=10;    
    m_day=-1 ;                          // invalid day
    m_curfile=0 ;
    time_dvrtime_init(&m_streamtime, 2000);
    time_dvrtime_init(&m_lasttime,2000);
    m_framebuf=NULL ;
    m_framesize=0 ;
    m_frametype=FRAMETYPE_UNKNOWN ;
    opennextfile() ;                    // actually open the first file
}

playback::~playback()
{
    close();
#ifdef DEBUG_PLAYBACK    
    if(fp){
       fclose(fp);
       fp=NULL;
    }
#endif    
}

DWORD playback::getcurrentfileheader()
{
  if (m_file.isopen())
    return m_file.getfileheader();
  return 0;
}

int playback::close()
{
    if( m_framebuf ) {
        mem_free( m_framebuf );
        m_framebuf=NULL ;
        m_framesize=0 ;
    }
    if( m_file.isopen() ) {
        m_file.close();
    }
    return 0;
}

int playback::seek( struct dvrtime * seekto )
{
    int bcdseekday ;
#if 0       
    m_streamtime=*seekto ;
    dvr_log("channel:%d seek:year:%d month:%d day:%d hour:%d minute:%d second:%d msec:%d\n",m_channel,
	   seekto->year,seekto->month,seekto->day,seekto->hour,seekto->minute,seekto->second,seekto->milliseconds);
#endif	   
  /*	  
    if(m_file.isopen()){
       printf("before seek:channel:%d pos:%d\n",m_channel,m_file.tell()); 
    }*/
    if( m_file.isopen() && m_file.seek( seekto ) ) {	// within current opened file?
        goto seek_end ;
    }
    close();
    
    bcdseekday = seekto->year*10000 + seekto->month*100 + seekto->day ;
    if( bcdseekday != m_day ) {	// seek to different day? update new day file list
        m_filelist.setsize(0);	// empty day file list
        m_day=bcdseekday ;
        // load new day file list
        disk_listday( m_filelist, seekto, m_channel );
    }
    
    // seek inside m_day (m_filelist)
    for(m_curfile=0; m_curfile<m_filelist.size(); m_curfile++) {
        int    fl ;		// file length
        dvrtime ft ;		// file time
        dvrtime ftend;
        char * filename = m_filelist[m_curfile].getstring() ;
        f264time( filename, &ft );
        fl = f264length( filename );
        ftend = ft + fl ;		// ft become file end time
#if 0        
        if(!(*seekto > ft) ) {	// found the file
            if( m_file.open(filename, "r") ) {	// open success
                m_file.seek(seekto);	// don't need to care if its success
                goto seek_end ;				// done!
            }
        }
#else
       if((*seekto>ft||(*seekto==ft))&&(*seekto<ftend)){
           if( m_file.open(filename, "r") ) {	// open success
                m_file.seek(seekto);	// don't need to care if its success
                  goto seek_end ;// done!
            }	 
       } else if(*seekto<ft){
            if( m_file.open(filename, "r") ) {	// open success
                m_file.seek(seekto);	// don't need to care if its success
                  goto seek_end ;				// done!
            }	 	 
       }
#endif        
    }
    
    // seek beyond the day
    opennextfile();

 seek_end:
    if( m_framebuf ) {
        mem_free( m_framebuf );
        m_framebuf=NULL ;
        m_framesize=0 ;
    }    
   // printf("after seek: channel:%d pos:%d\n",m_channel,m_file.tell());
    if (m_file.isopen() && m_file.isframeencrypt()){
    //  dvr_log("seek file is encrypted");
      return 1;
    }
    else if(m_file.isopen()){
  //    dvr_log("seek file is not encrypted");
      return 0;
    } else {
      return -1; 
    }
} 

// open next file on list, return 0 on no more file
int playback::opennextfile()
{
    int i;
    dvrtime dvrt ;
    if( m_file.isopen() ) {
        m_file.close();
    }
    time_dvrtime_init( &dvrt, 2000);
    
    while( !m_file.isopen() ) {
        m_curfile++;
        while( m_curfile>=m_filelist.size()) {	// end of day file
            m_curfile=0;
            // go next day
            for( i=0 ; i<m_daylist.size(); i++ ) {
                if( m_daylist[i]>m_day ) {
                    m_day=m_daylist[i] ;
                    dvrt.year  = m_day/10000 ;
                    dvrt.month = m_day%10000/100 ;
                    dvrt.day   = m_day%100 ;
                    m_filelist.setsize(0);	// empty day file list
                    disk_listday( m_filelist, &dvrt, m_channel );
                    break;
                }
            }
            if( i>=m_daylist.size() ) {
                return 0 ;
            }
        }
      //  printf("m_day:%d channel:%d %d open:%s\n",m_day,m_channel,m_curfile,m_filelist[m_curfile].getstring());
        m_file.open( m_filelist[m_curfile].getstring(), "r") ;
    }
    
    return 1;
}

void playback::readframeex()
{
    struct dvrtime*  ptime;
    
    unsigned char *ptemp;
    void * pframe;
    int exlength=sizeof(struct dvrtime);
    if( m_framebuf ) {
        mem_free( m_framebuf );
        m_framebuf=NULL ;
    }

    if( m_file.isopen() ) {
        while(1){
            m_frametype=m_file.frametypeandsize(&m_framesize);
            if(m_frametype!=FRAMETYPE_UNKNOWN)
               break;
            if( opennextfile()==0 ) {		// end of stream data
                return ;
            }
        }

      //  dvr_log("=====framesize=%d type=%d",m_framesize,m_frametype);
        m_framebuf=mem_alloc( m_framesize+exlength );
        ptemp=(unsigned char*)m_framebuf;
        pframe=(void*)(ptemp+exlength);
        
	//printf("channel:%d pos:%d\n",m_channel,m_file.tell());
        if( m_file.readframe(pframe, m_framesize)!=m_framesize ) {
            // error !
            dvr_log( "Read frame data error!");
        }
	
        m_streamtime = m_file.frametime();
	
#if 0
        printf("channel:%d year:%d month:%d day:%d hour:%d minute:%d second:%d mill:%d\n", \
               m_channel,m_streamtime.year, m_streamtime.month, \
               m_streamtime.day,m_streamtime.hour, m_streamtime.minute, \
               m_streamtime.second, m_streamtime.milliseconds);
#endif
        memcpy(m_framebuf,(void*)&m_streamtime,sizeof(struct dvrtime));
        m_framesize+=exlength;
	
    }
}
void playback::readframe()
{
    int mSize;
    if( m_framebuf ) {
        mem_free( m_framebuf );
        m_framebuf=NULL ;
    }
    
    // read frame();
   // m_framesize=0;
    if( m_file.isopen() ) {
#if 1
        while(1){
            m_frametype=m_file.frametypeandsize(&m_framesize);
            if(m_frametype!=FRAMETYPE_UNKNOWN)
               break;
            if( opennextfile()==0 ) {		// end of stream data
                return ;
            }
        }
#endif
      //  dvr_log("framesize=%d type=%d",m_framesize,m_frametype);
        m_framebuf=mem_alloc( m_framesize );
        mSize=m_file.readframe(m_framebuf, m_framesize);
        if( mSize!=m_framesize ) {
            // error !
            dvr_log( "Read frame data error!");
        }
        m_streamtime = m_file.frametime();
#if 0
        printf("year:%d month:%d day:%d minute:%d second:%d mill:%d\n", \
               m_streamtime.year, m_streamtime.month, \
               m_streamtime.day, m_streamtime.minute, \
               m_streamtime.second, m_streamtime.milliseconds);
#endif
#if 0
	if(m_streamtime<m_lasttime){
	   printf("wrong:");
	   printf("frame: year:%d month:%d day:%d minute:%d second:%d mill:%d\n", \
               m_streamtime.year, m_streamtime.month, \
               m_streamtime.day, m_streamtime.minute, \
               m_streamtime.second, m_streamtime.milliseconds);	   
	   printf("last: year:%d month:%d day:%d minute:%d second:%d mill:%d\n", \
                m_lasttime.year, m_lasttime.month, \
                m_lasttime.day,  m_lasttime.minute, \
                m_lasttime.second, m_lasttime.milliseconds);	  
	}
	m_lasttime=m_streamtime;
#endif	
    }
}
void playback::getstreamdataex(void ** getbuf, int * getsize, int * frametype)
{
    if( norecplayback ) {
        rec_pause = 50 ;            // temperary pause recording, for 5 seconds
    }
    net_playarc=5; 
 //   printf("m_framesize0=%d\n",m_framesize);
    if( m_framesize==0 ){
    //    printf("readframeex\n");
        readframeex();
    }
 //   printf("m_framesize1=%d\n",m_framesize);
    
    if( m_framesize==0 ) {		// can't read frame
        * getsize=0 ;
        return ;
    }
   
    *getsize=m_framesize ;
    *frametype=m_frametype ;
    if( getbuf!=NULL ) {		// only return buffer size
#ifdef DEBUG_PLAYBACK        
        if(m_channel==0){
	    if(fp){
	        char* pchar=(char*)m_framebuf;
		fwrite(pchar+sizeof(struct dvrtime),1,m_framesize-sizeof(struct dvrtime),fp); 
	    }
	}
#endif            
        *getbuf = m_framebuf ;
        m_framesize=0 ;
    }
 //   printf("m_framesize3=%d\n",m_framesize);
}

void playback::getstreamdata(void ** getbuf, int * getsize, int * frametype)
{
    if( norecplayback ) {
        rec_pause = 50 ;            // temperary pause recording, for 5 seconds
    }
    net_playarc=5; 
    if( m_framesize==0 )
        readframe();
    if( m_framesize==0 ) {		// can't read frame
        * getsize=0 ;
        return ;
    }
    *getsize=m_framesize ;
    *frametype=m_frametype ;
    if( getbuf!=NULL ) {		// only return buffer size
        *getbuf = m_framebuf ;
        m_framesize=0 ;
    }
}

void playback::preread()
{
    if( m_framesize==0 )
        readframe();
}

void playback::prereadex()
{
    if( m_framesize==0 )
        readframeex();
}

void playback::getdayinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday)
{
    struct dvrtime t ;
    int len ;
    int i;
    struct dayinfoitem di ;
    struct dayinfoitem di_x ;
    array <f264name> * filelist ;
    int bcdday ;
    
    time_dvrtime_zerohour(pday);
    
    di.ontime=0; 
    di.offtime=0;

    bcdday = pday->year*10000 + pday->month*100 + pday->day ;
    if( bcdday != m_day ) {			// not current day list
        filelist = new array <f264name> ;
        disk_listday( *filelist, pday, m_channel );
    }
    else {
        filelist = &m_filelist ;
    }
    
    for( i=0; i<filelist->size(); i++) {
        char * fname = (*filelist)[i].getstring();
        len=f264length(fname) ;
        if( len<=0 ) {
            continue ;
        }
        f264time(fname, &t);
        di_x.ontime=t.hour * 3600 + t.minute * 60 + t.second ;
        di_x.offtime=di_x.ontime+len ;
        if( (di_x.ontime-di.offtime)>5 ) {
            if( di.offtime>di.ontime ) {
                dayinfo.add(di);
            }
            di.ontime=di_x.ontime ;
        }
        di.offtime=di_x.offtime ;
    }

    if( bcdday != m_day ) {
        delete filelist ;
    }

    if( di.offtime>di.ontime ) {
        dayinfo.add(di);
    }
}

int playback::getdaylist( int * * daylist )
{
    if( m_daylist.size()>0 ) {
        *daylist = m_daylist.at(0);
        return m_daylist.size();
    }
    return 0 ;
}

DWORD playback::getmonthinfo(struct dvrtime * month)
{
    DWORD minfo  ;
    int day ;
    minfo=0;
    int i;
    for( i=0; i<m_daylist.size(); i++) {
        day = m_daylist[i] ;
        if( month->year == day/10000 &&
           month->month == day%10000/100 ) {
               minfo|=1<<(day%100-1) ;
           }
    }
    return minfo ;
}

void playback::getlockinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday)
{
    struct dvrtime t ;
    int len ;
    int locklen ;
    int i;
    struct dayinfoitem di ;
    struct dayinfoitem di_x ;
    array <f264name> * filelist ;
    int bcdday ;
    
    time_dvrtime_zerohour(pday);

    di.ontime=0; 
    di.offtime=0;

    bcdday = pday->year*10000 + pday->month*100 + pday->day ;
    if( bcdday != m_day ) {
        filelist = new array <f264name> ;
        disk_listday( *filelist, pday, m_channel );
    }
    else {
        filelist = &m_filelist ;
    }
    
    for( i=0; i<filelist->size(); i++) {
        char * fname = (*filelist)[i].getstring();
        if( strstr(fname, "_L_") ) {
            len=f264length(fname) ;
            locklen = f264locklength( fname );
            if( len<=0 || locklen<=0 ) {
                continue ;
            }
            f264time(fname, &t);
            di_x.ontime=t.hour * 3600 + t.minute * 60 + t.second ;
            di_x.offtime=di_x.ontime+len ;
            di_x.ontime=di_x.offtime-locklen ;
            if( (di_x.ontime-di.offtime)>5 ) {
                if( di.offtime>di.ontime ) {
                    dayinfo.add(di);
                }
                di.ontime=di_x.ontime ;
            }
            di.offtime=di_x.offtime ;
        }
    }
    
    if( bcdday != m_day ) {
        delete filelist ;
    }
    
    if( di.offtime>di.ontime ) {
        dayinfo.add(di);
    }
}
int  playback::getfileheader(void * buffer,size_t headersize)
{
     return m_file.readheader(buffer,headersize);
}

void play_init()
{
    config dvrconfig(dvrconfigfile);
    norecplayback=dvrconfig.getvalueint("system", "norecplayback");
}

void play_uninit()
{
}
