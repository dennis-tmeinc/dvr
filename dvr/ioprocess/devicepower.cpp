#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "diomap.h"

int main(int argc, char * argv[])
{
	unsigned int devicepower;
    
	if( argc<2 ) {
		printf("Usage: devicepower [devicepowermap]\n");
		printf("       powermap: bit0=GPS, bit1=Slave Eagle32, bit2=Network switch\n");
		return 1;
	}
	
	devicepower=0 ;
	sscanf(argv[1], "%i", &devicepower);

    if( dio_mmap() ) {
    	p_dio_mmap->devicepower = devicepower & 0xffff ;		
        dio_munmap();
    }
    return 0;
}

