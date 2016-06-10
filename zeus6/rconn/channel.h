#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include "net.h"
#include "list.h"

#ifndef CHANNEL_BUFSIZE
#define CHANNEL_BUFSIZE 	(256*1024) 
#endif

#define STAGE_CLOSED		(0)			// closed
#define STAGE_CONNECTING	(1)			// waiting for connecting from pw
#define STAGE_CONNECTED		(2)			// connected
#define STAGE_CLOSING		(3)			// closing (peer closed)
#define STAGE_CMD			(4)			// wait for command 
#define STAGE_REG			(5)			// registered pw
#define STAGE_MSERVER		(6)			// main server
#define STAGE_VSERVER		(7)			// virtual server

class packet {

protected:	
	char   * buf ;
	int    refs ;	// number of reference
	int    s ;		// valid data start - for read
	int    e ;		// valid data end 	- for write
	int    z ;		// buffer size ;
	
public:

	packet( int siz ) {
		buf = new char [siz] ;
		refs = 1 ;
		z = siz ;
		reset();
	}
	
	~packet() { 
		delete [] buf ;
	}
	
	static void release( packet * p ) {
		if( (--(p->refs)) <= 0 ) delete p ;
	}	

	packet * addref() {
		refs++ ;
		return this ;
	}
	
	void reset() {
		s = 0 ;
		e = 0 ;
	}
	
	// address for read
	char * r() {
		return buf + s ;
	}

	// available length for read
	int len() { return e-s ; }
	
	// bytes been read 
	void use( int l ) {
		s+=l ;
		if( s > e ) s=e ;
	}
	
	// address for write
	char * w() {
		return buf + e ;
	}
	
	// available size for write
	int siz() { return z-e ; }
	
	// bytes been written
	int writ( int l ) {
		e += l ;
		if( e > z ) e = z ;
	}	
} ;

/*	
class packet {
public:	
	char * d ;			// data buffer, must be created by new char[]
	int    s ;			// buffer size	
	int    l ;			// valid data length - for write
	int    p ;			// read pointer - for read
	int    r ;			// references
	packet( int si ) {
		s = si ;
		d = new char [s] ;
		l = 0 ;
		p = 0 ;
		r = 1 ;
	}
	~packet() { if(d) delete d; }
	int len() { return l-p ; }
	int addref() {
		return ++r ;
	}
	static void release( packet * p ) {
		if( --(p->r) <= 0 ) delete p ;
	}
};
*/
	
// interface of basic network channels
class channel {
public:
	int stage ;					// channel stage
	int sock ;
	char id[40] ;
	struct pollfd * sfd ;		// poll event
	channel * target ;			// data target channel	
	slist sfifo ;				// sending fifo
	int activetime ;
	
	int  r_xoff ;				// xoff reading
	int  s_xoff_flag ;			// xoff send flag

	channel() ;
	channel( int s ) ;
	// just to declare it as virtual
	virtual ~channel() ;
	
	// interfaces
	virtual void closechannel(void)  ;
	virtual void connect( channel * peer );
	virtual int  setfd( struct pollfd * pfd, int max ) ;
	virtual int  process()  ;
	virtual void closepeer( channel * peer ) ;
	virtual int  sendpacket( packet * p, channel * from ) ;
	virtual void xoff( channel * peer ) ;
	virtual void xon( channel * peer ) ;
	virtual int  block() {
		return (sfifo.first()!=NULL) ;
	}
	
	// methods
	void sendLine( const char * line );
	void sendLineFormat( const char * format, ... );	
	void setid( char * );

protected:
	void do_send();				// push out pending send fifo
	int  do_read(int maxsize) ;	// do read packet from socket and send to target

} ;

extern int g_runtime ;

#endif	// __CHANNEL_H__
