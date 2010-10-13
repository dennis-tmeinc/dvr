#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "diomap.h"

int main(int argc, char * argv[])
{
	unsigned int panelled;
    
	if( argc<2 ) {
		printf("Usage: panelled [panel_led_map]\n");
		printf("       led map: bit0=USB FLASH, bit1=Error, bit2=Video lost Led\n");
		return 1 ;
	}
	
	panelled=0 ;
	sscanf(argv[1], "%i", &panelled);

    if( dio_mmap() ) {
    	p_dio_mmap->panel_led = panelled & 0xffff ;		
        dio_munmap();
    }
    return 0;
}

