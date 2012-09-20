

#ifndef __IPCAPTURE_H__
#define __IPCAPTURE_H__

#include "capture.h"

class localcapture : public capture {
protected:
    int m_localchannel ; 	//  remote camera channel

public:
    localcapture(int channel);
    virtual ~localcapture();

#ifdef EAGLE368
    int eagle368_startcapture();
    int eagle368_stopcapture();
#endif

    virtual int geteaglechannel() {
        return m_localchannel;
    }
// virtual functions
    virtual void update(int updosd);	// periodic update procedure, updosd: require to update OSD
    virtual void start();				// start capture
    virtual void stop();				// stop capture
    virtual void setosd( struct hik_osd_type * posd );
} ;


#endif  // __IPCAPTURE_H__
