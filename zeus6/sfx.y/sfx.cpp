#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int extract( const char * filename, const char * xfile );
const char sfxfile[]="/proc/self/exe" ;
const char sfxrun[]="sfxrun" ;

int main(int argc, char * argv[])
{
	int l ;
    if( argc<2 ) {
		// run without argument
		extract( sfxfile, sfxrun ) ;
		if( access( sfxrun, X_OK )==0 ) {
			char sfxenv[256] ;
			l = readlink( sfxfile, sfxenv, 255 );
			if( l>0 ) {
				sfxenv[l]=0 ;
				setenv("SFX", sfxenv, 1);
				execl(sfxrun, sfxrun, NULL );
			}
		}
	}
	else {
		extract( sfxfile, argv[1] ) ;
	}
	return 0;
}
