#ifndef	__COMMON_H__
#define	__COMMON_H__
#include "struct_def.h"

extern int startUpTask ( /*pthread_t *pid, */int pri, int stackSize, int ( *entryPoint )( int, int, int, int, int ),
						int arg1, int arg2, int arg3, int arg4, int arg5 );
extern void *taskWrap ( void *arg  );
extern int taskPriority ( int pri );
extern void DspCallback ( int channel, void* buf, int size, int frame_type,void* context );
extern void netServerTask ( void );
extern void AlarnInCtrl(void);
extern void Alarmprocess(void);
//extern void StartupWatchdog ( );
extern int SysInit ();

extern int semMTake (pthread_mutex_t  *mSem);
extern int semMGive (pthread_mutex_t  *mSem);
extern void printBoardInfo ( struct board_info boardInfo );
extern int serVideoPara ();
extern int sendAlarmToHost ( int channel );
extern int netGetAlarm(int netfd,char *cmd);
extern int  netSetAlarm(int netfd,char *cmd);

extern int readDevCfg ( DEVCFG *pDevCfg );
extern int writeDevCfg ( DEVCFG *pDevCfg );


extern int netVoiceTalkTaskRun( int netfd, char *clientCmd, int clientAddr );
extern int netSetOsdPara ( int netfd, char *clientCmd, int clientAddr );
extern int netSetDspTime ( int netfd, char *clientCmd, int clientAddr );
extern int netSetSerialMode ( int netfd, char *clientCmd, int clientAddr );
extern int netSetVideoColor ( int netfd, char *clientCmd, int clientAddr );
extern int netSetStreamPara ( int netfd, char *clientCmd, int clientAddr );


extern int netGetStreamPara ( int netfd, char *clientCmd, int clientAddr );
extern void StreamRec();
extern void MotStreamRec();
extern int netLocalRecord(int netfd,char * cmd,int clientAddr);
#ifdef DVR_DEMO
extern int netPlayBack(int netfd,char * cmd,int clientAddr);
extern void PlayBack(int channel);
extern void netSetPreviewOrder(int netfd,char * cmd,int clientAddr);

//menu
extern int setOsdTransparency(unsigned char trans, BOOL bAll,int x_offset, int y_offset, int w, int h);
extern void draw_line();
extern int  DrawFrame(UINT16 *DestBuf,UINT32 Stride,UINT32 x,UINT32 y,UINT32 w,UINT32 h,UINT32 width,UINT16 RGBcolor,UINT8 AlphaValue);
extern int  FillArea(UINT16 *DestBuf,UINT32 Stride,UINT32 x,UINT32 y,UINT32 w,UINT32 h,UINT16 FillRGBcolor,
                        BOOL bFrame,UINT8 AreAlphaValue,UINT8 FrameAlphaValue,UINT16 FrameRGBcolor,UINT32 Framewidth);
extern int  DrawBMP(char * picture_name,UINT16 *DestBuf,UINT32 Stride,UINT32 x,UINT32 y,BOOL bFrame,UINT8 AreAlphaValue,
                          UINT8 FrameAlphaValue,UINT16 FrameRGBcolor,UINT32 Framewidth);

extern int  DrawMenu(UINT16 *DestBuf);
extern int netSetOSDDisplay( int netfd, char *clientCmd, int clientAddr );
#endif

#endif
