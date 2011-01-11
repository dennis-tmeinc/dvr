#include "dvr.h"

string rec_basedir;

static pthread_t rec_threadid;
static int rec_run = 0 ;           // 0: quit record thread, 1: recording

int    rec_busy ;
int    rec_pause = 0 ;             // 1: to pause recording, while network playback
static int rec_fifo_size = 4000000 ;
int    rec_fifobusy ;
int    rec_lock_all = 0 ;			// all files save as locked file

static pthread_mutex_t rec_mutex;
static pthread_cond_t  rec_cond;
static int rec_sig ;
static int rec_lowmemory ;

#ifdef PWII_APP
static int pwii_recnostopall ;
#endif

inline void rec_lock() {
    pthread_mutex_lock(&rec_mutex);
}

inline void rec_unlock() {
    pthread_mutex_unlock(&rec_mutex);
}

static void rec_wait()
{
    rec_lock();
    if( --rec_sig<0 ) {
        pthread_cond_wait(&rec_cond, &rec_mutex);
    }
    rec_sig=0 ;
    rec_unlock();
}

static void rec_signal()
{
    rec_lock();
    if( rec_sig < 0 ) {
        pthread_cond_signal(&rec_cond);
    }
    rec_sig=1 ;
    rec_unlock();
}

enum REC_STATE {
    REC_STOP,           // no recording
    REC_PRERECORD,      // pre-recording
    REC_RECORD,         // recording
    REC_LOCK            // recording lock file
};

struct rec_fifo {
    REC_STATE rectype;          // recording type
    struct dvrtime time;        // captured time
    int bufsize;
    int frametype;
    char *buf;
    struct rec_fifo *next;
};

#define MAX_TRIGGERSENSOR   (32)

static char rec_stname(enum REC_STATE recst)
{
    if( recst == REC_LOCK ) {
        return 'L' ;
    }
    else if( recst == REC_RECORD ) {
		if( rec_lock_all ) {
			return 'L' ;
		}
		else {	
			return 'N' ;
		}
    }
    else return 'P' ;
}

#define REC_FORCEON     (1)
#define REC_FORCEOFF    (0)

class rec_channel {
    protected:
        int     m_channel;
        
        int     m_recordmode ;			// 0: continue mode, 123: triggering mode, other: no recording

        int     m_recordjpeg ;			// 1: to record jpeg files
        
        unsigned int m_triggersensor[MAX_TRIGGERSENSOR] ;  // trigger sensor bit maps

        int     m_recordalarm ;
        int     m_recordalarmpattern ;

        REC_STATE	m_recstate;

        // variable for file write
        string m_filename;
        REC_STATE   m_filerecstate ;
		
        string m_prevfilename ;				// previous recorded file name.
        REC_STATE   m_prevfilerecstate ;

        dvrfile m_file;						// recording file
		
        struct rec_fifo *m_fifohead;
        struct rec_fifo *m_fifotail;
        int	   m_fifosize ;

        // allocate new fifo block and buffer
        rec_fifo *  newfifo( char *fifobuf, int bufsize, int frametype=FRAMETYPE_UNKNOWN ) ;
        // delete fifo ( release fifo buffer and fifo block )
        void  deletefifo( rec_fifo * rf );
        struct rec_fifo *getfifo();
        void putfifo(rec_fifo * fifo);
        int  clearfifo();
        
        int m_lock_endtime ;	        // lock end time
        int m_rec_endtime ;	            // recording end time
            
        // pre-record variables
        int m_prerecord_time ;				// allowed pre-recording time
        // post-record variables
        int m_postrecord_time ;				// post-recording time	length
        
        // lock record variables
        int m_prelock_time ;				// pre lock time length
        int m_postlock_time ;				// post lock time length
      
        int m_maxfilesize;
        int m_maxfiletime;
        dvrtime m_filebegintime;
        dvrtime m_fileendtime;
        dvrtime m_keytime ;
        int m_fileday;						// day of opened file, used for breaking file across midnight
        
        int m_reqbreak;				// reqested for file break

        int m_recording ;
        
        int m_activetime ;

        //	rec_index m_index ;			// on/off index
        //	int m_onoff ;				// on/off state

        dvrfile m_prelock_file;			// pre-lock recording file

        // g-force trigger record parameters
        int m_gforce_trigger ;
        int m_gforce_lock ;
        float m_gforce_gf ;
        float m_gforce_gr ;
        float m_gforce_gd ;
        
    public:
        rec_channel(int channel);
        ~rec_channel();
        
        void start();				// set start record stat
        void stop();				// stop recording, frame data discarded

#ifdef  PWII_APP
        
        int     m_forcerecordontime ;     // force recording turn on time
        int     m_forcerecordofftime ;    // force recording trun off time

        // force recording feature for PWII
        void setforcerecording(int force){
            if( force == REC_FORCEON ) {
                m_recstate = REC_LOCK ;
                m_lock_endtime = g_timetick+1000*m_forcerecordontime ;
                m_rec_endtime = m_lock_endtime ;
            }
            else {
                m_recstate = REC_PRERECORD ;
                m_rec_endtime = m_lock_endtime = g_timetick ;
            }
        }
#endif

        
        void onframe(cap_frame * pframe);
        int dorecord();
        
        int recorddata(rec_fifo * data);		// record stream data
        //	void closeprerecord( int lock );				// close precord data, save them to normal video file
        //	void prerecorddata(rec_fifo * data);			// record pre record data
		void prelock(string & lockfilename, struct dvrtime * locktime);	// process prelock data
        
        void closefile(enum REC_STATE newrecst = REC_STOP);
        
        void breakfile(){			// start a new recording file
            m_reqbreak = 1;
        }
        int  recstate() {
            return( 
                   m_recording &&
                   ( m_recstate == REC_RECORD || 
                     m_recstate == REC_LOCK )
                   );
        }
        int  lockstate() {
            return( m_recording && m_recstate == REC_LOCK ) ;
        }
        void update();          						// update recording status
        void alarm();						            // set recording alarm.

        void prelock_thread() ;

};

static rec_channel * recchannel[MAXCHANNEL];
static int rec_channels;

int rec_maxfilesize;
int rec_maxfiletime;

rec_channel::rec_channel(int channel)
{
    int i;
    char buf[64];
    config dvrconfig(dvrconfigfile);
    string v ;
    char cameraname[80] ;

    m_fifohead = m_fifotail = NULL;
    m_fifosize = 0 ;

    m_channel = channel;
    sprintf(cameraname, "camera%d", m_channel+1 );

    m_recordmode = dvrconfig.getvalueint( cameraname, "recordmode");
    
    for(i=0;i<num_sensors&&i<MAX_TRIGGERSENSOR;i++) {
        sprintf(buf, "trigger%d", i+1);
        m_triggersensor[i] = dvrconfig.getvalueint( cameraname, buf );
    }

    m_recordjpeg = dvrconfig.getvalueint( cameraname, "recordjpeg" );
        
    m_recordalarm = dvrconfig.getvalueint( cameraname, "recordalarm" );
    m_recordalarmpattern = dvrconfig.getvalueint( cameraname, "recordalarmpattern" );
    
    m_recstate=REC_STOP ;
    m_filerecstate=REC_STOP ;
	m_prevfilerecstate=REC_STOP ;
    
    // initialize pre-record variables
    m_prerecord_time = dvrconfig.getvalueint(cameraname, "prerecordtime");
    if( m_prerecord_time<2 )
        m_prerecord_time=2 ;
    if( m_prerecord_time>rec_maxfiletime )
        m_prerecord_time=rec_maxfiletime ;
    //	m_prerecord_file = NULL ;
    //	time_dvrtime_init( &m_prerecord_filestart, 2000 );
    
    // initialize post-record variables
    m_postrecord_time = dvrconfig.getvalueint(cameraname, "postrecordtime");
    if( m_postrecord_time<10 )
        m_postrecord_time=10 ;
    if( m_postrecord_time>(3600*12) )
        m_postrecord_time=(3600*12) ;

#ifdef PWII_APP    
// pwii, force on/off recording
    m_forcerecordontime = dvrconfig.getvalueint( cameraname, "forcerecordontime" ) ;
    m_forcerecordofftime = dvrconfig.getvalueint( cameraname, "forcerecordofftime" ) ;
#endif
    
    // go for each trigger sensor setting time
/*    
    for(i=0;i<num_sensors&&i<MAX_TRIGGERSENSOR;i++) {
        int  ti ;
        sprintf(buf, "trigger%d_prerec", i+1);
        ti = dvrconfig.getvalueint( cameraname, buf ); 
        if( ti>m_prerecord_time ) {
            m_prerecord_time = ti ;
        }
        sprintf(buf, "trigger%d_postrec", i+1);
        ti = dvrconfig.getvalueint( cameraname, buf ); 
        if( ti>m_postrecord_time ) {
            m_postrecord_time = ti ;
        }
    }
*/    

    // initialize lock record variables
    m_prelock_time=dvrconfig.getvalueint("system", "prelock" );
    m_postlock_time=dvrconfig.getvalueint("system", "postlock" );
    if( m_recordmode == 1 || m_recordmode == 3 ) {      // sensor triggering
        if( m_prelock_time<2*m_prerecord_time ) {
            m_prelock_time = 2*m_prerecord_time ;       // make it double time of pre-trigger, so all pre-recording file become _L_
        }
        if( m_postlock_time<m_postrecord_time ) {
            m_postlock_time=m_postrecord_time ;
        }
    }
    
    // initialize variables for file write
    m_maxfilesize = rec_maxfilesize;
    m_maxfiletime = rec_maxfiletime;
    time_dvrtime_init( &m_filebegintime, 2000 );
    time_dvrtime_init( &m_fileendtime, 2000 );
    m_fileday=0;
//    m_lastkeytimestamp = 0 ;
    m_reqbreak = 0 ;

    m_lock_endtime=0;
    m_rec_endtime=0;

    // g-force trigger record parameters
    m_gforce_trigger = dvrconfig.getvalueint(cameraname, "gforce_trigger") ;
    m_gforce_lock    = dvrconfig.getvalueint(cameraname, "gforce_lock") ;

    float a, b ;
    a=3.0 ;
    b=3.0 ;
    v = dvrconfig.getvalue("io", "gsensor_forward_crash") ;
    if( v.length()>0 ) {
        sscanf( v.getstring(), "%f", &a );
    }
    v = dvrconfig.getvalue("io", "gsensor_backward_crash") ;
    if( v.length()>0 ) {
        sscanf( v.getstring(), "%f", &b );
    }
    if( a<0.0 ) a=-a ;
    if( b<0.0 ) b=-b ;
    m_gforce_gf=a<b?a:b ;

    a=3.0 ;
    b=3.0 ;
    v = dvrconfig.getvalue("io", "gsensor_right_crash") ;
    if( v.length()>0 ) {
        sscanf( v.getstring(), "%f", &a );
    }
    v = dvrconfig.getvalue("io", "gsensor_left_crash") ;
    if( v.length()>0 ) {
        sscanf( v.getstring(), "%f", &b );
    }
    if( a<0.0 ) a=-a ;
    if( b<0.0 ) b=-b ;
    m_gforce_gr=a<b?a:b ;

    a=3.0 ;
    b=3.0 ;
    v = dvrconfig.getvalue("io", "gsensor_down_crash") ;
    if( v.length()>0 ) {
        sscanf( v.getstring(), "%f", &a );
    }
    v = dvrconfig.getvalue("io", "gsensor_up_crash") ;
    if( v.length()>0 ) {
        sscanf( v.getstring(), "%f", &b );
    }
    if( a<0.0 ) a=-a ;
    if( b<0.0 ) b=-b ;
    m_gforce_gd=a<b?a:b ;
    
    // start recording	
    start();

#ifdef TVS_APP
    // for tvs, required by nelson, 2010-04-13. All camera start recording wile power on
    if( m_recstate == REC_PRERECORD ) {
        m_recstate = REC_LOCK ;
        m_lock_endtime = g_timetick + 1000*m_postlock_time ;
        m_rec_endtime = g_timetick + 1000*m_prerecord_time ;
    }
#endif

}

rec_channel::~rec_channel()
{ 
	m_recstate = REC_STOP;
	closefile();
	clearfifo();
}

// start record
void rec_channel::start()
{
    if( m_recordmode == 0 ) {			// continue recording mode
        m_recstate = REC_RECORD ;
    }
    else if( m_recordmode == 1 || m_recordmode == 2 || m_recordmode == 3 ) { // trigger mode
        m_recstate = REC_PRERECORD ;
    }
    else {
        m_recstate = REC_STOP ;
    }
	m_recording = 0 ;
}

// stop recording, frame data discarded
void rec_channel::stop()
{
    m_recstate = REC_STOP;
}

void rec_channel::onframe(cap_frame * pframe)
{
    struct rec_fifo *nfifo;

    if( m_recstate == REC_STOP ||
        pframe == NULL ||
        rec_pause>0 ||
        dio_record == 0 ||
        rec_basedir.length()<=0 )
    {
        clearfifo();
        if( m_file.isopen() ) {
            nfifo = newfifo( NULL, 0 );
            if( nfifo ) {
                nfifo->rectype = REC_STOP ;
                putfifo( nfifo );
            }
        }
    }
    else {
        // build new fifo frame
        nfifo = newfifo( pframe->framedata, pframe->framesize, pframe->frametype );
        if( nfifo ) {
            // set frame recording type
            nfifo->rectype = m_recstate ;
            putfifo( nfifo );
        }
    }
    rec_signal();
}

// allocate new fifo
rec_fifo *  rec_channel::newfifo( char *fifobuf, int bufsize, int frametype )
{
    rec_fifo * nf = new rec_fifo ;
    time_now(&(nf->time));
    nf->frametype = frametype ;
    if( fifobuf ) {
        nf->bufsize=bufsize ;
        nf->buf = (char *)mem_ref(fifobuf, bufsize) ;
    }
    else {
        nf->buf=NULL ;
        nf->bufsize=0;
    }
    nf->next = NULL ;
    return nf ;
}

// delete fifo ( release fifo buffer and fifo block )
void  rec_channel::deletefifo( rec_fifo * rf )
{
    if( rf!=NULL ) {
        if( rf->buf!=NULL ) {
            mem_free( rf->buf );
        }
        delete rf ;
    }
}

// add fifo into fifo queue
void rec_channel::putfifo(rec_fifo * fifo)
{
    if( fifo!=NULL ){
        rec_lock();
        fifo->next = NULL ;
        if (m_fifohead == NULL ) {              // fifo is empty
            m_fifotail = m_fifohead = fifo;
            m_fifosize = fifo->bufsize ;
        } else {
            m_fifotail->next = fifo;
            m_fifotail = fifo;
            m_fifosize+=fifo->bufsize ;
        }
        rec_unlock();
    }
}

// remove fifo from queue
struct rec_fifo *rec_channel::getfifo()
{
    struct rec_fifo *head;
    rec_lock();
    if ( (head = m_fifohead) != NULL) {
        m_fifohead = head->next;
        m_fifosize-= head->bufsize ;
    }
    rec_unlock();
    return head;
}

// clear fifos, return fifos deleted.
int rec_channel::clearfifo()
{
    struct rec_fifo * rf ;
    int c=0 ;
    while( (rf=getfifo())!=NULL ) {
        deletefifo( rf );
        c++ ;
    }
    return c ;
}

struct file_copy_struct {
    int  start ;
    char sourcename[512];
    char destname[512];
};

// file copying thread
void *rec_filecopy(void *param)
{
    struct file_copy_struct * fc=(struct file_copy_struct *)param;
    FILE * source ;
    FILE * dest ;
    int  rdsize ;
    int totalsize ;
    dvrtime starttime, endtime ;
    source = file_open( fc->sourcename, "r" ) ;
    if( source==NULL ) {
        fc->start=1 ;
        return NULL ; 
    }
    time_now(&starttime);
    dvr_log("filecopy: source-%s", fc->sourcename );
    dest = file_open( fc->destname, "w" ) ;
    if( dest==NULL ) {
        fc->start=1 ;
        file_close( source );
        return NULL ;
    }
    dvr_log("filecopy: dest-%s", fc->destname );
    fc->start=1;
    totalsize = 0 ;

    char buf[4096] ;
    while( (rdsize=file_read(buf, sizeof(buf), source))>0 ) {
        totalsize+=file_write(buf, rdsize, dest);
    }
    file_close( source );
    file_close( dest );
    time_now(&endtime);
    dvr_log("filecopy: size=%dk : time=%ds.", totalsize/1024, endtime-starttime );
    return NULL ;
}

// close all openning files
void rec_channel::closefile(enum REC_STATE newrecst)
{
    char newfilename[512];
    char *rn;
    int  filelength ;
    
    if (m_file.isopen()) {
        m_file.close();
        filelength=(int)(m_fileendtime - m_filebegintime) ;
        if( filelength<1 || filelength>=86400 ) {
            m_file.remove(m_filename.getstring());
            m_filename="";
            return ;
        }

        // need to rename to contain file lengh
        strcpy(newfilename, m_filename.getstring());
        if ((rn = strstr(newfilename, "_0_")) != NULL) {
            sprintf( rn, "_%d_%c_%s%s", 
                    filelength, 
                    rec_stname( m_filerecstate ),
                    g_hostname, g_264ext );
            m_file.rename( m_filename.getstring(), newfilename ) ;
			m_filename = newfilename ;

			if( newrecst == REC_PRERECORD ) {
				if( m_prevfilerecstate == REC_PRERECORD && m_prevfilename.length()>0 ) {
					m_file.remove( m_prevfilename.getstring() );
				}
				if( m_filerecstate == REC_RECORD || m_filerecstate == REC_LOCK ) {
					disk_renew( m_filename.getstring(), 1 );
				}
				m_prevfilename = newfilename ;
				m_prevfilerecstate = m_filerecstate ;
			}
			else if( newrecst == REC_RECORD ) {
				if( m_prevfilerecstate == REC_PRERECORD && m_prevfilename.length()>0 ) {
					m_file.chrecmod( m_prevfilename, rec_stname(m_prevfilerecstate), rec_stname(newrecst) );
					disk_renew( m_prevfilename.getstring(), 1 );
				}
				if( m_filerecstate == REC_PRERECORD ) {
					m_file.chrecmod( m_filename, rec_stname(m_filerecstate), rec_stname(newrecst) );
					m_filerecstate = newrecst ;
				}
				if( m_filerecstate == REC_RECORD || m_filerecstate == REC_LOCK ) {
					disk_renew( m_filename.getstring(), 1 );
				}
				m_prevfilename = m_filename ;
				m_prevfilerecstate = m_filerecstate ;
			}
			else if( newrecst == REC_LOCK ) {
                // change current and prev file to RECORD (_N_) file first
				if( m_prevfilerecstate == REC_PRERECORD && m_prevfilename.length()>0 ) {
					m_file.chrecmod( m_prevfilename, rec_stname(m_prevfilerecstate), rec_stname(REC_RECORD) );
					m_prevfilerecstate = REC_RECORD ;
					disk_renew( m_prevfilename.getstring(), 1 );
				}
				if( m_filerecstate == REC_PRERECORD ) {
					m_file.chrecmod( m_filename, rec_stname(m_filerecstate), rec_stname(REC_RECORD) );
					m_filerecstate = REC_RECORD ;
				}
                // do pre-locking
				if( m_filerecstate == REC_RECORD ) {
					if( rec_stname(REC_LOCK) == rec_stname(REC_RECORD) ) {      // all files are locked.
						disk_renew( m_filename.getstring(), 1 );
					}
					else {
						struct dvrtime prelocktime ;
						if( (m_prelock_time-filelength)>5 ) {
							m_file.chrecmod( m_filename, rec_stname(m_filerecstate), rec_stname(REC_LOCK) );
							disk_renew( m_filename.getstring(), 1 );
							if( m_prevfilerecstate == REC_RECORD ) {
								disk_renew( m_prevfilename.getstring(), 0 );
								prelocktime = m_fileendtime ;
								time_dvrtime_del(&prelocktime, m_prelock_time);
								prelock( m_prevfilename, &prelocktime );
								disk_renew( m_prevfilename.getstring(), 1 );
							}
						}
						else if( (filelength-m_prelock_time)>5 ) {
							prelocktime = m_fileendtime ;
							time_dvrtime_del(&prelocktime, m_prelock_time);
							prelock( m_filename, &prelocktime );
							disk_renew( m_filename.getstring(), 1 );
						}
						else {
							m_file.chrecmod( m_filename, rec_stname(m_filerecstate), rec_stname(REC_LOCK) );
							disk_renew( m_filename.getstring(), 1 );
							if( m_prevfilerecstate == REC_RECORD ) {
								disk_renew( m_prevfilename.getstring(), 0 );
								m_file.chrecmod( m_prevfilename, rec_stname(m_prevfilerecstate), rec_stname(REC_LOCK) );
								disk_renew( m_prevfilename.getstring(), 1 );
							}
						}
					}
				}
				else if( m_filerecstate == REC_LOCK ) {
					disk_renew( m_filename.getstring(), 1 );
				}

				m_prevfilename = "";
				m_prevfilerecstate = REC_STOP ;
			}
			else {				// REC_STOP
				if( m_prevfilerecstate == REC_PRERECORD && m_prevfilename.length()>0 ) {
					m_file.remove( m_prevfilename.getstring() );
				}
				if( m_filerecstate == REC_PRERECORD ) {
					m_file.remove( m_filename.getstring() );
				}
				if( m_filerecstate == REC_RECORD || m_filerecstate == REC_LOCK ) {
					disk_renew( m_filename.getstring(), 1 );
				}
				m_prevfilename = "";
				m_prevfilerecstate = REC_STOP ;
			}
        }
        m_filename="";
		m_filerecstate = REC_STOP ;
    }
}

// write frame data to disk, return 1: data wrote to disk success, 0: failed
int rec_channel::recorddata(rec_fifo * data)
{
    char newfilename[512];
    int l;
    
    if( data==NULL || 
        data->buf==NULL || 
        data->rectype == REC_STOP || 
        data->bufsize<=0 ||
        rec_basedir.length() <= 0 ) 
    {
        closefile();
        return 0;
    }
    
    // check if file need to be broken
    if( data->frametype == FRAMETYPE_KEYVIDEO ) {   // close or open file on key frame
//        DWORD timestamp ;
//        timestamp = ((struct hd_frame *)(data->buf))->timestamp ;

        if ( m_file.isopen() ) {	
            if( m_reqbreak ||
                data->rectype != m_filerecstate ||
//                timestamp < m_lastkeytimestamp || 
//                (timestamp-m_lastkeytimestamp)>1000 ||
                m_file.tell() > m_maxfilesize ||
                data->time.day != m_fileday ||
                data->time < m_keytime ||
                time_dvrtime_diff( &m_fileendtime, &m_filebegintime ) > m_maxfiletime ||
                ( m_filerecstate == REC_PRERECORD  && time_dvrtime_diff(&m_fileendtime, &m_filebegintime) > m_prerecord_time )
                )
            {
                if( time_dvrtime_diff( &(data->time), &m_fileendtime) < 5 ) {
                    m_fileendtime=data->time ;    
                }
                closefile(data->rectype);
                m_reqbreak=0;
            }
        }
//      m_lastkeytimestamp = timestamp ;
        m_keytime = data->time ;
        
        // create new dvr file
        if ( !m_file.isopen()  ) {	            // open a new file
            // create new record file
            if (rec_basedir.length() <= 0) {	// no base dir, no recording disk available
                // frame data is discarded
                return 0;
            }

            struct stat dstat ;

            // make host directory
            sprintf(newfilename, "%s/_%s_", 
                rec_basedir.getstring(), g_hostname );
            if( stat(newfilename, &dstat)!=0 ) {
                mkdir( newfilename, S_IFDIR | 0777 );
            }

            // make date directory
            l = strlen(newfilename);
            sprintf(newfilename+l, "/%04d%02d%02d", 
                data->time.year,
                data->time.month,
                data->time.day );
            if( stat(newfilename, &dstat)!=0 ) {
                mkdir( newfilename, S_IFDIR | 0777 );
            }

            // make channel directory
            l = strlen(newfilename);
            sprintf(newfilename + l, "/CH%02d", m_channel);
            if( stat(newfilename, &dstat)!=0 ) {
                mkdir( newfilename, S_IFDIR | 0777 );
            }

            // make new file name
            l = strlen(newfilename);
            sprintf(newfilename + l,
                "/CH%02d_%04d%02d%02d%02d%02d%02d_0_%c_%s%s", 
                m_channel,
                data->time.year,
                data->time.month,
                data->time.day,
                data->time.hour,
                data->time.minute,
                data->time.second,
                rec_stname( data->rectype ),
                g_hostname, g_264ext);
            m_fileday = data->time.day ;
            if (m_file.open(newfilename, "wb")) {
                m_filename = newfilename;
                m_filebegintime = data->time;
                m_filebegintime.milliseconds=0 ;
                m_filerecstate = data->rectype ;
            } else {
                dvr_log("File open error:%s", newfilename);
                rec_basedir="";
            }
        }
    }
    else if( data->frametype == FRAMETYPE_JPEG ) {
        if( m_recordjpeg ) {
            dvr_log("Jpeg frame captured." );
            sprintf(newfilename, "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_%04d%02d%02d%02d%02d%02d%03d.JPG", 
                    rec_basedir.getstring(), 
                    g_hostname,
                    data->time.year,
                    data->time.month,
                    data->time.day,
                    m_channel,
                    m_channel,
                    data->time.year,
                    data->time.month,
                    data->time.day,
                    data->time.hour,
                    data->time.minute,
                    data->time.second,
                    data->time.milliseconds ) ;
            FILE * jpegf = fopen( newfilename, "wb") ;
            if( jpegf ) {
                fwrite( data->buf, 1, data->bufsize, jpegf );
                fclose( jpegf );
            }
        }
        return 1 ;
    }
    
    // record frame
    if( m_file.isopen() ) {
        m_fileendtime=data->time ;    
        if( m_file.writeframe(data->buf, data->bufsize, data->frametype, &(data->time) )>0 ) {            
            return 1 ;
        }
        dvr_log("record:write file error.");
        closefile();
        rec_basedir="";
    }
    return 0 ;
}

// use prelock_lock to ensure only one prelock thread is running. ( to reduce cpu usage and improve performance )
static int rec_prelock_lock = 0 ;

void * rec_prelock_thread(void * param)
{
    while( rec_prelock_lock ) {
        sleep(1);
    }
    rec_prelock_lock = 1 ;
    
    dvrfile prelockfile ;
    
    if (prelockfile.open((char *)param, "r+")) {
        prelockfile.repairpartiallock();
        prelockfile.close();
    }
    
    delete (char *)(param);

    rec_prelock_lock = 0 ;

    return NULL ;
}


// duplication pre-lock data
void rec_channel::prelock(string & lockfilename, struct dvrtime * locktime)
{
    char * lockmark ;
    struct dvrtime filetime ;
    int    filelength ;
	int    locklen ;

    filelength = f264length ( lockfilename.getstring() );
    f264time ( lockfilename.getstring(), & filetime );
	locklen = filelength - (int)(*locktime-filetime) ;
	
	if( locklen<5 ) {
		return ;				// lock length too short ?
	}
    else if( (filelength-locklen)<5 ) {	// close to full locked file
		dvrfile::chrecmod( lockfilename, 'N', 'L' );
		return ;
	}
		
	// rename to partial locked file name
	lockmark = strstr( lockfilename.getstring(), "_N_" ) ;
	if( lockmark ) {
        char * thlockfilename = new char [lockfilename.length()+40] ;
		strcpy( thlockfilename, lockfilename.getstring() );
		char * ptail = &thlockfilename[ lockmark-lockfilename.getstring() ] ;
        sprintf( ptail, "_L_%d_%s",
					locklen,
                    lockmark+3 );
        dvrfile::rename( lockfilename.getstring(), thlockfilename );
		lockfilename = thlockfilename ;
            
		// create a thread to break this partial locked file
		pthread_t prelockthread ;
		if( pthread_create(&prelockthread, NULL, rec_prelock_thread, (void *)thlockfilename ) == 0 ) {
			pthread_detach( prelockthread );
		}
		else {
            delete thlockfilename ;
		}
	}

}

int rec_channel::dorecord()
{
    rec_fifo *fifo;
    fifo = getfifo();
    if (fifo == NULL)	{	
        // no fifos pending
		if( m_recording && (g_timetick-m_activetime)>5000 ) {  // inactive for 5 seconds
			closefile();
			m_recording = 0 ;
		}
		return 0;
    }
    else {    
        m_recording = recorddata( fifo ) ;
        m_activetime=g_timetick;
        // release buffer
        deletefifo( fifo );
        if( m_fifosize>rec_fifobusy ) {
            rec_busy=1 ;
        }
        // check fifo size.
        if( g_memfree < rec_lowmemory ) {			// total memory available is critical 
            m_reqbreak = 1 ;
            int s=clearfifo();
            dvr_log("Memory too low, frames dropped! (CH%02d)-(%d)", m_channel, s ); 
        }
        if( m_fifosize>rec_fifo_size ) {			// fifo maximum size 4Mb per channel
            m_reqbreak = 1 ;
            int s=clearfifo();
            dvr_log("Recording fifo too big, frames dropped! (CH%02d)-(%d)", m_channel, s ); 
        }
        return 1 ;
    }
}

void *rec_thread(void *param)
{
    int disktick=0 ;
	int ch;

    while (rec_run) {
        int w=0 ;
        for( ch=0 ; ch<rec_channels; ch++ ) {
            if(recchannel[ch]->dorecord() ) {
                w=1 ;
            }
        }
        if( w==0 ) {
            rec_busy=0;
            rec_wait();
        }
        // check disk space every 3 seconds, moved from main thread, so main thread can be more responsive to IO input.
        if( g_timetick>disktick ) {
            disk_check();
            disktick = g_timetick+3000 ;
        }
    }        
    for (ch = 0; ch < rec_channels; ch++) {
        recchannel[ch]->closefile();
    }
    rec_busy=0 ;

    return NULL;
}

// update record status, sensor status channged or motion status changed
void rec_channel::update()
{
    int i;

    if( m_recstate == REC_STOP || dio_record==0 ) {
        if (m_file.isopen()) {
            onframe(NULL);
        }
        return ;
    }

    // triggering
    int trigger = 0 ;           // trigger 0: no trigger, 1: N trigger, 2: L trigger

    if( m_recordmode == 0 ) {			// continue recording mode
        trigger = 1 ;                   // as always triggered
    }
    else if( m_recordmode==2 || m_recordmode==3 ) {	// motion trigger mode
        if( cap_channel[m_channel]->getmotion() ) { // check motion
            trigger=1 ;
        }
    }    

    if( event_tm ) {
        trigger = 2 ;
    }
    
    if( m_recordmode==0 || m_recordmode==1 || m_recordmode==3 ) {	// check sensor for trigggering and event marker
        // check sensors
        for(i=0; i<num_sensors && trigger<2 ; i++) {
            int tr = sensors[i]->eventmarker()? 2 : 1 ;
            if( ( m_triggersensor[i] & 1 ) )              // sensor on 
            {
                if(sensors[i]->value() ) 
                {
                    trigger=tr ;
                }
            }
            else if( ( m_triggersensor[i] & 2 ) )          // sensor off
            {
                if( sensors[i]->value()==0 ) 
                {
                    trigger=tr ;
                }
            }
            else if( sensors[i]->toggle() ) {
                if( ( m_triggersensor[i] & 4 ) &&          // sensor turn on
                    sensors[i]->value() ) 
                {
                    trigger = tr ;
                }
                if( (m_triggersensor[i] & 8 ) &&           // sensor turn off
                    sensors[i]->value()==0 ) 
                {
                    trigger = tr ;
                }
            }
        }
    }

    if( m_gforce_trigger && 
       ( trigger==0 || (trigger==1 && m_gforce_lock!=0 ) ) )
    {
        // G-Force sensor triggering check
        float gf, gr, gd ;
        if( dio_getgforce( &gf, &gr, &gd) ) {
            if(gf<0.0) gf=-gf ;
            if(gr<0.0) gr=-gr ;
            if(gd<0.0) gd=-gd ;
            if( gf>=m_gforce_gf ||
               gr>=m_gforce_gr ||
               gd>=m_gforce_gd ) 
            {
                trigger =  m_gforce_lock?2:1 ;
            }
        }
    }

    if( trigger == 2 ) {
#ifdef PWII_APP    
        if( m_recstate != REC_LOCK ) {
            screen_setliveview( -1 ) ;
        }
#endif
        m_recstate = REC_LOCK ;

        i = g_timetick + 1000*m_postlock_time ;
        if( m_lock_endtime<i ) {
            m_lock_endtime = i ;
        }
        i = g_timetick + 1000*m_prerecord_time ;
        if( m_rec_endtime<i ) {
            m_rec_endtime = i ;
        }
    }
    else if( trigger == 1 ) {
        if( m_recstate == REC_PRERECORD ) {
            m_recstate = REC_RECORD ;
        }
        i = g_timetick + 1000*m_prerecord_time ;
        if( m_rec_endtime<i ) {
            m_rec_endtime = i ;
        }
    }

    // check if LOCK mode finished
    if( m_recstate == REC_LOCK ) {
        if( g_timetick > m_lock_endtime ) {
            m_recstate = REC_RECORD ;
        }
    }
   
    // check if RECORD mode finished
    if( m_recstate == REC_RECORD ) {
        if( g_timetick > m_rec_endtime ) {
            m_recstate = REC_PRERECORD ;
        }
    }

    if( m_recstate == REC_PRERECORD ) {
        m_lock_endtime = m_rec_endtime = g_timetick ;
    }
    
}

 
// updating recording alarms
void rec_channel::alarm()
{
    if( recstate() &&
        m_recordalarmpattern > 0 && 
        m_recordalarm>=0 && m_recordalarm<num_alarms ) 
    {
        alarms[m_recordalarm]->setvalue(m_recordalarmpattern);
    }
}

void rec_init()
{
    int i;
    string v;
    char * p ;
    config dvrconfig(dvrconfigfile);

    // initialize fifo lock
    pthread_mutex_init(&rec_mutex, NULL);
    rec_sig = 0 ;
    pthread_cond_init(&rec_cond, NULL);

    v = dvrconfig.getvalue("system", "maxfilesize");
    if (sscanf(v.getstring(), "%d", &rec_maxfilesize)>0) {
        i = v.length();
        p=v.getstring();
        if (p[i - 1] == 'M' || p[i - 1] == 'm')
            rec_maxfilesize *= 1000000;
        else if (p[i - 1] == 'K' || p[i - 1] == 'k')
            rec_maxfilesize *= 1000;
    } else {
        rec_maxfilesize = DEFMAXFILESIZE;
    }
    if (rec_maxfilesize < 10000000)
        rec_maxfilesize = 10000000;
    if (rec_maxfilesize > 1000000000)
        rec_maxfilesize = 1000000000;
    
    p = dvrconfig.getvalue("system", "maxfilelength");
    if (sscanf(p, "%d", &rec_maxfiletime)>0) {
        i = strlen(p);
        if (p[i - 1] == 'H' || p[i - 1] == 'h')
            rec_maxfiletime *= 3600;
        else if (p[i - 1] == 'M' || p[i - 1] == 'm')
            rec_maxfiletime *= 60;
    } else {
        rec_maxfiletime = DEFMAXFILETIME;
    }
    if (rec_maxfiletime < 60)
        rec_maxfiletime = 60;
    if (rec_maxfiletime > (24 * 3600))
        rec_maxfiletime = (24 * 3600);
    
    i=dvrconfig.getvalueint("system", "record_fifo_size");
    if( i>1000000 && i<16000000 ) {
        rec_fifo_size=i ;
    }

    rec_fifobusy = dvrconfig.getvalueint("system", "record_fifobusy");
    if( rec_fifobusy < rec_fifo_size/10 ) {
        rec_fifobusy = rec_fifo_size/10 ;
    }
    else if( rec_fifobusy > rec_fifo_size/2 ) {
        rec_fifobusy = rec_fifo_size/2 ;
    }

    rec_lowmemory = dvrconfig.getvalueint("system", "record_lowmemory");
    if( rec_lowmemory  < ( g_lowmemory+2000 ) ) {
        rec_lowmemory = g_lowmemory+2000 ;
    }
    
    rec_threadid=0 ;
    rec_channels=0 ;

    for (i = 0; i < MAXCHANNEL; i++) {
        recchannel[i]=NULL ;
    }

    if( dvrconfig.getvalueint("system", "norecord")>0 ) {
        dvr_log( "Dvr no recording mode."); 
    }
    else {
        rec_channels = dvrconfig.getvalueint("system", "totalcamera");
        if( rec_channels<=0 ) {
            rec_channels = cap_channels ;
        }
        if( rec_channels>MAXCHANNEL ) {
            rec_channels = MAXCHANNEL ;
        }
        if( rec_channels > 0 ) {
            for (i = 0; i < rec_channels; i++) {
                recchannel[i]=new rec_channel( i ) ;
            }
            rec_run = 1;
            pthread_create(&rec_threadid, NULL, rec_thread, NULL);
        }
        
        dvr_log("Record initialized.");
    }

	rec_lock_all = dvrconfig.getvalueint("system", "lock_all");

#ifdef PWII_APP
    pwii_recnostopall = dvrconfig.getvalueint("pwii", "recnostopal");
#endif    
    
    rec_busy=0;

}

void rec_uninit()
{
    // stop rec_thread
    rec_run = 0;
    rec_signal();

    rec_stop() ;

    // wait for pre-lock threads to finish
    int delay;
    for( delay=0; delay<3000; delay++ ) {
        if( rec_prelock_lock ) {
            usleep(100000);
        }
        else {
            break;
        }
    }

    if( rec_threadid ) {
        pthread_join(rec_threadid, NULL);
        rec_threadid = 0 ;
    }

    while( rec_channels>0 ) {
        --rec_channels ;
        if( recchannel[rec_channels] ) {
            delete recchannel[rec_channels] ;
            recchannel[rec_channels] = 0 ;
        }
    }

    pthread_mutex_destroy(&rec_mutex);
    pthread_cond_destroy(&rec_cond);

    dvr_log("Record uninitialized.");

}

void rec_onframe(cap_frame * pframe)
{
    if ( dio_record &&
        rec_run && 
        pframe->channel < rec_channels &&
        recchannel[pframe->channel]!=NULL )
    {
        recchannel[pframe->channel]->onframe(pframe);
    }
}

void rec_record(int channel)
{
    if (channel < rec_channels)
        recchannel[channel]->start();
}

void rec_start()
{
    int i;
    rec_busy=0;
    for(i=0; i<rec_channels; i++) {
        recchannel[i]->start();
    }
}

void rec_stop()
{
    int i;
    for(i=0; i<rec_channels; i++) {
        recchannel[i]->stop();
    }
    rec_busy=0;
}

// return recording channel state
//    0: not recording
//	  1: recording
int  rec_state(int channel)
{
    if (channel>=0 && channel < rec_channels)
        return recchannel[channel]->recstate();
    else {
        int i;
        for( i=0; i<rec_channels; i++ ) {
            if( recchannel[i]->recstate() ) {
                return 1;
            }
        }
    }    
    return rec_prelock_lock ;       // if pre-lock thread running, also consider busy
}

// return recording channel state
//    0: not recording
//	  1: recording
int  rec_lockstate(int channel)
{
    if (channel>=0 && channel < rec_channels)
        return recchannel[channel]->lockstate();
    else {
        int i;
        for( i=0; i<rec_channels; i++ ) {
            if( recchannel[i]->lockstate() ) {
                return 1;
            }
        }
    }    
    return 0 ;
}

void rec_break()
{
    int i;
    for (i = 0; i < rec_channels; i++) {
        recchannel[i]->breakfile();
    }
}

// update recording status
void rec_update()
{
    int i;
    if( rec_run ) {
        if( rec_pause>0 ) {
            rec_pause--;
        }
        for(i=0; i<rec_channels; i++) {
            recchannel[i]->update();
        }
        rec_signal();
    }
}

// update recording alarm
void rec_alarm()
{
    int i;
    for(i=0; i<rec_channels; i++) {
        recchannel[i]->alarm();
    }
}

// open new created file for reading. new vidoe file copying support
FILE * rec_opennfile(int channel, struct nfileinfo * nfi )
{
    return NULL ;
}

#ifdef    PWII_APP

void rec_pwii_recon()
{
    int ch ;
    // turn off all recording
    for( ch=0; ch<rec_channels ; ch++) {
        recchannel[ch]->setforcerecording(REC_FORCEON) ;  // force stop recording
    }
}

void rec_pwii_recoff()
{
    int ch;
    // turn off all recording
    for( ch=0; ch<rec_channels ; ch++) {
        recchannel[ch]->setforcerecording(REC_FORCEOFF) ;  // force stop recording
    }
}

void rec_pwii_toggle_rec( int ch )
{
    if( ch>=0 && ch<rec_channels ) {
        if( recchannel[ch]->recstate() == 0 ) {
            // turn on recording
            recchannel[ch]->setforcerecording(REC_FORCEON) ;       // force start recording
        }
        else {
            // turn off recording
            if( ch == pwii_front_ch ) {                     // first cam
                if( pwii_recnostopall ) {
                    recchannel[ch]->setforcerecording(REC_FORCEOFF) ;  // force stop recording
                }
                else {
                    // turn off all recording
                    rec_pwii_recoff();
                }
            }
            else {
                // turn off this camera
                recchannel[ch]->setforcerecording(REC_FORCEOFF) ;       // force stop recording
            }
        }
    }
}

#endif
