
#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <poll.h>

const char * dbgfile = "/var/dvr/dbgout" ;
int main()
{
	char buf[512] ;
	struct pollfd fds ;
	mkfifo( dbgfile, 0777 );

	int dbgout = open(dbgfile, O_RDONLY|O_NONBLOCK) ;
	while( dbgout>0 ) {
		fds.fd = dbgout ;
		fds.events = POLLIN ;
		if( poll( &fds, 1, 1000000 )>0 ) {
			int r ;
			r = read( dbgout, buf, 512 ) ;
			if( r>0 )
				write( 1, buf, r ) ;
			else {
				usleep(1000);
			}
		}
	}
	remove(dbgfile);

	return 0 ;
}
