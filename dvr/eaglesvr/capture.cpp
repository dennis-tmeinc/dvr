
#include "eaglesvr.h"

void cap_stop()
{
    int i;
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->stop();
    }
}

// initial capture card, return channel numbers
void cap_init()
{
    eagle_init();
}

// uninitial capture card
void cap_uninit()
{
    eagle_uninit ();
}

void capture::onframe(cap_frame * pcapframe)
{
    if( net_onframe(pcapframe)<=0 ) {
        dvr_log( "No network connect, to stop channel %d ", m_channel );
        stop();
    }
}

// shared library interface

typedef void (* framecallback)(struct cap_frame *frame ) ;

static framecallback framecb ;

void defframecb(struct cap_frame * )
{
    // do nothing ;
}

int eagle_cap_init(framecallback fcb)
{
    eagle_init();
    framecb = fcb;
    return cap_channels ;
}

void eagle_cap_uninit()
{
    framecb = defframecb;
    eagle_uninit();
}

void eagle_cap_finish()
{
    eagle_finish();
}

void eagle_cap_start()
{
    int i;
    for( i=0; i<cap_channels; i++) {
        if( cap_channel[i]->enabled() ) {
            cap_channel[i]->start();
        }
    }
}

void eagle_cap_stop()
{
    int i;
    for( i=0; i<cap_channels; i++) {
        cap_channel[i]->stop();
    }
}

void onframe(cap_frame * pcapframe);	// send frames to network or to recording

void eagle_cap_setattr(int ch, struct DvrChannel_attr * pattr)
{
    if( ch>=0 && ch<cap_channels ) {
        cap_channel[ch]->setattr(pattr);
    }
}

void eagle_cap_setosd(int ch, struct hik_osd_type * posd )
{
    if( ch>=0 && ch<cap_channels ) {
        cap_channel[ch]->setosd(posd);
    }
}

void eagle_cap_captureIFrame(int ch)
{
    if( ch>=0 && ch<cap_channels ) {
        cap_channel[ch]->captureIFrame();
    }
}

void eagle_cap_captureJPEG(int ch, int quality, int pic)
{
    if( ch>=0 && ch<cap_channels ) {
        cap_channel[ch]->captureJPEG(quality, pic);
    }
}

int eagle_cap_getsignal(int ch)
{
    if( ch>=0 && ch<cap_channels ) {
        return cap_channel[ch]->getsignal();
    }
    return 0;
}

int eagle_cap_getmotion(int ch)
{
    if( ch>=0 && ch<cap_channels ) {
        return cap_channel[ch]->getmotion();
    }
    return 0;
}
