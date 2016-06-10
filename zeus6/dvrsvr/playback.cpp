
#include "dvr.h"

#define DAYLENGTH (24*60*60)

static int norecplayback = 1 ;
int net_playarc=0;

playback::playback( int channel )
{
    m_channel=channel ;
    net_playarc=50;
    if( norecplayback ) {
        rec_pause = 850 ;            // temperary pause recording, for 5 seconds
    }    
    
    disk_getdaylist(m_daylist, channel);     
    m_day=0 ;                      
	
	// get data available days
    if( norecplayback ) {
        rec_pause = 100 ;            // temperary pause recording, for 5 seconds
    }     
    net_playarc=10;    

    m_curfile=-1 ;					// make opennextfile() open first file			
    opennextfile() ;                // actually open the first file
}

playback::~playback()
{
    close();
}

DWORD playback::getcurrentfileheader()
{
  if (m_file.isopen())
    return m_file.getfileheader();
  return 0;
}

int playback::close()
{
    m_file.close();
    return 0;
}

int playback::seek( struct dvrtime * seekto )
{
	if( m_file.seek( seekto ) ) {
		return 1;
	}
	m_file.close();
	
    int newday = seekto->year*10000 + seekto->month*100 + seekto->day ;
    if( newday != m_day ) {	// seek to different day? update new day file list
        // load new day file list
        m_day=newday ;
        disk_listday( m_filelist, seekto, m_channel );
    }
    m_curfile = 0;
    int s = m_filelist.size() ;
    if( s>0 ) {
		for( m_curfile = 0; m_curfile < s-1 ; m_curfile++ ) {
			dvrtime ft ;		// file time
			f264time( m_filelist[m_curfile+1].getstring(), &ft );
			if( *seekto<ft ){
				break ;
			}
		}

		m_file.open( m_filelist[m_curfile].getstring(), "r" ) ;			
		if( m_file.isopen() ) {
			return m_file.seek(seekto);		// don't need to care if its success
		}
	}
    return opennextfile();
} 

// open next file on list, return 0 on no more file
int playback::opennextfile()
{
    int day = 0 ;
    dvrtime dvrt ;
    time_dvrtime_init( &dvrt, 2000);
    m_file.close();
    while( !m_file.isopen() && day < m_daylist.size() ) {
		if( m_daylist[day] == m_day ) {
			if(++m_curfile<0) 
				m_curfile = 0 ;		
			if( m_curfile < m_filelist.size() ) {
				m_file.open( m_filelist[m_curfile].getstring(), "r") ;
			}
			else {
				day++;
			}
		}
		else if( m_daylist[day]>m_day ) {
			// advanced to next day
			m_day=m_daylist[day] ;
			dvrt.year  = m_day/10000 ;
			dvrt.month = m_day%10000/100 ;
			dvrt.day   = m_day%100 ;
			disk_listday( m_filelist, &dvrt, m_channel );
			m_curfile = -1 ;			
		}
		else { 		// m_daylist[day]<m_day 
			day++ ;
		}
    }

    return m_file.isopen();
}

// read current frame , if success advance to next frame after read
int playback::readframe( frame_info * fi )
{
	int framesize = m_file.readframe(fi) ;
	if(framesize>0 ) {
		// for norecplayback
		if( norecplayback ) {
			rec_pause = 50 ;            // temperary pause recording, for 5 seconds
		}
		net_playarc=5; 

		return framesize ;
	}
	else {
		opennextfile() ;
		return m_file.readframe(fi) ;
	}
}

int playback::getstreamtime(dvrtime * dvrt)
{
	frame_info fi ;
	m_file.readframe(&fi) ;	
	*dvrt = fi.frametime ;
	return 1 ;
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
