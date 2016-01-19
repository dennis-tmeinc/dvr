#ifndef __RCONN_H__
#define __RCONN_H__

#define TUNNEL_BUFSIZE (4096)

extern int g_port ;
extern int g_keepalive ;

int runtime();
void rconn_loop( int ssock, int tsock );

#define LINEBUFSIZE (1024)

int rconn_reg(int s);

void adb_client_init();
void adb_client_cleanup();

#endif
