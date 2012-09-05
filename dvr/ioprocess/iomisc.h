
#ifndef __IOMISC_H__
#define __IOMISC_H__

#include "../cfg.h"

#include "netdbg.h"
#include "mcu.h"
#include "gforce.h"
#include "diomap.h"

extern unsigned int runtime ;
extern int hdlock ;								// HD lock status
extern int hdinserted ;


int dvr_log(char *fmt, ...);
// set onboard rtc
void rtc_set(time_t utctime);

void buzzer(int times, int ontime, int offtime);

void time_setmcu();
// sync system time to rtc
void time_syncrtc();
int time_rtctosys();
void time_syncgps ();
void time_syncmcu();

void check_rtccmd() ;
void check_cpuusage();
void check_temperature();
void check_watchdog();


void dvrsvr_down();
void dvrsvr_susp();
void dvrsvr_resume();
void glog_susp();
void glog_resume();


#ifdef PWII_APP

extern int  zoomcam_enable ;
extern char zoomcam_dev[] ;
extern int  zoomcam_baud ;
extern int  zoomcam_handle ;

void zoomcam_open();
void zoomcam_close();
void zoomcam_prep();
void zoomcam_zoomin();
void zoomcam_zoomout();
void zoomcam_led(int on);
#endif

#endif		// __IOMISC_H__
