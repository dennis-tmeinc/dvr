#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

struct dio_mmap * p_dio_mmap ;

char dvriomap[256] = "/var/dvr/dvriomap" ;

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;

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
    p_dio_mmap=NULL ;
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
    if( (p_dio_mmap->iopid > 0 ) ) {
        return 1 ;
    }
    else {
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
        return 0 ;
    }
}

// app finish, clean up
void appfinish()
{
    // clean up shared memory
    if( p_dio_mmap ) {
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}

int main(int argc, char * argv[])
{
    int buz_times=1 ;
    int buz_interval_on=1000 ;
    int buz_interval_off=1000 ;
    
    
	if( argc<2 ) {
		printf("Usage: buzzer <times> [interval_on_ms] [interval_off_ms]\n");
		return 1 ;
	}
    buz_times = atoi( argv[1] ) ;
    if( buz_times<=0 || buz_times>50 ) {
        buz_times=1 ;
    }
    
    if(argc>2 ) {
        buz_interval_on = atoi( argv[2] ) ;
        if( buz_interval_on<10 || buz_interval_on>10000 ) {
            buz_interval_on=1000 ;
        }
        buz_interval_off=buz_interval_on ;
    }
    
    if( argc>3 ) {
        buz_interval_off = atoi( argv[3] ) ;
        if( buz_interval_off<10 || buz_interval_off>10000 ) {
            buz_interval_off=1000 ;
        }
    }
	
    if( appinit()==0 ) {
        return 1;
    }
    
    while( buz_times-- ) {
        p_dio_mmap->outputmap |= 0x100 ;     // buzzer on
        usleep( buz_interval_on*1000 ) ;
        p_dio_mmap->outputmap &= (~0x100) ;     // buzzer off
        usleep( buz_interval_off*1000 ) ;
    }

	appfinish();
    return 0;
}

