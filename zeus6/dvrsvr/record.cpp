/* --- Changes ---
 * 09/02/2009 by Harrison
 *   1. Fixed Record status for Sensor trigger mode
 *
 * 10/08/2009 by Harrison
 *   1. Fixed Sensor trigger recording bug (offset by 1)
 *
 */

#include "dvr.h"
#include "../ioprocess/diomap.h"

string rec_basedir;

static pthread_t rec_threadid;
static int rec_run = 0 ;           // 0: quit record thread, 1: recording

int    rec_trigger = 0 ;
int    rec_busy = 0 ;              // 1: recording thread is busy, 0: recording is idle
int    rec_pause = 0 ;             // 1: to pause recording, while network playback
static int rec_fifo_size = 4000000 ;
pthread_mutex_t prelock_mutex;
static int rec_minimum_inmem = 30000 ;	// 20 M
static int rec_prerecord_inmem = 60 ;

//#define FRAME_DEBUG

enum REC_STATE {
    REC_STOP,           // no recording
    REC_PRERECORD,      // pre-recording
    REC_RECORD,         // recording
    REC_LOCK,           // recording lock file
};


struct rec_fifo {
    struct rec_fifo *next;
    struct dvrtime time;        // captured time
    int key;                    // a key frame
    char *buf;
    int bufsize;
    DWORD timestamp;
    REC_STATE rectype;          // recording type
};

class rec_channel {
    protected:
        int     m_channel;
        
        int     m_recordmode ;			// 0: continue mode, 123: triggering mode, other: no recording
#if defined(TVS_APP) || defined(PWII_APP)
        unsigned int m_triggersensor[32] ;  // trigger sensors
#endif

#ifdef MDVR_APP
        unsigned int m_triggersensor ;  		// trigger sensor bit maps
#endif

        int     m_recordalarm ;
        int     m_recordalarmpattern ;

        REC_STATE	m_recstate;
        int     m_recreq ;						// recording requested, ( trigger by key frames )
        
        struct rec_fifo *m_fifohead;
        struct rec_fifo *m_fifotail;
        int	   m_fifosize ;
        
        int    m_inmemprerecord ;
        
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
        
        int m_gforce_trigger ;				// gforce trigger enable
		float m_gforce_trigger_value ;		// gforce trigger value 
		int m_speed_trigger ;				// enable speed trigger ? 
		float m_speed_trigger_value ;		// speed trigger value 

        
        // variable for file write
        string m_prevfilename ;				// previous recorded file name.
        string m_filename;
        REC_STATE   m_filerecstate ;
        dvrfile m_file;						// recording file
        
        int m_maxfilesize;
        int m_maxfiletime;
        dvrtime m_filebegintime;
        dvrtime m_fileendtime;
        int m_fileday;						// day of opened file, used for breaking file across midnight
        DWORD m_lastkeytimestamp ;
        DWORD m_lasttimestamp ;
        
        int m_reqbreak;				// reqested for file break
        
        int m_active ;
        time_t m_activetime ;

        //	rec_index m_index ;			// on/off index
        //	int m_onoff ;				// on/off state

        dvrfile m_prelock_file;			// pre-lock recording file
        struct dvrtime m_state_starttime ;	        // record state start time      
        int m_framerate;
		int m_width;
		int m_height;
		int framedroped;
        struct File_format_info mFileHeader;
		pthread_mutex_t m_lock;
		
	public:
        rec_channel(int channel);
        ~rec_channel();
        
        void start();				// set start record stat
        void stop();				// stop recording, frame data discarded


#ifdef  PWII_APP

		int     m_forcerecordontime ;     // force recording turn on time
		int     m_lock_endtime ;

#ifdef APP_PWZ5
		int     m_c_channel ;		// force recording channel
#endif        
       
		// force recording feature for PWII
		void setforcerecording(int force){
			int tick = time_gettick();
			if( force ) {
				m_recstate = REC_LOCK ;
				m_lock_endtime = tick + 1000*m_forcerecordontime ;
			}
			else {
				m_recstate = REC_PRERECORD ;
				m_lock_endtime = tick ;
			}
		}
		
		// acturally to check if recording mode is on
		int checkforcerecording() {
			return ( m_recstate == REC_RECORD ||
					// m_recstate == REC_POSTRECORD ||
					// m_recstate == REC_POSTLOCK ||
					m_recstate == REC_LOCK );
		}
#endif

        struct rec_fifo * peekfifo();
        struct rec_fifo * getfifo();
        void putfifo(rec_fifo * fifo);
        void releasefifo(struct rec_fifo *fifo);
        int  clearfifo();
        void addfifo(cap_frame * pframe);
        int setfifotype( REC_STATE t );
        
        void onframe(cap_frame * pframe);
        int dorecord();
        
        void recorddata(rec_fifo * data);				// record stream data
        //	void closeprerecord( int lock );			// close precord data, save them to normal video file
        //	void prerecorddata(rec_fifo * data);		// record pre record data
        void prelock();									// process prelock data
        
        void closefile();
        int  fileclosed();

        void breakfile(){			// start a new recording file
            m_reqbreak = 1;
        }
        int  recstate() {
            return( m_file.isopen() &&
		    (m_filerecstate == REC_RECORD ||
		     //m_filerecstate == REC_POSTLOCK ||
		     //m_filerecstate == REC_POSTRECORD ||
		     m_filerecstate == REC_LOCK) );
        }
        int  lockstate() {
            return( m_file.isopen() &&
		    m_filerecstate == REC_LOCK  );
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
    int trigger ;
    string value ;
    
    m_channel = channel;
    sprintf(cameraname, "camera%d", m_channel+1 );

    m_recordmode = dvrconfig.getvalueint( cameraname, "recordmode");
    
#ifdef MDVR_APP    
    m_triggersensor = 0 ;
    for(i=0;i<num_sensors;i++) {
        sprintf(buf, "trigger%d", i+1);
        trigger = dvrconfig.getvalueint( cameraname, buf );
        if( trigger>0 && trigger<32 ) {
			  m_triggersensor |= 1<<(trigger - 1) ;
        }
    }
#endif

#if defined(TVS_APP) || defined(PWII_APP)
	memset( &m_triggersensor, 0, sizeof(m_triggersensor) );   
   for(i=0;i<num_sensors;i++) {
        sprintf(buf, "trigger%d", i+1);
        m_triggersensor[i] = dvrconfig.getvalueint( cameraname, buf );
   }
#endif

#ifdef PWII_APP
// pwii, force on/off recording
    m_forcerecordontime = dvrconfig.getvalueint( cameraname, "forcerecordontime" ) ;
    if(m_forcerecordontime==0) {
		m_forcerecordontime = 7200 ;
	}
	m_lock_endtime = 0 ;
	
	m_gforce_trigger = dvrconfig.getvalueint( cameraname, "gforce_trigger" ) ;
    value = dvrconfig.getvalue(cameraname, "gforce_trigger_value");
	if( value.length()>0 ) {
		sscanf( (char *)value, "%f", &m_gforce_trigger_value );
	}
	else {
		// make it impossible
		m_gforce_trigger_value = 100.0 ;
	}

	m_speed_trigger = dvrconfig.getvalueint( cameraname, "speed_trigger" ) ;
    value = dvrconfig.getvalue(cameraname, "speed_trigger_value");
	if( value.length()>0 ) {
		sscanf( (char *)value, "%f", &m_speed_trigger_value );
	}
	else {
		// make it impossible
		m_speed_trigger_value = 10000.0 ;
	}
	
#endif

#ifdef APP_PWZ5
	m_c_channel = dvrconfig.getvalueint( cameraname, "forcerecordchannel" ) ;
#endif        

    m_recordalarm = dvrconfig.getvalueint( cameraname, "recordalarm" );
    m_recordalarmpattern = dvrconfig.getvalueint( cameraname, "recordalarmpattern" );
    
    m_recstate=REC_STOP ;
    m_filerecstate=REC_STOP ;
    
    m_fifohead = m_fifotail = NULL;
    m_fifosize = 0 ;
    m_inmemprerecord = 0 ;
    
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
    
    m_framerate=dvrconfig.getvalueint(cameraname, "framerate");
 
    m_width=-1;
    m_height=-1;
    
    memcpy((void*)&mFileHeader,Dvr264Header,40);
    
    m_width=GetVideoWidth(m_channel);
    m_height=GetVideoHight(m_channel);   
    mFileHeader.video_height=m_height;
    mFileHeader.video_width=m_width;
    
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
    framedroped=0;
    pthread_mutex_init(&m_lock,NULL);
    // start recording	
    start();
}

rec_channel::~rec_channel()
{ 
    m_recstate = REC_STOP;
    closefile();
    clearfifo();
    pthread_mutex_destroy(&m_lock);
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
}

// stop recording, frame data discarded
void rec_channel::stop()
{
    m_recstate = REC_STOP;
}
///////////////////////////
void rec_channel::addfifo(cap_frame * pframe)
{
    int s ;
    struct rec_fifo *nfifo;
    
    if( m_recstate == REC_STOP ||
       rec_pause ||
       rec_basedir.length()<=0 ||
       dio_standby_mode )
    {
		clearfifo();
        if( m_file.isopen() ) {
			closefile();
        }
        return ;  // if stop recording.
    }
    
	if(framedroped && pframe->frametype == FRAMETYPE_VIDEO ) {			// discard non-key video frames after dropping frames
		return;
	}
	
    if( (!m_inmemprerecord) && m_fifosize > rec_fifo_size ){
		if( pframe->frametype == FRAMETYPE_KEYVIDEO ) {
			if( mem_available() < 20000 ) {	// critical low memory !!!
				// drop all fifos
				s = clearfifo();
				dvr_log("Very low free memory, %d frames of channel % dropped!", s, m_channel);
			}
		}
		else if( pframe->frametype == FRAMETYPE_VIDEO ) {	// non key video frame
			// to drop non key frames
			framedroped=1;
			return;
		} 
    }
    
    // build new fifo frame
    nfifo = (struct rec_fifo *) mem_alloc(sizeof(struct rec_fifo));
	if(nfifo==NULL){
		// No memory???,
		s = clearfifo();
		dvr_log("Memory allocation failed!!! %d frames dropped!", s );
		framedroped=1;
		return ;
	}
	memset( nfifo, 0, sizeof( rec_fifo ) );

    time_now(&nfifo->time);
	nfifo->bufsize = pframe->framesize;
    if (mem_check(pframe->framedata)) {
        nfifo->buf = (char *) mem_addref(pframe->framedata);
    } 
    else {
        nfifo->buf = (char *) mem_alloc(nfifo->bufsize);
        if( nfifo->buf ) {
			mem_cpy(nfifo->buf, pframe->framedata, nfifo->bufsize);
		}
		else {
			// No memory???,
            mem_free(nfifo);

			// No memory???,
			s = clearfifo();
			dvr_log("Memory allocation failed!!! %d frames dropped!", s );
			framedroped=1;
			
			return ;
        }
    }
    
    if( pframe->frametype == FRAMETYPE_KEYVIDEO )   {  // this is a keyframe ?
        framedroped=0;
        nfifo->key=1 ;
        m_recreq = 1 ;			// to continue recording thread
    }
    else {
        nfifo->key=0 ;
    }
    
    // set frame recording type
    nfifo->rectype = m_recstate ;
    nfifo->timestamp=pframe->timestamp;
    putfifo( nfifo );
    
    // get out of in-memory pre-recording
    if( m_inmemprerecord && nfifo->rectype != REC_PRERECORD ) {
		// change all pre-record frame to new rec type
		setfifotype( nfifo->rectype );
		m_inmemprerecord = 0 ;
	}

}

// add fifo into fifo queue
void rec_channel::putfifo(rec_fifo * fifo)
{
    if( fifo==NULL )
        return ;
    pthread_mutex_lock(&m_lock);
    if (m_fifotail == NULL) {
        m_fifotail = m_fifohead = fifo;
        m_fifosize = fifo->bufsize ;
    } else {
        m_fifotail->next = fifo;
        m_fifotail = fifo;
        m_fifosize+=fifo->bufsize ;
    }
    fifo->next = NULL ;
    pthread_mutex_unlock(&m_lock);
}

struct rec_fifo *rec_channel::getfifo()
{
    struct rec_fifo *head;
    pthread_mutex_lock(&m_lock);
    head = m_fifohead;
    if (head != NULL) {
        m_fifohead = head->next;
        if (m_fifohead == NULL){
            m_fifotail = NULL;
            m_fifosize = 0 ;
        }
        else {
			m_fifosize-=head->bufsize ;
		}
    }
    pthread_mutex_unlock(&m_lock);
    return head;
}

struct rec_fifo *rec_channel::peekfifo()
{
	return m_fifohead ;
}

void rec_channel::releasefifo(struct rec_fifo *fifo)
{
	if( fifo ) {
		if( fifo->buf ) {
			mem_free( fifo->buf );
		}
		mem_free( fifo );
	}
}

// clear fifos, return fifos deleted.
int rec_channel::clearfifo()
{
    int c=0;
    struct rec_fifo * head ;
    pthread_mutex_lock(&m_lock);
    while (m_fifohead) {
        head = m_fifohead ;
        m_fifohead = head->next ;
        releasefifo( head );
        c++ ;
    }
    m_fifotail=NULL ;
    m_fifosize=0 ;
    pthread_mutex_unlock(&m_lock);
    return c ;
}

// set all fifos to new rec type
int rec_channel::setfifotype( REC_STATE t )
{
    struct rec_fifo * w ;
    pthread_mutex_lock(&m_lock);
    w = m_fifohead ;
    while (w) {
		w->rectype = t ;
		w=w->next ;
    }
    pthread_mutex_unlock(&m_lock);
    return t ;
}

void rec_channel::onframe(cap_frame * pframe)
{
    if (pframe->channel == m_channel ) {
        addfifo(pframe);
    }
}

/*
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
    source = fopen( fc->sourcename, "r" ) ;
    if( source==NULL ) {
        fc->start=1 ;
        return NULL ; 
    }
    time_now(&starttime);
    dvr_log("filecopy: source-%s", fc->sourcename );
    dest = fopen( fc->destname, "w" ) ;
    if( dest==NULL ) {
        fc->start=1 ;
        fclose( source );
        return NULL ;
    }
    dvr_log("filecopy: dest-%s", fc->destname );
    fc->start=1;
    totalsize = 0 ;
    buf = new char [1024*1024] ;
    while( (rdsize=fread(buf, 1, 1024*1024, source))>0 ) {
        totalsize+=fwrite(buf, 1, rdsize, dest);
        sleep(1);
    }
    delete buf ;
    fclose( source );
    fclose( dest );
    time_now(&endtime);
    dvr_log("filecopy: size=%dk : time=%ds.", totalsize/1024, endtime-starttime );
    return NULL ;
}
*/
int rec_channel::fileclosed()
{
    if(m_file.isopen()){
       return 0;
    } else {
       return 1;
    }
}

// close all openning files
void rec_channel::closefile()
{
    char newfilename[512];
    char *rn;
    int  filelength ;
    
    if (m_file.isopen()) {
        m_file.close();
		filelength = time_dvrtime_diff(&m_fileendtime, &m_filebegintime);
        if( filelength<5 || filelength>10000 ) {
            m_file.remove(m_filename.getstring());
            m_filename="";
            return ;
        }
        // need to rename to contain file lengh
        strcpy(newfilename, m_filename.getstring());
        if ((rn = strstr(newfilename, "_0_")) != NULL) {
            sprintf( rn, "_%d_%c_%s.266", 
                    filelength, 
                    m_filerecstate == REC_LOCK ? 'L' : 'N',
                    g_hostname );
            m_file.rename( m_filename.getstring(), newfilename ) ;
            
            if( m_filerecstate == REC_PRERECORD ) {			
				// remove previous pre-recording clip
                if( m_prevfilename.length()>0 ) {
                    m_file.remove( m_prevfilename.getstring() );
                }
				// save file name as previous name
				m_prevfilename = newfilename ;
            }
            else {
				m_prevfilename = "" ;
				disk_renew( newfilename );
			}
			
        }
        m_filename="";
		
		// drop caches, hope to free up some main memory
		mem_dropcaches();        
    }
}

// write frame data to disk
//  return  0: data not used (keep in fifo), 1: data used or should be discard
void rec_channel::recorddata(rec_fifo * data)
{
    char newfilename[512];
    int l;
    DWORD timestamp ;
    
    if( data->bufsize<=0 || data->buf==NULL ) {
        closefile();
        return ;
    }
    
    timestamp = data->timestamp; //((struct hd_frame *)(data->buf))->timestamp ;

    // check if file need to be broken
    if ( (m_file.isopen() && data->key) ) {	// must be a key frame
        if( m_reqbreak ||
           timestamp < m_lastkeytimestamp ||
          // (timestamp-m_lastkeytimestamp)>10000 ||
           m_file.tell() > m_maxfilesize ||
           data->time.day != m_fileday ||
          // time_dvrtime_diff( &data->time, &m_filebegintime ) > 240||
           time_dvrtime_diff( &data->time, &m_filebegintime ) > m_maxfiletime ||
           ( m_filerecstate == REC_PRERECORD  && time_dvrtime_diff(&data->time, &m_filebegintime) > m_prerecord_time )
           )
        {
            m_reqbreak=0;
            closefile();
        }
        m_lastkeytimestamp = timestamp ;
    }
    
    // create new dvr file
    if ( (!m_file.isopen()) && data->key ) {	// need to open a new file
        // create new record file
        if (rec_basedir.length() <= 0) {	// no base dir, no recording disk available
            // frame data should be discarded
            return ;
        }

      //  if(m_width<320){
	  m_width=GetVideoWidth(m_channel);
	  m_height=GetVideoHight(m_channel);   
	//}
	//dvr_log("width:%d height:%d, framerate:%d",m_width,m_height,m_framerate);
        mFileHeader.video_height=m_height;
        mFileHeader.video_width=m_width;
	mFileHeader.video_framerate=m_framerate;
        // make date directory
        sprintf(newfilename, "%s/%04d%02d%02d", 
                rec_basedir.getstring(),
                data->time.year,
                data->time.month,
                data->time.day );
        mkdir(newfilename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
        
        // make channel directory
        l = strlen(newfilename);
        sprintf(newfilename + l, "/CH%02d", m_channel);
        mkdir(newfilename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
        
        // make new file name
        l = strlen(newfilename);
        sprintf(newfilename + l,
                "/CH%02d_%04d%02d%02d%02d%02d%02d_0_%c_%s.266", 
                m_channel,
                data->time.year,
                data->time.month,
                data->time.day,
                data->time.hour,
                data->time.minute,
                data->time.second,
                data->rectype==REC_LOCK?'L':'N', 
                g_hostname);
        m_fileday = data->time.day ;
	
        if (m_file.writeopen(newfilename, "wb", &mFileHeader)) {
            m_filename = newfilename;
            m_filebegintime = data->time;
            m_filebegintime.milliseconds=0 ;
            m_lastkeytimestamp = timestamp ;
            m_filerecstate = data->rectype ;
            //			dvr_log("Start recording file: %s", newfilename);
        } else {
            dvr_log("File open error:%s", newfilename);
            rec_basedir="";
        }
    }
    
    // record frame
    if( m_file.isopen() ) {
        if( m_file.writeframe(data->buf, data->bufsize, data->key, &(data->time) )<=0 ) {
            dvr_log("record:write file error.");
            closefile();
            rec_basedir="";
        }
    }
    return  ;
}

// use prelock_lock to ensure only one prelock thread is running. ( to reduce cpu usage and improve performance )
static int rec_prelock_lock = 0 ;

void * rec_prelock_thread(void * param)
{
    dvr_lock(&rec_prelock_lock, 1000000);

 //   pthread_mutex_lock(&prelock_mutex);
    dvrfile prelockfile ;
  //  rec_prelock_lock++;   
    if (prelockfile.open((char *)param, "r+")) {
        prelockfile.repairpartiallock();
        prelockfile.close();
    }
    
    delete (char *)param ;
 //   rec_prelock_lock--;   
 //   pthread_mutex_unlock(&prelock_mutex);
    dvr_unlock(&rec_prelock_lock);
    return NULL ;
}

#if 0
/* rename the recording file(full path) to type */
static int get_new_file_name(char *filename, char *newname, char type)
{
  char *ptr;
  int ret = 1;

  //fprintf(stderr, "rename: %s\n", filename);
  strcpy(newname, filename);
  ptr = strrchr(newname, '/');
  if (ptr) {
    /* look for 1st underbar after CHxx */
    ptr = strchr(ptr + 1, '_');
    if (ptr) {
      /* look for 2nd under bar after YYYYMMDDhhmmss(mmm) */
      ptr = strchr(ptr + 1, '_');
      if (ptr) {
	/* look for 3rd under bar after Length */
	ptr = strchr(ptr + 1, '_');
	if (ptr) {
	  *(ptr + 1) = type;
	  //fprintf(stderr, "%s rename to %s\n", filename, newname);
	  ret = 0;
	}
      }
    }
  }
    
  return ret;
}
#endif

// duplication pre-lock data
void rec_channel::prelock()
{
    char * lockmark ;
    char lockfilename[300] ;
    struct dvrtime pfiletime ;
    int    pfilelength ;
  
#if 0  
    if (!get_new_file_name(m_filename.getstring(), lockfilename, 'L')) {
      m_file.rename( m_filename.getstring(), lockfilename ) ;
      m_filename = lockfilename;
      fprintf(stderr, "renamed to %s\n", m_filename.getstring());
    }
#endif

    pfilelength = time_dvrtime_diff(&m_fileendtime, &m_filebegintime);
    if( pfilelength > m_prelock_time + 10 ) {
        closefile();
    }
    
    if( m_prevfilename.length()<=5 ) {
        return ;
    }
    
    strcpy( lockfilename, m_prevfilename.getstring() );
    pfilelength = f264length ( lockfilename );
    f264time ( lockfilename, & pfiletime );
    
    int beforelock = time_dvrtime_diff(&m_fileendtime, &pfiletime ) - m_prelock_time ;
    if( beforelock < 10 ) {
        // rename file to full lcoked file name
        lockmark = strstr( lockfilename, "_N_" ) ;
        if( lockmark ) {
            lockmark[1] = 'L' ;
            dvrfile::rename( m_prevfilename.getstring(), lockfilename );
            disk_renew( lockfilename );
        }
    } else if( beforelock > 0 && pfilelength - beforelock > 5 ) {
        // rename to partial locked file name
        lockmark = strstr( lockfilename, "_N_" ) ;
        if( lockmark && strlen(lockmark) > 4) {
            char tail[256] ;
            strcpy( tail, &lockmark[3] );
            sprintf( lockmark, "_L_%d_%s", 
                    pfilelength - beforelock, 
                    tail );
            dvrfile::rename( m_prevfilename.getstring(), lockfilename );
            disk_renew( lockfilename );
            
            // create a thread to break this partial locked file
            pthread_t prelockthread ;
            char * thlockfilename ;
            thlockfilename = new char [strlen(lockfilename)+2] ;
            strcpy( thlockfilename, lockfilename );
            if( pthread_create(&prelockthread, NULL, rec_prelock_thread, (void *)thlockfilename ) == 0 ) {
                pthread_detach( prelockthread );
            }
            else {
                delete thlockfilename ;
            }
        }
    }
    // else  all file no locked
}

int rec_channel::dorecord()
{
    rec_fifo *fifo;
    
    fifo = peekfifo();
    if (fifo == NULL)	{	// no fifos pending
        if( m_file.isopen() ) {
            if( m_active ) {
                m_active=0 ;
                m_activetime=time(NULL);
            }
            else {
                time_t t = time(NULL);
                if( (int)(t-m_activetime)>5 ) {
                    closefile();
                }
            }
        }
        m_recreq = 0 ;			//  stop recording until next key frames
        return 0 ;
    }
    
    if( m_recreq == 0 ) {
		return 0 ;
	}
    
    // check in-memory pre-recording
	if( m_inmemprerecord )
	{
	    pthread_mutex_lock(&m_lock);			// preview adding filfo to fifo tail
        while( m_fifohead!=NULL && time_dvrtime_diff( &(m_fifotail->time), &(m_fifohead->time) ) > m_prerecord_time ){
			// release one key frame blocks
			while( m_fifohead!=NULL ) {
				fifo = m_fifohead ;
				m_fifohead = fifo->next ;
				m_fifosize-= fifo->bufsize ;
				releasefifo( fifo ) ;
				if( m_fifohead == NULL ) {
					m_fifotail = 0 ;
					m_fifosize = 0 ;
					break;
				}
				else if( m_fifohead->key ) {
					break;
				}
			}
		}
		pthread_mutex_unlock(&m_lock);

		// check avaialbe memory
		if( mem_available() < rec_minimum_inmem ) {
			rec_minimum_inmem -= 2000 ;		// lesser, so other channel may continue using inmem
			if( rec_minimum_inmem<24000 )
				rec_minimum_inmem = 30000 ;
			m_inmemprerecord = 0 ;			// stop in-mem pre-record, and fall back to in-file pre-recording
			m_filerecstate = REC_PRERECORD ;
			m_recreq = 1 ;					// to dump out all fifos
		}
		else {
	        m_recreq = 0 ;						//  stop pre-recording check until next key frames arrived
		}
		return m_recreq ;
	}
   
    m_fileendtime=fifo->time ;
    if( fifo->key ) {
		
        if( fifo->rectype != m_filerecstate ) {
            if( fifo->rectype == REC_PRERECORD ) {
                closefile();
                m_prevfilename = "" ;
                
                if( m_prerecord_time <= rec_prerecord_inmem ) {
					m_inmemprerecord = 1 ;				//  to allow in-memory pre-recording
					return 1;
				}
                
            }
            else if(fifo->rectype == REC_RECORD ) {
                if( m_filerecstate == REC_LOCK ) {
                    closefile();
                }
                else if( m_filerecstate == REC_PRERECORD ) {
                    m_filerecstate = REC_RECORD ;
                }
            }
            else if(fifo->rectype == REC_LOCK ) {
                if( m_filerecstate == REC_RECORD || m_filerecstate == REC_PRERECORD) {
                    m_filerecstate = REC_RECORD ;
                    prelock();
                }
                m_filerecstate = REC_LOCK ;
            }
        }
    }
   
    m_active=1 ;
    fifo = getfifo();
    
    if(fifo->rectype == REC_STOP ) {
        closefile();                       
    }
    else {
        recorddata( fifo ) ;
    }

    // release fifo
	releasefifo( fifo );

    return 1 ;
}


void *rec_thread(void *param)
{
    int ch;
    int i ;
    int wframes ;
    int norec=0 ;
    
    while (rec_run) {
        wframes = 0 ;
        for( ch = 0 ; ch <rec_channels && rec_run ; ch++ ) {
            for( i=0; i<100 ; i++ ) {
                if(recchannel[ch]->dorecord()) {
                    wframes++;
                }
                else { 
                    break;
                }
            }
        }
        if( wframes==0 && rec_run ) {
            rec_busy = 0 ;              // recording thread idle
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
            rec_busy=1 ;            
            norec=0 ;
           // file_sync();
        }
    }        
    for (ch = 0; ch < rec_channels; ch++) {
        recchannel[ch]->closefile();
    }
    file_sync();
    sync();
    return NULL;
}

// update record status, sensor status channged or motion status changed
void rec_channel::update()
{
    int i;
    struct dvrtime tnow ;
    int eventmark = 0 ;		// event marker
    int trigger = 0 ;		// trigger

    if( rec_run == 0 ) {
        return ;
    }

    if( m_recstate == REC_STOP )
        return;


#ifdef PWII_APP        
	int tick = time_gettick();
	if( tick < m_lock_endtime ) {
        m_recstate=REC_LOCK;
        return ;
	}
#endif

    time_now(&tnow);

	// check lock/record timeout
	if( m_recstate == REC_LOCK ) {
        if(time_dvrtime_diff(&tnow, &m_state_starttime) > m_postlock_time){
			m_recstate = REC_PRERECORD ;
        }
	}
	else if( m_recstate == REC_RECORD ) {
        if( time_dvrtime_diff(&tnow,&m_state_starttime) > m_postrecord_time ){
			m_recstate = REC_PRERECORD ;
        }   
	}

	// global trigger
	trigger = rec_trigger ;

    if( event_tm ) {
		eventmark = 1 ;
	}

	// G-Force Trigger
	float fb, lr, ud ;
	if( eventmark == 0 && m_gforce_trigger && dio_get_gforce( &fb, &lr, &ud ) ) {
		if( fb > m_gforce_trigger_value ||
			fb < -m_gforce_trigger_value ||
			lr > m_gforce_trigger_value ||
			lr < -m_gforce_trigger_value ||
			ud > m_gforce_trigger_value ||
			ud < -m_gforce_trigger_value 
		){
			eventmark = 1 ;
		}
	}
		
	// GPS speed trigger
	if( eventmark == 0 && m_speed_trigger && dio_get_gps_speed() > m_speed_trigger_value ) {
		eventmark = 1 ;
	}
	
	// sensor trigger
     if( m_recordmode==1 || m_recordmode==3 ) {	// sensor triggger mode				// sensor triggger mode

		for(i=0; eventmark == 0 && i<num_sensors; i++) {
			int sv = sensors[i]->value() ;
			int xsv = sensors[i]->xvalue() ;
			int tr=m_triggersensor[i] ;
			int trig = ( (tr&1) && sv ) ||
						 ( (tr&2) && (!sv) ) ||
						 ( (tr&4) && sv && (!xsv) ) ||
						 ( (tr&8) && (!sv) && xsv ) ;
			if( trig ) {
				trigger = 1 ;
				if( sensors[i]->iseventmarker() ) {
					eventmark = 1 ;
				}
			}

		}

	}
	
	// motion trigger
	if( eventmark == 0 && trigger==0 && ( m_recordmode==2 || m_recordmode==3 ) ) {				// motion trigger mode
		if( cap_channel[m_channel]->getmotion() ) {
			trigger=1 ;
		}
	}

	// set record state
	if( eventmark ) {
        m_state_starttime = tnow ;		
		m_recstate = REC_LOCK ;
	}
	else if( trigger ) {
        m_state_starttime = tnow ;		
        if( m_recstate == REC_PRERECORD ) {
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
    
    // maximum in memory pre-recording length
    rec_prerecord_inmem=dvrconfig.getvalueint("system", "prerecord_inmem");
    if( rec_prerecord_inmem<30 || rec_prerecord_inmem>300 ) {
		rec_prerecord_inmem=60 ;
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
    pthread_mutex_init(&prelock_mutex,NULL);
    
}

void rec_uninit()
{
    int i;
    i=rec_channels ;
   // rec_channels=0;
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
     rec_channels=0;
     pthread_mutex_destroy(&prelock_mutex);
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

int fileclosed()
{
    int i;
    int result=1;
    for(i=0;i<rec_channels; i++) {
        if(!recchannel[i]->fileclosed()){
           result=0;
           break;
        }
    }
    return result;
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

// return force recording channel state (requested recording ?)
//    0: not request recording
//	  1: request recording
int  rec_forcestate(int channel)
{
    if (channel>=0 && channel < rec_channels)
        return recchannel[channel]->checkforcerecording() ;
    return 0 ;
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
    for (i = 0; i < rec_channels; i++)
        recchannel[i]->breakfile();
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

#ifdef APP_PWZ5

void rec_pwii_force_rec( int c, int rec )
{
	int i ;

	for( i=0; i<rec_channels ; i++) {
        if( recchannel[i]->m_c_channel == c ) {
            recchannel[i]->setforcerecording(rec) ;       // force recording
		}
	}	

}

void rec_pwii_toggle_rec( int c )
{
	int i ;
	int rec = 0 ;
	for( i=0; i<rec_channels ; i++ ) {
        if( recchannel[i]->m_c_channel == c ) {
			if( recchannel[i]->checkforcerecording() ) {
				rec = 0 ;
			}
			else {
				rec = 1 ;
			}
			break ;
		}
	}
	rec_pwii_force_rec( c, rec ) ;
}
#else
void rec_pwii_toggle_rec( int ch )
{
	int i ;
    if( ch>=0 && ch<rec_channels ) {
        if( recchannel[ch]->recstate() == 0 ) {
            // turn on recording
            recchannel[ch]->setforcerecording(1) ;       // force start recording
        }
        else {
            // turn off recording
            
            extern int pwii_front_ch ;
            
            if( ch == pwii_front_ch ) {                     // first cam
                // turn off all recording
				for( i=0; i<rec_channels ; i++) {
					recchannel[i]->setforcerecording(0) ;  // force stop recording
				}
            }
            else {
                // turn off this camera
                recchannel[ch]->setforcerecording(0) ;       // force stop recording
            }
        }
    }
}
#endif

#endif

