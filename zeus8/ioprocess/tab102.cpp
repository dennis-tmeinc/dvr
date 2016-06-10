// Tab102b related functions

#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
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

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "diomap.h"
#include "mcu.h"

int dvr_log(const char *fmt, ...);

#define UPLOAD_ACK_SIZE 10

enum {TYPE_CONTINUOUS, TYPE_PEAK};

extern int gsensor_direction  ;
extern float g_sensor_trigger_forward ;
extern float g_sensor_trigger_backward ;
extern float g_sensor_trigger_right ;
extern float g_sensor_trigger_left ;
extern float g_sensor_trigger_down ;
extern float g_sensor_trigger_up ;
extern float g_sensor_base_forward ;
extern float g_sensor_base_backward ;
extern float g_sensor_base_right ;
extern float g_sensor_base_left ;
extern float g_sensor_base_down ;
extern float g_sensor_base_up ;
extern float g_sensor_crash_forward ;
extern float g_sensor_crash_backward ;
extern float g_sensor_crash_right ;
extern float g_sensor_crash_left ;
extern float g_sensor_crash_down ;
extern float g_sensor_crash_up ;
extern int  g_sensor_available ;
extern char direction_table[24][3] ;


static int binaryToBCD(int x)
{
  int m, I, b, cc;
  int bcd;

  cc = (x % 100) / 10;
  b = (x % 1000) / 100;
  I = (x % 10000) / 1000;
  m = x / 10000;

  bcd = (m * 9256 + I * 516 + b * 26 + cc) * 6 + x;

  return bcd;
}


/*
 * read_nbytes:read at least rx_size and as much data as possible with timeout
 * return:
 *    n: # of bytes received
 *   -1: bad size parameters
 */  

static int read_nbytes(unsigned char *buf, int bufsize,
		int rx_size)
{
  //struct timeval tv;
  int total = 0, bytes;
  int left=0;
  if (bufsize < rx_size)
    return -1;
  left=rx_size;
  while(mcu_dataready(50000000)){
      bytes=mcu_read((char *)buf+total,left);
      if(bytes>0){
	total+=bytes;
	left-=bytes;
      }
      if(left==0)
	break;
  }

  return total;
}

static int verifyChecksum(unsigned char *buf, int size)
{
  unsigned char cs = 0;
  int i;

  for (i = 0; i < size; i++) {
    cs += buf[i];
  }

  if (cs)
    return 1;

  return  0;
}

void writeGforceStatus(char *status)
{
  FILE *fp;
  fp = fopen("/var/dvr/gforce", "w");
  if (fp) {
    fprintf(fp, "%s\n", status);
    fclose(fp);
  }
}

void writeTab102Status(char *status)
{
  FILE *fp;
  fp = fopen("/var/dvr/tab102", "w");
  if (fp) {
    fprintf(fp, "%s\n", status);
    fclose(fp);
  }
}

void writeTab102Data(unsigned char *buf, int len, int type)
{
  char hostname[128] ;
  char filename[256];
  char curdisk[256];
  char path[256];
  int copy=0;
  int r = 0;
  struct tm tm;
  // printf("\nstart to write Tab102B data\n");
  FILE *fp = fopen("/var/dvr/dvrcurdisk", "r");
  if (fp != NULL) {
   // r = fread(curdisk, 1, sizeof(curdisk) - 1, fp);
  //  printf("step1\n ");
    fscanf(fp, "%s", curdisk);
   // printf("step2\n");
    fclose(fp);
  } else{
    printf("no /var/dvr/dvrcurdisk");
    return;
  }
  //printf("step3\n");
  snprintf(path, sizeof(path), "%s/smartlog", curdisk);
  if(mkdir(path, 0777) == -1 ) {
    if(errno == EEXIST) {
      copy = 1; // directory exists
    }
  } else {
    copy = 1; // directory created
  }
 // printf("step4\n");
  time_t t = time(NULL);
  localtime_r(&t, &tm);
  gethostname(hostname, 128);
  snprintf(filename, sizeof(filename),
	   "%s/smartlog/%s_%04d%02d%02d%02d%02d%02d_TAB102log_L.%s",
	   curdisk,hostname,
	   tm.tm_year + 1900,
	   tm.tm_mon + 1,
	   tm.tm_mday,
	   tm.tm_hour,
	   tm.tm_min,
	   tm.tm_sec,
	   (type == TYPE_CONTINUOUS) ? "log" : "peak");   
//  printf("opening %s len=%d\n", filename,len);
  fp = fopen(filename, "w");
  if (fp) {
   // dprintf("OK");
    fwrite(buf, 1, len, fp);
    //safe_fwrite(buf, 1, len, fp);
    fclose(fp);
  }    
}

int Tab102b_SetRTC()
{
    int i=0;
	struct timeval tv;
	struct tm bt;

	gettimeofday(&tv, NULL);
	gmtime_r(&tv.tv_sec, &bt);
  
  printf("tab102 set time\n");
  
    if( mcu_cmd(0x07, 7,
		(int)(unsigned char)binaryToBCD(bt.tm_sec),
		(int)(unsigned char)binaryToBCD(bt.tm_min),
		(int)(unsigned char)binaryToBCD(bt.tm_hour),
		bt.tm_wday + 1,
		(int)(unsigned char)binaryToBCD(bt.tm_mday),
		(int)(unsigned char)binaryToBCD(bt.tm_mon + 1),
		(int)(unsigned char)binaryToBCD(bt.tm_year - 100) )  ) {
			return 0 ;		// success
	}
	return -1 ; 			// failed
}

int Tab102b_setTrigger()
{
  char txbuf[64];
  int v;
  int value;

 // printf("inside Tab102b_setTrigger\n");
  txbuf[0] = 0x2b; // len
  txbuf[1] = 0x04; // Tab module addr
  txbuf[2] = 0x00; // my addr
  txbuf[3] = 0x12; // cmd
  txbuf[4] = 0x02; // req

  // base trigger
  v = (int)((g_sensor_base_forward-0.0)* 37.0) + 0x200;
  txbuf[5] = (v >> 8) & 0xff;
  txbuf[6] =  v & 0xff;
  v = (int)((g_sensor_base_backward-0.0)* 37.0) + 0x200;
  txbuf[7] = (v >> 8) & 0xff;
  txbuf[8] =  v & 0xff;
  v = (int)((g_sensor_base_right-0.0) * 37.0) + 0x200;
  txbuf[9] = (v >> 8) & 0xff;
  txbuf[10] =  v & 0xff;
  v = (int)((g_sensor_base_left-0.0) * 37.0) + 0x200;
  txbuf[11] = (v >> 8) & 0xff;
  txbuf[12] =  v & 0xff;
  v = (int)((g_sensor_base_down-0.0) * 37.0) + 0x200;
  txbuf[13] = (v >> 8) & 0xff;
  txbuf[14] =  v & 0xff;
  v = (int)((g_sensor_base_up-0.0)* 37.0) + 0x200;
  txbuf[15] = (v >> 8) & 0xff;
  txbuf[16] =  v & 0xff;

  // peak trigger
  v = (int)((g_sensor_trigger_forward-0.0)* 37.0) + 0x200;
  txbuf[17] = (v >> 8) & 0xff;
  txbuf[18] =  v & 0xff;
  v = (int)((g_sensor_trigger_backward-0.0) * 37.0) + 0x200;
  txbuf[19] = (v >> 8) & 0xff;
  txbuf[20] =  v & 0xff;
  v = (int)((g_sensor_trigger_right-0.0) * 37.0) + 0x200;
  txbuf[21] = (v >> 8) & 0xff;
  txbuf[22] =  v & 0xff;
  v = (int)((g_sensor_trigger_left-0.0) * 37.0) + 0x200;
  txbuf[23] = (v >> 8) & 0xff;
  txbuf[24] =  v & 0xff;
  v = (int)((g_sensor_trigger_down-0.0) * 37.0) + 0x200;
  txbuf[25] = (v >> 8) & 0xff;
  txbuf[26] =  v & 0xff;
  v = (int)((g_sensor_trigger_up-0.0) *37.0) + 0x200;
  txbuf[27] = (v >> 8) & 0xff;
  txbuf[28] =  v & 0xff;

  // crash trigger
  v = (int)((g_sensor_crash_forward-0.0) * 37.0) + 0x200;
  txbuf[29] = (v >> 8) & 0xff;
  txbuf[30] =  v & 0xff;
  v = (int)((g_sensor_crash_backward-0.0) *37.0) + 0x200;
  txbuf[31] = (v >> 8) & 0xff;
  txbuf[32] =  v & 0xff;
  v = (int)((g_sensor_crash_right-0.0) * 37.0) + 0x200;
  txbuf[33] = (v >> 8) & 0xff;
  txbuf[34] =  v & 0xff;
  v = (int)((g_sensor_crash_left-0.0) *37.0) + 0x200;
  txbuf[35] = (v >> 8) & 0xff;
  txbuf[36] =  v & 0xff;
  v = (int)((g_sensor_crash_down-0.0) * 37.0) + 0x200;
  txbuf[37] = (v >> 8) & 0xff;
  txbuf[38] =  v & 0xff;
  v = (int)((g_sensor_crash_up-0.0) *37.0) + 0x200;
  txbuf[39] = (v >> 8) & 0xff;
  txbuf[40] =  v & 0xff;
  txbuf[41] = direction_table[gsensor_direction][2]; // direction
  
  int retry=3; 
  while( retry-- > 0 ) {
	  mcu_send( txbuf ) ;
	  char * rsp = mcu_recv();
	  if( rsp ) {
		if(rsp[0]=='\x06' &&
		rsp[2]=='\x04' &&
		rsp[3]=='\x12' &&
		rsp[4]=='\x03') 
		{
			break;
			
		}
       }
  }

  return (retry > 0) ? 0 : 1 ;
 
}

void Tab102b_sendUploadConfirm()
{
	mcu_sendcmd_target(ID_RF, 0x1a, 1, 1 ) ;
}

void Tab102b_sendUploadPeakConfirm()
{
	mcu_sendcmd_target(ID_RF, 0x20, 1, 1 ) ;
}

int Tab102b_version(char * version)
{
	char * rsp = mcu_cmd( 0x0e );
	if( rsp ) {
		memcpy(version,&rsp[5],rsp[0]-6);
		version[rsp[0]-6]=0;
		return (rsp[0]-6);
	}
	return 0;
}

int Tab102b_enablePeak()
{
	if( mcu_cmd( 0x0f ) ) {
		return 0 ;
	}
	return -1 ;
}

int Tab102b_UploadStart()
{
	if( mcu_cmd( 0x42, 1, 1 ) ) {
	  dvr_log("Upload Start sent");
	  return 0 ;
    }
    return -1 ;
}

int Tab102b_UploadEnd()
{
	if( mcu_cmd(0x42, 1, 0) ) {
	  dvr_log("Upload End sent");
	  return 0 ;	// success ;
	}
	return -1 ;		// failed ;
}

int Tab102b_checkContinuousData()
{
  char log[32];
  int nbytes, uploadSize;
  int error = 0;

    writeGforceStatus(log);

	char * rsp = mcu_cmd_target(ID_RF, 0x19);
	if( rsp ) {
	   uploadSize = ((int)rsp[5] << 24) | 
	                ((int)rsp[6] << 16) | 
	                ((int)rsp[7] << 8) |
	                 (int)rsp[8];
	   printf("UPLOAD acked:%d\n", uploadSize);
	   if (uploadSize) {
		writeGforceStatus("load");
		//1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
		// + 8(0g + checksum)
		int bufsize = uploadSize + 1024;
		unsigned char *tbuf = (unsigned char *)malloc(bufsize);
		if( tbuf ) {
      
		nbytes = read_nbytes(tbuf, bufsize,
				    uploadSize + UPLOAD_ACK_SIZE + 8);
		int downloaded = nbytes;
		printf("UPLOAD data:%d(%d)\n", downloaded,uploadSize );
		if (downloaded >= uploadSize + UPLOAD_ACK_SIZE + 8) {
		    if (!verifyChecksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
		      
			writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
				      TYPE_CONTINUOUS);
			Tab102b_sendUploadConfirm();		      			     
		    } else {
			printf("*********checksum error************\n");
			error = 2;
		    }
		} /* else {
		    printf("UPLoad date size is less than it should be\n"); 
		    writeTab102Data(tbuf, downloaded,
				      TYPE_CONTINUOUS);
		}*/
 
		free(tbuf);
		}
	  } 
	}
      

  sprintf(log, "done:%d", error);
  writeGforceStatus(log);

  return error==0; 
  
}

int Tab102b_checkPeakData()
{
  char log[32];
  int nbytes, uploadSize;
  int error = 0;

	writeGforceStatus(log);

	char * rsp = mcu_cmd_target( ID_RF, 0x1f ) ;
	if( rsp ) {
	   uploadSize = ((int)rsp[5] << 24) | 
	                ((int)rsp[6] << 16) | 
	                ((int)rsp[7] << 8) |
	                 (int)rsp[8];
	   printf("UPLOAD acked:%d\n", uploadSize);
	   if (uploadSize) {
		writeGforceStatus("load");
		//1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
		// + 8(0g + checksum)
		int bufsize = uploadSize + 1024;
		unsigned char *tbuf = (unsigned char *)malloc(bufsize);
      
		nbytes = read_nbytes(tbuf, bufsize,
				    uploadSize + UPLOAD_ACK_SIZE + 8);
		int downloaded = nbytes;
		printf("UPLOAD data:%d(%d)\n", downloaded,uploadSize );
		if (downloaded >= uploadSize + UPLOAD_ACK_SIZE + 8) {
		    if (!verifyChecksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
			writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
				      TYPE_PEAK);
		    } else {
			printf("*********checksum error************\n");
			error = 2;
		    }
		}
		Tab102b_sendUploadPeakConfirm();
		free(tbuf);
	  } 
	}

  sprintf(log, "done:%d",  error);
  writeGforceStatus(log);

  return error==0; 
}

int Tab102b_startADC()
{
	if( mcu_cmd_target( ID_RF, 0x08 ) ) {
        writeGforceStatus("ADC Started");
        return 1;
	}
	else {
		return 0 ;
	}
}

int Tab102b_ContinousDownload()
{
    if(Tab102b_UploadStart()){
       dvr_log("Tab102b UploadStart Failed"); 
       return -1;
    }
    if(Tab102b_checkContinuousData()){
      dvr_log("Tab102b CheckContinousData Failed");
      if(Tab102b_UploadEnd()){
	  dvr_log("Tab102b UploadEnd Failed");
      }     
      return -1; 
    }   
    if(Tab102b_UploadEnd()){
       dvr_log("Tab102b UploadEnd Failed");
       return -1;
    }
    return 0;
}

int Tab102b_setup()
{
   int ret=0;
   static struct timeval TimeCur;
   int starttime0,count;
   char tab102b_firmware_version[80];
   //set  RTC for Tab102b

   if(Tab102b_SetRTC()){
      dvr_log("Tab101b set RTC failed");
      return -1;
   }

   //check the continuous data
#if 0   
   if(Tab102b_ContinousDownload()){
      dvr_log("Tab102b Continous Download Failed");
      return -1; 
   }
#endif   
   //get the version of Tab102b firmware
   if( Tab102b_version(tab102b_firmware_version) ) {
	FILE * tab102versionfile=fopen("/var/dvr/tab102version", "w");
	if( tab102versionfile ) {
		fprintf( tab102versionfile, "%s", tab102b_firmware_version );
		fclose( tab102versionfile );
	}
   } else {
      dvr_log("Get Tab101b firmware version failed") ;
      return -1;
   }   
   //set the trigger for Tab102b
   if(Tab102b_setTrigger()){
      dvr_log("Tab101b Trigger Setting Failed");
      return -1;
   }
   gettimeofday(&TimeCur, NULL);
   starttime0=TimeCur.tv_sec;    
   while(1){
	sleep(1);
	gettimeofday(&TimeCur, NULL);
	if((TimeCur.tv_sec-starttime0)>10)
	  break;
   }   
   
   if(Tab102b_enablePeak()){
      dvr_log("Tab101b enablePeak failed") ;
      return -1;
   } 
   
   if(Tab102b_startADC()){
       dvr_log("Tab101b StartADC failed") ;
       return -1;
   }

   return 0;
}
