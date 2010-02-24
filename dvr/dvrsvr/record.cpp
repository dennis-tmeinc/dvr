#include "dvr.h"

string rec_basedir;

static pthread_t rec_threadid;
static int rec_run = 0 ;           // 0: quit record thread, 1: recording

int    rec_pause = 0 ;             // 1: to pause recording, while network playback
static int rec_fifo_size = 4000000 ;
int    rec_lock_all = 0 ;			// all files save as locked file
struct dvrtime  rec_cliptime ;      // clip starting time. (pre-recording mode only)
int    rec_on ;                     // recording on


enum REC_STATE {
    REC_STOP,           // no recording
    REC_PRERECORD,      // pre-recording
    REC_RECORD,         // recording
    REC_LOCK            // recording lock file
};

struct rec_fifo {
    struct rec_fifo *next;
    struct dvrtime time;        // captured time
    int key;                    // a key frame
    char *buf;
    int bufsize;
    REC_STATE rectype;          // recording type
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

class rec_channel {
    protected:
        int     m_channel;
        
        int     m_recordmode ;			// 0: continue mode, 123: triggering mode, other: no recording
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
		
        int     m_forcerecording ;        // 0: no force, 1: force recording, 2: force no recording. (added for pwii project)
        int     m_forcerecordontime ;     // force recording turn on time
        int     m_forcerecordofftime ;    // force recording trun off time
        
        struct rec_fifo *m_fifohead;
        struct rec_fifo *m_fifotail;
        int	   m_fifosize ;
        pthread_mutex_t m_fifo_mutex;
        void    fifo_lock() {
            pthread_mutex_lock(&m_fifo_mutex);
        }
        void    fifo_unlock() {
            pthread_mutex_unlock(&m_fifo_mutex);
        }
        
        int m_state_starttime ;	        // record state start time

        // pre-record variables
        int m_prerecord_time ;				// allowed pre-recording time
        //	list <int> m_prerecord_list ;		// pre-record pos list
        //	FILE * m_prerecord_file ;			// pre-record file
        //	string m_prerecord_filename ;		// pre-record filename
        //	dvrtime m_prerecord_filestart ;		// pre-record file starttime
        
        // post-record variables
        int m_postrecord_time ;				// post-recording time	length
        
        // lock record variables
        int m_prelock_time ;				// pre lock time length
        int m_postlock_time ;				// post lock time length
        
      
        int m_maxfilesize;
        int m_maxfiletime;
        dvrtime m_filebegintime;
        dvrtime m_fileendtime;
        int m_fileday;						// day of opened file, used for breaking file across midnight
        DWORD m_lastkeytimestamp ;
        
        int m_reqbreak;				// reqested for file break

        int m_recording ;
        
        int m_activetime ;

        //	rec_index m_index ;			// on/off index
        //	int m_onoff ;				// on/off state

        dvrfile m_prelock_file;			// pre-lock recording file

        
    public:
        rec_channel(int channel);
        ~rec_channel();
        
        void start();				// set start record stat
        void stop();				// stop recording, frame data discarded

#ifdef  PWII_APP
        int getforcerecording() {
            return m_forcerecording ;
        }
        void setforcerecording(int force){
            m_forcerecording = force ;
            m_state_starttime = g_timetick ;
        }
#endif
        
        struct rec_fifo *getfifo();
        void putfifo(rec_fifo * fifo);
        int  clearfifo();
        
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
        void update();          						// update recording status
        void alarm();						            // set recording alarm.

        void prelock_thread() ;

};

static rec_channel **recchannel;
static int rec_channels;

int rec_maxfilesize;
int rec_maxfiletime;

rec_channel::rec_channel(int channel)
{
    int i;
    char buf[64];
    config dvrconfig(dvrconfigfile);
    char cameraname[80] ;

    // initialize fifo lock
    memcpy( &m_fifo_mutex, &mutex_init, sizeof(mutex_init));
    fifo_lock();
    m_fifohead = m_fifotail = NULL;
    m_fifosize = 0 ;
    fifo_unlock();

    m_channel = channel;
    sprintf(cameraname, "camera%d", m_channel+1 );

    m_recordmode = dvrconfig.getvalueint( cameraname, "recordmode");
    
    for(i=0;i<num_sensors&&i<MAX_TRIGGERSENSOR;i++) {
        sprintf(buf, "trigger%d", i+1);
        m_triggersensor[i] = dvrconfig.getvalueint( cameraname, buf );
    }

    m_recordalarm = dvrconfig.getvalueint( cameraname, "recordalarm" );
    m_recordalarmpattern = dvrconfig.getvalueint( cameraname, "recordalarmpattern" );
    
    m_recstate=REC_STOP ;
    m_filerecstate=REC_STOP ;
	m_prevfilerecstate=REC_STOP ;
    m_forcerecording = 0 ;
    
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

// pwii, force on/off recording
    m_forcerecordontime = dvrconfig.getvalueint( cameraname, "forcerecordontime" ) ;
    m_forcerecordofftime = dvrconfig.getvalueint( cameraname, "forcerecordofftime" ) ;
    
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

    m_state_starttime = 0 ;
    
    // initialize lock record variables
    m_prelock_time=dvrconfig.getvalueint("system", "prelock" );
    m_postlock_time=dvrconfig.getvalueint("system", "postlock" );
    
    // initialize variables for file write
    m_maxfilesize = rec_maxfilesize;
    m_maxfiletime = rec_maxfiletime;
    time_dvrtime_init( &m_filebegintime, 2000 );
    time_dvrtime_init( &m_fileendtime, 2000 );
    m_fileday=0;
    m_lastkeytimestamp = 0 ;
    m_reqbreak = 0 ;
    
    // start recording	
    start();
}

rec_channel::~rec_channel()
{ 
	m_recstate = REC_STOP;
	closefile();
	clearfifo();
    pthread_mutex_destroy(&m_fifo_mutex);
}

// start record
void rec_channel::start()
{
    time_now( &rec_cliptime ) ;
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
    int s ;
    struct rec_fifo *nfifo;
    
    if( m_recstate == REC_STOP ||
       rec_pause ||
       rec_basedir.length()<=0 ||
       dio_standby_mode )
    {
        if( m_file.isopen() ) {
            nfifo = (struct rec_fifo *) mem_alloc(sizeof(struct rec_fifo));
            nfifo->rectype = REC_STOP ;
            nfifo->buf=NULL ;
            nfifo->bufsize=0 ;
            nfifo->key=1 ;
            time_now(&nfifo->time);
            putfifo( nfifo );
        }
        return ;								// if stop recording.
    }

    // check fifo size.
    if(  pframe->frametype == FRAMETYPE_KEYVIDEO && m_fifosize>rec_fifo_size ) {			// fifo maximum size 4Mb per channel
        s=clearfifo();
        m_reqbreak = 1 ;
        dvr_log("Recording fifo too big, frames dropped! (CH%02d)-(%d)", m_channel, s ); 
    }
    
    // build new fifo frame
    nfifo = (struct rec_fifo *) mem_alloc(sizeof(struct rec_fifo));

    if (mem_check(pframe->framedata)) {
        nfifo->buf = (char *) mem_addref(pframe->framedata);
    } else {
        nfifo->buf = (char *) mem_alloc(nfifo->bufsize);
        mem_cpy32(nfifo->buf, pframe->framedata, nfifo->bufsize);
    }
    nfifo->bufsize = pframe->framesize;
    
    if( pframe->frametype == FRAMETYPE_KEYVIDEO )   {           // this is a keyframe ?
        nfifo->key=1 ;
    }
    else {
        nfifo->key=0 ;
    }
    
    // set frame recording type
    nfifo->rectype = m_recstate ;
    time_now(&nfifo->time);
    putfifo( nfifo );

}

// add fifo into fifo queue
void rec_channel::putfifo(rec_fifo * fifo)
{
    fifo_lock();
    if( fifo!=NULL ){
        if (m_fifohead == NULL) {
            m_fifotail = m_fifohead = fifo;
            m_fifosize = fifo->bufsize ;
        } else {
            m_fifotail->next = fifo;
            m_fifotail = fifo;
            m_fifosize+=fifo->bufsize ;
        }
        fifo->next = NULL ;
    }
    fifo_unlock();
}

struct rec_fifo *rec_channel::getfifo()
{
    struct rec_fifo *head;
    fifo_lock();
    if ( (head = m_fifohead) != NULL) {
        m_fifohead = head->next;
        m_fifosize-= head->bufsize ;
    }
    fifo_unlock();
    return head;
}

// clear fifos, return fifos deleted.
int rec_channel::clearfifo()
{
    int c=0;
    struct rec_fifo * head ;
    struct rec_fifo * n ;

    fifo_lock();
    head = m_fifohead ;
    m_fifohead=NULL ;
    m_fifosize=0 ;
    fifo_unlock();
    
    while (head) {
        n = head->next ;
        if( head->buf ) {
            mem_free(head->buf);
        }
        mem_free(head);
        c++ ;
        head = n ;
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
    char * buf ;
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
    buf = (char *)mem_alloc(1024) ;
    while( (rdsize=file_read(buf, 1024, source))>0 ) {
        totalsize+=file_write(buf, rdsize, dest);
        sleep(1);
    }
    mem_free( buf );
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
            sprintf( rn, "_%d_%c_%s.264", 
                    filelength, 
                    rec_stname( m_filerecstate ),
                    g_hostname );
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

            file_sync();										// sync file data and file name to disk
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
    
    if( data->bufsize<=0 || data->buf==NULL ) {
        closefile();
        return 0;
    }
    
    // check if file need to be broken
    if( data->key ) {           // close or open file on key frame
        DWORD timestamp ;
        timestamp = ((struct hd_frame *)(data->buf))->timestamp ;

        if ( m_file.isopen() ) {	
            if( m_reqbreak ||
                data->rectype != m_filerecstate ||
                timestamp < m_lastkeytimestamp || 
                (timestamp-m_lastkeytimestamp)>1000 ||
                m_file.tell() > m_maxfilesize ||
                data->time.day != m_fileday ||
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
            m_lastkeytimestamp = timestamp ;
        }
        
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
                "/CH%02d_%04d%02d%02d%02d%02d%02d_0_%c_%s.264", 
                m_channel,
                data->time.year,
                data->time.month,
                data->time.day,
                data->time.hour,
                data->time.minute,
                data->time.second,
                rec_stname( data->rectype ),
                g_hostname);
            m_fileday = data->time.day ;
            if (m_file.open(newfilename, "wb")) {
                m_filename = newfilename;
                m_filebegintime = data->time;
                m_fileendtime = data->time;
                m_filebegintime.milliseconds=0 ;
                m_lastkeytimestamp = timestamp ;
                m_filerecstate = data->rectype ;
                //			dvr_log("Start recording file: %s", newfilename);
            } else {
                dvr_log("File open error:%s", newfilename);
                rec_basedir="";
            }
        }
    }
    
    // record frame
    if( m_file.isopen() ) {
        if( m_file.writeframe(data->buf, data->bufsize, data->key, &(data->time) )>0 ) {
            m_fileendtime=data->time ;    
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
    sleep(1);                                   // let recording thread 

    dvr_lock() ;
    while( rec_prelock_lock ) {
        dvr_unlock();
        sleep(1);
        dvr_lock();
    }
    rec_prelock_lock = 1 ;
    dvr_unlock() ;
    
    dvrfile prelockfile ;
    
    if (prelockfile.open((char *)param, "r+")) {
        prelockfile.repairpartiallock();
        prelockfile.close();
    }
    
    mem_free( param );

    dvr_lock() ;
    rec_prelock_lock = 0 ;
    dvr_unlock() ;

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
        char * thlockfilename = (char *) mem_alloc( lockfilename.length()+40 ) ;
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
			mem_free(thlockfilename) ;
		}
	}

}

int rec_channel::dorecord()
{
    rec_fifo *fifo;
    fifo = getfifo();
    if (fifo == NULL)	{	// no fifos pending
		if( m_recording && (g_timetick-m_activetime)>5000 ) {  // inactive for 5 seconds
			closefile();
			m_recording = 0 ;
		}
		return 0;
    }
	m_activetime=g_timetick;
    if(fifo->rectype == REC_STOP  || fifo->buf == NULL ) {
        m_fileendtime=fifo->time ;
        closefile();                       
        m_recording = 0 ;
    }
    else {
        m_recording = recorddata( fifo ) ;
    }

    // release buffer
    if( fifo->buf ) {
        mem_free(fifo->buf);
    }
    mem_free(fifo);

    return 1 ;
}


void *rec_thread(void *param)
{
	int ch;
    int i ;
    int wframes ;
    int norec=0 ;
    int rtick=0 ;

    while (rec_run) {

        file_sync();
        
        // check disk space every 3 seconds, moved from main thread, so main thread can be more responsive to IO input.
        if( (g_timetick-rtick)>3000 ) {
            disk_check();
            rtick = g_timetick ;
        }
        
        wframes = 0 ;
        for( ch = 0 ; ch <rec_channels ; ch++ ) {
            for( i=0; i<100 ; i++ ) {
                if(recchannel[ch]->dorecord()) {
                    wframes++;
                }
                else { 
                    break;
                }
            }
        }
        if( wframes==0 ) {
            usleep(100000);
			if( ++norec > 50 ) {
                for (ch = 0; ch < rec_channels; ch++) {
                    recchannel[ch]->closefile();
                }
                norec=0 ;
            }
            else if( rec_pause > 0 ) {
                rec_pause-- ;
            }
        }
        else {
			norec=0 ;
		}
    }        
    for (ch = 0; ch < rec_channels; ch++) {
        recchannel[ch]->closefile();
    }
    file_sync();
    return NULL;
}

// update record status, sensor status channged or motion status changed
void rec_channel::update()
{
    int i;

    if( m_recstate == REC_STOP ) {
        return ;
    }

#ifdef    PWII_APP    

    if( m_forcerecording ) {
        if( m_forcerecording == 1 ) {
            if( (g_timetick-m_state_starttime )/1000 > m_forcerecordontime ) {     // force recording timeout
                m_forcerecording = 0 ;
                m_recstate = REC_PRERECORD ;
            }
//            if( m_recstate == REC_PRERECORD ) {
//                m_recstate = REC_RECORD ;
//            }
            m_recstate = REC_LOCK ;             // REC button make locked file
        }
        else if( m_forcerecording==2 ) {
            if( (g_timetick-m_state_starttime )/1000 > m_forcerecordofftime ) {     // force recording off timeout
                m_forcerecording = 0 ;
            }
            m_recstate = REC_PRERECORD ;
        }
        return ;                    // dont continue ;
    }

#endif

    if( event_marker ) {
        m_recstate = REC_LOCK ;
        m_state_starttime = g_timetick ;
        return ;
    }

    if( m_recstate == REC_LOCK && 
       (g_timetick - m_state_starttime)/1000 < m_postlock_time )
    {
        return ;								
    }
    
    if( m_recordmode == 0 ) {			// continue recording mode
        m_recstate = REC_RECORD ;
        return ;
    }

    // check trigging mode
    int trigger = 0 ;
    // check sensors
    if( m_recordmode==1 || m_recordmode==3 ) {				// sensor triggger mode
        for(i=0; i<num_sensors; i++) {
            if( ( m_triggersensor[i] & 1 ) &&              // sensor on
               sensors[i]->value() ) 
            {
                trigger=1 ;
                break ;
            }
            if( ( m_triggersensor[i] & 2 ) &&              // sensor off
               sensors[i]->value()==0 ) 
            {
                trigger = 1 ;
                break ;
            }
            if( sensors[i]->toggle() ) {
                if( ( m_triggersensor[i] & 4 ) &&              // sensor turn on
                   sensors[i]->value() ) 
                {
                    trigger = 1 ;
                    break ;
                }
                if( (m_triggersensor[i] & 8 ) &&              // sensor turn off
                   sensors[i]->value()==0 ) 
                {
                    trigger = 1 ;
                    break ;
                }
            }
        }
    }

    // check motion
    if( trigger==0 && ( m_recordmode==2 || m_recordmode==3 ) ) {				// motion trigger mode
        if( cap_channel[m_channel]->getmotion() ) {
            trigger=1 ;
        }
    }    

    if( trigger ) {
        m_recstate = REC_RECORD ;
        m_state_starttime = g_timetick ;
    }
    else if( m_recstate != REC_PRERECORD ) {
        if( (g_timetick - m_state_starttime)/1000 > m_postrecord_time ){
            m_recstate = REC_PRERECORD ;
        }
        else {
            m_recstate = REC_RECORD ;
        }
    }
}

 
// updating recording alarms
void rec_channel::alarm()
{
    if( recstate() &&
        m_recordalarmpattern > 0 && 
        m_recordalarm>=0 && m_recordalarm<num_alarms ) {
            alarms[m_recordalarm]->setvalue(m_recordalarmpattern);
        }
}

void rec_init()
{
    int i;
    string v;
    config dvrconfig(dvrconfigfile);
    v = dvrconfig.getvalue("system", "maxfilesize");
    if (sscanf(v.getstring(), "%d", &rec_maxfilesize)>0) {
        i = v.length();
        if (v[i - 1] == 'M' || v[i - 1] == 'm')
            rec_maxfilesize *= 1000000;
        else if (v[i - 1] == 'K' || v[i - 1] == 'k')
            rec_maxfilesize *= 1000;
    } else {
        rec_maxfilesize = DEFMAXFILESIZE;
    }
    if (rec_maxfilesize < 10000000)
        rec_maxfilesize = 10000000;
    if (rec_maxfilesize > 1000000000)
        rec_maxfilesize = 1000000000;
    
    v = dvrconfig.getvalue("system", "maxfilelength");
    if (sscanf(v.getstring(), "%d", &rec_maxfiletime)>0) {
        i = v.length();
        if (v[i - 1] == 'H' || v[i - 1] == 'h')
            rec_maxfiletime *= 3600;
        else if (v[i - 1] == 'M' || v[i - 1] == 'm')
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
    
    rec_threadid=0 ;
    if( dvrconfig.getvalueint("system", "norecord")>0 ) {
        rec_channels = 0 ;
        recchannel = NULL ;
        dvr_log( "Dvr no recording mode."); 
    }
    else {
        rec_channels = dvrconfig.getvalueint("system", "totalcamera");
        if( rec_channels<=0 ) {
            rec_channels = cap_channels ;
        }
        if( rec_channels > 0 ) {
            recchannel =  new rec_channel * [ rec_channels ];
            for (i = 0; i < rec_channels; i++) {
                recchannel[i]=new rec_channel( i ) ;
            }
            rec_run = 1;
            pthread_create(&rec_threadid, NULL, rec_thread, NULL);
        }
        
        dvr_log("Record initialized.");
    }

	rec_lock_all = dvrconfig.getvalueint("system", "lock_all");
    rec_on=0 ;
}

void rec_uninit()
{
    int i;
    i=rec_channels ;
    rec_channels=0;
    if( recchannel ) {
        // stop rec_thread
        rec_run = 0;
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
        
        for (i--; i>=0; i--) {
            delete recchannel[i] ;
        }
        
        delete recchannel ;
        recchannel=NULL ;
        dvr_log("Record uninitialized.");
    }
    rec_on=0 ;
}

void rec_onframe(cap_frame * pframe)
{
    if ( rec_run && pframe->channel < rec_channels &&
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
                if( rec_on == 0 ) {
                    rec_on = 1 ;
                    time_now(&rec_cliptime) ;
                    dvr_log( "Recording started. ID: %s_%02d%02d%02d%02d%02d", g_hostname,
                        rec_cliptime.year%100,
                        rec_cliptime.month,
                        rec_cliptime.day,
                        rec_cliptime.hour,
                        rec_cliptime.minute
                        );
                }
                return 1;
            }
        }
    }    
    if( rec_on ) {
        rec_on=0;
        dvr_log( "Recording stopped." );
    }
    return rec_prelock_lock ;       // if pre-lock thread running, also consider busy
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
    for(i=0; i<rec_channels; i++) {
        recchannel[i]->update();
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
    if (channel>=0 && channel < rec_channels) {
        return NULL;
    }
    return NULL ;
}

#ifdef    PWII_APP

extern int pwii_front_ch ;         // pwii front camera channel
extern int pwii_rear_ch ;          // pwii real camera channel

void rec_pwii_toggle_rec_front()
{
    if( rec_channels > 1 ) {
        screen_setliveview(pwii_front_ch);                              // start live view front camera
        int state1 = recchannel[pwii_front_ch]->getforcerecording() ;
        if( state1 == 0 ) {
            state1 = recchannel[pwii_front_ch]->recstate();
            if( state1 ) {
                recchannel[pwii_front_ch]->setforcerecording(2) ;       // force stop recording
                recchannel[pwii_rear_ch]->setforcerecording(2) ;       // force stop recording
            }
            else {
                recchannel[pwii_front_ch]->setforcerecording(1) ;       // force start recording
            }
        }
        else if( state1 ==1 ) {
                recchannel[pwii_front_ch]->setforcerecording(2) ;       // force stop recording
                recchannel[pwii_rear_ch]->setforcerecording(2) ;       // force stop recording
        }
        else {
                recchannel[pwii_front_ch]->setforcerecording(1) ;       // force start recording
        }
    }
}
            
void rec_pwii_toggle_rec_rear() 
{
    if( rec_channels > 1 ) {
        screen_setliveview(pwii_rear_ch);                              // start live view rear camera
        int state1 = recchannel[pwii_rear_ch]->getforcerecording() ;
        if( state1 == 0 ) {
            state1 = recchannel[pwii_rear_ch]->recstate();
            if( state1 ) {
                recchannel[pwii_rear_ch]->setforcerecording(2) ;       // force stop recording
            }
            else {
                recchannel[pwii_rear_ch]->setforcerecording(1) ;       // force start recording
            }
        }
        else if( state1 ==1 ) {
                recchannel[pwii_rear_ch]->setforcerecording(2) ;       // force stop recording
        }
        else {
                recchannel[pwii_rear_ch]->setforcerecording(1) ;       // force start recording
        }
    }
}

#endif
