
#ifndef __GFORCE_H__
#define __GFORCE_H__

extern int gforce_log_enable;

void gforce_log( float gright, float gback, float gbuttom ) ;
void gforce_init( config & dvrconfig ) ;
void gforce_finish();


#endif      // __GFORCE_H__
