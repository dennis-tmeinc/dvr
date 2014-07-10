#ifndef	__NET_CLIENT_H__
#define	__NET_CLIENT_H__

extern int tcpSend ( int netfd, void *vptr, size_t nSize );
extern int tcpRecv ( int netfd, void *buf, int size );
extern void processClientRequest ( int netfd, int clientAddr );
extern void netServerTask ( void );
extern int sendNetReturnVal ( int netfd, UINT32 returnVal );
extern int netClientPreview ( int netfd, char *clientCmd);
extern int netSendVideo ( int netfd, int chnl, int streamType, int index );
extern int netVideoSendCtrl ( int netfd, int chnl, int streamType, int index );
#endif
