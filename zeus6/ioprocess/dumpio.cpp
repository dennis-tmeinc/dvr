
#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mount.h>
#include <sys/file.h>

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"

struct dio_mmap * p_dio_mmap ;

void dio_init()
{
    int i;
    int fd ;
    void * p ;
    string iomapfile ;

    config dvrconfig(CFG_FILE);
    iomapfile = dvrconfig.getvalue( "system", "iomapfile");
    
    p_dio_mmap=NULL ;
    if( iomapfile.length()==0 ) {
        return ;						// no DIO.
    }


    fd = open(iomapfile.getstring(), O_RDWR );
    if( fd<=0 ) {
        printf( "IO module not started!\n");
        return ;
    }
    
    p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );                                // don't need fd to use memory map.
    if( p==(void *)-1 || p==NULL ) {
        printf( "IO memory map failed!\n");
        return ;
    }
    p_dio_mmap = (struct dio_mmap *)p ;

}

void dio_uninit()
{
    if( p_dio_mmap ) {
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    }
}

#define DUMP_I(F) printf("%3d: "#F"  \t: %d\n",  offsetof(struct dio_mmap, F), (int)(p_dio_mmap->F) );
#define DUMP_X(F) printf("%3d: "#F"  \t: 0x%08x\n", offsetof(struct dio_mmap, F), (unsigned int)(p_dio_mmap->F) );
#define DUMP_F(F) printf("%3d: "#F"  \t: %g\n", offsetof(struct dio_mmap, F), (double)(p_dio_mmap->F) );
#define DUMP_S(F) printf("%3d: "#F"  \t: %s\n", offsetof(struct dio_mmap, F), (char *)&(p_dio_mmap->F) );

// dump fields of p_dio_mmap
void print_dio()
{
	printf("Dump DIOMAP\n");
	
	DUMP_I(usage);
	DUMP_I(iopid);
	DUMP_I(dvrpid);
	DUMP_I(glogpid);

	DUMP_I(inputnum);
	DUMP_X(inputmap);
	DUMP_I(outputnum);
	DUMP_X(outputmap);

	DUMP_I(dvrcmd);
	DUMP_X(dvrstatus);
	DUMP_I(poweroff);
	DUMP_I(lockpower);
	DUMP_I(dvrwatchdog);

	DUMP_I(rtc_year);
	DUMP_I(rtc_month);
	DUMP_I(rtc_day);
	DUMP_I(rtc_hour);
	DUMP_I(rtc_minute);
	DUMP_I(rtc_second);
	DUMP_I(rtc_millisecond);
	DUMP_I(rtc_cmd);

    // GPS communicate area
	DUMP_I(gps_connection);
	DUMP_I(gps_valid);

	DUMP_F(gps_speed);
	DUMP_F(gps_direction);
	DUMP_F(gps_latitude);
	DUMP_F(gps_longitud);
	DUMP_F(gps_gpstime);
    

	DUMP_X(panel_led);
	DUMP_X(devicepower);

	DUMP_I(iotemperature);
	DUMP_I(hdtemperature1);
	DUMP_I(hdtemperature2);
   

	DUMP_I(gforce_log0);
	DUMP_F(gforce_right_0);
	DUMP_F(gforce_forward_0);
	DUMP_F(gforce_down_0);

	DUMP_I(gforce_log1);
	DUMP_F(gforce_right_1);
	DUMP_F(gforce_forward_1);
	DUMP_F(gforce_down_1);
    
	DUMP_F(gforce_forward_d);
	DUMP_F(gforce_down_d);
	DUMP_F(gforce_right_d);

	DUMP_I(gforce_changed);
	DUMP_I(synctimestart);

	DUMP_I(beeper);
   
  // app_mode
	DUMP_I(current_mode);

  // don't do disk_check (also used to stop copy process)
	DUMP_I(nodiskcheck);

    // PWII mcu support
   	DUMP_X(pwii_buttons);

   	DUMP_X(pwii_output);
   	DUMP_I(pwii_error_LED_flash_timer);
   	DUMP_S(pwii_VRI);
   	
   	printf("%3d: camera_status  \t: ",  offsetof(struct dio_mmap, camera_status) );
   	for( int i=0; i<16; i++ ) {
		printf("%02x ", (unsigned int)(p_dio_mmap->camera_status[i]) );
	}
	printf("\n");

}

int main(int argc, char * argv[])
{
	dio_init();
	if( p_dio_mmap == NULL ) {
		printf("Dio init failed!\n");
		return 1 ;
	}

	if( argc<4 ) {
		print_dio();
	}
	else if( argv[1][0] == 'b' || argv[1][0] == 'B' ) {
	}
	else if( argv[1][0] == 'w' || argv[1][0] == 'W' ) {
	}
	else if( argv[1][0] == 'd' || argv[1][0] == 'D' ) {
	}
	else if( argv[1][0] == 'f' || argv[1][0] == 'F' ) {
	}

	dio_uninit();
	return 0 ;
}


