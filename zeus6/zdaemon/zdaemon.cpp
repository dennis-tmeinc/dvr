#include <unistd.h>

/*
void x_daemon()
{
	int fd;
	if( fork() ) {
		exit(0);
	}

	fd = open("/dev/null", O_RDWR);
	dup2(fd, 0); 
	dup2(fd, 1); 
	dup2(fd, 2); 

	for( fd=3; fd<20; fd++ )
		close(fd);
		
	setsid();

	if( fork() ) {
		exit(0);
	}
		
	return ;
}
*/

int main( int argc, char * argv[])
{
	if( argc>1 ) {
		// make this process a daemon
		daemon( 1, 0 );
		execvp(argv[1], &argv[1]);
	}
	return 0 ;
}
