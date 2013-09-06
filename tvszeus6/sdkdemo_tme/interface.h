
#ifndef _INTERFACE_H
#define _INTERFACE_H

/* Function error codes */
#define SUCCESS         0
#define FAILURE         -1

/* Screen dimensions */
#define SCREEN_WIDTH    720
#define SCREEN_HEIGHT 	576 
#define SCREEN_BPP      16
#define SCREEN_SIZE     SCREEN_WIDTH * SCREEN_HEIGHT * SCREEN_BPP / 8

#ifndef BOOL
#define BOOL int
#endif

extern int video_osd_setup( unsigned char trans);
extern int video_osd_cleanup(void);
extern int setOsdTransparency(unsigned char trans, int bAll,int x_offset, int y_offset, int w, int h);
extern unsigned short* getDisplayBuffer(void);

#endif 


