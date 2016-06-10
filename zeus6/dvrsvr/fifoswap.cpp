#include "dvr.h"
#include "fifoswap.h"

struct swap_type {
	int pos ;
	int size ;
	swap_type * next ;
};

static swap_type * swaplist ;
static FILE * swap_file ;

static void swap_clean()
{
	while( swaplist != NULL ) {
		swap_type * nst = swaplist->next ;		
		delete swaplist ;
		swaplist = nst ;
	}	
}

static FILE * swap_open()
{
	char swapfilename[260] ;
	char * basedisk = disk_getbasedisk( 0 ) ;
	if( basedisk == NULL || strlen(basedisk)<8 ) {	// no base dir, no recording disk available
		return NULL ;
	}
	sprintf( swapfilename, "%s/.fifoswap",  basedisk );
	FILE * f = fopen( swapfilename, "w+" );
	if( f ) {
		fwrite("SWAP", 1, 4, f );
		return f ;
	}
	return NULL ;
}

int swap_out( void * mem, int size )
{
	if( mem==NULL || size<=0 ) {
		return 0;
	}
	
	if( swap_file == NULL ) {
		swap_file = swap_open() ;
		if( swap_file == NULL ) {
			return 0 ;
		}
		swap_clean();		
	}
		
	// find swap space 
	int xpos = 4 ;			// start from a positve value
	swap_type * pst = NULL ;
	swap_type * nst = swaplist ;
	while( nst ) {
		if( nst->pos >= (xpos+size) ) {		// found enough space
			break ;
		}
		else {
			xpos = nst->pos + nst->size ;
			pst = nst ;
			nst = nst->next ;
		}
	}
		
	swap_type * st = new swap_type ;
	st->next = nst ;
	st->pos = xpos ;
	st->size = size ;
		
	if( pst ) {
		pst->next = st ;
	}
	else {
		swaplist = st ;
	}

	fseek( swap_file, xpos, SEEK_SET );
	fwrite( mem, 1, size, swap_file );

	return xpos ;
}

int  swap_in( void * mem, int size, int swappos )
{
	swap_type * pst = NULL ;
	swap_type * st = swaplist ;
	while( st ) {
		if( st->pos == swappos ) {				// matched
			if( mem != NULL && size>0 ) {
				fseek( swap_file, swappos, SEEK_SET );
				fread( mem, 1, size, swap_file );
			}
			
			if( pst ) {
				pst->next = st->next ;
			}
			else {
				swaplist = st->next ;
			}
			
			delete st ;
			
			if( swaplist == NULL ) {			// emptied
				fseek( swap_file, 0, SEEK_SET );
				ftruncate( fileno(swap_file), 4 );				
			}
			return 1 ;
		}
		pst = st ;
		st = st->next ;
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
	swaplist = NULL ;
}

void swap_finish()
{
	swap_clean();
	if( swap_file ) {
		ftruncate( fileno( swap_file ), 0 );
		fclose( swap_file );
		swap_file = NULL ;
	}
}
