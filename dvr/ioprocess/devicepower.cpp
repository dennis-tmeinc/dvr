#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/reboot.h>

#include "diomap.h"

int power_bit[]= {
	DEVICE_POWER_GPS ,
	DEVICE_POWER_SLAVE,
	DEVICE_POWER_NETWORKSWITCH,
	DEVICE_POWER_POE,
	DEVICE_POWER_CAMERA,
	DEVICE_POWER_WIFI,
	DEVICE_POWER_POEPOWER,
	DEVICE_POWER_RADAR,
	DEVICE_POWER_HD 
} ;

const int power_num = sizeof(power_bit)/sizeof(power_bit[0]) ;

int cdc_power_bit[] =  {	
	PWII_LED_C1,
	PWII_LED_C2,
	PWII_LED_MIC,
	PWII_LED_ERROR,
	PWII_LED_POWER,
	PWII_LED_BO,
	PWII_LED_BACKLIGHT,
	PWII_POWER_ANTENNA,
	PWII_POWER_GPS,
	PWII_POWER_RF900,
	PWII_POWER_LCD,
	PWII_POWER_STANDBY,
	PWII_POWER_WIFI
} ;

const int cdc_power_num = sizeof(cdc_power_bit)/sizeof(cdc_power_bit[0]) ;

int main(int argc, char * argv[])
{
	unsigned int device;
    
	if( argc<2 ) {
		printf(
		       "Usage: devicepower devicenumber [on|off|state]\n"
		       "           device : \n"
		       "               1  => GPS,\n"
		       "               2  => Slave Eagle,\n"
		       "               3  => Network switch,\n"
		       "               4  => POE,\n"
		       "               5  => CAMERA,\n"
		       "               6  => WIFI,\n"
		       "               7  => POE (zeus),\n"
		       "               8  => RADAR,\n"
		       "               9  => Hard drive,\n"
		       "             101  => CDC C1 LED,\n"
		       "             102  => CDC C2 LED,\n"
		       "             103  => CDC Mic LED,\n"
		       "             104  => CDC Error LED,\n"
		       "             105  => CDC Power LED,\n"
		       "             106  => CDC BO LED,\n"
		       "             107  => CDC Backlight LED,\n"
		       "             108  => CDC Antenna power,\n"
		       "             109  => CDC GPS power,\n"
		       "             110  => CDC RF900 Power,\n"
		       "             111  => CDC LCD Power,\n"
		       "             112  => CDC Standyby mode,\n"
		       "             113  => CDC WIFI power\n"
		       );
		return 1;
	}

	device=0 ;
	sscanf(argv[1], "%i", &device);

    if( dio_mmap() ) {
        if( argc>2 && strcmp( argv[2], "on" )==0 ) {
			if( device>0 && device <= power_num ) {
				p_dio_mmap->devicepower |= power_bit[device-1] ;
			}
			else if( device>100 && device <= (cdc_power_num+100) ) {
	            p_dio_mmap->pwii_output |= cdc_power_bit[device-101] ;
			}
			else {
				printf("Wrong device!\n");
			}
        }
        else if( argc>2 && strcmp( argv[2], "off" )==0 ) {
			if( device>0 && device<= power_num ) {
				p_dio_mmap->devicepower &= ~(power_bit[device-1]) ;
			}
			else if( device>100 && device <= (cdc_power_num+100) ) {
	            p_dio_mmap->pwii_output &= ~(cdc_power_bit[device-101]) ;
			}
			else {
				printf("Wrong device!\n");
			}
        }
        else {
			if( device>0 && device<= power_num ) {
				printf("%s\n", 
				       (p_dio_mmap->devicepower & (power_bit[device-1]))?"on":"off" );
			}
			else if( device>100 && device <= (cdc_power_num+100) ) {
				printf("%s\n", 
				       (p_dio_mmap->pwii_output & (cdc_power_bit[device-101]))?"on":"off" );
			}
			else {
				printf("Wrong device!\n");
			}
        }
        dio_munmap();
    }
    return 0;
}

