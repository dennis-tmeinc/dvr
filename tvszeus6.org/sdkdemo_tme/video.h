#ifndef	__VIDEO_H__
#define	__VIDEO_H__
extern int serVideoPara ();
extern int netSetOsdPara ( int netfd, char *clientCmd, int clientAddr );
extern int netSetVideoColor ( int netfd, char *clientCmd, int clientAddr );
extern int netGetVideoColor ( int netfd, char *clientCmd, int clientAddr );
extern int netSetStreamPara ( int netfd, char *clientCmd, int clientAddr );
extern int netGetStreamPara ( int netfd, char *clientCmd, int clientAddr );
extern int netSetMaskArea(int netfd,char *cliendCmd);
extern int netCancelMaskArea(int netfd,char *cliendCmd);
extern int netSetOSDDisplay( int netfd, char *clientCmd, int clientAddr );
#endif
