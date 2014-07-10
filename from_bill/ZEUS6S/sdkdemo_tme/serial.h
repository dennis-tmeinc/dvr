#ifndef	__SERIAL_H__
#define	__SERIAL_H__
extern int redef232Mode ( int netfd, char *clientCmd, int clientAddr );
extern int set232Mode ( int fd, BOOL mode );
extern int setSerialBaud ( int fd, int rate );
extern int setSerialOtherPara ( int fd, int databits,int stopbits, int parity );
extern int setSerialFlowCtrl ( int fd, int control, int enable );
extern int setSerialPara ( SERIALPARA serialPara, BOOL serialType );
extern int netSetSerialPara ( int netfd, char *clientCmd, int clientAddr, int serialType );
extern int netSetSerialMode ( int netfd, char *clientCmd, int clientAddr );

#endif

