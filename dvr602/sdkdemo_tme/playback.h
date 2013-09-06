#ifndef __PLAYBACK_H__
#define __PLAYBACK_H__
extern void PlayBack_Write();
extern void  PlayBack(int channel);
extern int netPlayBack(int netfd,char * cmd,int clientAddr);
extern void netPlayBackNextframe(int netfd,char *cmd,int clientAddr);
extern void netPlayBackSpeedDown(int netfd,char *cmd,int clientAddr);
extern void netPlayBackSpeedUp(int netfd,char *cmd,int clientAddr);
#endif
