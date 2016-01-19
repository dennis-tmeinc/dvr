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
	
char vri_log_header[] = "vri, 64, tag event, 32, case number, 64, Priority, 20, Officer Id, 32, notes, 256 " ; 
#define VRI_RECORD_LENGTH	(64+32+64+20+32+256)

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

void * vri_setvri( char * vri, char * vbuf )
{
	string vlogfile, vnlogfile ;
	int y=0, m=1, d=1 ;
	char * dash = strchr( vri, '-' );
	if( dash ) {
		sscanf( dash+1, "%02d%02d%02d", &y, &m, &d) ;
	}
	
	char * logdir = disk_getlogdir();

	sprintf( vlogfile.setbufsize(500), "%s/../smartlog/%s_%04d%02d%02d_L.010", 
		logdir,
		(char *)g_hostname,
		y+2000,
		m, 
		d );
		
	// _N file ?
	sprintf( vnlogfile.setbufsize(500), "%s/../smartlog/%s_%04d%02d%02d_N.010", 
		logdir,
		(char *)g_hostname,
		y+2000,
		m, 
		d );
	
	struct stat stbuf ; 

	if( stat( (char *)vnlogfile, &stbuf )==0 ) {
		rename( (char *)vnlogfile, (char *)vlogfile ) ;
	}

	FILE * log = fopen( vlogfile, "r+" ) ;
	if( log==NULL ) {
		// just in case smartlog directory is not there
		sprintf( vnlogfile.setbufsize(500), "%s/../smartlog", 
			logdir );
		mkdir( vnlogfile, 0777 );	
		log = fopen( vlogfile, "w" ) ;
	}
	if( log ) {
		fseek( log, 0, SEEK_END ) ;
		int pos = ftell( log );
		if( pos < VRI_RECORD_LENGTH ) {
			// write file header
			fseek( log, 0, SEEK_SET );
			fprintf( log, "%-467s\n", vri_log_header );
		}
		else {
			int vlen = strlen( vri );
			pos = VRI_RECORD_LENGTH ;
			fseek(log, pos, SEEK_SET ) ;
			int r ;
			char vribuf[65] ;
			vribuf[64] = 0 ;
			while( (r = fread( vribuf, 1, 64, log )) == 64 ) {
				if( strcmp( vri, trimtail(vribuf) ) == 0 ) {
					break ;
				}
				pos += VRI_RECORD_LENGTH ;
				fseek(log, pos, SEEK_SET ) ;
			}
			fseek(log, pos, SEEK_SET ) ;
		}
		
		if( vbuf ) {
			fwrite( vbuf, 1, VRI_RECORD_LENGTH, log );
		}
		else {
			fprintf( log, "%-467s\n", vri );
		}
		fclose( log );
	}
}

void vri_log( char * vri )
{
	if( disk_getlogdir()!=NULL ) {
		vri_setvri( vri, NULL );
	}
}

// get buffer size required to retrieve vri items
int vri_getlistsize( int * itemsize )
{
	int vsize = 0 ;
	int rsize = 0 ;
	struct dvrtime logtime ;
	*itemsize = VRI_RECORD_LENGTH ;
	char * logdir = disk_getlogdir();
	if( logdir!=NULL ) {
		string vlogdir ;
		sprintf( vlogdir.setbufsize(500), "%s/../smartlog/", 
			logdir );

		dir d( vlogdir ) ;
		while( d.find( "*_L.010" ) ) {
			if( d.isfile() ) {
				FILE * log = fopen( d.pathname() , "r" ) ;
				if( log ) {
					fseek( log, 0, SEEK_END ) ;
					rsize = ftell( log );
					fclose( log );
					if( rsize>VRI_RECORD_LENGTH ) {
						vsize += rsize - VRI_RECORD_LENGTH ;
					}
				}
			}
		}
	}
	return vsize/VRI_RECORD_LENGTH ;
}

// retrieve vri list
int vri_getlist( char * buf, int bufsize )
{
	int vsize = 0 ;
	int rsize = 0 ;
	struct dvrtime logtime ;
	char * logdir = disk_getlogdir();
	if( logdir!=NULL ) {
		string vlogdir ;
		sprintf( vlogdir.setbufsize(500), "%s/../smartlog/", 
			logdir );

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

// set vri list items
void vri_setlist( char * buf, int bufsize )
{
	string str ;
	int r ;
	str.setbufsize( 80 );
	while( bufsize >= VRI_RECORD_LENGTH ) {
		// extract vri
		strncpy( (char *)str, buf, 64 ) ;
		((char *)str)[64]=0 ;
		trimtail( ((char *)str) );
		vri_setvri( (char *)str, buf ) ;
		buf+=VRI_RECORD_LENGTH ;
		bufsize -= VRI_RECORD_LENGTH ;
	}
}

