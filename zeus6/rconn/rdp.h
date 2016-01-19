#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include "net.h"
#include "list.h"

/* Reliable datagram 
 */
 
/* packet format:
 *		byte 0 : 		: 11    ( indentify from TURN packet )
 * 		bit 2-7	:		: packet type
 *		byte 2-3		: seq
 * 
 * 		payload type, payload len, payload
 * 		...
 */
 
 
 packet type

0:		punch
1:		reset			( seq )
2:		payload			( seq + payload )
3:      ack				( seqs )

 ack format
	n		(byte 0-1, len of bytes of recevied sequences) ( may not exist or 0 )
	0 - (n-1)  bytes of seq  ( offset from packet seq )
 

 attributes  payload format	(align to 4bytes boundary)
	a type				bit  0-3
	a payload len (n):	byte 0: bit 4-7, byte 1   (12 bits)
	a session no		byte 2-3
	a payload			n bytes

ATYPE: 
0:		ack
1:		ack1
2:		connect 0
3:		connect 1
4:		close 0
5:		close 1
6:		window			( flow control, payload: 32bits windows size )
7:		data

// p2p supports, to do later
class rdp : public channel {
public:
	
	// interfaces
	virtual void closechannel(void)  ;
	virtual int  setfd( struct pollfd * pfd, int max ) ;
	virtual int  process()  ;
	
};

extern int g_runtime ;

#endif	// __CHANNEL_H__
