/* 
 * vri.cpp
 *    misc functions related to vri feature
 */

#include "dvr.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <errno.h>
#include <sys/sem.h>

#include "../ioprocess/diomap.h"

#include "dir.h"
#include "vri.h"
	
char vri_log_header[] = "vri, 64, tag event, 32, case number, 64, Priority, 20, Officer Id, 32, notes, 256 " ; 

static char * trimtail( char * str )
{
	int l = strlen( str );
	while( l>0 ) {
		if( str[l-1] > ' ' ) break ;
		str[l-1] = 0 ;
		l-- ;
	}
	return str ;
}

// get read all vri records or get vri records number
//    return record number
static int vri_list_day_help( char * logdisk, array <vri> & vlist, struct dvrtime * day  )
{
	int trec = 0 ;
	char pattern[128] ;
		
	string vlogdir ;
	sprintf( vlogdir.setbufsize(500), "%s/smartlog/", 
		logdisk );

	sprintf( pattern, "*_%04d%02d%02d_?.010", 		
		day->year,
		day->month,
		day->day );
		
	int hostlen = strlen((char*)g_hostname)  ;
	dir d( vlogdir ) ;
	while( d.find( pattern ) ) {
		if( d.isfile() ) {
			FILE * log = fopen( d.pathname() , "r" ) ;
			if( log ) {
				fseek( log, VRI_RECORD_LENGTH, SEEK_SET );
				while( trec++ < 100000 ) {
					vri v ;
					int r = fread( v.v, 1, VRI_RECORD_LENGTH, log ) ;
					if( r<VRI_RECORD_LENGTH ) {
						break;
					}
					if( memcmp(v.v, (char*)g_hostname, hostlen)==0 && v.v[hostlen] == '-' ) {		// vri for this host
						vlist.add( v ) ;
					}
				}
				fclose( log );
			}
		}
	}
	return trec ;
}
					
int vri_list_day( array <vri> & vlist, struct dvrtime * day ) 
{
	int vsize = 0 ;
	char * logdisk ;
	
	logdisk = disk_getdisk( 0 );
	if( logdisk != NULL ) {
		vsize += vri_list_day_help( logdisk, vlist, day );
	}
	logdisk = disk_getdisk( 1 );
	if( logdisk != NULL ) {
		vsize += vri_list_day_help( logdisk, vlist, day );
	}
	return vsize ;
}

// get read all vri records or get vri records number
//    return record number
static void vri_remove_day_help( char * logdisk, struct dvrtime * day  )
{
	char pattern[128] ;
		
	string vlogdir ;
	sprintf( vlogdir.setbufsize(500), "%s/smartlog/", 
		logdisk );

	sprintf( pattern, "*_%04d%02d%02d_?.010", 		
		day->year,
		day->month,
		day->day );
		
	dir d( vlogdir ) ;
	while( d.find( pattern ) ) {
		if( d.isfile() ) {
			remove(  d.pathname() );
		}
	}
}

// get read all vri records or get vri records number
//    return record number
static void vri_save_day_help( char * logdisk, array <vri> & vlist, struct dvrtime * day  )
{
	string vlogname ;
	
	sprintf( vlogname.setbufsize(500), "%s/smartlog", 
		logdisk );
	mkdir( (char *)vlogname, 0777 );
	
	sprintf( vlogname.setbufsize(500), "%s/smartlog/%s_%04d%02d%02d_L.010", 
		(char *)logdisk,
		(char *)g_hostname,
		day->year,
		day->month,
		day->day );
	
	FILE * log = fopen( vlogname, "wb" ) ;
	if( log ) {
		
		vlist.sort();
		
		// write file header
		vri header ;
		header.set( vri_log_header );
		fwrite( header.v, 1, VRI_RECORD_LENGTH, log );
		int i ;
		for( i=0; i<vlist.size(); i++ ) {
			fwrite( vlist[i].v, 1, VRI_RECORD_LENGTH, log );
		}
		fclose( log );
	}
}


void vri_save_day( array <vri> & vlist, struct dvrtime * day ) 
{
	char * logdisk ;
	
	logdisk = disk_getdisk( 0 );
	if( logdisk != NULL ) {
		vri_remove_day_help( logdisk, day );
	}
	logdisk = disk_getdisk( 1 );
	if( logdisk != NULL ) {
		vri_remove_day_help( logdisk, day );
	}

	logdisk = disk_getlogdisk();
	if( logdisk == NULL ) {
		logdisk = disk_getdisk( 0 );
	}
	if( logdisk == NULL ) {
		logdisk = disk_getdisk( 1 );
	}
	if( logdisk != NULL ) {
		vri_save_day_help( logdisk, vlist, day );
	}
}

// return vri item size 
int vri_isize() 
{
	return VRI_RECORD_LENGTH ;
}

// tag vri
// parameter
//   buf : contain all vri fields
// return
//   lookup buffer, should call mem_free to release the buffer
char * vri_lookup( struct dvrtime * dvrt )
{
	int i;
	int lh = strlen((char *)g_hostname) ;
	struct dvrtime lookuptime ;
	array <vri> vlist ;
	
	if( dvrt == NULL || dvrt->year == 0 ) {
		time_now( &lookuptime );
	}
	else {
		lookuptime = *dvrt ;
	}

	struct dvrtime pday = lookuptime ;

	// search for previous 10 days 
	for( i=0; i<10; i++ ) {
		dvrtime_prevday( &pday );
		vri_list_day( vlist, &pday );
		if( vlist.size()>0 ) break;
	}
	vri_list_day( vlist, &lookuptime );
	// sort
	vlist.sort();
	
	// search
	struct dvrtime dt ;
	memset( &dt, 0, sizeof(dt));
	for( i = vlist.size() - 1 ; i>=0 ; i-- ) {
		if( sscanf( vlist[i].v+lh, "-%2d%2d%2d%2d%2d", &(dt.year), &(dt.month), &(dt.day), &(dt.hour), &(dt.minute) ) == 5 ) {
			dt.year+=2000;
			if( i==0 || (! (dt > lookuptime)) ) {
				// found
				char * vri = (char *) mem_alloc(VRI_RECORD_LENGTH+4) ;
				memcpy( vri, vlist[i].v,  VRI_RECORD_LENGTH+1 );
				return vri ;
			}
		}
	}
	return NULL ;
}
	

// tag vri
// parameter
//   buf : contain all vri fields
void vri_tag( char * buf, int tagsize )
{
	int lh ;
	int i;
	
	vri v ;
	v.set( buf, tagsize );

	lh = strlen((char *)g_hostname);
	struct dvrtime dvrt ;
	memset( &dvrt, 0, sizeof(dvrt));
	if( sscanf( buf+lh, "-%2d%2d%2d", &(dvrt.year), &(dvrt.month), &(dvrt.day) ) == 3 ) {
		dvrt.year+=2000 ;
		array <vri> vlist ;
		vri_list_day( vlist, &dvrt );
		
		// sort and clear
		vlist.sort();
		for( i=vlist.size()-1; i>=0 ; i-- ) {
			if( vlist[i].match( v ) ) {					// match new id
				vlist.remove(i);
			}
			else if( i>0 && vlist[i].match( vlist[i-1] ) ) {	// duplicated id
				vlist.remove(i) ;
			}
		}
		
		// add new vri tag
		vlist.add(v);

		// save this vri
		vri_save_day( vlist, &dvrt );
	}
}

// log a new vri,
// parameter:
//   vri: contain vri id only
void vri_log( char * buf )
{
	vri_tag( buf, strlen(buf) ) ;
}


