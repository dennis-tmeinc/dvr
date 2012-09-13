

#ifndef __IPCAPTURE_H__
#define __IPCAPTURE_H__

#include "capture.h"

class netcapture : public capture {
protected:
    int m_sockfd ;		//  control socket for ip camera
    string m_ip ;		//  remote camera ip address
    int m_port ;		//  remote camera port
    int m_ipchannel ; 	//  remote camera channel
    pthread_t m_streamthreadid ;	// streaming thread id
    int m_state;		//  thread running state, 0: stop, 1: running, 2: started but not connected.
    int m_timesynctimer ;
    int m_updtimer ;
    int m_local ;       // connect to local process
    int m_shm_enabled ; // enable shared memory
    void * m_shm ;      // shared memory heap
public:
    netcapture(int channel);
    virtual ~netcapture();

    void streamthread_net();
    void streamthread_shm();
    void streamthread();
    int  connect();

#ifdef EAGLE368
    int eagle368_startcapture();
    int eagle368_stopcapture();
#endif

    virtual int geteaglechannel() {
        return m_ipchannel;
    }
// virtual functions
    virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD
    virtual void start();				// start capture
    virtual void stop();				// stop capture
    virtual void setosd( struct hik_osd_type * posd );
} ;


#endif  // __IPCAPTURE_H__
