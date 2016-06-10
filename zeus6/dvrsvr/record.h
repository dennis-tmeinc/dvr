
#include "dvr.h"
#include "../ioprocess/diomap.h"

#ifndef __RECORD_H__
#define __RECORD_H__

#include "fifoswap.h"

#ifdef APP_PWZ5
extern int disk_pw_recordmethod ;		// 0: one disk, 1: jic, 2: autobackup
#endif

//#define FRAME_DEBUG

class rec_channel {
    protected:
        int     m_channel;
        int     m_recordmode ;				// 0: continue mode, 123: triggering mode, other: no recording

#if defined(TVS_APP) || defined(PWII_APP)
        unsigned int m_triggersensor[32] ;  // trigger sensors
#endif

#ifdef MDVR_APP
        unsigned int m_triggersensor ;  		// trigger sensor bit maps
#endif

        int     m_recordalarm ;
        int     m_recordalarmpattern ;

        REC_STATE	m_recstate;
        
        struct rec_fifo *m_fifohead;
        struct rec_fifo *m_fifotail;
        int    m_fifokey ;						// keyframe available in fifo
        
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
      
        REC_STATE   m_filerecstate ;
        dvrfile 	m_file;					// recording file
        
        // variable for file write
        string m_prevfilename ;				// previous recorded file name.
        int    m_prevfilemode ;				// REC_PRERECORD,  REC_RECORD,  REC_LOCK
        int    m_prevfilelen ;				// previous file length in seconds
                
        int m_reqbreak;						// reqested for file break
        
        time_t m_inactivetime ;
        
        struct dvrtime m_lock_starttime ;	        // L rec start time      
        struct dvrtime m_rec_starttime ;	        // N rec start time      

        //	rec_index m_index ;			// on/off index
        //	int m_onoff ;				// on/off state
        dvrfile m_prelock_file;			// pre-lock recording file
        
		int framedroped;

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

		struct rec_fifo * newfifo( cap_frame * pframe );
        struct rec_fifo * getfifo();
        void putfifo(rec_fifo * fifo);
        int  clearfifo();
        int setfifotype( REC_STATE t );
		void lock_fifo() {
			pthread_mutex_lock(&m_lock);			
		}
		void unlock_fifo() {
			pthread_mutex_unlock(&m_lock);			
		}

		// return total fifo size
		int fifosize() 
		{
			int fs = 0 ;
			struct rec_fifo * nfifo ;
			lock_fifo();
			nfifo = m_fifohead ;
			while ( nfifo ) {
				fs += nfifo->bufsize ;
				nfifo=nfifo->next ;
			}
			unlock_fifo();
			return fs ;
		}
		
		// return in memory fifo size (exclude swapped out fifo)
		int fifomemsize() {
			int fs = 0 ;
			struct rec_fifo * nfifo ;
			lock_fifo();
			nfifo = m_fifohead ;
			while ( nfifo ) {
				if( nfifo->buf ) {
					fs += nfifo->bufsize ;
				}
				nfifo=nfifo->next ;
			}
			unlock_fifo();
			return fs ; 			
		}
		
		void releasefifo(struct rec_fifo *fifo) ;
 		
		// return size swapped out
		int fifoswapout(struct rec_fifo *fifo) {
			if( fifo && fifo->buf ) {
				int pos = swap_out( fifo->buf, fifo->bufsize ) ;
				if( pos>0 ) {
					fifo->swappos = pos ;
					mem_free( fifo->buf );
					fifo->buf = NULL ;
					return fifo->bufsize ;
				}
			}
			return 0;
		}

		int fifoswapin(struct rec_fifo *fifo) {
			if( fifo != NULL && fifo->buf == NULL && fifo->bufsize>0 && fifo->swappos>0 ) {
				char * buf = (char *) mem_alloc(fifo->bufsize) ;
				if( buf ) {
					if( swap_in( buf, fifo->bufsize, fifo->swappos ) ) {
						fifo->buf = buf ;
						fifo->swappos = 0 ;
						return 1 ;
					}
					else {
						mem_free( buf );
					}
				}
			}
			return 0;
		}

        void onframe(cap_frame * pframe);
        int dorecord();
        
        void recorddata(rec_fifo * data);				// record stream data
        //	void closeprerecord( int lock );			// close precord data, save them to normal video file
        //	void prerecorddata(rec_fifo * data);		// record pre record data
        void prelock();									// process prelock data
        
        void closefile();
        int  fileclosed();

        void breakfile(){								// start a new recording file
            m_reqbreak = 1;
        }
        int  lockstate() {
            return( m_file.isopen() &&
		    m_filerecstate == REC_LOCK  );
		}
		int  recstate_x() {
            return( m_file.isopen() &&
		    (m_filerecstate == REC_RECORD ||
		     m_filerecstate == REC_LOCK) );
        }
		int  recstate() {
#ifdef APP_PWZ5
			return( m_file.isopen() && m_recstate == REC_LOCK );
#else
            return( m_file.isopen() &&
		    (m_filerecstate == REC_RECORD ||
		     m_filerecstate == REC_LOCK) );
#endif		
        }

		int  recstate_rec() {
#ifdef APP_PWZ5
			return( m_recstate == REC_LOCK );
#else
            return( 
		    (m_filerecstate == REC_RECORD ||
		     m_filerecstate == REC_LOCK) );
#endif		
        }
        
        // get record detail state
        //  bits definition
		//         2: recording
		//         3: force-recording
		//         4: lock recording
		//         5: pre-recording
		//         6: in-memory pre-recording
        int recstate2() {
			int st = 0 ;
			if(  m_file.isopen() ) {
				if( m_filerecstate == REC_RECORD )
					st |= 4 ;
				if( m_filerecstate == REC_LOCK ) 
					st |= (4|16) ;
			}
			if( m_recstate == REC_PRERECORD ) {
				st |= 1<<5 ;
				if( ! m_file.isopen() ) {				// no file opened , consider it is in memory
					st |= 1<<6 ;
				}
			}
			return st ;
		}
        
        void update();          						// update recording status
        void alarm();						            // set recording alarm.

};

#endif		// __RECORD_H__
