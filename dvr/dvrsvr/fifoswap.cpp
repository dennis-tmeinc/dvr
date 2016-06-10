#include "dvr.h"
#include "fifoswap.h"

struct swap_type {
	int pos ;
	int size ;
};

static swap_type * swaplist ;
static int swaplistarraysize ;		// array size
static int swaplistsize ;			// valid elemnts
static FILE * swap_file ;

static FILE * swap_open()
{
	char swapfilename[260] ;
	if( rec_basedir.length() <= 0 ) {	// no base dir, no recording disk available
		return NULL ;
	}
	swaplistsize = 0 ;
	sprintf( swapfilename, "%s/.preswap", (char *)rec_basedir );
	FILE * f = fopen( swapfilename, "w+" );
	if( f ) {
		fwrite("SWAP", 1, 4, f );
		return f ;
	}
	return NULL ;
}

static void swap_compact()
{
	int s, d=0 ;
	for( s=0, d=0; s<swaplistsize; s++ ) {
		if( swaplist[s].pos>0 ) {
			if( s != d ) {
				swaplist[d].pos = swaplist[s].pos ;
				swaplist[d].size = swaplist[s].size ;
			}
			d++ ;
		}
	}
	swaplistsize = d ;
	if( swaplistsize==0 ) {
		// swap space clear, clean the swap file
		rewind(swap_file);
		ftruncate( fileno(swap_file), 4 );
	}

}

static void swap_setarraysize()
{
	if( swaplistarraysize <= swaplistsize ) {
		swaplistarraysize = swaplistsize + 8000 ;
		if( swaplist==NULL ) {
			swaplist = (swap_type *) malloc( swaplistarraysize * sizeof(swap_type) );
		}
		else {
			swaplist = (swap_type *) realloc(swaplist, swaplistarraysize * sizeof(swap_type) );
		}
	}
}
	
int swap_out( void * mem, int size )
{
	if( swap_file == NULL ) {
		swap_file = swap_open() ;
		if( swap_file == NULL ) {
			return 0 ;
		}
	}
	
	// try increase swap list array to fit all entry
	swap_setarraysize();
	
	// find swap space 
	int xpos = 4 ;			// start from a positve value
	int i  ;
	int entry = 0 ;
	for( i=0; i<swaplistsize; i++ ) {
		if( swaplist[i].pos <= 0 ) {
			continue ;
		}
		if( swaplist[i].pos >= (xpos+size) ) {			// found enough space 
			break;
		}
		xpos = swaplist[i].pos + swaplist[i].size ;			// point to the end of this swap
		entry = i+1 ;
	}
	
	if( entry >= swaplistsize ) {
		// append an entry
		swaplistsize++;
	}
	else if( swaplist[entry].pos > 0 ) {
		// to insert an entry
		memmove( &swaplist[entry+1],  &swaplist[entry], ( swaplistsize-entry ) * sizeof(swap_type) );
		swaplistsize ++ ;
	}
	
	swaplist[entry].pos = xpos ;
	swaplist[entry].size = size ;

	fseek( swap_file, swaplist[entry].pos, SEEK_SET );
	fwrite( mem, 1, swaplist[entry].size, swap_file );

	return swaplist[entry].pos ;
}

int  swap_in( void * mem, int size, int swappos )
{
	int i ;
	int eslot = 0 ;
	for( i=0; i<swaplistsize; i++ ) {
		if( swaplist[i].pos <= 0 ) {
			eslot ++ ;
		}
		else if( swaplist[i].pos == swappos ) {				// matched 
			if( mem != NULL && size>0 ) {
				fseek( swap_file, swappos, SEEK_SET );
				fread( mem, 1, size, swap_file );
			}
			// mark invalid entry
			swaplist[i].pos = 0 ;
			
			if( eslot>500 || i==0 ) {
				swap_compact();
			}
			
			return 1 ;
		}
	}
	
	// dump errors
	
	printf("DUMP\n  mem: %p, size: %d, pos: %d\n", mem, size, swappos ) ;
	
	return 0 ;
}

int  swap_discard( int swappos )
{
	return swap_in( NULL, 0, swappos ) ;
}

void swap_init()
{
	swap_file = NULL;
	swaplistarraysize = 0 ;
	swaplist = NULL ;
	swaplistsize = 0 ;				// zero
}

void swap_finish()
{
	swaplistarraysize = 0 ;
	swaplistsize = 0 ;	
	if( swaplist != NULL ) {
		free( swaplist );
		swaplist = NULL ;
	}	
	if( swap_file ) {
		ftruncate( fileno(swap_file), 0 );
		fclose( swap_file );
		swap_file = NULL ;
	}
}


