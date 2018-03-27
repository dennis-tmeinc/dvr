/* 
 * vri.h
 *    header file for vri.cpp
 */

#include "dvr.h"

#ifndef __VRI_H__
#define __VRI_H__
	
// 468 bytes per record
#define VRI_ID_LENGTH		(64)
#define VRI_RECORD_LENGTH	(VRI_ID_LENGTH+32+64+20+32+256)

class vri {
public:
	char v[VRI_RECORD_LENGTH+2] ;
	
	vri() {
		memset( v, ' ', VRI_RECORD_LENGTH );
		v[VRI_RECORD_LENGTH] = 0 ;
	}
	
	void set( char * tag, int tagsize = 0 ) {
		if( tagsize==0 )
			tagsize = strlen(tag);
		if( tagsize>VRI_RECORD_LENGTH ) tagsize=VRI_RECORD_LENGTH ;
		memcpy( v, tag, tagsize );
		for( ; tagsize<VRI_RECORD_LENGTH; tagsize++ ) {
			v[tagsize]=' ' ;
		}
		v[VRI_RECORD_LENGTH]=0 ;
	}
	
	vri & operator = ( char * src ) {
		set( src );
		return *this;
	}
	
	vri & operator = (vri & x) {
		set( x.v, VRI_RECORD_LENGTH );
		return *this;
	}
	
	int match( vri & x) {
		return (strncmp( v, x.v, VRI_ID_LENGTH) == 0 );
	}
			
	int operator < ( vri & s2 ) {
		return ( strncmp( v, s2.v, VRI_ID_LENGTH ) < 0 ) ;
	}
	
} ;


void vri_log( char * buf );
void vri_tag( char * buf, int tagsize );
char * vri_lookup( struct dvrtime * dvrt );

#endif		// __VRI_H__
