#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/reboot.h>

#include "diomap.h"

int main(int argc, char * argv[])
{
	unsigned int devicepower;
    
	if( argc<3 ) {
		printf("Usage: devicepower [on|off] [devicepowermap]\n");
		printf("       powermap: bit0=GPS, bit1=Slave Eagle32, bit2=Network switch\n");
		printf("       CDC powermap: bit8=GPS antenna, bit9=GPS power, bit13=WIFI power\n");
		return 1;
	}

	devicepower=0 ;
	sscanf(argv[2], "%i", &devicepower);


    if( dio_mmap() ) {
        if( strcmp( argv[1], "on" )==0 ) {
    	    p_dio_mmap->devicepower |= devicepower & 0xff ;		
            p_dio_mmap->pwii_output |= devicepower & 0xff00 ;
        }
        else {
    	    p_dio_mmap->devicepower &= (~devicepower) | 0xffffff00 ;		
            p_dio_mmap->pwii_output &= (~devicepower) | 0xffff00ff ;
        }
        dio_munmap();
    }
    return 0;
}

