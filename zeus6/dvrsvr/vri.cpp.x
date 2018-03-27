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
static int vri_getlist_help( char * logdisk, char * vribuf, int bufsize  )
{
	int trec = 0 ;
	int mrec = bufsize/VRI_RECORD_LENGTH ;
		
	string vlogdir ;
	sprintf( vlogdir.setbufsize(500), "%s/smartlog/", 
		logdisk );

	dir d( vlogdir ) ;
	while( d.find( "*_?.010" ) && (vribuf==NULL || mrec > trec) ) {
		if( d.isfile() ) {
			FILE * log = fopen( d.pathname() , "r" ) ;
			if( log ) {
				fseek( log, 0, SEEK_END );
				int fsize = ftell( log );
				int rsize = fsize/VRI_RECORD_LENGTH ;
				if( rsize>=2 ) {
					rsize-- ;
					if( vribuf==NULL ) {
						trec += rsize ;
					}
					else {
						fseek( log, VRI_RECORD_LENGTH, SEEK_SET ) ;
						if( rsize > (mrec-trec) ) {
							rsize = (mrec-trec) ;
						}
						rsize = fread( vribuf+(trec*VRI_RECORD_LENGTH), 1, rsize*VRI_RECORD_LENGTH, log );
						if( rsize>0 ) 
							trec += rsize/VRI_RECORD_LENGTH ;
					}
				}
				fclose( log );
			}
		}
	}
	
	printf("Get VRI help: %s -- %d\n", (char *)vlogdir, trec );
	
	return trec ;
}

// get buffer size required to retrieve vri items
int vri_getlistsize( int * itemsize )
{
	int vsize = 0 ;
	*itemsize = VRI_RECORD_LENGTH ;
	char * logdisk ;
	
	
	printf("Getlistsize\n");
	
	logdisk = disk_getdisk( 0 );
	if( logdisk != NULL ) {
		vsize += vri_getlist_help( logdisk, NULL, 0 );
	}
	logdisk = disk_getdisk( 1 );
	if( logdisk != NULL ) {
		vsize += vri_getlist_help( logdisk, NULL, 0 );
	}
	return vsize ;
}


// retrieve vri list, return total bytes read
int vri_getlist( char * buf, int bufsize )
{
	
	printf("Getlist\n");

	
	int vsize = 0 ;
	char * logdisk ;
	
	logdisk = disk_getdisk( 0 );
	if( logdisk != NULL ) {
		vsize += vri_getlist_help( logdisk, buf+vsize*VRI_RECORD_LENGTH, bufsize-vsize*VRI_RECORD_LENGTH ) ;
	}
	logdisk = disk_getdisk( 1 );
	if( logdisk != NULL ) {
		vsize += vri_getlist_help( logdisk, buf+vsize*VRI_RECORD_LENGTH, bufsize-vsize*VRI_RECORD_LENGTH ) ;
	}
	
	printf("Getlist Done. vsize: %d\n", vsize);
	
	return vsize*VRI_RECORD_LENGTH ;
}

// retrieve vri list
int vri_getlist_x( char * buf, int bufsize )
{
	int vsize = 0 ;
	int rsize = 0 ;
	struct dvrtime logtime ;
	char * logdisk = disk_getlogdisk();
	if( logdisk!=NULL ) {
		string vlogdir ;
		sprintf( vlogdir.setbufsize(500), "%s/smartlog/", 
			logdisk );

		dir d( vlogdir ) ;		
		while( d.find( "*_L.010" ) && bufsize>vsize ) {
			if( d.isfile() ) {
				FILE * log = fopen( d.pathname() , "r" ) ;
				if( log ) {
					fseek( log, VRI_RECORD_LENGTH, SEEK_SET ) ;
					rsize = fread( buf+vsize, 1, bufsize-vsize, log );
					fclose( log );
					if( rsize>0 ) vsize+=rsize ;
				}
			}
		}
		
		// scan through _N log and delete old files (30 days old?)
		
		struct dvrtime now ;
		time_now(&now) ;
		int dn = (now.year-2000)*365 + (now.month-1)*30 + now.day ;
							
		d.open( vlogdir ) ;		
		while( d.find( "*_N.010" ) ) {
			char * dash = strrchr( d.filename(), '_' );
			if( dash ) {
				int y, m, day ;
				if( sscanf( dash-8, "%04d%02d%02d", &y, &m, &day) == 3 ) {
					day = (y-2000)*365 + (m-1)*30 + day ;
					if( (dn-day) > 30 ) {		// more than 30 days old
						unlink( d.pathname() );
					}
				}
			}
		}

	}
	return vsize ;
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

	sprintf( pattern, "%s_%04d%02d%02d_?.010", 		
		(char *)g_hostname,
		day->year,
		day->month,
		day->day );
		
	dir d( vlogdir ) ;
	while( d.find( pattern ) ) {
		if( d.isfile() ) {
			FILE * log = fopen( d.pathname() , "r" ) ;
			if( log ) {
				fseek( log, VRI_RECORD_LENGTH, SEEK_SET );
				while( trec < 100000 ) {
					int vpos = vlist.size();
					vri * pvri = vlist.at(vpos);
					int r = fread( pvri->v, 1, VRI_RECORD_LENGTH, log ) ;
					if( r<VRI_RECORD_LENGTH || memcmp(pvri->v, (char*)g_hostname, strlen((char*)g_hostname) )!=0 ) {
						vlist.remove(vpos) ;
						break;
					}
					trec++;
				}
				fclose( log );
			}
		}
	}
	return trec ;
}

// get read all vri records or get vri records number
//    return record number
static void vri_remove_day_help( char * logdisk, struct dvrtime * day  )
{
	char pattern[128] ;
		
	string vlogdir ;
	sprintf( vlogdir.setbufsize(500), "%s/smartlog/", 
		logdisk );

	sprintf( pattern, "%s_%04d%02d%02d_?.010", 		
		(char *)g_hostname,
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

// list vri of multiple day
int vri_list_mday( array <vri> & vlist, struct dvrtime * dvrt, int days ) 
{
	struct dvrtime day ;
	if( dvrt == NULL ) {
		time_now( &day );
	}
	else {
		day = *dvrt ;
	}
	while( days>0 ) {
		vri_list_day( vlist, &day );
		dvrtime_prevday( &day );
		days--;
	}
	return vlist.size();
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
//   size of bytes put in buffer, 0 : failed
vri * vri_lookup( struct dvrtime * dvrt, int mdays )
{
	int i;
	int lh ;
	lh = strlen((char *)g_hostname);
	
	array <vri> vlist ;
	vri_list_mday( vlist, dvrt, mdays );
	// sort
	vlist.sort();
	// search
	for( i = vlist.size() - 1 ; i>=0 ; i-- ) {
		struct dvrtime dt = *dvrt ;
		if( sscanf( vlist[i].v+lh, "-%2d%2d%2d%2d%2d", &(dt.year), &(dt.month), &(dt.day), &(dt.hour), &(dt.minute) ) == 5 ) {
			dt.year+=2000;
			if( dt < *dvrt ) {
				// found
				vri * v = new vri ;
				*v = vlist[i] ;
				return v ;
			}
		}
	}
	return NULL ;
}
	

// tag vri
// parameter
//   buf : contain all vri fields
void vri_tag( char * buf )
{
	int lh ;
	int i;
	lh = strlen((char *)g_hostname);
	if( memcmp( buf, (char *)g_hostname, lh ) == 0 ) {
		struct dvrtime dvrt ;
		memset( &dvrt, 0, sizeof(dvrt));
		if( sscanf( buf+lh, "-%2d%2d%2d", &(dvrt.year), &(dvrt.month), &(dvrt.day) ) == 3 ) {
			array <vri> vlist ;
			vri_list_day( vlist, &dvrt );
			
			// sort and clear
			vlist.sort();
			for( i=vlist.size()-1; i>0 ; i-- ) {
				if( memcmp( vlist[i].v, vlist[i-1].v, VRI_ID_LENGTH ) == 0 ) {	// duplicated id
					vlist.remove(i) ;
				}
			}
			// find the same id
			for( i=0; i<vlist.size(); i++ ) {
				if( memcmp( buf, vlist[i].v, VRI_ID_LENGTH )==0 ) {	// found id
					break ;
				}
			}
			// add this vri
			memcpy( vlist[i].v, buf, VRI_RECORD_LENGTH );
			// sort again
			vlist.sort();
			
			// save this vri
			vri_save_day( vlist, &dvrt );
		}
	}
}

// log a new vri,
// parameter:
//   vri: contain vri id only
void vri_log( char * buf )
{
	char vribuf[ VRI_RECORD_LENGTH + 4 ] ;
	memset( vribuf,  ' ', VRI_RECORD_LENGTH+4 );
	int l = strlen( buf );
	if( l>VRI_RECORD_LENGTH ) l=VRI_RECORD_LENGTH ;
	memcpy( vribuf, buf, l );
	vri_tag( vribuf ) ;
}


