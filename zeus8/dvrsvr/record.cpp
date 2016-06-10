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

#define MAX_BUFFER_TIME  20
string rec_basedir;

static pthread_t rec_threadid;
static int rec_run = 0 ;           // 0: quit record thread, 1: recording

int    rec_busy = 0 ;              // 1: recording thread is busy, 0: recording is idle
int    rec_pause = 0 ;             // 1: to pause recording, while network playback
static int rec_fifo_size = 6000000 ;
pthread_mutex_t prelock_mutex;

//#define FRAME_DEBUG

enum REC_STATE {
    REC_STOP,           // no recording
    REC_RECORD,         // recording
    REC_PRERECORD,      // pre-recording
    REC_POSTRECORD,     // post-recording
    REC_LOCK,           // recording lock file
    REC_POSTLOCK        // post locking
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
        
	int     enable;
        int     m_recordmode ;			// 0: continue mode, 123: triggering mode, other: no recording
        unsigned int m_triggersensor ;  // trigger sensor bit maps

        int     m_recordalarm ;
        int     m_recordalarmpattern ;

        REC_STATE	m_recstate;
        REC_STATE   m_filerecstate ;
        
        struct rec_fifo *m_fifohead;
        struct rec_fifo *m_fifotail;
        int	   m_fifosize ;
	int        m_fifoMax;	
        
        // pre-record variables
        int m_prerecord_time ;				// allowed pre-recording time
        //	list <int> m_prerecord_list ;		// pre-record pos list
        //	FILE * m_prerecord_file ;			// pre-record file
        //	string m_prerecord_filename ;		// pre-record filename
        //	dvrtime m_prerecord_filestart ;		// pre-record file starttime
        
        // post-record variables
        int m_postrecord_time ;				// post-recording time	length
        dvrtime m_postrecord_starttime ;	// post-record start time
        
        // lock record variables
        int m_prelock_time ;				// pre lock time length
        int m_postlock_time ;				// post lock time length
        dvrtime m_postlock_starttime ;		// post lock start time
        int m_eventmarker;					// event marker, sensor_no + 1
        
        // variable for file write
        string m_prevfilename ;				// previous recorded file name.
        string m_filename;
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
        array<int>  mSensorState;
	dvrtime mLasttriggerTime;
	dvrtime mLastEvTime;
        struct dvrtime m_state_starttime ;	        // record state start time      
        int m_framerate;
        int bitrate;
	int m_width;
	int m_height;
	int framedroped;
        struct File_format_info mFileHeader;
	pthread_mutex_t m_lock;
    public:
        rec_channel(int channel);
        ~rec_channel();
        
	int enabled(){return enable;};
        void start();				// set start record stat
        void stop();				// stop recording, frame data discarded
        
        struct rec_fifo *getfifo();
        void putfifo(rec_fifo * fifo);
        int  clearfifo();
        void addfifo(cap_frame * pframe);
        
        void onframe(cap_frame * pframe);
        int dorecord();
        
        void recorddata(rec_fifo * data);		// record stream data
        //	void closeprerecord( int lock );				// close precord data, save them to normal video file
        //	void prerecorddata(rec_fifo * data);			// record pre record data
        void prelock();									// process prelock data
        
        void closefile();
        int  fileclosed();

        void breakfile(){			// start a new recording file
            if(m_file.isopen())
             m_reqbreak = 1;
        }
        int  recstate() {
            return( m_file.isopen() &&
		    (m_filerecstate == REC_RECORD ||
		     m_filerecstate == REC_LOCK ||
		     m_filerecstate == REC_POSTRECORD ||
		     m_filerecstate == REC_POSTLOCK) );
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
    int mCh;
    m_channel = channel;
    if(m_channel>=0&&m_channel<8){
       sprintf(cameraname, "camera%d", m_channel+1 );  
    } else if(m_channel>=8){
       mCh=m_channel-8; 
       sprintf(cameraname, "camera%d", mCh+1 );  
    } 
    enable=dvrconfig.getvalueint( cameraname, "enable");
    
    if(m_channel>=0&&m_channel<8){
       m_recordmode = dvrconfig.getvalueint( cameraname, "recordmode");
    } else {
       m_recordmode = dvrconfig.getvalueint( cameraname, "recordmode_second");
    }
    m_triggersensor = 0 ;
    for(i=0;i<num_sensors;i++) {
        sprintf(buf, "trigger%d", i+1);
        trigger = dvrconfig.getvalueint( cameraname, buf );
        if( trigger>0 && trigger<32 ) {
	  m_triggersensor |= 1<<(trigger - 1) ;
        }
    }

    m_recordalarm = dvrconfig.getvalueint( cameraname, "recordalarm" );
    m_recordalarmpattern = dvrconfig.getvalueint( cameraname, "recordalarmpattern" );
    
    m_recstate=REC_STOP ;
    m_filerecstate=REC_STOP ;
    
    m_fifohead = m_fifotail = NULL;
    m_fifosize = 0 ;
    
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
    
    if(m_channel>=0&&m_channel<8){
        m_framerate=dvrconfig.getvalueint(cameraname, "framerate");
    } else {
       m_framerate=dvrconfig.getvalueint(cameraname, "framerate_second");
    }
    if(m_channel>=0&&m_channel<8){
       bitrate=dvrconfig.getvalueint( cameraname, "bitrate");
    } else {
       bitrate=dvrconfig.getvalueint( cameraname, "bitrate_second");
    }

    m_fifoMax=(bitrate/8)*MAX_BUFFER_TIME;
    if(m_fifoMax>rec_fifo_size){
	m_fifoMax=rec_fifo_size; 
    }   
    m_width=-1;
    m_height=-1;
    
    memcpy((void*)&mFileHeader,Dvr264Header,40);
    
    m_width=GetVideoWidth(m_channel);
    m_height=GetVideoHight(m_channel);   
    mFileHeader.video_height=m_height;
    mFileHeader.video_width=m_width;
    
    time_dvrtime_init( &m_postrecord_starttime, 2000 );
    
    // initialize lock record variables
#ifdef SUPPORT_EVENTMARKER
    m_prelock_time=dvrconfig.getvalueint("eventmarker", "prelock" );
    m_postlock_time=dvrconfig.getvalueint("eventmarker", "postlock" );
    m_eventmarker=dvrconfig.getvalueint("eventmarker", "eventmarker");
    if( m_eventmarker>num_sensors ) {
        m_eventmarker=0 ;
    }
    time_dvrtime_init( &m_postlock_starttime, 2000 );
#endif
    
    // initialize variables for file write
    m_maxfilesize = rec_maxfilesize;
    m_maxfiletime = rec_maxfiletime;
    time_dvrtime_init( &m_filebegintime, 2000 );
    time_dvrtime_init( &m_fileendtime, 2000 );
    m_fileday=0;
    m_lastkeytimestamp = 0 ;
    m_reqbreak = 0 ;
    framedroped=0;
    mSensorState.empty();
    time_dvrtime_init( &mLasttriggerTime, 2000 );
    time_dvrtime_init( &mLastEvTime, 2000 );       
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

#define MINIMUM_FREE_DDR  45000       //kbytes

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
        if( m_file.isopen() ) {
            nfifo = (struct rec_fifo *) mem_alloc(sizeof(struct rec_fifo));
            nfifo->rectype = REC_STOP ;
            nfifo->buf=NULL ;
            nfifo->bufsize=0 ;
            nfifo->key=1 ;
            time_now(&nfifo->time);
            putfifo( nfifo );
        }
        return ;  // if stop recording.
    }
    
    if(framedroped){
       if(pframe->frametype == FRAMETYPE_VIDEO){
#ifdef 	FRAME_DEBUG 
           printf("ch%d one frame droped\n",m_channel);
#endif	   
           return;
       }     
    }
    if( m_fifosize> m_fifoMax ){

       if(pframe->frametype == FRAMETYPE_VIDEO){
	   framedroped=1;
#ifdef 	FRAME_DEBUG 	   
           printf("ch%d one frame droped\n",m_channel);
#endif	   
           return;
       }       
    } else {       
       if(pframe->frametype == FRAMETYPE_KEYVIDEO){
	    if(mem_available()<MINIMUM_FREE_DDR){
		framedroped=1; 
#ifdef 	FRAME_DEBUG 	   
                dvr_log("ch%d I frame droped\n",m_channel);
#endif		
		return;
	    }	 
       }      
    }
    
    // build new fifo frame
    nfifo = (struct rec_fifo *) mem_alloc(sizeof(struct rec_fifo));
    if(nfifo==NULL){
       //  printf("No buffer in addfifo\n");
         if(pframe->frametype == FRAMETYPE_KEYVIDEO){
            int i=0;
            for(i=0;i<5;++i){
		 nfifo = (struct rec_fifo *) mem_alloc(sizeof(struct rec_fifo));
                 if(nfifo)
                  break;
            }
         }
         if(nfifo==NULL){
	      if(pframe->frametype == FRAMETYPE_KEYVIDEO){
		framedroped=1;
	      }
              return;
	 }
    }
    if (mem_check(pframe->framedata)) {
        nfifo->buf = (char *) mem_addref(pframe->framedata);
    } else {
        nfifo->buf = (char *) mem_alloc(nfifo->bufsize);
        if(nfifo->buf==NULL){
     //       printf("no buffer in addfifo nfifo->buf\n");
            mem_free(nfifo->buf);
            return;
        }
        mem_cpy(nfifo->buf, pframe->framedata, nfifo->bufsize);
      //  mem_cpy32(nfifo->buf, pframe->framedata, nfifo->bufsize);
    }
    nfifo->bufsize = pframe->framesize;
    
    if( pframe->frametype == FRAMETYPE_KEYVIDEO )   {  // this is a keyframe ?
        framedroped=0;
        nfifo->key=1 ;
        time_now(&nfifo->time);    // only keyframe have fifo time
    }
    else {
        nfifo->key=0 ;
        time_now(&nfifo->time); 
    }
    
    // set frame recording type
    if( m_recstate == REC_POSTRECORD ) {
        nfifo->rectype = REC_RECORD ;
    }
    else if( m_recstate == REC_POSTLOCK ) {
        nfifo->rectype = REC_LOCK ;
    }
    else {
        nfifo->rectype = m_recstate ;
    }
    nfifo->timestamp=pframe->timestamp;
    putfifo( nfifo );

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
        }
        m_fifosize-=head->bufsize ;
    }
    pthread_mutex_unlock(&m_lock);
    return head;
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
        if( head->buf ) {
            mem_free(head->buf);
        }
        mem_free(head);
        c++ ;
    }
    m_fifotail=NULL ;
    m_fifosize=0 ;
    pthread_mutex_unlock(&m_lock);
    return c ;
}

void rec_channel::onframe(cap_frame * pframe)
{
    if (pframe->channel == m_channel ) {
       // printf("channel_onframe:%d\n",pframe->channel);
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
        if( filelength<5 || filelength>5000 ) {
            m_file.remove(m_filename.getstring());
            m_filename="";
            return ;
        }
        // need to rename to contain file lengh
        strcpy(newfilename, m_filename.getstring());
        if ((rn = strstr(newfilename, "_0_")) != NULL) {
	    if(m_channel<8){
		sprintf( rn, "_%d_%c_%s.266", 
			filelength, 
			m_filerecstate == REC_LOCK ? 'L' : 'N',
			g_hostname );
	    } else {
		sprintf( rn, "_%d_N_%s.266", 
			filelength, 
			g_hostname );	      
	    }
            m_file.rename( m_filename.getstring(), newfilename ) ;
            
            if( m_filerecstate == REC_PRERECORD ) {			
		// remove previous pre-recording clip
                if( m_prevfilename.length()>0 ) {
                    m_file.remove( m_prevfilename.getstring() );
                    m_prevfilename = "" ;
                }
            }
            
            // save file name as previous name
            m_prevfilename = newfilename ;
            disk_renew( newfilename );
        }
        m_filename="";
    }
}

// write frame data to disk
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
    
   // printf("channel:%d\n",m_channel);
    // create new dvr file
    if ( (!m_file.isopen()) && data->key ) {	// need to open a new file
        // create new record file
        if (rec_basedir.length() <= 0) {	// no base dir, no recording disk available
            // frame data is discarded
            return;
        }
      //  if(m_width<320){
	  m_width=GetVideoWidth(m_channel);
	  m_height=GetVideoHight(m_channel);   
	//}
	//dvr_log("channel:%d width:%d height:%d, framerate:%d",m_channel,m_width,m_height,m_framerate);
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
           // dvr_log("Start recording file: %s", newfilename);
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
    
    fifo = getfifo();
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
        return 0;
    }
    m_active=1 ;
    
    m_fileendtime=fifo->time ;
    if( fifo->key ) {
        //m_fileendtime=fifo->time ;
        if(m_channel<8){
	    if( fifo->rectype != m_filerecstate ) {
		if( fifo->rectype == REC_PRERECORD ) {
		    closefile();
		    m_prevfilename = "" ;
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
	} else {
	    if( fifo->rectype != m_filerecstate ) {
		if( fifo->rectype == REC_PRERECORD ) {
		    closefile();
		    m_prevfilename = "" ;
		}
		else if(fifo->rectype == REC_RECORD ) {
		     m_filerecstate = REC_RECORD ;
		}
		else if(fifo->rectype == REC_LOCK ) {
		     m_filerecstate = REC_RECORD ;
		}
	    }	  	  
	}
    }
    
    if(fifo->rectype == REC_STOP ) {
        closefile();                       
    }
    else {
        recorddata( fifo );
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
    
    while (rec_run) {
        wframes = 0 ;
        for( ch = 0 ; ch <rec_channels ; ch++ ) {
	    if(recchannel[ch]->enabled()){
		for( i=0; i<10 ; i++ ) {
		    if(recchannel[ch]->dorecord()) {
			wframes++;
		    }
		    else { 
			break;
		    }
		}
	    }
        }
        if( wframes==0 ) {
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
#if 0
void rec_channel::update()
{
    int i;
    int record = 0 ;
    struct dvrtime tnow ;

    if( rec_run == 0 ) {
        return ;
    }

    time_now(&tnow);

    if( m_recstate == REC_STOP )
        return;
    // check if post recording finished?
    if( ( m_recstate == REC_POSTRECORD  &&
         (tnow - m_postrecord_starttime) > m_postrecord_time )		||
       ( m_recstate == REC_POSTLOCK  &&
        (tnow - m_postlock_starttime) > m_postlock_time )) 
    {
        start();
    }
    // check for status change
#ifdef SUPPORT_EVENTMARKER
    record = 0 ;
    if( m_eventmarker > 0 ) {
        if( sensors[m_eventmarker-1]->value() ) {
            record = 1 ;
        }
    }
    
    if( record ) {
        m_recstate = REC_LOCK ;
    }
    else if( m_recstate == REC_LOCK ) {
        m_postlock_starttime = tnow ;
        m_recstate = REC_POSTLOCK ;
    }

    if( m_recstate == REC_LOCK || m_recstate == REC_POSTLOCK ) {
        return ;								
      // lock event has higher priority 
    }

#endif
    
    if( m_recordmode == 0 ) {			// continue recording mode
        record = 1 ;
    }
    
    // check sensors
    if( record==0 && (m_recordmode==1 || m_recordmode==3) ) {				// sensor triggger mode
        for(i=0; i<num_sensors; i++) {
            if( ( m_triggersensor & (1<<i) ) && sensors[i]->value() ) {
                    record=1 ;
            }
        }
    }
    
    // check motion
    if( record==0 && ( m_recordmode==2 || m_recordmode==3 ) ) {				// motion trigger mode
        if( cap_channel[m_channel]->getmotion() ) {
            record=1 ;
        }
    }    
    
    if( record==1 ) {
        m_recstate = REC_RECORD ;
    }
    else if( m_recstate == REC_RECORD ) {
        m_postrecord_starttime = tnow ;
        m_recstate = REC_POSTRECORD ;
    }    
    
}
#endif
#if 1
void rec_channel::update()
{
    int i;
    struct dvrtime tnow ;
    int trigger = 0 ;
    int msensor=0;
    int mCh;
    if( rec_run == 0 ) {
        return ;
    }

    if(m_channel>=0 &&m_channel<8){
       mCh=m_channel; 
    } else {
       mCh=m_channel-8; 
    }
    time_now(&tnow);

    if( m_recstate == REC_STOP )
        return;
    
    // check for sensor status change
    
    if( m_eventmarker > 0 ) {
        if( sensors[m_eventmarker-1]->value() ) {
            trigger = 1 ;
        }
    }
    
    if(trigger>0){
     //   if(time_dvrtime_diff(&tnow,&mLastEvTime)>SENSOR_TRIGGER_MIN){
	if(m_recstate!=REC_LOCK){
	    mSensorState.empty(); 
	}
	mLastEvTime=tnow; 
	msensor=1;
	mSensorState.add(msensor);
//	}
        
    } else {
	if( m_recordmode==1 || m_recordmode==3 ) {	// sensor triggger mode				// sensor triggger mode
	    for(i=0; i<num_sensors; i++) {
		if( ( m_triggersensor & (1<<i) ) && sensors[i]->value() ) {
			 trigger=1 ;
		}
	    }	  
	}     
        if( trigger==0 && ( m_recordmode==2 || m_recordmode==3 ) ) {				// motion trigger mode
	    if( cap_channel[mCh]->getmotion() ) {
		trigger=1 ;
	    }
        }  	
	
        if(trigger==1){
	      if(mSensorState.size()<3){
		mLasttriggerTime=tnow;
		msensor=2;
		mSensorState.add(msensor);
	      }	       	  
	}   
    }
    //end of sensor triger check;
    //check whether the ev happened
    trigger=0;
    for(i=0;i<mSensorState.size();++i)
    {
       if(mSensorState[i]==1){
	  trigger=1; 
       }
    }
    if(trigger>0){
        mSensorState.empty();
	m_recstate=REC_LOCK;
	m_postlock_starttime = tnow ;
	//dvr_log("event mark triggered");
    }

    
    if( m_recstate == REC_LOCK){
   //     dvr_log("lock recording");
        if(time_dvrtime_diff(&tnow, &m_postlock_starttime) < m_postlock_time){
            return ;	
        } else {
            start();
           // dvr_log("channel:%d lock end",m_channel);
        }							
    }
    
    if( m_recordmode == 0 ) {			// continue recording mode
         m_recstate = REC_RECORD ;
	 mSensorState.empty();
         return;
    }
    
    if(mSensorState.size()>0){
          m_recstate = REC_RECORD ;
	  m_state_starttime = tnow ;
	  mSensorState.empty();
	//  mSensorState.remove(0);
	//  dvr_log("sensor triggered");
	/*
	if(m_recstate == REC_RECORD)
	{
	  dvr_log("channel:%d state:record",m_channel);
	}*/	
    }

    if(m_recstate == REC_RECORD){
         if( time_dvrtime_diff(&tnow,&m_state_starttime)> m_postrecord_time ){
	    start();
	 }       
    }
}
#endif

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
/*    
    i=dvrconfig.getvalueint("system", "record_fifo_size");
    if( i>1000000 && i<16000000 ) {
        rec_fifo_size=i ;
    }
    */
    rec_threadid=0 ;
    if( dvrconfig.getvalueint("system", "norecord")>0 ) {
        rec_channels = 0 ;
        recchannel = NULL ;
        dvr_log( "Dvr no recording mode."); 
    }
    else {
        rec_channels = dvrconfig.getvalueint("system", "totalcamera");
   	//rec_channels=MAXCHANNEL;
	
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
	    recchannel[i]->clearfifo();
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
       // printf("channel:%d\n",pframe->channel);      
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
