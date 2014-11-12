#ifndef _IPC_HEADER_H
#define _IPC_HEADER_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/shm.h>

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
union semun {
  int val;
  struct semid_ds *buf;
  unsigned short int *array;
  struct seminfo *__buf;
};
#endif

struct ipc_header {
  unsigned char sender_id;
  unsigned char command;
  unsigned char data_size_HI, data_size_LO; /* command dependant */
  char *data; 
};

/* IOPROCESS --> DVRSVR
 * 1. start recording : IPC_START (DVRSVR send IPC_STATUS when completed)
 * 2. stop recording : IPC_STOP (DVRSVR send IPC_STATUS when completed)
 * 3. reload config : IPC_RESTART
 * 4. DI change : IPC_DICHANGE 
 *                 data: bitmap b31...b0
 * 5. standby : IPC_STANDBY (for hybrid disk) 
 *
 */

/* IOPROCESS --> GLOG
 * 1. start logging : IPC_START (default)
 * 2. stop logging : IPC_STOP 
 * 3. reload config : IPC_RESTART
 * 4. DI change : IPC_DICHANGE 
 *                 data: bitmap b31...b0
 * 5. standby : IPC_STANDBY (for tab101 continuous data) 
 *
 */

/* IOPROCESS --> HTTP
 * 1. temperatures: through shared memory 
 * 2. standby mode: through shared memory 
 */

/* HTTP --> IOPROCESS
 * 1. IPC_TIMESYNC */

/* DVRSVR --> IOPROCESS
 * 1. status / watchdog :
 *        IPC_STATUS (bit 0: recording, bit1: video lost)
 */

/* 
 * GLOG --> DVRSVR
 * 1. gps data :
 *        IPC_GPS
 * struct gps_t {
 *   int valid;  1: valid 
 *   unsigned int fix_time;  epoch time 
 *   double latitude ;
 *   double longitude ;
 *   double speed ;
 *   double direction ;
 * };
 * 2. gforce data change :
 *        IPC_GFORCE 
 *
 * GLOG --> IOPROCESS
 * 1. gps data change :
 *        IPC_GPS
 * 2. DIQUERY :
 *        request current DI on start up.
 * 3. DIQUERY :
 *        request current DI on start up.
 */

enum {
  IPC_UNKNOWN,
  /* action */
  IPC_START,
  IPC_STOP, 
  IPC_RESTART,
  IPC_HDPOWER,
  IPC_DO,
  /* data[0]: output_port; #(0-4), see mcu.h
   * data[1]: on-1, off-0
   */
  /* struct cmd_do {
	   * char output_port; #(0-4)
	   * char ontime; (125ms unit)
	   * char offtime; (125ms unit)
	   * char terminate_flashing_after; (125ms unit, 0:continue)
	   */
  IPC_DOFLASH, /* data[0]: output port #(0-4), see mcu.h
		* data[1]: ontime(125ms unit), data[2]: offtime(125ms unit)
		* data[3]: terminate flashing after(125ms unit), 0:continue  */
  /* status change */
  /* DVRSVR --> IOPROCESS 
   * data[0]: bit 0 - recording 
   */
  IPC_STATUS = 100,
  IPC_DICHANGE,
  IPC_IGNITION, /* IOPROCESS --> GLOG data[0]: 1(ign_on)/0(ign_off) */
  IPC_GPS,
  /* GPS data
   *
   */
  IPC_DIQUERY,
  IPC_TIMESYNC, /* HTTP --> IOPROCESS
		 * data: "struct timeval" on setup page time sync */

  IPC_GFORCE,
  IPC_STANDBY,
  IPC_TAB101_DONE, /* tab 101 continuous data download finished */
  IPC_TIMEDIFF, /* GLOG --> IOPROCESS, data[0..3]: diff */
  IPC_LOG,
  IPC_EVENT, /* used in modem (IOPROCESS --> MODEM) */
  IPC_SYSTEMP, /* used in modem (IOPROCESS --> MODEM) */
  IPC_EV_FILE_READY,
  IPC_END
};

enum {
  ID_UNKNOWN,
  ID_IOPROCESS,
  ID_DVRSVR,
  ID_GLOG,
  ID_GFORCE, /* spare */
  ID_HTTP,
  ID_MODEM
};

#define S_RECORDING  0x01
#define S_VIDEO_LOST 0x02

#define SOCKPATH_IO "/var/run/iosock"
#define SOCKPATH_DVR "/var/run/dvrsock"
#define SOCKPATH_GLOG "/var/run/glogsock"
#define SOCKPATH_GFORCE "/var/run/gforcesock"
#define SOCKPATH_HTTP "/var/run/httpsock"
#define SOCKPATH_MODEM "/var/run/modemsock"

struct shmmem_t {
  int iotemp, hdtemp;
  int io_status; /* bit 0: 1 - running, bit 1: 1 - standby */
};

struct gps_t {
  int valid; /* 1: valid */
  unsigned int fix_time; /* epoch time */  
  double latitude ;
  double longitude ;
  double speed ;
  double direction ; 
};

struct gforce_t {
  float fb, lr, ud;
};



#endif