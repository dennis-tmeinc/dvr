
#include "dvr.h"
#include "../ioprocess/diomap.h"

#include "fifoswap.h"
#include "record.h"

static pthread_t rec_threadid;
static int rec_run = 0 ;           	// 0: quit record thread, 1: recording
static int s_supportswap = 0 ;		// if support swap file ?		

int    rec_busy = 0 ;              // 1: recording thread is busy, 0: recording is idle
int    rec_pause = 0 ;             // 1: to pause recording, while network playback

//#define FRAME_DEBUG

static rec_channel **recchannel;
static int rec_channels;

int rec_maxfilesize;

rec_channel::rec_channel(int channel)
{
    int i;
    char buf[64];
    
    // init mutex
    m_lock = mutex_init ;
    
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
    
    // initialize pre-record variables
    m_prerecord_time = dvrconfig.getvalueint(cameraname, "prerecordtime");
    if( m_prerecord_time<2 )
        m_prerecord_time=2 ;
    if( m_prerecord_time>300 )
        m_prerecord_time=300 ;
    //	m_prerecord_file = NULL ;
    m_prevfilemode == REC_STOP  ;
    
    //	time_dvrtime_init( &m_prerecord_filestart, 2000 );
    
    // initialize post-record variables
    m_postrecord_time = dvrconfig.getvalueint(cameraname, "postrecordtime");
    if( m_postrecord_time<10 )
        m_postrecord_time=10 ;
    if( m_postrecord_time>(3600*12) )
        m_postrecord_time=(3600*12) ;
    
    // initialize lock record variables
    
    m_prelock_time=dvrconfig.getvalueint("system", "prelock" );
    m_postlock_time=dvrconfig.getvalueint("system", "postlock" );
    
    // initialize variables for file write
    m_reqbreak = 0 ;
    framedroped=0;
    
    m_inactivetime=0 ;

    // start recording	
    start();
}

rec_channel::~rec_channel()
{ 
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
void rec_channel::onframe(cap_frame * pframe)
{
    struct rec_fifo *nfifo;
    
    if( m_recstate == REC_STOP ||
       rec_pause > 0 ||
       dio_standby_mode )
    {
        if( m_fifohead == NULL ) {
			putfifo( newfifo( NULL ) );		// give a file close time
        }
    }
    else {
		// build new fifo frame
		nfifo = newfifo( pframe );
		nfifo->rectype = m_recstate ;
		putfifo( nfifo );
		if( pframe->frametype == FRAMETYPE_KEYVIDEO ) {
			m_fifokey = 1 ;
		}
		
		// memory too low ?
		if( g_memfree < 20000 && fifomemsize() > 1000000 ) {
			dvr_log("Memory low, clear fifos on channel %d (%d M)!", m_channel, clearfifo()/1000000 );
		}
		else if( g_memfree < 35000 ) {
			nfifo->swappos = -1 ;			// mark fifo for swapout
		}
	}
}

// allocate new fifo
struct rec_fifo * rec_channel::newfifo( cap_frame * pframe )
{
    rec_fifo * nf = new rec_fifo ;
    nf->next = NULL ;
    time_now(&(nf->time));
    nf->rectype = REC_STOP ;
    nf->frametype = FRAMETYPE_UNKNOWN ;
    nf->buf = NULL ;
    nf->bufsize = 0 ;
    nf->swappos = 0 ;
    if( pframe ) {
		nf->frametype = pframe->frametype ;
		nf->timestamp=pframe->timestamp;		
		if( pframe->framedata!=NULL && pframe->framesize > 0 ) {
			nf->buf = (char *) mem_addref(pframe->framedata, pframe->framesize );
			if( nf->buf ) {
				nf->bufsize = pframe->framesize ;
			}
		}
	}
	return nf ;
}
    
void rec_channel::releasefifo(struct rec_fifo *fifo)
{
	if( fifo ) {
		if( fifo->buf ) {
			mem_free( fifo->buf ) ;
		}
		else if( fifo->swappos>0 ) {
			swap_discard( fifo->swappos ) ;
		}
		delete fifo ;
	}
}

// add fifo into fifo queue
void rec_channel::putfifo(rec_fifo * fifo)
{
	if( fifo ) {
		lock_fifo();
		if (m_fifotail == NULL) {
			m_fifotail = m_fifohead = fifo;
		} else {
			m_fifotail->next = fifo;
			m_fifotail = fifo;
		}
		fifo->next = NULL ;
		unlock_fifo();
	}
}

struct rec_fifo *rec_channel::getfifo()
{
    struct rec_fifo *head;
    lock_fifo();
    head = m_fifohead;
    if( head != NULL ) {
        m_fifohead = head->next;
        if (m_fifohead == NULL){
            m_fifotail = NULL;
        }
    }
    unlock_fifo();
    return head;
}

// clear fifos, return buffer size released
int rec_channel::clearfifo()
{
    int r=0;
    struct rec_fifo * x ;
    lock_fifo();
    while ( m_fifohead ) {
        struct rec_fifo * x = m_fifohead ;
        r+=x->bufsize ;
        m_fifohead = x->next ;
        releasefifo( x );
    }
    m_fifotail=NULL ;
    unlock_fifo();
    return r ;
}

// set all fifos to new rec type
int rec_channel::setfifotype( REC_STATE t )
{
    struct rec_fifo * w ;
    lock_fifo();
    w = m_fifohead ;
    while (w) {
		if(w->rectype < t )
			w->rectype = t ;
		w=w->next ;
    }
    unlock_fifo();
    return t ;
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
	return ! m_file.isopen();
}

// close all openning files
void rec_channel::closefile()
{
	
    if (m_file.isopen()) {
		string filename ;
		string newfilename ;
		char *rn;
		int  filelength ;
    		
		rec_fifo rf ;			// end of file frame
		rf.frametype = FRAMETYPE_KEYVIDEO ;
		if( m_fifohead ) {
			rf.time = m_fifohead->time ;
		}
		else {
			time_now( &(rf.time) );
		}
		rf.bufsize=0;			// end of file key frame
		m_file.writeframe( &rf );
		
		filelength = time_dvrtime_diff( &rf.time, m_file.getfiletime());
		filename = m_file.getfilename();
        m_file.close();

        // need to rename to contain file lengh
        newfilename.setbufsize(512) ;
        strcpy( (char *)newfilename, (char *)filename );
        char * nbase = basefilename( (char *)newfilename );
        
        if ((rn = strstr(nbase+10, "_0_")) != NULL) {
            sprintf( rn, "_%d_%c_%s.266", 
                    filelength, 
                    (m_filerecstate == REC_LOCK) ? 'L' : 'N',
                    g_hostname );
            dvrfile::rename( filename, newfilename ) ;
            
			// remove previous pre-record video
			if( m_recstate == REC_PRERECORD && m_prevfilemode == REC_PRERECORD && m_prevfilename.length()>0 ) {
				dvrfile::remove( m_prevfilename.getstring() );
			}
               
			// save file name as previous name
			m_prevfilename = newfilename ;
			m_prevfilelen = filelength ;
			m_prevfilemode = m_filerecstate ;
			
			// request disk N/L rescan
			if( m_prevfilemode > REC_PRERECORD ) {
				disk_rescan();		
			}
        }
    }
    
}

// write frame data to disk
//  return  0: data not used (keep in fifo), 1: data used or should be discard
void rec_channel::recorddata(rec_fifo * data)
{
    int l;

    if( data == NULL || data->rectype == REC_STOP || data->bufsize<=0 ) {
        closefile();
        return ;
    }
    
    // check if file need to be broken
    if ( m_file.isopen() && data->frametype == FRAMETYPE_KEYVIDEO ) {	// must be a key frame
        if( m_reqbreak ||
			m_file.tell() > rec_maxfilesize ||
			m_file.getfiletime()->day != data->time.day ) 
		{
			closefile();
		}
		else if( m_filerecstate == REC_PRERECORD ) {
			dvrtime now ;
			time_now(&now) ;
			if( time_dvrtime_diff(&now, m_file.getfiletime()) > m_prerecord_time ) {
				closefile();
			}
		}
    }
		
    // create new dvr file
    if ( (!m_file.isopen()) && data->frametype == FRAMETYPE_KEYVIDEO ) {	// need to open a new file
		m_reqbreak=0;

		// retrieve base dir depends on file type ?
		char * newfilename = dvrfile::makedvrfilename( &(data->time), m_channel, data->rectype==REC_LOCK , 0 ) ;
		if( newfilename != NULL ) {
	
			if (m_file.open(newfilename, "w" )) {
				m_filerecstate = data->rectype ;
				//			dvr_log("Start recording file: %s", newfilename);
			} else {
				dvr_log("File open error:%s", newfilename);
				closefile();
			}
			
			delete newfilename ;
		}
    }
    
    // record frame
    if( m_file.isopen() ) {
		if( data->buf == NULL ) {
			fifoswapin(data);			// try swap in fifo
		}
		if( m_file.writeframe( data ) <= 0 ) {
            dvr_log("record:write file error.");
            closefile();
        }
    }
    return  ;
}

struct rec_prelock_thread_param {
	string lockfile ;
	struct dvrtime locktime ;
} ;

static void * rec_prelock_thread( void * param )
{
    struct rec_prelock_thread_param * pa = ( struct rec_prelock_thread_param * )param ;
    
    nice(15);

	dvrfile::breakLfile(pa->lockfile, &(pa->locktime));

	delete pa ;			// delete parameter
	return NULL ;
}

// recoding mode just turn from rec to lock
// duplication pre-lock data
void rec_channel::prelock( )
{
	pthread_t prelockthread ;
	struct rec_prelock_thread_param * pa ;
	
	if( disk_getbasedisk(1) == NULL ) {
		return ;
	}
    
    dvrtime locktime ;
    time_now(&locktime) ;
    locktime.milliseconds = 0 ;
    time_dvrtime_add( &locktime, - m_prelock_time );
    
    // prelock for previous file
    if( m_prevfilename.length() > 20 ) {
		// create a thread to break this partial locked file	
		pa = new struct rec_prelock_thread_param ;
		pa->lockfile = (char *)m_prevfilename ;
		pa->locktime = locktime ; 
		
		// create a thread to break this partial locked file
		if( pthread_create(&prelockthread, NULL, rec_prelock_thread, (void *)pa ) == 0 ) {
			pthread_detach( prelockthread );
		}
		else {
			delete pa ;
		}
		m_prevfilename = "";
	}
	
	closefile();
	// prelock just closed file
    if( m_prevfilename.length() > 20 ) {
		// create a thread to break this partial locked file	
		pa = new struct rec_prelock_thread_param ;
		pa->lockfile = (char *)m_prevfilename ;
		pa->locktime = locktime ; 
		
		// create a thread to break this partial locked file
		if( pthread_create(&prelockthread, NULL, rec_prelock_thread, (void *)pa ) == 0 ) {
			pthread_detach( prelockthread );
		}
		else {
			delete pa ;
		}
		m_prevfilename = "";
	}
}	

int rec_channel::dorecord()
{
	if( rec_run == 0 ||
		m_recstate == REC_STOP ||
		rec_pause>0 ||
		dio_standby_mode ) 
	{
		closefile();
		clearfifo();
		return 0;
	}

    if( m_fifohead == NULL )	{	// no fifos pending
        if( m_file.isopen() ) {
			if( m_inactivetime == 0 ) {
				m_inactivetime = time(NULL);
			}
			else {
                time_t t = time(NULL);
                if( (int)(t-m_inactivetime)>5 ) {
                    closefile();
                }
            }
        }
        return 0 ;
    }	
	m_inactivetime = 0 ;
	
	if( mem_available() < 23000 && fifomemsize() > 500000  ) {		// critical low memory !!!
		// drop all fifos
		dvr_log("Free memory low, %d bytes of channel %d fifos dropped!", clearfifo(), m_channel);
		closefile();
		return 0 ;
	}

    // get out of in-memory pre-recording
    if( m_fifohead !=NULL && m_recstate > m_fifohead->rectype ) {
		// change all pre-record frame to new rec type
		setfifotype( m_recstate );
	}
    
    // keep in mem pre-recording
    if( m_recstate == REC_PRERECORD &&
		m_fifohead !=NULL &&
		m_fifohead->rectype == REC_PRERECORD ) 
	{
		dvrtime now ;
		time_now(&now);
        while( m_fifohead!=NULL && time_dvrtime_diff( &now, &(m_fifohead->time) ) > m_prerecord_time ) {
			// close pre-recording file before release the frame
			if( m_file.isopen() ) {
				closefile();
				// remove this pre-record video
				if( m_prevfilemode == REC_PRERECORD && m_prevfilename.length()>0 ) {
					dvrfile::remove( (char *)m_prevfilename );
					m_prevfilename = (char *)NULL ;
				}
			}
			releasefifo( getfifo() ) ;
		}
		
		if( g_memfree > 35000 ) {
			return 0 ;
		}
		else if( s_supportswap ) {
			// swap out some fifo
			lock_fifo();
			rec_fifo *fifo = m_fifohead ;
			while( fifo ) {
				if( fifo->swappos == -1 ) {		// marked fifo
					unlock_fifo();
					fifoswapout(fifo) ;
					lock_fifo();
				}
				fifo = fifo->next ;
			}
			unlock_fifo();
			
			return 1 ;
		}
		
		// to break pre-recording files		
		if( m_file.isopen() && time_dvrtime_diff(&now, m_file.getfiletime()) > m_prerecord_time ) {
			m_reqbreak = 1 ;
		}
	}	
	
	// recording to files
	int wsize = 0 ;
	if( m_fifokey || fifomemsize() > 500000 ) {
		
		int k=0; 
		while( m_fifohead && wsize<1000000 && k<5 && rec_run ) {
			if( m_fifohead->frametype == FRAMETYPE_KEYVIDEO ) 
			{
				if(	 m_file.isopen() && m_fifohead->rectype != m_filerecstate ) 
				{
					if( m_fifohead->rectype == REC_PRERECORD ) {
						closefile();
						m_filerecstate = m_fifohead->rectype ;
						break ;
					}
					else if(m_fifohead->rectype == REC_RECORD ) {
						if( m_filerecstate == REC_LOCK ) {		// lock -> normal
							closefile();
						}
					}
					else if(m_fifohead->rectype == REC_LOCK ) {	// normal -> lock
						prelock();
					}
					m_filerecstate = m_fifohead->rectype ;
				}
				k++ ;
			}
		   
			rec_fifo * fifo = getfifo();
			// do recording
			recorddata( fifo ) ;
			wsize += fifo->bufsize ;
			// release fifo
			releasefifo( fifo );
		}
		
		if( m_fifohead == NULL ) {
			m_fifokey = 0 ;
		}
	
	}

	return wsize ;
}

void *rec_thread(void *param)
{
    int ch;
    int norec=0 ;
    int r ;
    
    nice(5);
    
    while (rec_run) {
        int w = 0 ;
        for( ch = 0 ; ch <rec_channels && rec_run ; ch++ ) {
			w += recchannel[ch]->dorecord() ;
        }
        if( w==0 && rec_run ) {
            rec_busy = 0 ;              // recording thread idle
            if( ++norec > 50 ) {
                for (ch = 0; ch < rec_channels; ch++) {
                    recchannel[ch]->closefile();
                }
                norec=0 ;
            }
            else if( rec_pause > 0 ) {
                rec_pause-- ;
            }
            for( w=0; w<5 && rec_run; w++ )
				usleep(100000);
        }
        else {
			if( g_memdirty > 100 ) {
				rec_busy=1 ;            
			}
            norec=0 ;
        }
        if( rec_run ) usleep(1000) ;
    }
    for (ch = 0; ch < rec_channels; ch++) {
        recchannel[ch]->closefile();
    }

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

	if( dio_runmode() == 0 ) {
		m_recstate == REC_STOP ;
	}
	else {
		if( m_recstate == REC_STOP )
			start();
	}

    if( m_recstate == REC_STOP )
        return;

#ifdef PWII_APP        
	// check lock/record timeout
	if(  m_recstate == REC_LOCK && time_gettick() <= m_lock_endtime ) {
		eventmark = 1 ;
	} 
#endif

	// global trigger
    if( event_tm ) {
		eventmark = 1 ;
	}

	// not a event marker now, 2015-10-01
			// force continue recording
#ifdef APP_PWZ5
	if( disk_pw_recordmethod >= 1 ) {
		trigger = 1 ;
	}
#endif

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

	// continue recording mode = always trigger
	if( m_recordmode == 0 ) { 
		trigger=1 ;
	}
	
	// motion trigger
	if( eventmark == 0 && trigger==0 && ( m_recordmode==2 || m_recordmode==3 ) ) {				// motion trigger mode
		if( cap_channel[m_channel]->getmotion() ) {
			trigger=1 ;
		}
	}

	// get current time 
    time_now(&tnow);

	// set record state
	if( eventmark ) {
        m_lock_starttime = tnow ;		
        m_rec_starttime = tnow ;
		m_recstate = REC_LOCK ;
	}
	else if( trigger ) {
        m_rec_starttime = tnow ;
        if( m_recstate == REC_PRERECORD ) {
			m_recstate = REC_RECORD ;
		}
	}
	
    if( m_recstate == REC_LOCK  && time_dvrtime_diff(&tnow, &m_lock_starttime) > m_postlock_time){
		m_recstate = REC_RECORD ;
	}

	if( m_recstate == REC_RECORD && time_dvrtime_diff(&tnow,&m_rec_starttime) > m_postrecord_time ){
		m_recstate = REC_PRERECORD ;
	}
	
}

// updating recording alarms
#ifdef  APP_PWZ5
// only locked recording considered
void rec_channel::alarm()
{
	if( m_file.isopen() &&
		m_filerecstate == REC_LOCK &&
		m_recordalarmpattern > 0 && 
		m_recordalarm>=0 && m_recordalarm<num_alarms ) {
		alarms[m_recordalarm]->setvalue(m_recordalarmpattern);
	}
}
#else
void rec_channel::alarm()
{
    if( recstate() &&
        m_recordalarmpattern > 0 && 
        m_recordalarm>=0 && m_recordalarm<num_alarms ) {
            alarms[m_recordalarm]->setvalue(m_recordalarmpattern);
        }
}
#endif

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
    if (rec_maxfilesize < 100000000)
        rec_maxfilesize = 100000000;
    if (rec_maxfilesize > 1500000000)
        rec_maxfilesize = 1500000000;
    
    rec_threadid=0 ;

	s_supportswap = dvrconfig.getvalueint("system", "prerecordswap");

	rec_channels = cap_channels ;
	if( rec_channels > 0 ) {
		recchannel =  new rec_channel * [ rec_channels ];
		for (i = 0; i < rec_channels; i++) {
			recchannel[i]=new rec_channel( i ) ;
		}
		rec_run = 1;
		pthread_create(&rec_threadid, NULL, rec_thread, NULL);
	}
	
	dvr_log("Record initialized.");
    
    mem_available() ;
    swap_init();
}

void rec_uninit()
{
	int i;
	rec_run = 0;		// to stop recording
    if( recchannel ) {
		
        // stop rec_thread
		for(i=0; i<rec_channels; i++) {
			recchannel[i]->stop();
		}
        
        if( rec_threadid ) {
            pthread_join(rec_threadid, NULL);
            rec_threadid = 0 ;
        }
        
        for( i=0; i<rec_channels; i++ ){
            delete recchannel[i] ;
        }
        delete recchannel ;
        
        recchannel=NULL ;
        dvr_log("Record uninitialized.");
	}
	rec_channels=0;
     
	swap_finish();
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
    return 0 ;     
}

int  rec_staterec(int channel)
{
    if (channel>=0 && channel < rec_channels)
        return recchannel[channel]->recstate_rec();
    return 0 ;     
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

int  rec_state2(int channel)
{
	if (channel>=0 && channel < rec_channels)
        return recchannel[channel]->recstate2();
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
			if( recchannel[i]->recstate_rec() ) {
				rec = 0 ;
				if( i==0 ) {
					// to turn off mic emg input
					dio_pwii_emg_off();
					// also turn off tracemark
					event_tm = 0 ;			
				}
			}
			else {
				rec = 1 ;
			}
			break ;
		}
	}
	rec_pwii_force_rec( c, rec ) ;
}

int  rec_forcechannel(int channel)
{
	if (channel>=0 && channel < rec_channels)
        return recchannel[channel]->m_c_channel;
    return 3 ;
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

