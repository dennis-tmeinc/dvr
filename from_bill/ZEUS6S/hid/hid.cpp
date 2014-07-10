#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>


#ifndef __s32
#define __s32 signed long int 
#endif

#ifndef __u32
#define __u32 unsigned long int 
#endif

#ifndef __s16
#define __s16 signed short int
#endif

#ifndef __u16
#define __u16 unsigned short int
#endif

#include <linux/hiddev.h>



int app_quit=0 ;

// unsigned int outputmap ;	// output pin map cache
char hiddevname[] = "/dev/usb/hiddev0" ;
int  hiddev ;

void sig_handler(int signum)
{
   app_quit=1 ;
}

// return 
//        0 : failed
//        1 : success
int appinit()
{
    hiddev = open( hiddevname, O_RDWR );
    return hiddev>0 ;
}

// app finish, clean up
void appfinit()
{
    close( hiddev );
}

void testhid()
{
    int i, j;
    char name[HID_STRING_SIZE] ;
    struct    hiddev_event    event ;
    struct hiddev_collection_info collectioninfo ;

	unsigned int version ;

  /* ioctl() accesses the underlying driver */
  ioctl(hiddev, HIDIOCGVERSION, &version);

  /* the HIDIOCGVERSION ioctl() returns an int */
  /* so we unpack it and display it */
  printf("hiddev driver version is %d.%d.%d\n",
         version >> 16, (version >> 8) & 0xff, version & 0xff);


    ioctl( hiddev, HIDIOCGNAME(128), name );
    printf("hidname:%s\n", name );

    printf("Collection info:\n");
    for(i=0;i<10;i++) {
        collectioninfo.index=i;
        if( ioctl( hiddev, HIDIOCGCOLLECTIONINFO, &collectioninfo )>=0 ) {
            printf("collection index:%d type:%d usage:%08x level:%d\n",
                collectioninfo.index,
                collectioninfo.type,
                collectioninfo.usage,
                collectioninfo.level );

            
		}
		else break;
    }
    

    struct hiddev_devinfo device_info;

	/* suck out some device information */
	ioctl(hiddev, HIDIOCGDEVINFO, &device_info);

	/* the HIDIOCGDEVINFO ioctl() returns hiddev_devinfo
	* structure - see <linux/hiddev.h> 
	* So we work through the various elements, displaying 
	* each of them 
	*/
	printf("vendor 0x%04hx product 0x%04hx version 0x%04hx ",
		  device_info.vendor, device_info.product, device_info.version);
	printf("has %i application%s ", device_info.num_applications,
		 (device_info.num_applications==1?"":"s"));
	printf("and is on bus: %d devnum: %d ifnum: %d\n",
		 device_info.busnum, device_info.devnum, device_info.ifnum);


  unsigned int yalv;
  int appl;


 /* Now that we have the number of applications (in the
  * device_info.num_applications field), 
  * we can retrieve them using the HIDIOCAPPLICATION ioctl()
  * applications are indexed from 0..{num_applications-1}
  */
  for (yalv = 0; yalv < device_info.num_applications; yalv++) {
    appl = ioctl(hiddev, HIDIOCAPPLICATION, yalv);
    if (appl > 0) {
	printf("Application %i is 0x%x ", yalv, appl);
	/* The magic values come from various usage table specs */
	switch ( appl >> 16)
	    {
	    case 0x01 :
		printf("(Generic Desktop Page)\n");
		break;
	    case 0x0c :
		printf("(Consumer Product Page)\n");
		break;
	    case 0x80 :
		printf("(USB Monitor Page)\n");
		break;
	    case 0x81 :
		printf("(USB Enumerated Values Page)\n");
		break;
	    case 0x82 :
		printf("(VESA Virtual Controls Page)\n");
		break;
	    case 0x83 :
		printf("(Reserved Monitor Page)\n");
		break;
	    case 0x84 :
		printf("(Power Device Page)\n");
		break;
	    case 0x85 :
		printf("(Battery System Page)\n");
		break;
	    case 0x86 :
	    case 0x87 :
		printf("(Reserved Power Device Page)\n");
		break;
	    default :
		printf("(Unknown page - needs to be added)\n");
	    }
    }
  }

    struct hiddev_report_info rinfo ;
    struct hiddev_field_info finfo ;
struct hiddev_usage_ref uref ;
	int ret ;
   rinfo.report_type = HID_REPORT_TYPE_INPUT;
   rinfo.report_id = HID_REPORT_ID_FIRST;
   ret = ioctl(hiddev, HIDIOCGREPORTINFO, &rinfo);

   int uc = 1 ; 

   while (ret >= 0) {
       for (i = 0; i < rinfo.num_fields; i++) { 
 	    finfo.report_type = rinfo.report_type;
           finfo.report_id = rinfo.report_id;
           finfo.field_index = i;
           ioctl(hiddev, HIDIOCGFIELDINFO, &finfo);
           for (j = 0; j < finfo.maxusage; j++) {
		uref.report_type = finfo.report_type ;
		uref.report_id   = finfo.report_id ;
                uref.field_index = i;
 		uref.usage_index = j;
		uref.usage_code = uc ;
 		ioctl(hiddev, HIDIOCGUCODE, &uref);
 		ioctl(hiddev, HIDIOCGUSAGE, &uref);
			printf( "uref report type:%d, report id:%d, field idx:%d usage idx: %d\n usage code:%x usage value:%x\n",
			    uref.report_type, uref.report_id, uref.field_index, uref.usage_index,
				uref.usage_code, uref.value );
			
           }
 	}
 	rinfo.report_id |= HID_REPORT_ID_NEXT;
 	ret = ioctl(hiddev, HIDIOCGREPORTINFO, &rinfo);
   }

    uref.report_type=HID_REPORT_TYPE_OUTPUT ;
	uref.report_id = HID_REPORT_ID_UNKNOWN ;
	uref.usage_code = 0x8004b ;
	uref.value = 1 ;
	ioctl(hiddev, HIDIOCSUSAGE, &uref );

    uref.report_type=HID_REPORT_TYPE_OUTPUT ;
	uref.report_id = HID_REPORT_ID_UNKNOWN ;
	uref.usage_code = 0x8002a ;
	uref.value = 1 ;
	ioctl(hiddev, HIDIOCSUSAGE, &uref );

 

    uref.report_type=1 ;
	uref.report_id = HID_REPORT_ID_UNKNOWN ;
	uref.usage_code = 0x10082 ;

    for( i=0; i<100; i++ ) {
	if( read( hiddev, &event, sizeof(event))>0 ) {
		printf( "hid event:  hid-%08x value-%08x\n", event.hid, event.value );
	}
    }	
}


int main1()
{
	if( appinit()==0 ) {
		printf("app init error!\n");		
		return 1;
	}

	testhid() ;
	appfinit();

	return 0;
}


#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <asm/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <linux/input.h>

/* this macro is used to tell if "bit" is set in "array"
 * it selects a byte from the array, and does a boolean AND 
 * operation with a byte that only has the relevant bit set. 
 * eg. to check for the 12th bit, we do (array[1] & 1<<4)
 */
#define test_bit(bit, array)    (array[bit/8] & (1<<(bit%8)))

int main (int argc, char **argv) {

  int fd = -1;
  char buf[256] ;
  uint8_t rel_bitmask[REL_MAX/8 + 1];
  int yalv;

  /* ioctl() requires a file descriptor, so we check we got one, and then open it */
  if (argc != 2) {
    fprintf(stderr, "usage: %s event-device - probably /dev/input/evdev0\n", argv[0]);
    exit(1);
  }
  if ((fd = open(argv[1], O_RDONLY)) < 0) {
    perror("evdev open");
    exit(1);
  }

   int i;
	int rd ;
   for( i=0; i<10000; i++ ) {
		memset( buf, 0, 256 ) ;
		usleep(100000);
		rd = read(fd, buf, 256) ;
		if( rd<=0 ) break;
		printf("read: %d bytes %02x %02x %02x\n", rd, buf[0], buf[1], buf[2] );
   } 

  close(fd);

  exit(0);
}
