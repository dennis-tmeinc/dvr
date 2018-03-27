#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

struct dio_mmap * p_dio_mmap ;

char dvriomap[256] = "/var/dvr/dvriomap" ;

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = CFG_FILE ;

// return 
//        0 : failed
//        1 : success
int appinit()
{
    int fd ;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    v = dvrconfig.getvalue( "system", "iomapfile");
    char * iomapfile = v.getstring();
    if( iomapfile && strlen(iomapfile)>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    fd = open(dvriomap, O_RDWR );
    if( fd<=0 ) {
        printf("Can't open io map file!\n");
        return 0 ;
    }
    p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );								// fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
	return (p_dio_mmap->iopid > 0 );
}

// app finish, clean up
void appfinish()
{
    // clean up shared memory
    munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
}

int main(int argc, char * argv[])
{
	unsigned int devicepower;
	
    if( appinit()==0 ) {
        return 1;
    }

	if( argc>=2 ) {
		devicepower=0 ;
		sscanf(argv[1], "%i", &devicepower);
		p_dio_mmap->devicepower = devicepower ;		
	}

	printf("Device Power: 0x%08x \n", p_dio_mmap->devicepower );

	appfinish();
    return 0;
}

