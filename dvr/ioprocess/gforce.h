
#ifndef __GFORCE_H__
#define __GFORCE_H__

extern int gforce_log_enable;
extern int gforce_available;
extern int gforce_crashdata_enable;
extern int gforce_crashdatalen;
extern unsigned char * gforce_crashdata ;
extern int gforce_crashdatalen ;

int gforce_calibration();
void gforce_calibrate_mountangle(int cal);
int gforce_getcrashdata();
void gforce_freecrashdata();
int gforce_savecrashdata();
void gforce_log( int x, int y, int z );
void gforce_reinit();
void gforce_init( config & dvrconfig ) ;
void gforce_finish();

void dvrsvr_susp();
void dvrsvr_resume();
void glog_susp();
void glog_resume();


#endif      // __GFORCE_H__
