
#include "list.h"
#include "channel.h"

#ifndef __RCONN_H__
#define __RCONN_H__

#define TUNNEL_BUFSIZE (4096)

extern int g_port ;
extern int g_keepalive ;
extern int g_runtime ;
extern int g_maxwaitms  ;

int runtime();
void rconn_loop( int ssock, int tsock );

#define LINEBUFSIZE (512)

int rconn_reg(int s);

void adb_client_init();
void adb_client_cleanup();

class rconn
	:public channel {
		
protected:
	int	m_block ;

public:
	char cmdline[LINEBUFSIZE] ;
	int  cmdptr ;
	int  datalen ;		// remaining data length to transfer to target
	int  maxidle ;		// max idle time
	int  nping ;
		
	slist channel_list ;

	rconn();
	virtual ~rconn();

	virtual void closechannel(void) ;
	virtual int  process()  ;
	virtual int  setfd( struct pollfd * pfd, int max ) ;	
	virtual void closepeer( channel * peer ) ;
	virtual int  sendpacket( packet * p, channel * from ) ;
	virtual void xoff( channel * peer ) ;
	virtual void xon( channel * peer ) ;	
	virtual int  block() {
		return m_block ;
	}		
	virtual void ping();
	
	channel * findbyId( char * id ) ;

protected:
	// connect to remote server, make it extendable
	virtual int connect_server();
	
	// command interface
	virtual void cmd( int argc, char * argv[] );


protected:
	// protected methods
	void do_cmd();
	void process_cmd(char * c);
	
private:
	// private command processing
	void  cmd_mconn( int argc, char * argv[] ) ;
	void  cmd_connect( int argc, char * argv[] ); 
	void  cmd_close( int argc, char * argv[] ) ;
	void  cmd_p2p( int argc, char * argv[] ) ;
	void  cmd_data( int argc, char * argv[] ) ;
	void  cmd_xon( int argc, char * argv[] ) ;
	void  cmd_xoff( int argc, char * argv[] ) ;
	void  cmd_echo( int argc, char * argv[] ) ;
};




#endif
