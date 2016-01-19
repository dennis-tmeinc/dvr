
/*
 * Remote connection channel
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>

#include "channel.h"

channel::channel() 
{
	stage = STAGE_CONNECTING ;		// to be init by sub classs
	sock = -1 ;
	snprintf( id, 40, "%p", this );
	sfd = NULL ;
	target = NULL ;
	r_xoff = 1 ;	
	s_xoff_flag = 0 ;				// xoff send flag
	activetime = g_runtime ;				
}

channel::channel( int s ) 
{
	stage = STAGE_CONNECTING ;		// to be init by sub classs
	sock = s ;
	snprintf( id, 40, "%d%p", s, this );
	sfd = NULL ;
	target = NULL ;
	r_xoff = 1 ;					
	s_xoff_flag = 0 ;
	activetime = g_runtime ;				
}
		
channel::~channel() 
{
	closechannel();
}

void channel::closechannel(void) 
{
	if( target!=NULL ) {
		target->closepeer(this);
		target = NULL ;
	}
	
	if( sock>=0 ) close(sock);
	sock=-1;
	
	while( sfifo.first() ) {
		packet * p = (packet *)(sfifo.first()->item) ;
		if( p ) delete p ;
		sfifo.remove(sfifo.first());
	}

	stage = STAGE_CLOSED ;
}
	
int  channel::setfd( struct pollfd * pfd, int max )
{
	if( stage == STAGE_CLOSED ) {
		return 0 ;
	}	
	
	sfd=NULL;
	if( max>0 ) {
		if( sock>=0 ) {
			pfd->events = 0 ;
			if( target!=NULL && !(target->block()) && r_xoff == 0 )
				pfd->events |= POLLIN ;
			if( sfifo.first()!=NULL )
				pfd->events |= POLLOUT ;
			if( pfd->events != 0 ) {
				pfd->fd = sock ;
				pfd->revents = 0 ;
				sfd = pfd ;
				return 1 ;
			}
		}
	}

	return 0 ;
}

int channel::process() 
{
	int r ;
	if( sfd!=NULL && sfd->fd == sock ) {
		
		if( sock>=0 && ( sfd->revents & POLLIN ) ) {
			do_read( CHANNEL_BUFSIZE ) ;
		}
		
		if( sock>=0 && ( sfd->revents & POLLOUT ) ) {
			do_send();
			
			if( target!=NULL && target->stage == STAGE_REG  ) {
				if( sfifo.first()!=NULL ) {
					// to send xoff to target
					if( s_xoff_flag == 0 ) {
						s_xoff_flag = 1 ;
						target->xoff( this );
					}
				}
				else {
					// to send xon to target
					if( s_xoff_flag != 0 ) {
						s_xoff_flag = 0 ;
						target->xon( this );
					}
				}
			}
				
		}
	}
		
	if( stage == STAGE_CLOSING && sfifo.first()==NULL ) {
		return 0 ;
	}
	
	if( sock<0 ||
		(g_runtime - activetime) > 3600 ) {		// 1 hrs idling to close
		return 0 ;
	}
	
	return stage ;
}

void channel::connect( channel * peer )
{
	target = peer ;
	r_xoff = 0 ;				// allow reading 
	stage = STAGE_CONNECTED ;
}

void channel::closepeer( channel * peer )
{
	if( target == peer ) {
		target = NULL ;
		stage = STAGE_CLOSING ;
	}
}

//process commands
void channel::sendLineFormat( const char * format, ... )
{
	int n ;
	if(sock>=0) {
		packet * p = new packet(1024);
		va_list va ;
		va_start( va, format );	
		p->setlen( vsnprintf( p->start(), 1024, format, va ) );
		va_end( va );
		sfifo.add( p );
	}
}

void channel::sendLine( const char * line )
{
	sendLineFormat("%s", line );
}

void channel::setid( char * nid )
{
	strncpy( id, nid, sizeof(id));
}

// sending data requested from peer
int channel::sendpacket( packet * p, channel * from )
{
	if( stage == STAGE_CONNECTED && target == from ) {
		sfifo.add( new packet(p) );
		return 1 ;
	}
	return 0 ;
}

void channel::xoff( channel * peer ) 
{
	if( target == peer ) {
		r_xoff = 1 ;
	}
}

void channel::xon( channel * peer ) 
{ 
	if( target == peer ) {
		r_xoff = 0 ;
	}
}

// push out pending send fifo
void channel::do_send()
{
	while( sock>=0 && sfifo.first()!=NULL && net_srdy( sock ) ) {
		packet * p = (packet *)(sfifo.first()->item) ;
		if( p->len() > 0 ) {
			int r = net_send( sock, p->start(), p->len() ) ;
			if( r>0 ) {
				p->use(r);
			}
			else {
				close(sock);
				sock=-1;
				break;
			}
		}
		if( p->len()<=0 ) {
			delete p ;
			sfifo.remove(sfifo.first());
			activetime = g_runtime ;				
		}
		else {
			break;
		}
	}
}

int  channel::do_read(int maxsize)	// do read packet from socket and send to target
{
	int r ;
	packet * p = new packet(maxsize) ;
	r = net_recv( sock, p->start(), p->len() );
	if( r>0 ) {
		activetime = g_runtime ;
		p->setlen(r) ;
		if( target ) {
			target->sendpacket( p, this );
		}
	}
	else {
		close(sock);
		sock=-1;
	}
	delete p ;
	return r ;
}

