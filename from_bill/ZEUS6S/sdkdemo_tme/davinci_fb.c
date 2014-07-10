/*#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <linux/fb.h> 

#include "interface.h"*/
#include "include_all.h"
/* Custom Davinci FBDEV defines */
#define VID0_INDEX 0
#define VID1_INDEX 1
#define ZOOM_1X    0
#define ZOOM_2X    1
#define ZOOM_4X    2

//获取OSD BUFFER物理地址
#define FBIO_GET_OSD0PHYSADDR	_IOR('F', 0x31, u_int32_t)
#define OSD_DISP_OFFSET	8
struct Zoom_Params
{
    u_int32_t WindowID;
    u_int32_t Zoom_H;
    u_int32_t Zoom_V;
};

#define FBIO_SETZOOM        _IOW('F', 0x24, struct Zoom_Params)
#define FBIO_WAITFORVSYNC   _IOW('F', 0x20, u_int32_t)
#define FBIO_GETSTD         _IOR('F', 0x25, u_int32_t)

#define FBVID_DEVICE        "/dev/fb/3"
#define OSD_DEVICE          "/dev/fb/0"
#define ATTR_DEVICE         "/dev/fb/2"

#define UYVY_BLACK          0x10801080

#define VIDEOS_NTSC 1

#define ERR printf
#define DBG printf

unsigned short* osd_buffer = NULL;
unsigned short* attr_buffer = NULL;

/* munuAlpha 在 davinci 中对应的 transparency 的值 */
int menuAlpha_trans[]={0x55, 0x66, 0x33, 0x77,0};	/* 1:1, 3:1, 1:3 */

/* Bits per pixel for video window */
#define SCREEN_BPP          16

/* Global variables to hold attribute and osd window attributes */
struct fb_var_screeninfo attrInfo;
struct fb_var_screeninfo osdInfo;


static unsigned short * osdDisplay;
static unsigned short * attrDisplay;
static int osdFd =-1,attrFd= -1;
unsigned int g_framebuf_size = 0;
unsigned char* disp_phy;
unsigned char* disp_phy_work;

#define NTSC_HIEGHT	480
#define PAL_HIEGHT	576
#define VIDEO_HIEGHT	720

int video_osd_set(unsigned char video_stantard);

unsigned char get_video_standard()
{
	return VIDEO_STANDARD;
}

/******************************************************************************
 *  video_osd_setup
 ******************************************************************************/
int video_osd_setup( unsigned char trans)
{
	int size;
	
	unsigned int addr = 0;

	osdFd = open(OSD_DEVICE, O_RDWR);

	if (osdFd == -1) {
		ERR("Failed to open OSD device %s\n", OSD_DEVICE);
		return FAILURE;
	}

	attrFd = open(ATTR_DEVICE, O_RDWR);

	if (attrFd == -1) {
		ERR("Failed to open attribute window device %s\n", ATTR_DEVICE);
		close(osdFd);
		return FAILURE;
	}

	DBG("OSD device %s opened with file descriptor %d\n", OSD_DEVICE, osdFd);
	DBG("Attribute device %s opened with file descriptor %d\n", ATTR_DEVICE, attrFd);
	if( video_osd_set(get_video_standard()) !=SUCCESS){
		ERR("Failed mmap on file descripor %d\n", osdFd);
		close(attrFd);
		close(osdFd);
		return FAILURE;		
	}

	/* Sixteen bits per pixel */
	size = (osdInfo.xres_virtual  * PAL_HIEGHT)* 2; 

	g_framebuf_size = size;

	osdDisplay = (unsigned short *) mmap(NULL, size*2,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, osdFd, 0);
	if (osdDisplay == MAP_FAILED) {
		ERR("Failed mmap on file descripor %d\n", osdFd);
		close(attrFd);
		close(osdFd);
		return FAILURE;
	}

	DBG("Mapped osd window to location %p, size %d (%#x)\n", osdDisplay, size, size);

	/* Fill the window with the new attribute value */
	memset(osdDisplay, 0, size*2);

	DBG("\tFilled OSD window with pattern: 0x0\n");

	/* One nibble per pixel */
	size = (attrInfo.xres_virtual  * PAL_HIEGHT) / 2; 

	attrDisplay = (unsigned short *) mmap(NULL, size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, attrFd, 0);
	if (attrDisplay == MAP_FAILED) {
		ERR("Failed mmap on file descripor %d\n", attrFd);
		close(attrFd);
		close(osdFd);
		return FAILURE;
	}

	DBG("Mapped attribute window to location %p, size %d (%#x)\n", attrDisplay, size, size);

	/* Fill the window with the new attribute value */
	//memset(attrDisplay, trans, size);

	DBG("\tFilled attribute window with pattern: %#x\n", trans);

	osd_buffer = osdDisplay+OSD_DISP_OFFSET;
	attr_buffer = attrDisplay+(OSD_DISP_OFFSET >> 2);

	ioctl(osdFd,FBIO_GET_OSD0PHYSADDR, &addr);

	disp_phy = (unsigned char*)addr;
	disp_phy_work = disp_phy + g_framebuf_size;


	return SUCCESS;
}

/******************************************************************************
 *  video_osd_set
 ******************************************************************************/
int video_osd_set(unsigned char video_stantard)
{
	//int size;
	
	//unsigned int addr = 0;
	
	if (osdFd == -1) {
		ERR("OSD device %s not open\n", OSD_DEVICE);
		return FAILURE;
	}

	if (attrFd == -1) {
		ERR("attribute window device %s not open\n", ATTR_DEVICE);
		close(osdFd);
		return FAILURE;
	}	

	if (ioctl(osdFd, FBIOGET_VSCREENINFO, &osdInfo) == -1) {
		ERR("Error reading variable info for file descriptor %d\n", osdFd);
		close(attrFd);
		close(osdFd);
		return FAILURE;
	}
	
	//osdInfo.xres_virtual = VIDEO_HIEGHT;
	if(video_stantard ==  VIDEOS_NTSC){		
		osdInfo.yres  =NTSC_HIEGHT;	
	}
	else{
		osdInfo.yres  =PAL_HIEGHT;	
	}
	
	if (ioctl(osdFd, FBIOPUT_VSCREENINFO, &osdInfo) == -1) {
		ERR("Error writeing variable info for file descriptor %d\n", osdFd);
		close(attrFd);
		close(osdFd);
		return FAILURE;
	}

	if (ioctl(attrFd, FBIOGET_VSCREENINFO, &attrInfo) == -1) {
		ERR("Error reading variable info for file descriptor %d\n", attrFd);
		close(attrFd);
		close(osdFd);
		return FAILURE;
	}
	
	//attrInfo.xres_virtual = VIDEO_HIEGHT;
	if(video_stantard ==  VIDEOS_NTSC){		
		attrInfo.yres  =NTSC_HIEGHT;	
	}
	else{		
		attrInfo.yres  =PAL_HIEGHT;	
	}
	
	if (ioctl(attrFd, FBIOPUT_VSCREENINFO, &attrInfo) == -1) {
		ERR("Error reading variable info for file descriptor %d\n", attrFd);
		close(attrFd);
		close(osdFd);
		return FAILURE;
	}	
	//printf("osdInfo.xres_virtual=%d  and attrInfo.xres_virtual=%d\n",osdInfo.xres_virtual,attrInfo.xres_virtual);

	return SUCCESS;
}


/******************************************************************************
 *  video_osd_place
 ******************************************************************************/
/*  NOTE: this function does not check offsets and picture size against       */
/*      dimensions of osd and attr window. If your math is wrong, you will    */
/*      get a segmentation fault (or worse)                                   */
/*                                                                            */
/******************************************************************************/
int setOsdTransparency(unsigned char trans, BOOL bAll,int x_offset, int y_offset, int w, int h)
{
	int i;
	unsigned char *attrOrigin;

	if (bAll){
		/* set transparency for full screen */
		memset(attr_buffer, trans, attrInfo.xres_virtual*attrInfo.yres/2);
	}
	else {
		attrOrigin = (unsigned char *)attr_buffer + (y_offset * (attrInfo.xres_virtual >> 1)) + (x_offset >> 1);
		for(i=0; i<h; i++){
			memset(attrOrigin + (i*(attrInfo.xres_virtual >> 1)), trans, (w >> 1) );
		}
	}

#if defined(MOUSE_SIMULATION)	
	if(bAll){
		w =0;
		h = 0;
	}
	refreshMouseTrans(attr_buffer,768,x_offset, y_offset, w, h);
#endif

	return SUCCESS;
}



