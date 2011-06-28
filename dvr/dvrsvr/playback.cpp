
#include "dvr.h"

#define DAYLENGTH (24*60*60)

static int norecplayback = 1 ;

playback::playback( int channel, int decrypt )
{
    m_channel=channel ;
    disk_getdaylist(m_daylist, channel);    // get data available days
    m_day=-1 ;                              // invalid day
    m_curfile=0 ;
    time_dvrtime_init(&m_streamtime, 2000);
    m_framebuf=NULL ;
    m_framesize=0 ;
    m_frametype=FRAMETYPE_UNKNOWN ;
    m_autodecrypt = decrypt ;
    opennextfile() ;                        // actually open the first file
}

playback::~playback()
{
    close();
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
    m_streamtime=*seekto ;

    // remove current frame buffer
    if( m_framebuf ) {
        mem_free( m_framebuf );
        m_framebuf=NULL ;
        m_framesize=0 ;
    }
 
    if( m_file.isopen() && m_file.seek( seekto ) ) {	// within current opened file?
        preread();
        goto seek_end ;
    }
    close();
    
    bcdseekday = seekto->year*10000 + seekto->month*100 + seekto->day ;
    if( bcdseekday != m_day ) {			// seek to different day? update new day file list
        m_filelist.setsize(0);			// empty day file list
        m_day=bcdseekday ;
        // load new day file list
        disk_listday( m_filelist, seekto, m_channel );
    }
    
    // seek inside m_day (m_filelist)
    for(m_curfile=0; m_curfile<m_filelist.size(); m_curfile++) {
        int    fl ;			// file length
        dvrtime ft ;		// file time
        char * filename = m_filelist[m_curfile] ;
        f264time( filename, &ft );
        fl = f264length( filename );
        ft = ft + fl ;		// ft become file end time
        if(!(*seekto > ft) ) {	// found the file
            if( m_file.open(filename, "rb") ) {	// open success
                if( m_autodecrypt ) {
                    m_file.autodecrypt(1);
                }
                m_file.seek(seekto);	// don't need to care if its success
                preread();
                goto seek_end ;				// done!
            }
        }
    }
    
    // seek beyond the day
    opennextfile();
    preread();

seek_end:
    if( m_file.isopen() ) {
        return m_file.gethdflag();
    }
    else {
        return 0 ;
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
        while( m_curfile>=m_filelist.size()) {			// end of day file
            m_curfile=0;
            // go next day
            for( i=0 ; i<m_daylist.size(); i++ ) {
                if( m_daylist[i]>m_day ) {
                    m_day=m_daylist[i] ;
                    dvrt.year  = m_day/10000 ;
                    dvrt.month = m_day%10000/100 ;
                    dvrt.day   = m_day%100 ;
                    m_filelist.setsize(0);						// empty day file list
                    if( disk_listday( m_filelist, &dvrt, m_channel )>0 ) 
                        break;
                }
            }
            if( i>=m_daylist.size() ) {
                return 0 ;
            }
        }
        if( m_filelist.size()>0 ) {
            m_file.open( m_filelist[m_curfile], "rb") ;
        }
    }
    if( m_file.isopen() && m_autodecrypt ) {
        m_file.autodecrypt(1);
    }
    return 1;
}

void playback::readframe()
{
    // remove previous frame buffer
    if( m_framebuf ) {
        mem_free( m_framebuf );
        m_framebuf=NULL ;
        m_framesize=0 ;
    }
    
    // read frame();
    if( m_file.isopen() ) {
        while( (m_framesize=m_file.framesize())==0 ) {
            // open next file 
            if( opennextfile()==0 ) {		// end of stream data
                return ;
            }
        }
        m_frametype = m_file.frametype();
        m_framebuf=mem_alloc( m_framesize );
        m_streamtime = m_file.frametime();
        if( m_file.readframe(m_framebuf, m_framesize)!=m_framesize ) {
            mem_free( m_framebuf );
            m_framesize = 0 ;
            m_framebuf = NULL ;
            // error !
            dvr_log( "Read frame data error!");
        }
    }
}

void playback::getstreamdata(void ** getbuf, int * getsize, int * frametype)
{
    if( norecplayback ) {
        rec_pause = 5 ;         // temperary pause recording, for 5 seconds
    }
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

int playback::getcurrentcliptime(struct dvrtime * begin, struct dvrtime * end)
{
    array <struct dayinfoitem> dayinfo ;
    int i ;
    int timeinday ;

    timeinday=m_streamtime.hour * 3600 + m_streamtime.minute * 60 + m_streamtime.second ;

    getdayinfo( dayinfo, &m_streamtime );

    for( i=0; i<dayinfo.size(); i++ ) {
        struct dayinfoitem & di = dayinfo[i] ;
        if( timeinday >= di.ontime && timeinday <= di.offtime ) {
            *begin = m_streamtime ;
            begin->hour = di.ontime / 3600 ;
            begin->minute = (di.ontime % 3600) / 60 ;
            begin->second = di.ontime % 60 ;
            begin->milliseconds = 0 ;
            *end = m_streamtime ;
            end->hour = di.offtime / 3600 ;
            end->minute = (di.offtime % 3600) / 60 ;
            end->second = di.offtime % 60 ;
            end->milliseconds = 0 ;
            return 1 ;
        }
    }

    return 0 ;
}

int playback::getnextcliptime(struct dvrtime * begin, struct dvrtime * end)
{
    array <struct dayinfoitem> dayinfo ;
    int timeinday ;
    struct dvrtime clipday ;
    int i, j;

    memset( &clipday, 0, sizeof(clipday));

    for( j=0 ; j<m_daylist.size(); j++ ) {
        if( m_daylist[j]>=m_day ) {
            if( m_daylist[j]==m_day ) {
                timeinday=m_streamtime.hour * 3600 + m_streamtime.minute * 60 + m_streamtime.second ;
            }
            else {
                timeinday = -100 ;
            }
            clipday.year = m_daylist[j] / 10000 ;
            clipday.month = (m_daylist[j]%10000) / 100 ;
            clipday.day = m_daylist[j] % 100 ;

            getdayinfo( dayinfo, &clipday );
            for( i=0; i<dayinfo.size(); i++ ) {
                struct dayinfoitem & di = dayinfo[i] ;
                if( di.ontime > timeinday ) {
                    *begin = clipday ;
                    begin->hour = di.ontime / 3600 ;
                    begin->minute = (di.ontime % 3600) / 60 ;
                    begin->second = di.ontime % 60 ;
                    *end = clipday ;
                    end->hour = di.offtime / 3600 ;
                    end->minute = (di.offtime % 3600) / 60 ;
                    end->second = di.offtime % 60 ;
                    return 1 ;
                }
            }
        }
    }

    return 0 ;
}

int playback::getprevcliptime(struct dvrtime * begin, struct dvrtime * end)
{
    array <struct dayinfoitem> dayinfo ;
    int timeinday ;
    struct dvrtime clipday ;
    int i, j;

    memset( &clipday, 0, sizeof(clipday));

    for( j=m_daylist.size()-1 ; j>=0 ; j-- ) {
        if( m_daylist[j] <= m_day ) {
            if( m_daylist[j]==m_day ) {
                timeinday=m_streamtime.hour * 3600 + m_streamtime.minute * 60 + m_streamtime.second ;
            }
            else {
                timeinday = 3600*24 + 100 ;
            }
            clipday.year = m_daylist[j] / 10000 ;
            clipday.month = (m_daylist[j]%10000) / 100 ;
            clipday.day = m_daylist[j] % 100 ;

            getdayinfo( dayinfo, &clipday );
            for( i=dayinfo.size()-1; i>=0; i-- ) {
                struct dayinfoitem & di = dayinfo[i] ;
                if( di.offtime < timeinday ) {
                    *begin = clipday ;
                    begin->hour = di.ontime / 3600 ;
                    begin->minute = (di.ontime % 3600) / 60 ;
                    begin->second = di.ontime % 60 ;
                    *end = clipday ;
                    end->hour = di.offtime / 3600 ;
                    end->minute = (di.offtime % 3600) / 60 ;
                    end->second = di.offtime % 60 ;
                    return 1 ;
                }
            }
        }
    }

    return 0 ;
}


void playback::getdayinfo(array <struct dayinfoitem> &dayinfo, struct dvrtime * pday)
{
    struct dvrtime t ;
    int len ;
    int i;
    struct dayinfoitem di ;
    struct dayinfoitem di_x ;
    array <f264name> filelist ;
    
    di.ontime=0; 
    di.offtime=0;

    disk_listday( filelist, pday, m_channel );

    dayinfo.setsize(0);
    for( i=0; i<filelist.size(); i++) {
        char * fname = filelist[i];
        len=f264length(fname) ;
        if( len<=0 || len>5000 ) {
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

    if( di.offtime>di.ontime ) {
        dayinfo.add(di);
    }
}

int  playback::getdaylist( array <int> & daylist )
{
    if( m_daylist.size()>0 ) {
        daylist=m_daylist ;
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
    array <f264name> filelist ;
    
    di.ontime=0; 
    di.offtime=0;

    disk_listday( filelist, pday, m_channel );

    dayinfo.setsize(0);
    for( i=0; i<filelist.size(); i++) {
        char * fname = filelist[i];
        if( strstr(fname, "_L_") ) {
            len=f264length(fname) ;
            locklen = f264locklength( fname );
            if( len<=0 || locklen<=0 || len>5000 || locklen>len ) {
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
    
    if( di.offtime>di.ontime ) {
        dayinfo.add(di);
    }
}

int playback::readfileheader(char *hdbuf, int hdsize)
{
    if( m_file.isopen() ) {
        return m_file.readheader( hdbuf, hdsize );
    }
    else {
        memcpy( hdbuf, cap_fileheader(0), hdsize );
        return hdsize ;
    }    
}

void play_init(config &dvrconfig)
{
    norecplayback=dvrconfig.getvalueint("system", "norecplayback");
}

void play_uninit()
{
}
