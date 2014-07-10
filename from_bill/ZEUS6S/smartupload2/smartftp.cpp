#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>
#include <stdarg.h>
#include <signal.h>
#include <net/if.h>
#include <dirent.h>
#include <sys/vfs.h>
#include <pthread.h>
#include "debug.h"
#include "crc.h"
#include "md5.h"
#include "parseconfig.h"
#include "gforce_peak.h"
#include "hikfile.h"

const int16_t MSGID = 0x4d53;
#define MSGVER (1)
#define BLOCK_SIZE (1024 * 1024)
#define MAX_FRAGMENTS 200
#define SMARTLOG_SIZE (1024 * 1024) /* 1 MB */
#define TASKFILE_SIZE (100 * 1024) /* 1 MB */
#define MAX_SOCK_ERROR 3
#define DBGOUT
#define RCV_TIMEOUT 60

char g_ext[4];
char g_busname[60] = "BUS100";
char g_password[60] = "247SECURITY";
int g_dvr_type;
int g_exit_port;
static const char smart_dir[] = "/mnt/ide";
#define SMART_DIR_LEN (sizeof(smart_dir) / sizeof(char) - 1)
/* # of files to upload */
int upload_file_count;
/* total bytes of all files to upload for progress meter */
long long upload_size, upload_done_size;
int event_count;
int64_t event_duration;
int g_videolost;
int g_hdd_error, g_read_error, g_socket_error;
int g_uploadrate = 1000 * 1024;
int upload_thread_end;
pthread_t upload_thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
unsigned int g_watchdog;
int g_upload_timeout;

enum {TYPE_SMART, TYPE_IMPACT, TYPE_PEAK};

enum {DVR_TYPE_DM500, DVR_TYPE_DM510, DVR_TYPE_DM606, DVR_TYPE_DM614};

enum {EXIT_CODE_SUCCESS,               // 0
      EXIT_CODE_BAD_ARGUMENT,          // 1
      EXIT_CODE_NO_DHCP,               // 2
      EXIT_CODE_NO_SERVER,             // 3
      EXIT_CODE_WRONG_FILE_TREE,       // 4
      EXIT_CODE_LOGIN_ERROR,           // 5
      EXIT_CODE_FILE_TRANSFER_TIMEOUT, // 6
      EXIT_CODE_NO_REPLY,              // 7
      EXIT_CODE_NETWORK_ERROR,         // 8
      EXIT_CODE_DVRCURDISK_ERROR,      // 9
      EXIT_CODE_CURLINIT_ERROR,        // 10
      EXIT_CODE_HD_ERROR,              // 11
      EXIT_CODE_UPLOAD_TIMEOUT,        // 12
      EXIT_CODE_BUS_ERROR};            // 500

enum {MSS_DISCOVERY_REQ = 1,
      MSS_LOGIN_REQ, 
      MSS_OPENFILE_REQ, 
      MSS_WRITEFILE_REQ, 
      MSS_CLOSEFILE_REQ, 
      MSS_STATUS_REPORT_REQ, 
      MSS_STATUS_RELAY_REQ, 
      MSS_TASKLIST_REQ,
      MSS_TASKLIST_DATA_REQ,
      MSS_TASKLIST_ACK,
      MSS_FILESIZE_REQ};

enum {MIN_FRAG_SIZE = (10 * 1024 * 1024)};

enum {ACK_NONE, ACK_FAIL, ACK_SUCCESS};
enum {REASON_NONE,
      REASON_UNKNOWN_COMMAND,
      REASON_AUTH_FAIL,
      REASON_LOGIN_WAIT,
      REASON_WRONG_FORMAT,
      REASON_FILE_ACCESS,
      REASON_SERVER_ACCESS,
      REASON_NO_DATA,
      REASON_TIMEOUT};

enum {TASKLIST_NONE,
      TASKLIST_REMOTEMARK,
      TASKLIST_MISSINGFILE,
      TASKLIST_REMOTEUNMARK};

enum {TASKFILE_UNKOWN,
      TASKFILE_DONE,
      TASKFILE_TODO};

struct time_range {
  time_t from, to;
  struct list_head list;
};

struct todo_taskid {
  int type, done;
  char *id;
  struct list_head list;
};

struct missing_fragment {
  char *filename; /* file name without path */
  char *filepath; /* full path without filename
		   * (filled during check_missing_fragment) */
  int from, to;   /* to: 0(continue to end fragment) */
  struct list_head list;
};

struct dirPath {
  int pathId;
  char pathStr[128];
};

struct dirEntry {
  struct dirEntry *next, *prev;
  struct dirPath path;
};

struct fileInfo {
  char ch, month, day, hour, min, sec, type, ext;
  unsigned short year, millisec;
  unsigned int len;
  unsigned int size; /* file size */
  unsigned int prelock;
  int pathId;
};

struct fileEntry {
  struct fileEntry *next, *prev;
  struct fileInfo fi;
};

struct sfileInfo {
  /* type: tab102 or smartlog, lock: L/N */
  char month, day, hour, min, sec, type, lock, reserved;
  unsigned short year, fileno;
  unsigned int size; /* file size */
  int pathId;
};

struct sfileEntry {
  struct sfileEntry *next, *prev;
  struct sfileInfo fi;
};

/* All values in LSB first(little endian: 0x01234567 = 67 45 23 01). */
struct mss_msg
{
  uint16_t id; // magic number: 'M','S' (id[0]='M',id[1]='S')
  uint8_t version; // command set version number (currently v.1)
  uint8_t command; // command ID of request when ack_code == 0
                   // command ID of reply when ack_code > 0
  uint8_t ack_code; // 0 for request, non-zero for replay(ACK_FAIL, ACK_SUCCESS)
  uint8_t reason;   // reason code for fail
  uint16_t wData;   // command specific data
  uint64_t qwData;  // command specific data
  uint32_t dwData;  // command specific data
  uint32_t ext_crc; // crc of external data(ext_datasize)
  uint32_t ext_datasize; // size of external data following this struct
  uint32_t crc;    // crc of this struct excluding this field
};

struct mss_msg_set
{
  struct mss_msg msg;
  char *ext_data;
  int ext_datasize;
};

const short mss_port = 30000;
const char multicast_port[] = "30001";
const char multicast_address[] = "239.255.0.1";

struct fileEntry *file_list;
struct sfileEntry *sfile_list;
struct dirEntry *dir_list;
struct time_range g_rml, g_ruml;
struct peak_event g_pel;
/* list used to save temporary todo task's until it's really done */
struct todo_taskid g_tdl;
struct missing_fragment g_mfl;
struct gforce_trigger_point g_peak_tp = {2.0, -2.0, 2.0, -2.0, 5.0, -3.0};
int g_pre_peak_event = 30, g_post_peak_event = 30;
int g_peak_time_in_utc;
int g_debug;
char g_dbgfile[128], g_taskfile_done[128], g_taskfile_todo[128];

void writeDebug(char *fmt, ...);
int connectTcp(struct in_addr inaddr, short port);
int add_mark_entry_to_list(struct time_range *rml, time_t from, time_t to);


void check_and_add_to_list(char *dir_path, char *servername, char *fullname,
			   struct fileInfo *fi);
			   
static size_t safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret = 0;

  do {
    clearerr(stream);
    ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
  } while (ret < nmemb && ferror(stream) && errno == EINTR);

  return ret;
}

int
safe_fwrite (const void *ptr, size_t size, size_t nmemb, FILE * stream)
{
    size_t ret = 0;

    do
    {
	clearerr (stream);
	ret +=
	    fwrite ((char *) ptr + (ret * size), size, nmemb - ret, stream);
    }
    while (ret < nmemb && ferror (stream) && errno == EINTR);

    return ret;
}

int read_small(char *buf, int size, FILE *fp)
{
  char tbuf[1024];
  int total = 0, bytes;

  while (total < size) {
    size_t to_read = size -  total;
    if (to_read > sizeof(tbuf)) {
      to_read = sizeof(tbuf);
    }
    bytes = safe_fread(tbuf, 1, to_read, fp);
    if (bytes == 0) {
      break;
    }
    memcpy(buf + total, tbuf, bytes);
    total += bytes;
  }

  return total;
}

int create_dir(char *dirname)
{
  if (mkdir(dirname, 0777) == -1) {
    if(errno != EEXIST) {
      perror("mkdir");
      return 1;
    }
  }

  return 0;
}

void reduce_txtfile(char *filename, int new_size)
{
  FILE *fp;
  char line[1024], *buf = NULL;
  int bytes = 0;

  fp = fopen(filename, "r");
  if (fp) {
    int size;
    if (!fseek(fp, 0, SEEK_END)) {
      size = ftell(fp);
      /* keep only new_size data */
      if (size != -1) {
	if (size > new_size) {
	  fseek(fp, size - new_size, SEEK_SET);
	  buf = (char *)malloc(new_size + 4);
	  if (buf) {
	    /* consume 1st partial line */
	    fgets(line, sizeof(line), fp);
	    bytes = read_small(buf, new_size, fp);
	    free(buf);
	  }
	}
      }
    }
    fclose(fp);
  }
 
  if (bytes > 0) {
    fp = fopen(filename, "w");
    if (fp) {
      bytes = safe_fwrite(buf, 1, bytes, fp);
      fclose(fp);
    }
  } 
}

void setDebugFileName(char *rootdir)
{
#ifdef DBGOUT
  if (g_dvr_type == DVR_TYPE_DM500) {
    snprintf(g_dbgfile, sizeof(g_dbgfile),
	     "%s/smartlog",
	     rootdir);
    create_dir(g_dbgfile);
    strcat(g_dbgfile, "/smartftp.txt");
  } else {
    FILE *fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (fp) {
      if (fscanf(fp, "%s", g_dbgfile) == 1) {
	strcat(g_dbgfile, "/smartlog");
	create_dir(g_dbgfile);
	strcat(g_dbgfile, "/smartftp.txt");
      }
      fclose(fp);
    }
  }


  if (g_dbgfile[0]) {
    struct stat sb;
    if (!stat(g_dbgfile, &sb)) {
      /* delete file if too big */
      if (sb.st_size > SMARTLOG_SIZE) {
	reduce_txtfile(g_dbgfile, SMARTLOG_SIZE / 2);
      }
    } 
  }
#endif
}

void setTaskFileName(char *rootdir)
{
  if (g_dvr_type == DVR_TYPE_DM500) {
    snprintf(g_taskfile_done, sizeof(g_taskfile_done),
	     "%s/smartlog",
	     rootdir);
    create_dir(g_taskfile_done);
    strcpy(g_taskfile_todo, g_taskfile_done);
    strcat(g_taskfile_done, "/taskdone.txt");
    strcat(g_taskfile_todo, "/tasktodo.txt");
  } else {
    FILE *fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (fp) {
      if (fscanf(fp, "%s", g_taskfile_done) == 1) {
	strcat(g_taskfile_done, "/smartlog");
	create_dir(g_taskfile_done);
	strcpy(g_taskfile_todo, g_taskfile_done);
	strcat(g_taskfile_done, "/taskdone.txt");
	strcat(g_taskfile_todo, "/tasktodo.txt");
      }
      fclose(fp);
    }
  }


  if (g_taskfile_done[0]) {
    struct stat sb;
    if (!stat(g_taskfile_done, &sb)) {
      /* reduce file if too big */
      if (sb.st_size > TASKFILE_SIZE) {
	reduce_txtfile(g_taskfile_done, TASKFILE_SIZE / 2);
      }
    } 
    if (!stat(g_taskfile_todo, &sb)) {
      /* reduce file if too big */
      if (sb.st_size > TASKFILE_SIZE) {
	reduce_txtfile(g_taskfile_todo, TASKFILE_SIZE / 2);
      }
    } 
  }
}

void add_missing_fragment_to_head(struct missing_fragment *mfl,
				  char *filename, char *filepath,
				  int from, int to)
{
  struct missing_fragment *tmp;
  struct list_head *pos;

  /* check first if a matching filename exists already */
  list_for_each(pos, &mfl->list) {
    tmp = list_entry(pos, struct missing_fragment, list);
    if (!strcmp(tmp->filename, filename)) {
      if (from < tmp->from)
	tmp->from = from;

      if (to == 0) {
	tmp->to = 0;
      }

      if ((tmp->to > 0) && (to > tmp->to))
	tmp->to = to; 

      return;
    }
  }

  tmp = (struct missing_fragment *)malloc(sizeof(struct missing_fragment));
  memset(tmp, 0, sizeof(struct missing_fragment));
  if (filename)
    tmp->filename = strdup(filename);
  if (filepath)
    tmp->filepath = strdup(filepath);
  tmp->from = from;
  tmp->to = to;
  list_add(&tmp->list, &mfl->list);
  fprintf(stderr, "added missing fragment %s(%d,%d) to head\n",
	  filename, from, to);
}

void add_todo_to_head(struct todo_taskid *tdl, char *id, int type)
{
  struct todo_taskid *tmp;
  struct list_head *pos;

  /* check first if a matching entry exists already */
  list_for_each(pos, &tdl->list) {
    tmp = list_entry(pos, struct todo_taskid, list);
    if ((tmp->type == type) && !strcmp(tmp->id, id)) {
      return;
    }
  }

  tmp = (struct todo_taskid *)malloc(sizeof(struct todo_taskid));
  tmp->id = (char *)malloc(strlen(id) + 1);
  strcpy(tmp->id, id);
  tmp->type = type;
  tmp->done = 0;
  list_add(&tmp->list, &tdl->list);
  fprintf(stderr, "added todo item %s(%d) to head\n", id, type);
}

void add_to_taskfile(int record_type, int task_type, char *task, char *id)
{
  FILE *fp;
  char *taskfile = NULL;

  if (record_type == TASKFILE_DONE) {
    taskfile = g_taskfile_done;
  } else if (record_type == TASKFILE_TODO) {
    taskfile = g_taskfile_todo;
  }

  /* it does not matter if we have duplicates here */
  if (taskfile && taskfile[0]) {
    printf("task file:%s\n",taskfile);
    fp = fopen(taskfile, "a");
    if (fp != NULL) {
      if (record_type == TASKFILE_DONE) {
        fprintf(fp, "%d:%s\n", task_type, id);
      } else {
	/* this file has xml data */
        fprintf(fp, "%s\n", task);
      }
      fclose(fp);
    }
  }
}

void remove_from_taskfile_todo(int task_type, char *task_id)
{
  FILE *fp;
  char line[1024], *buf = NULL, *taskfile;
  int size;

  taskfile = g_taskfile_todo;

  fp = fopen(taskfile, "r");
  if (!fp) {
    return;
  }

  if (!fseek(fp, 0, SEEK_END)) {
    size = ftell(fp);
    if (size != -1) {
      fseek(fp, 0, SEEK_SET);
      buf = (char *)malloc(size + 4);
      if (buf) {
	buf[0] = '\0';
	while(fgets(line, sizeof(line), fp)) {
	  int type, ret;
	  char task[1024];
	  ret = sscanf(line, "%d:%s\n", &type, task);
	  if (ret == 2) {
	    if ((task_type != type) || !strstr(task, task_id)) {
	      strcat(buf, line);
	    }
	  }
	}
      }
    }
  }
  fclose(fp);
  
  unlink(taskfile);

  if (buf) {
    if (strlen(buf) > 0) {
      fp = fopen(taskfile, "w");
      if (fp) {
	safe_fwrite(buf, 1, strlen(buf), fp);
	fclose(fp);
      }
    } 
    free(buf);
  }
}

int is_task_done(int task_type, char *task_id)
{
  FILE *fp;
  char line[1024];

  fp = fopen(g_taskfile_done, "r");
  if (fp) {
    while(fgets(line, sizeof(line), fp)) {
      int type, ret;
      char id[1024];
      ret = sscanf(line, "%d:%s\n", &type, id);
      if (ret == 2) {
	if ((task_type == type) && !strcmp(id, task_id)) {
	  return 1;
	}
      }
    }
    fclose(fp);
  }

  return 0;
}  


int cmpFileEntry(struct fileEntry *a, struct fileEntry *b)
{
  int ret;
  struct tm tma, tmb;
  time_t timea, timeb;
  
  tma.tm_year = a->fi.year - 1900;
  tma.tm_mon = a->fi.month - 1;
  tma.tm_mday = a->fi.day;
  tma.tm_hour = a->fi.hour;
  tma.tm_min = a->fi.min;
  tma.tm_sec = a->fi.sec;
  tma.tm_isdst = -1; // let OS decide
  
  tmb.tm_year = b->fi.year - 1900;
  tmb.tm_mon = b->fi.month - 1;
  tmb.tm_mday = b->fi.day;
  tmb.tm_hour = b->fi.hour;
  tmb.tm_min = b->fi.min;
  tmb.tm_sec = b->fi.sec;
  tmb.tm_isdst = -1; // let OS decide
  
  timea = mktime(&tma);
  if (timea == -1) {
	return 0; // what to do?
  }
  timeb = mktime(&tmb);
  if (timeb == -1) {
	return 0; // what to do?
  }
  ret = timea - timeb;
  
  if (!ret) {
    ret = a->fi.millisec - b->fi.millisec;
    if (!ret) {
      ret = a->fi.len - b->fi.len;
      if (!ret) {
	ret = a->fi.type - b->fi.type;
	if (!ret) {
	  ret = a->fi.ch - b->fi.ch; // check ch 1st before ext
	  if (!ret) {
	    ret = a->fi.ext - b->fi.ext;
	  }
	}
      }
    }
  }
  
  return ret;
}

struct fileEntry *listsort(struct fileEntry *list) {
  struct fileEntry *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;
  
  if (!list)
	return NULL;
  
  insize = 1;
  
  while (1) {
	p = list;
	list = NULL;
	tail = NULL;
	
	nmerges = 0;  /* count number of merges we do in this pass */
	
	while (p) {
	  nmerges++;  /* there exists a merge to be done */
	  /* step `insize' places along from p */
	  q = p;
	  psize = 0;
	  for (i = 0; i < insize; i++) {
		psize++;
		q = q->next;
		if (!q) break;
	  }
	  
	  /* if q hasn't fallen off end, we have two lists to merge */
	  qsize = insize;
	  
	  /* now we have two lists; merge them */
	  while (psize > 0 || (qsize > 0 && q)) {
		
		/* decide whether next element of merge comes from p or q */
		if (psize == 0) {
		  /* p is empty; e must come from q. */
		  e = q; q = q->next; qsize--;
		} else if (qsize == 0 || !q) {
		  /* q is empty; e must come from p. */
		  e = p; p = p->next; psize--;
		} else if (cmpFileEntry(p,q) >= 0) { /* latest first */
		  /* First element of p is lower (or same);
		   * e must come from p. */
		  e = p; p = p->next; psize--;
		} else {
		  /* First element of q is lower; e must come from q. */
		  e = q; q = q->next; qsize--;
		}
		
		/* add the next element to the merged list */
		if (tail) {
		  tail->next = e;
		} else {
		  list = e;
		}
		tail = e;
	  }
	  
	  /* now p has stepped `insize' places along, and q has too */
	  p = q;
	}
	tail->next = NULL;
	
	/* If we have done only one merge, we're finished. */
	if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
	  return list;
	
	/* Otherwise repeat, merging lists twice the size */
	insize *= 2;
  }
}

int cmpSfileEntry(struct sfileEntry *a, struct sfileEntry *b)
{
  int ret;
  struct tm tma, tmb;
  time_t timea, timeb;
  
  tma.tm_year = a->fi.year - 1900;
  tma.tm_mon = a->fi.month - 1;
  tma.tm_mday = a->fi.day;
  tma.tm_hour = a->fi.hour;
  tma.tm_min = a->fi.min;
  tma.tm_sec = a->fi.sec;
  tma.tm_isdst = -1; // let OS decide
  
  tmb.tm_year = b->fi.year - 1900;
  tmb.tm_mon = b->fi.month - 1;
  tmb.tm_mday = b->fi.day;
  tmb.tm_hour = b->fi.hour;
  tmb.tm_min = b->fi.min;
  tmb.tm_sec = b->fi.sec;
  tmb.tm_isdst = -1; // let OS decide
  
  timea = mktime(&tma);
  if (timea == -1) {
	return 0; // what to do?
  }
  timeb = mktime(&tmb);
  if (timeb == -1) {
	return 0; // what to do?
  }
  ret = timea - timeb;
  
  if (!ret) {
    ret = a->fi.type - b->fi.type;
    if (!ret) {
      ret = a->fi.fileno - b->fi.fileno;
      if (!ret) {
	ret = a->fi.size - b->fi.size;
      }
    }
  }
  
  return ret;
}

struct sfileEntry *slistsort(struct sfileEntry *list) {
  struct sfileEntry *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;
  
  if (!list)
	return NULL;
  
  insize = 1;
  
  while (1) {
	p = list;
	list = NULL;
	tail = NULL;
	
	nmerges = 0;  /* count number of merges we do in this pass */
	
	while (p) {
	  nmerges++;  /* there exists a merge to be done */
	  /* step `insize' places along from p */
	  q = p;
	  psize = 0;
	  for (i = 0; i < insize; i++) {
		psize++;
		q = q->next;
		if (!q) break;
	  }
	  
	  /* if q hasn't fallen off end, we have two lists to merge */
	  qsize = insize;
	  
	  /* now we have two lists; merge them */
	  while (psize > 0 || (qsize > 0 && q)) {
		
		/* decide whether next element of merge comes from p or q */
		if (psize == 0) {
		  /* p is empty; e must come from q. */
		  e = q; q = q->next; qsize--;
		} else if (qsize == 0 || !q) {
		  /* q is empty; e must come from p. */
		  e = p; p = p->next; psize--;
		} else if (cmpSfileEntry(p,q) >= 0) { /* latest first */
		  /* First element of p is lower (or same);
		   * e must come from p. */
		  e = p; p = p->next; psize--;
		} else {
		  /* First element of q is lower; e must come from q. */
		  e = q; q = q->next; qsize--;
		}
		
		/* add the next element to the merged list */
		if (tail) {
		  tail->next = e;
		} else {
		  list = e;
		}
		tail = e;
	  }
	  
	  /* now p has stepped `insize' places along, and q has too */
	  p = q;
	}
	tail->next = NULL;
	
	/* If we have done only one merge, we're finished. */
	if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
	  return list;
	
	/* Otherwise repeat, merging lists twice the size */
	insize *= 2;
  }
}

void send_exit_code(int code, short port)
{
  int sfd, bi;
  unsigned char txbuf[4];
  struct in_addr addr;

  writeDebug("send_exit_code:%d", code);

  inet_pton(AF_INET, "127.0.0.1", &addr);
  while(1) {
    sfd = connectTcp(addr, port);
    if (sfd != -1) {
      bi = 0;
      txbuf[bi++] = 0x02;
      txbuf[bi++] = 'b';
      txbuf[bi++] = (unsigned char)code;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0;
      txbuf[bi++] = 0x03;
      bi = send(sfd, txbuf, bi, MSG_NOSIGNAL);
      close(sfd);
    }
    sleep(10);
  }
}

void exit_out(int code)
{
  if (g_exit_port) 
    send_exit_code(code, g_exit_port);
  else
    exit(code);
}

inline size_t min(size_t a, size_t b) { return (a < b) ? a : b; }

inline uint16_t GetWBE(void const *_p)
{
  uint8_t *p = (uint8_t *)_p;
  return ( ((uint16_t)p[0] << 8) | 
	   p[1] );
}

inline uint16_t GetWLE(void const *_p)
{
  uint8_t *p = (uint8_t *)_p;
  return ( ((uint16_t)p[1] << 8) | 
	   p[0] );
}

inline uint32_t GetDWLE(void const *_p)
{
  uint8_t *p = (uint8_t *)_p;
  return ( ((uint32_t)p[3] << 24) | 
	   ((uint32_t)p[2] << 16) |
	   ((uint32_t)p[1] <<  8) |
	   p[0] );
}

inline uint64_t GetQWLE(void const *_p)
{
  uint8_t *p = (uint8_t *)_p;
  return ( ((uint64_t)p[7] << 56) | 
	   ((uint64_t)p[6] << 48) |
	   ((uint64_t)p[5] << 40) |
	   ((uint64_t)p[4] << 32) |
	   ((uint64_t)p[3] << 24) | 
	   ((uint64_t)p[2] << 16) |
	   ((uint64_t)p[1] <<  8) |
	   p[0] );
}

inline void SetWBE(uint8_t *p, uint16_t i_w)
{
  p[0] = (i_w >>  8) & 0xff;
  p[1] = (i_w      ) & 0xff;
}

inline void SetWLE(uint8_t *p, uint16_t i_w)
{
  p[1] = (i_w >>  8) & 0xff;
  p[0] = (i_w      ) & 0xff;
}

inline void SetDWLE(uint8_t *p, uint32_t i_dw)
{
  p[3] = (i_dw >> 24) & 0xff;
  p[2] = (i_dw >> 16) & 0xff;
  p[1] = (i_dw >>  8) & 0xff;
  p[0] = (i_dw      ) & 0xff;
}

inline void SetQWLE(uint8_t *p, uint64_t i_qw)
{
  SetDWLE(p, i_qw & 0xffffffff);
  SetDWLE(p + 4, (i_qw >> 32) & 0xffffffff);
}

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int sendall(int s, unsigned char *buf, int *len)
{
  int total = 0;
  int bytesleft = *len;
  int n = 0;

  while (total < *len) {
    n = send(s, buf + total, bytesleft, MSG_NOSIGNAL);
    if (n == -1) {
      writeDebug("send error (err:%d, t:%d, l:%d)", errno, total, bytesleft);
      break;
    }
    total += n;
  }

  *len = total;

  return n == -1 ? -1 : 0;
}

int connectTcp(struct in_addr inaddr, short port)
{
  int sfd;
  struct sockaddr_in destAddr;

  //fprintf(stderr, "connectTcp(%s,%d)\n", addr,port);
  sfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    return -1;

  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(port);
  destAddr.sin_addr = inaddr;
  memset(&(destAddr.sin_zero), '\0', 8);

  if (connect(sfd, (struct sockaddr *)&destAddr,
	      sizeof(struct sockaddr)) == -1) {
    close(sfd);
    return -1;
  }

  return sfd;
}

/* return:
 *   -1: select error
 *    0: timeout
 *    1: data available
 *
 */
static int blockUntilReadable(int socket, struct timeval* timeout) {
  int result = -1;
  do {
    fd_set rd_set;
    FD_ZERO(&rd_set);
    if (socket < 0) break;
    FD_SET((unsigned) socket, &rd_set);
    const unsigned numFds = socket+1;

    /* call select only when no data is available right away */
    unsigned int cbData = 0;
    if (!ioctl(socket, FIONREAD, &cbData) && (cbData > 0)) {
      result = 1;
      break;
    }

    result = select(numFds, &rd_set, NULL, NULL, timeout);
    if (timeout != NULL && result == 0) {
      break; // this is OK - timeout occurred
    } else if (result <= 0) {
#if defined(__WIN32__) || defined(_WIN32)
#else
      if (errno == EINTR || errno == EAGAIN) continue;
#endif
      dprintf("select() error: \n");
      break;
    }
    
    if (!FD_ISSET(socket, &rd_set)) {
      dprintf("select() error - !FD_ISSET\n");
      break;
    }
  } while (0);

  return result;
}

/*
 * read_nbytes:read at least rx_size and as much data as possible with timeout
 * return:
 *    n: # of bytes received
 *   -1: bad size parameters
 */  

int read_nbytes(int fd, unsigned char *buf, int bufsize,
		int rx_size, struct timeval *timeout_tv,
		int showdata, int showprogress)
{
  struct timeval tv;
  int total = 0, bytes;

  if (bufsize < rx_size)
    return -1;

  if (rx_size == 0)
    return 0;

  while (1) {
    bytes = 0;
    tv = *timeout_tv;
    if (blockUntilReadable(fd, &tv) > 0) {
      bytes = read(fd, buf + total, bufsize - total);
      if (bytes > 0) {
	if (showdata)
	  dump(buf + total, bytes);
	total += bytes;
	if (showprogress) dprintf(".");
      }

      if (total >= rx_size)
	break;
    }

    if (bytes <= 0) { // timeout without receiving any data
      break;
    }
  }

  return total;
}

int change_extension(char *filename, char *extension) {
  char *ptr = strrchr(filename, '.');
  if (ptr) {
    ptr++;
    *ptr = '\0';
    strcat(filename, extension);
    return 0;
  }
  return 1;
}

/* 0: success */
static int
get_filename_from_fullpath(char *fullname, char *name, int name_size)
{
  char *nameWithoutPath = strrchr(fullname, '/');
  if (nameWithoutPath) {
    strncpy(name, ++nameWithoutPath, name_size);
  } else {
    strncpy(name, fullname, name_size);
  }

  return (strlen(name) == 0) ? 1 : 0;
}

void create_message(struct mss_msg_set *set, unsigned char type,
		    uint8_t ack_code, uint8_t reason,
		    uint16_t wData, uint32_t dwData, uint64_t qwData,
		    char *data = NULL, int data_size = 0)
{
  uint32_t crc;
  struct mss_msg *msg = &set->msg;
  memset(set, 0, sizeof(struct mss_msg_set));
  SetWBE((uint8_t *)&msg->id, MSGID);
  msg->version = MSGVER;
  msg->command = type;
  msg->ack_code = ack_code;
  msg->reason = reason;
  msg->wData = wData;
  msg->dwData = dwData;
  msg->qwData = qwData;
  if (data && (data_size > 0)) {
    set->ext_data = data;
    set->ext_datasize = data_size;
    SetDWLE((uint8_t *)&msg->ext_datasize, data_size);
    crc = crc32(0, (unsigned char*)data, data_size);
    SetDWLE((uint8_t *)&msg->ext_crc, crc);
  }
  crc = crc32(0, (unsigned char*)msg,
	      sizeof(struct mss_msg) - sizeof(int));
  SetDWLE((uint8_t *)&msg->crc, crc);
}

int is_valid_msg(struct mss_msg *msg, unsigned char type)
{
  if ((GetWBE(&msg->id) == MSGID) && 
      (msg->command == type)) {
    uint32_t crc = crc32(0, (unsigned char *)msg,
			 sizeof(struct mss_msg) - sizeof(int));
    uint32_t crc2 = GetDWLE(&msg->crc);
    //fprintf(stderr, "crc: %08x,%08x\n", crc, crc2);
    if (crc != crc2) {
      fprintf(stderr, "crc error: %d,%d\n", crc, crc2);
      dump((unsigned char*)msg, sizeof(struct mss_msg) - sizeof(int));
      return 0;
    }
    return 1;
  }
  return 0;
}

int discover_mss(struct sockaddr_storage *mss_addr)
{
  int ret, s = -1;
  struct addrinfo hints, *res = NULL, *p;

  /* bind the socket for discovery port */
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  ret = getaddrinfo(NULL, multicast_port, &hints, &res);
  if (ret) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    return 1;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == -1) {
      perror("listener:socket");
      continue;
    }

    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      perror("setsockopt");
    }

    if (bind(s, p->ai_addr, p->ai_addrlen) == -1) {
      perror("listener:bind");
      close(s);
      continue;
    }

    break;
  }

  freeaddrinfo(res);

  if (p == NULL) {
    fprintf(stderr, "failed to bind socket\n");
    return 1;
  }

  /* prepare to send a discovery message */
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  ret = getaddrinfo(multicast_address, multicast_port, &hints, &res);
  if (ret) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    return 1;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == -1) {
      perror("socket");
      continue;
    }
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "failed to open socket\n");
    close(s);
    freeaddrinfo(res);
    return 1;
  }

  int retry = 60, printed = 0;;
  struct mss_msg_set set;
  struct mss_msg *msg;
  unsigned char buf[1024];
  struct timeval tv;
  size_t addr_len;
  int bytes;
  char addr[INET6_ADDRSTRLEN];

  create_message(&set, MSS_DISCOVERY_REQ, 0, 0, 0, 0, 0, NULL, 0);
  //dump((unsigned char *)&set.msg, sizeof(struct mss_msg));

  msg = &set.msg;
  while (retry > 0) {
    void *a;
    char *ipver;
    if (res->ai_family == AF_INET) {
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
      a = &(ipv4->sin_addr);
      ipver = "IPv4";
    } else {
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
      a = &(ipv6->sin6_addr);
      ipver = "IPv6";
    }

    inet_ntop(res->ai_family, a, addr, sizeof addr);
    if (!printed) {
      printed = 1;
      fprintf(stderr, "sending discovery message to %s", addr);
    } else {
      fprintf(stderr, ".");
    }
    ret = sendto(s, msg, sizeof(struct mss_msg), 0, p->ai_addr, p->ai_addrlen);
    if (ret == -1) {
      perror("sendto");
      break;
    }

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (blockUntilReadable(s, &tv) > 0) {
      addr_len = sizeof(struct sockaddr_storage);
      bytes = recvfrom(s, buf, sizeof buf, 0,
		       (struct sockaddr *)mss_addr, &addr_len);
      if (bytes == 0) {
	fprintf(stderr, "connection closed\n"); /* not possible in UDP */
      } else if (bytes == -1) {
	perror("recvfrom");
      } else {
	inet_ntop(mss_addr->ss_family, 
		  get_in_addr((struct sockaddr *)mss_addr),
		  addr, sizeof addr);
	fprintf(stderr, "\ngot message from %s\n", addr);
	if (((size_t)bytes >= sizeof(struct mss_msg)) &&
	    is_valid_msg((struct mss_msg*)buf, MSS_DISCOVERY_REQ)) {
	  break;
	}
	//dump(buf, bytes); 
      }
    }
    
    retry--;
  }
  
  freeaddrinfo(res);
  
  close(s);
  
  if (!retry) {
    fprintf(stderr, "\n");
  }

  return (retry > 0) ? 0 : 1;
}

int connect_to_server(struct sockaddr_storage *mss_addr)
{
  int s;

  // lazy...
  if (mss_addr->ss_family != AF_INET)
    return -1;
  
  struct sockaddr_in *sin = (struct sockaddr_in *)mss_addr;
  s = connectTcp(sin->sin_addr, mss_port);
  if (s < 0) {
    fprintf(stderr, "connection to server failed\n");
    return -1;
  }

  fprintf(stderr, "connected to server\n");

  return s;
}

int send_message(int s, unsigned char type, 
		 uint8_t ack_code, uint8_t reason,
		 uint16_t wData, uint32_t dwData, uint64_t qwData,
		 char *data, int data_size)
{
  struct mss_msg_set set;
  int size;

  create_message(&set, type, ack_code, reason,
		 wData, dwData, qwData, data, data_size);

  size = sizeof(struct mss_msg);
  if(sendall(s, (unsigned char *)&set.msg, &size)){
    writeDebug("send_message send error1.");
    return 1;
  }

  if (data_size) {
    int fixbufsize = 1*1024;
    int sendbufsize = 0;
    int remaindatalen = data_size;
    while (remaindatalen > 0) {
      //writeDebug("%d, %d", remaindatalen, fixbufsize);
      if (remaindatalen > fixbufsize){
	sendbufsize = fixbufsize;
      }else{
	sendbufsize = remaindatalen;
      }
      size = sendbufsize;
      //writeDebug("sending data %d", size);
      if(sendall(s, (unsigned char *)(data+(data_size-remaindatalen)), &size)) {
	writeDebug("send_message send error2.");
	return 1;
      }else{
	//writeDebug("send_message sent %d bytes", size);
	remaindatalen-= sendbufsize;
      }
    }
  }

  return 0;
}


int login_to_mss(int *sd, struct sockaddr_storage *mss_addr)
{
  int s;
  struct mss_msg *reply;
  unsigned char rxbuf[4096];
  struct timeval tv;
  int bytes, ret = -1;

  *sd = -1;

  s = connect_to_server(mss_addr);
  if (s < 0) {
    return -1;
  }

  snprintf((char *)rxbuf, sizeof(rxbuf),
	   "<mssp><user>%s</user><pass>%s</pass></mssp>",
	   g_busname, g_password);

  if (send_message(s, MSS_LOGIN_REQ, 0, 0, 0, 0, 0,
		   (char *)rxbuf, strlen((char *)rxbuf) + 1)) {
    close(s);
    return -1;
  }

  fprintf(stderr, "login req sent to server\n");
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "login reply:%d bytes recved\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes == sizeof(struct mss_msg)) {
    reply = (struct mss_msg *)rxbuf;
    if (is_valid_msg(reply, MSS_LOGIN_REQ)) {
       uint8_t reason = reply->reason;
      if (reply->ack_code == ACK_SUCCESS) {
	fprintf(stderr, "login accepted\n");
	*sd = s;
	ret = 0;
      } else if ((reply->ack_code == ACK_FAIL) &&
		 (reason == REASON_LOGIN_WAIT)) {
	/* waiting time */
	ret = GetDWLE(&reply->dwData);
	fprintf(stderr, "login wait(%d sec) requested\n", ret);
      } else {
	/* login rejected */
	ret = -1;
	fprintf(stderr, "login rejected\n");
      }
    }
  }

  /* leave connection on if login accepted */
  if (ret != 0)
    close(s);

  return ret;
}

/* returns total bytes processed, or -1 on fseek error */
int calculate_md5(MD5_CTX *mdc, FILE *fp, off_t offset)
{
  const int bufsize = 2048;
  unsigned char buf[bufsize];
  int bytes = 0, total = 0;
  int toread;

  if (fseek(fp, offset, SEEK_SET) == -1) {
    perror("fseek");
    return -1;
  }

  MD5Init (mdc);
  while (1) {
    toread = min(bufsize, MIN_FRAG_SIZE - total);
    bytes = read_small((char *)buf, toread, fp);
    if (bytes == 0) {
      break;
    }
    MD5Update (mdc, buf, bytes);
    total += bytes;
  }

  MD5Final (mdc);

  return total;
}

int get_ffiname_from_fullpath(char *ffi_path, char *fullname,
			      char *ffiname, int ffiname_size)
{
  char filename[128];

  if (get_filename_from_fullpath(fullname, filename, sizeof(filename))) {
    fprintf(stderr, "wrong file:%s\n", fullname);
    return 1;
  }

  snprintf(ffiname, ffiname_size, "%s/%s", ffi_path, filename);
  strncat(ffiname, ".ffi",  ffiname_size);

  return 0;
}

int create_ffi(char *ffi_path, char *fullname, size_t filesize, int *fragments)
{
  FILE *fp, *fp2;
  char filename[128], ffiname[256];
  int nfragment, i, j, bytes;
  MD5_CTX mdContext;

  fragments[0] = 0;

  if (get_filename_from_fullpath(fullname, filename, sizeof(filename))) {
    fprintf(stderr, "wrong file:%s\n", fullname);
    return 1;
  }

  snprintf(ffiname, sizeof(ffiname), "%s/%s", ffi_path, filename);
  strncat(ffiname, ".ffi",  sizeof(ffiname));

  fprintf(stderr, "creating ffi:%s\n", ffiname);
  fp = fopen(ffiname, "w");
  if (fp == NULL) {
    fprintf(stderr, "fopen1 %s:%s\n", ffiname, strerror(errno));
    return 1;
  }

  fp2 = fopen(fullname, "r");
  if (fp2 == NULL) {
    fprintf(stderr, "fopen2 %s:%s\n", fullname, strerror(errno));
    fclose(fp);
    return 1;
  }

  nfragment = (filesize / MIN_FRAG_SIZE) + ((filesize % MIN_FRAG_SIZE) ? 1 : 0);
  fprintf(fp, "[global]\n");
  fprintf(fp, "filename=%s\n", filename);
  fprintf(fp, "filesize=%d\n", filesize);
  fprintf(fp, "fragments=%d\n", nfragment);
  fragments[nfragment] = 0; /* mark end */
  for (i = 0; i < nfragment; i++) {
    bytes = calculate_md5(&mdContext, fp2, i * MIN_FRAG_SIZE);
    if (bytes <= 0) {
      fragments[i] = 0; /* mark end */
      //fprintf(stderr, "ffi %d:%d\n", i, fragments[i]);
      break;
    }
    fprintf(fp, "[fragment%d]\n", i + 1);
    fprintf(fp, "size=%d\n", bytes);
    fprintf(fp, "md5sum=");
    for (j = 0; j < 16; j++)
      fprintf(fp, "%02x", mdContext.digest[j]);
    fprintf(fp, "\n");

    fragments[i] = bytes;
    //fprintf(stderr, "ffi %d:%d\n", i, fragments[i]);
  }

  fclose(fp2);
  fclose(fp);

  return 0;
}

int get_data_from_message(int s, unsigned char *rxbuf, int rx_size,
			  char **data, int *data_size)
{
  struct timeval tv;
  struct mss_msg *reply;
  int bytes, ext_datasize;
  unsigned char buf[1024];

  *data_size = 0;
  *data = NULL;

  if (rx_size < (int)sizeof(struct mss_msg))
    return 1;

  reply = (struct mss_msg *)rxbuf;

  /* check if external data came with reply */
  ext_datasize = GetDWLE((uint8_t *)(&reply->ext_datasize));
  if (ext_datasize && data) {
    *data = (char *)malloc(ext_datasize + 1);
    if (*data) {
      /* receive extended data */
      int surplus = rx_size - (int)sizeof(struct mss_msg);
      int to_read = ext_datasize - surplus;
      memcpy(*data, rxbuf + sizeof(struct mss_msg), surplus);
      tv.tv_sec = RCV_TIMEOUT;
      tv.tv_usec = 0;
      bytes = read_nbytes(s, buf, sizeof(buf),
			  to_read, &tv,
			  g_debug, 0);
      if (bytes < to_read) {
	fprintf(stderr, "get_data_from_message error:%d\n", bytes);
	free(*data); *data = NULL;
	return 1;
      }
      memcpy(*data + surplus, buf, to_read);
      (*data)[ext_datasize] = '\0';
      uint32_t crc = crc32(0, (unsigned char*)*data, ext_datasize);
      uint32_t crc2 = GetDWLE(&reply->ext_crc);
      if (crc != crc2) {
	fprintf(stderr, "crc error2: %08x,%08x\n", crc, crc2);
	dump((unsigned char*)*data, ext_datasize);
	free(*data); *data = NULL;
	return 1;
      } else {
	*data_size = ext_datasize;
	return 0;
      }
    }
  }

  return 1;
}

/* parse_xml: get xml value for <name> in xml_line
 *            return: value (should be called by the caller)
 *                    NULL if not found
 *            next: continue next parsing from here
 */           
char *parse_xml(char *xml_line, char *name, char **next)
{
  char *value, *start, *end, *tag, *str_end;

  if (xml_line == NULL)
    return NULL;

  int str_len = strlen(xml_line);
  if (str_len == 0)
    return NULL;

  tag = (char *)malloc(str_len + 4);
  if (tag == NULL)
    return NULL;

  value = NULL;
  str_end = xml_line + str_len;

  sprintf(tag, "<%s>", name);
  start = strstr(xml_line, tag);
  if (start) {
    start += strlen(tag);
    if (start < str_end) {
      sprintf(tag, "</%s>", name);
      end = strstr(start, tag);
      if (end) {
	int value_len = end - start;
	value = (char *)malloc(value_len +1);
	memcpy(value, start, value_len);
	value[value_len] = '\0';
	if (next) {
	  *next = end + strlen(name) + 3;
	}
      }
    }
  }

  free(tag);
  return value;
}

int get_tm_from_string(char *strtime, struct tm *bt)
{
  if (6 == sscanf(strtime,
		  "%04d%02d%02d%02d%02d%02d",
		  &bt->tm_year, &bt->tm_mon, &bt->tm_mday,
		  &bt->tm_hour, &bt->tm_min, &bt->tm_sec)) {
    bt->tm_year -= 1900;
    bt->tm_mon -= 1;
    bt->tm_isdst = -1;

    return 0;
  }

  return 1;
}

int confirm_tasklist_data(int s, int type, char *id)
{
  int bytes;
  struct mss_msg *reply;
  unsigned char rxbuf[4096];
  struct timeval tv;

  snprintf((char *)rxbuf, sizeof(rxbuf),
	   "<mssp><dvrid>%s</dvrid>"
	   "<type>%d</type>"
	   "<id>%s</id></mssp>",
	   g_busname, type, id);

  if (send_message(s, MSS_TASKLIST_ACK, 0, 0, 0, 0, 0,
		   (char *)rxbuf, strlen((char *)rxbuf) + 1)) {
    perror("confirm_tasklist:send");
    return -1;
  }

  fprintf(stderr, "confim task(%d:%s) sent to server\n", type, id);
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "confim task reply:%d\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    fprintf(stderr, "confim task reply error:%d\n", bytes);
    return -1;
  }
    
  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_TASKLIST_ACK)) {
    if (reply->ack_code == ACK_SUCCESS) {
      return s;
    }
  }

  fprintf(stderr, "confim task reply fail:%d\n", reply->reason);
  return -1;
}

/* set id=NULL, when called from cached tasklist_todo */
void do_remote_mark(int s, char *data, char *id, int type)
{
 // printf("===========================================\n");
  char *id_rcvd = parse_xml(data, "id", NULL);
  if (id_rcvd) {
   if (!id || !strcmp(id, id_rcvd)) {
      char *range = parse_xml(data, "range", NULL);
      if (range) {
	char *start = range;
	while (1) {
	  char *next, *rm_start, *rm_end;
	  struct tm tm_start, tm_end;
	  char *item = parse_xml(start, "rangeitem", &next);
	  if (item == NULL)
	    break;
	  fprintf(stderr, "rangeitem:%s\n", item);
	  rm_start = parse_xml(item, "start", NULL);
	  if (rm_start) {
	    rm_end = parse_xml(item, "end", NULL);
	    if (rm_end) {
	      fprintf(stderr, "start:%s,end:%s\n", rm_start,rm_end);
	      if (!get_tm_from_string(rm_start, &tm_start) &&
		  !get_tm_from_string(rm_end, &tm_end)) {
		/* add to temporay list
		 * remove_from_taskfile_todo after files are marked */
		//printf("add_todo_to_head=\n");
		add_todo_to_head(&g_tdl, id_rcvd, type);
		/* don't add when reading taskfile_todo */
		if (id) {
		 //  printf("add_to_taskfile==\n");
		  add_to_taskfile(TASKFILE_TODO, type, data, NULL);
		}
		//printf("add_mark_entry_to_list===\n");
		//printf("start:%d:%d:%d=***\n",tm_start.tm_hour,tm_start.tm_min,tm_start.tm_sec);
		add_mark_entry_to_list((type == TASKLIST_REMOTEMARK) ?
				       &g_rml : &g_ruml,
				       timegm(&tm_start), timegm(&tm_end));
		//printf("confirm_tasklist_data====\n");
		confirm_tasklist_data(s, type, id_rcvd);
	      }
	      free(rm_end);
	    }
	    free(rm_start);
	  }	    
	  free(item);
	  start = next;
	}
	free(range);
      } /* range */
    }
    free(id_rcvd);
  } /* id_rcvd */
}

/* set id=NULL, when called from cached tasklist_todo */
void do_missing_fragment(int s, char *data, char *id)
{
  char *id_rcvd = parse_xml(data, "id", NULL);
  if (id_rcvd) {
    if (!id || !strcmp(id, id_rcvd)) {
      char *filename = parse_xml(data, "filename", NULL);
      if (filename) {
	char *fragment = parse_xml(data, "fragment", NULL);
	if (fragment) {
	  /* add to temporay list, 
	   * remove_from_taskfile_todo after files are uploaded */
	  add_todo_to_head(&g_tdl, id_rcvd, TASKLIST_MISSINGFILE);
	  /* don't add when reading taskfile_todo */
	  if (id) {
	    add_to_taskfile(TASKFILE_TODO,
			    TASKLIST_MISSINGFILE, data, NULL);
	  }
	  add_missing_fragment_to_head(&g_mfl, filename, NULL,
				       atoi(fragment), atoi(fragment));
	  confirm_tasklist_data(s, TASKLIST_MISSINGFILE, id_rcvd);
	  free(fragment);
	}
	free(filename);
      } /* filename */
    }
    free(id_rcvd);
  } /* id_rcvd */
}

int get_tasklist_data(int s, char *id, int type)
{
  int bytes;
  struct mss_msg *reply;
  unsigned char rxbuf[4096];
  char *data;
  int data_size;
  struct timeval tv;

  if (s < 0) {
    return -1;
  }

  snprintf((char *)rxbuf, sizeof(rxbuf),
	   "<mssp><dvrid>%s</dvrid><type>%d</type><id>%s</id></mssp>",
	   g_busname, type, id);

  if (send_message(s, MSS_TASKLIST_DATA_REQ, 0, 0, 0, 0, 0,
		   (char *)rxbuf, strlen((char *)rxbuf) + 1)) {
    return -1;
  }

  fprintf(stderr, "tasklist data req sent to server:%d\n", type);
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "tasklist data reply:%d bytes recvd\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    writeDebug("tasklist data reply error:%d", bytes);
    return -1;
  }
    
  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_TASKLIST_DATA_REQ)) {
    if (reply->ack_code == ACK_FAIL) {
      writeDebug("tasklist data reply fail");
      return -1;
    }

    if (get_data_from_message(s, rxbuf, bytes, &data, &data_size)) {
      return -1;
    }
    fprintf(stderr, "data:%s\n", data);
    if (type == TASKLIST_REMOTEMARK || type == TASKLIST_REMOTEUNMARK) {
      do_remote_mark(s, data, id, type);
    } else if (type == TASKLIST_MISSINGFILE) {
      do_missing_fragment(s, data, id);
    }
    free(data);
  }

  return s;
}

int get_tasklist(int s)
{
  int bytes;
  struct mss_msg *reply;
  unsigned char rxbuf[4096];
  char *tasklist, *item, *start, *type, *id;
  int tasklist_size;
  struct timeval tv;

  if (s < 0) {
    return -1;
  }

  snprintf((char *)rxbuf, sizeof(rxbuf),
	   "<dvrid>%s</dvrid>",
	   g_busname);

  if (send_message(s, MSS_TASKLIST_REQ, 0, 0, 0, 0, 0,
		   (char *)rxbuf, strlen((char *)rxbuf) + 1)) {
    return -1;
  }

  fprintf(stderr, "tasklist req sent to server\n");
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "tasklist reply:%d bytes received\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    writeDebug("tasklist reply error:%d", bytes);
    return -1;
  }
    
  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_TASKLIST_REQ)) {
    if (reply->ack_code == ACK_FAIL) {
      writeDebug("tasklist reply fail");
      return -1;
    }

    if (get_data_from_message(s, rxbuf, bytes, &tasklist, &tasklist_size)) {
      return -1;
    }
    fprintf(stderr, "tasklist:%s\n", tasklist);
    start = tasklist;
    while (1) {
      char *next;
      item = parse_xml(start, "tasklistitem", &next);
      if (item == NULL)
	break;
      fprintf(stderr, "tasklistitem:%s\n", item);
      type = parse_xml(item, "type", NULL);
      if (type) {
	int task_type = atoi(type);
	id = parse_xml(item, "id", NULL);
	if (id) {
	  if (!is_task_done(task_type, id)) {
	    get_tasklist_data(s, id, task_type);
	  } else {
	    /* mess up in SS, just confirm it */
	    fprintf(stderr, "task already done\n");
	    confirm_tasklist_data(s, task_type, id);
	  }
	  free(id);
	}
	free(type);
      }

      free(item);
      start = next;
    }
    free(tasklist);
  }

  return s;
}

int send_status_report(int s)
{
  int bytes;
  struct mss_msg *reply;
  unsigned char rxbuf[4096];
  struct timeval tv;
  time_t now;
  struct tm tm;

  now = time(NULL);
  gmtime_r(&now, &tm);

  if (s < 0) {
    return -1;
  }

  snprintf((char *)rxbuf, sizeof(rxbuf),
	   "<mssp><dvrid>%s</dvrid>"
	   "<dvrtime>%04d%02d%02d%02d%02d%02d</dvrtime>"
	   "<hddfail>%d</hddfail>"
	   "<videolost>%d</videolost>"
	   "<eventcount>%d</eventcount>"
	   "<eventduration>%lld</eventduration></mssp>",
	   g_busname,
	   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	   tm.tm_hour, tm.tm_min, tm.tm_sec,
	   g_hdd_error > 5 ? 1 : 0, g_videolost, 
	   event_count, event_duration);

  if (send_message(s, MSS_STATUS_REPORT_REQ, 0, 0, 0, 0, 0,
		   (char *)rxbuf, strlen((char *)rxbuf) + 1)) {
    perror("get_tasklist:send");
    return -1;
  }

  fprintf(stderr, "status report sent to server\n");
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "status report reply:%d\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    fprintf(stderr, "status report reply error:%d\n", bytes);
    return -2;
  }
    
  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_STATUS_REPORT_REQ)) {
    if (reply->ack_code == ACK_SUCCESS) {
      return 1;
    } else {
      if (reply->reason == REASON_AUTH_FAIL) {
	return 0;
      }
    }
  }

  fprintf(stderr, "status report reply fail:%d\n", reply->reason);
  return -2;
}

int64_t remote_size(int s, char *filename)
{
  int bytes;
  struct mss_msg *reply;
  struct timeval tv;
  unsigned char rxbuf[4096];

  if (s < 0) {
    return -1;
  }

  snprintf((char *)rxbuf, sizeof(rxbuf),
	   "<mssp><dvrid>%s</dvrid><filename>%s</filename></mssp>",
	   g_busname, filename);

  if (send_message(s, MSS_FILESIZE_REQ, 0, 0, 0, 0, 0,
		   (char *)rxbuf, strlen((char *)rxbuf) + 1)) {
    return -1;
  }

  fprintf(stderr, "filesize(%s) req sent to server\n", filename);
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "filesize reply:%d\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    fprintf(stderr, "filesize reply error:%d\n", bytes);
    return -1;
  }

  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_FILESIZE_REQ)) {
    int64_t size = GetQWLE(&reply->qwData);
    fprintf(stderr, "filesize(%d,%d):%lld\n",
	    reply->ack_code, reply->reason, size);
    if (reply->ack_code == ACK_SUCCESS) {
      return size;
    }
  }

  return -1;
}

int remote_open(int s, char *filename)
{
  int bytes;
  struct mss_msg *reply;
  struct timeval tv;
  unsigned char rxbuf[4096];

  if (s < 0) {
    return -1;
  }

  snprintf((char *)rxbuf, sizeof(rxbuf),
	   "<mssp><dvrid>%s</dvrid><filename>%s</filename></mssp>",
	   g_busname, filename);

  if (send_message(s, MSS_OPENFILE_REQ, 0, 0, 0, 0, 0,
		   (char *)rxbuf, strlen((char *)rxbuf) + 1)) {
    return -1;
  }

  fprintf(stderr, "openfile(%s) req sent to server\n", filename);
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "openfile reply:%d\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    writeDebug("openfile reply error:%d", bytes);
    g_socket_error++;
    //close(s);
    return -1;
  }

  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_OPENFILE_REQ)) {
    if (reply->ack_code == ACK_SUCCESS) {
      return s;
    } else if (reply->reason == REASON_TIMEOUT) {
      writeDebug("upload timeout");
      g_upload_timeout = 1;
    }
  }

  return -1;
}

/*
 * offset: offset in the target
 */
int remote_write(int s, uint8_t *data, int datasize, off_t offset)
{
  int bytes;
  struct mss_msg *reply;
  struct timeval tv;
  unsigned char rxbuf[4096];

  if (send_message(s, MSS_WRITEFILE_REQ, 0, 0, 0, 0, offset,
		   (char *)data, datasize)) {
    return -1;
  }

  fprintf(stderr, "writefile(%d) req sent to server\n", datasize);

  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "writefile reply:%d\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    writeDebug("writefile reply error:%d", bytes);
    g_socket_error++;
    return -1;
  }

  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_WRITEFILE_REQ)) {
    if (reply->ack_code == ACK_SUCCESS) {
      return 0;
    }
    writeDebug("writefile error:%d", reply->reason);
  }

  writeDebug("writefile failure");

  return -1;
}

void remote_close(int s)
{
  int bytes;
  struct mss_msg *reply;
  struct timeval tv;
  unsigned char rxbuf[4096];

  if (send_message(s, MSS_CLOSEFILE_REQ, 0, 0, 0, 0, 0,
		   NULL, 0)) {
    return;
  }

  fprintf(stderr, "closefile req sent to server\n");
  tv.tv_sec = RCV_TIMEOUT;
  tv.tv_usec = 0;
  bytes = read_nbytes(s, rxbuf, sizeof(rxbuf),
		      sizeof(struct mss_msg), &tv,
		      g_debug, 0);
  fprintf(stderr, "closefile reply:%d\n", bytes);
  //dump(rxbuf, bytes); 
  if (bytes < (int)sizeof(struct mss_msg)) {
    fprintf(stderr, "closefile reply error:%d\n", bytes);
    g_socket_error++;
    //close(s);
    return;
  }

  reply = (struct mss_msg *)rxbuf;
  if (is_valid_msg(reply, MSS_CLOSEFILE_REQ)) {
    if (reply->ack_code == ACK_SUCCESS) {
      return;
    }
  }

  fprintf(stderr, "closefile failure\n");
}

/* res = x - y
 * returns 1 if the difference is negative, otherwise 0
 */
int timeval_subtract(struct timeval *res, struct timeval *x, struct timeval *y)
{
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }

  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  res->tv_sec = x->tv_sec - y->tv_sec;
  res->tv_usec = x->tv_usec - y->tv_usec;

  return x->tv_sec < y->tv_sec;
}

static void safe_sleep(unsigned int usec) {
  struct timespec delayTs, remTs;

  delayTs.tv_sec = usec / 1000000;
  delayTs.tv_nsec = (usec % 1000000) * 1000;

  while ((nanosleep(&delayTs, &remTs) == -1) && (errno ==  EINTR)) {
    delayTs = remTs;
  }    
}

/*
 * from_offset: from offset in the local file
 * to_offset: to offset in the local file (0 means to EOF)
 */
int send_file(int sd, char *fullname,
	      off_t from_offset, off_t to_offset,
	      char *target_name)
{
  int s;
  int bytes, ret = 1, to_read;
  char filename[128];
  FILE *fp;
  off_t cur_offset, target_offset;
  char readbuf[BLOCK_SIZE];
  size_t totalcopied = 0;

  if (get_filename_from_fullpath(fullname, filename, sizeof(filename))) {
    writeDebug("send_file:wrong file(%s)", fullname);
    return 1;
  }

  //writeDebug("send_file:%s(%ld, %ld)", filename, from_offset, to_offset);

  fp = fopen(fullname, "r");
  if (fp == NULL) {
    writeDebug("send_file:fopen(%s)", strerror(errno));
    return 1;
  }

  if (fseek(fp, from_offset, SEEK_SET)) {
    writeDebug("send_file:fseek(%s)", strerror(errno));
    fclose(fp);
    return 1;
  }

  char *fname = target_name ? target_name : filename;
  s = remote_open(sd, fname);
  if (s < 0) {
    writeDebug("send_file:remote_open(%s)", fname);
    fclose(fp);
    return 1;
  }

  struct timeval start_time;
  gettimeofday(&start_time, NULL);
  /* target speed: 1MB/sec -> 1B/1usec */
  while (1) {
    cur_offset = ftell(fp);
    target_offset = cur_offset - from_offset;

    if (totalcopied && g_uploadrate) {
      struct timeval now, diff;
      gettimeofday(&now, NULL);
      if (!timeval_subtract(&diff, &now, &start_time)) {
	unsigned int diff_in_usec = diff.tv_sec * 1000000 + diff.tv_usec;
	//writeDebug("diff_in_usec(%d)", diff_in_usec);
	double usec_per_byte = 1000000.0 / g_uploadrate;
	unsigned int usec_for_bytes = 
	  (unsigned int)(usec_per_byte * totalcopied);

	if (diff_in_usec < usec_for_bytes) {
	  writeDebug("upload speed control(%d)",
		     usec_for_bytes - diff_in_usec);
	  safe_sleep(usec_for_bytes - diff_in_usec);
	}
      }
    }

    /* how many bytes to read? */
    to_read = to_offset ? (to_offset - cur_offset) : BLOCK_SIZE;
    if (to_read > BLOCK_SIZE)
      to_read = BLOCK_SIZE;

    //writeDebug("reading");
    bytes = read_small(readbuf, to_read, fp);
    fprintf(stderr, "read:%d\n", bytes);
    if (ferror(fp)) {
      g_read_error++;
      writeDebug("readfile fail:%d", g_read_error);
      break;
    }

    if (remote_write(s, (unsigned char *)readbuf, bytes, target_offset)) {
      writeDebug("remotewrite file fail");
      break;
    }else{
      totalcopied += bytes;
    }
    g_watchdog = 0;

    if (feof(fp) ||
	(to_offset && ((cur_offset + bytes) >= to_offset))) {
      //writeDebug("EOF");
      ret = 0;
      break;
    }
  }

  remote_close(s);
  fclose(fp);

  return ret;
}

static int check_servername(char *filename, char *servername,
			    char type, unsigned int *prelock)
{
  char *ptr, *afterType, str[128];
  int ret, duration, len;

  *prelock = 0; /* used only in 510 */

  sprintf(str, "_%c_", type);

  /* look for _L_ or _N_, etc */
  ptr = strstr(filename, str);
  if (!ptr)
    return 1; /* shouldn't happen */

  /* check for servername */
  len = strlen(servername);
  afterType = ptr + 3;
  if (!strncmp(afterType, servername, len)) {
    /* servername right after TYPE */
    return 0;
  }

  /* look for preloc duration for mDVR51xx (_L_150_busname) */
  ret = sscanf(afterType, "%d_%s", &duration, str);
  if ((ret == 2) && !strncmp(str, servername, len)) {
    *prelock = duration;
    return 0;
  }

  return 1;
}

static int
get_fileinfo_from_name(struct fileInfo *fi, char *filename, char *servername)
{
  int ret;

  fi->size = 0;
  fi->pathId = 0;

  /* get the file extension */
  char *ptr = strrchr(filename, '.');
  if (!ptr) {
    writeDebug("wrong filename:%s", filename);
    return 1;
  }
  fi->ext = *(ptr + 1); /* 1st character of the extension */

  fi->millisec = 0;
  if (g_dvr_type == DVR_TYPE_DM500) {
    ret = sscanf(filename,
		 "CH%02hhd_%04hd%02hhd%02hhd"
		 "%02hhd%02hhd%02hhd%03hd_%u_%c_",
		 &fi->ch, &fi->year, &fi->month, &fi->day, &fi->hour, &fi->min,
		 &fi->sec, &fi->millisec, &fi->len, &fi->type);
  } else {
    ret = sscanf(filename,
		 "CH%02hhd_%04hd%02hhd%02hhd"
		 "%02hhd%02hhd%02hhd_%u_%c_",
		 &fi->ch, &fi->year, &fi->month, &fi->day, &fi->hour, &fi->min,
		 &fi->sec, &fi->len, &fi->type);
  }
  
  if (((g_dvr_type == DVR_TYPE_DM500) && (ret == 10)) ||
      ((g_dvr_type != DVR_TYPE_DM500) && (ret == 9))) {
    if (!check_servername(filename, servername, fi->type, &fi->prelock))
      return 0;
  }

  return 1;
}

static int
get_fileinfo_from_fullname(struct fileInfo *fi, char *fullname,
			   char *servername)
{
  char filename[128];
  if (!get_filename_from_fullpath(fullname, filename, sizeof(filename))) {
    return get_fileinfo_from_name(fi, filename, servername);
  }

  return 1;
}

int read_resume_info(char *dirname, char *servername)
{
  FILE *fp;
  char path[256], relative_path[128], filename[128];
  int ret, from, to;
  
  snprintf(path, sizeof(path),
	   "%s/resumeinfo",
	   dirname);
  fp = fopen(path, "r");
  if (fp) {
    ret = fscanf(fp, "%d,%d,%s", &from, &to, relative_path);
    if (ret == 3) {
      writeDebug("resume info:%d,%d,%s", from, to, relative_path); 
      if (!get_filename_from_fullpath(relative_path,
				      filename, sizeof(filename))) {
	/* let the path filled during check_missing_file */
	add_missing_fragment_to_head(&g_mfl, filename, NULL, from, to); 
      }
    }
    fclose(fp);

    return 0;
  }

  return 1;
}

 void delete_resume_info(char *dirname)
{
  char path[256];

  snprintf(path, sizeof(path),
	   "%s/resumeinfo",
	   dirname);
  unlink(path);
}

/*
 * Params
 *  fullname: path + filename
 *  path_to_resumeinfo : path to resumeinfo file
 * Return  1: parameter error
 *        -1: disk error
 *         0: success
 */
int write_resume_info(char *fullname, 
		      int from_fragment, int to_fragment,
		      char *path_to_resumeinfo)
{
  FILE *fp;
  char path[256], filename[256];

  if (!fullname || !path_to_resumeinfo)
    return 1;

  /* get relative path from path_to_resumeinfo */
  char *ptr = fullname;
  int len = strlen(path_to_resumeinfo);
  if (!strncmp(fullname, path_to_resumeinfo, len)) {
    ptr = fullname + len;
    if (*ptr == '/') {
      ptr++;
    }
  }
  strcpy(filename, ptr);

  snprintf(path, sizeof(path),
	   "%s/resumeinfo",
	   path_to_resumeinfo);
  fp = fopen(path, "w");
  if (fp) {
    /* format: from,to,path
     * from_fragment no (1,2,..),
     * to_fragment no,
     * relative_path_to_filename_from_this_file */
    if ((to_fragment != 0) && (to_fragment < from_fragment)) {
      to_fragment = from_fragment;
    }
    fprintf(fp, "%d,%d,%s\n", from_fragment, to_fragment, filename);
    fclose(fp);
    return 0;
  }

  return -1;
}

int upload_file(int sd, struct sockaddr_storage *mss_addr,
		char *ffi_path, char *fullname,
		int64_t *uploadsize = NULL, int check_filesize = 0,
		int start_from_fragment = 0, int to_fragment = 0)
{
  struct stat st;
  char filename[128], filename2[136], ffiname[256], extension[8];
  int fragments[MAX_FRAGMENTS];
  off_t uploaded_size = 0, filesize;
  int i, ret = 0;

  writeDebug("uploading %s", fullname);

  if (uploadsize) *uploadsize = 0;

  if (stat(fullname, &st)) {
    writeDebug("error stat(%s):%s", fullname, strerror(errno));
    return 1;
  }

  if (!S_ISREG(st.st_mode) || !st.st_size) {
    writeDebug("stat error");
    return 1;
  }

  if (get_filename_from_fullpath(fullname, filename, sizeof(filename))) {
    writeDebug("wrong file:%s", fullname);
    return 1;
  }

  if (check_filesize) {
    filesize = remote_size(sd, filename);
    if (filesize == st.st_size) {
      writeDebug("file exists already:%ld", filesize);
      return 0;
    }
  }

  /* (file > frag_size)?
   * then create file fragment info (filename.ffi) */
  if (st.st_size > MIN_FRAG_SIZE) {
    create_ffi(ffi_path, fullname, st.st_size, fragments);
    /* upload ffi */
    if (get_ffiname_from_fullpath(ffi_path, fullname,
				  ffiname, sizeof(ffiname))) {
      writeDebug("ffiname error");
      return 1;
    }
    if (send_file(sd, ffiname, 0, 0, NULL)) {
      writeDebug("send_file error");
      return 1;;
    }

    /* upload fragments */
    for (i = 0; fragments[i] > 0; i++) { 
      if ((i + 1) < start_from_fragment) {
	uploaded_size += fragments[i];
	continue;
      }
      sprintf(extension, ".%04d", i + 1);
      strcpy(filename2, filename);
      strncat(filename2, extension,  sizeof(filename2));
      write_resume_info(fullname, i + 1, to_fragment, ffi_path);
      if (send_file(sd, fullname,
		    uploaded_size, uploaded_size + fragments[i],
		    filename2)) {
	writeDebug("send_file error2");
	ret = 1;
	break;
      }
      uploaded_size += fragments[i];
      if (to_fragment > 0) {
	if ((i + 1) >= to_fragment)
	  break;
      }
    }

    /* delete resume info & ffi if all fragments are successfully uploaded */
    if (ret == 0) {
      delete_resume_info(ffi_path);
      unlink(ffiname);
    }
  } else {
    if (send_file(sd, fullname, 0, 0, NULL)) {
      writeDebug("send_file error3");
      ret = 1;
    } else {
      uploaded_size = st.st_size;
    }
  }

  if (uploadsize) *uploadsize = uploaded_size;

  return ret;
}

void writeDebug(char *fmt, ...)
{
  va_list vl;
  char str[256];

  va_start(vl, fmt);
  vsprintf(str, fmt, vl);
  va_end(vl);

  fprintf(stderr, "%s\n", str);

#ifdef DBGOUT
  char ts[64];
  time_t t = time(NULL);
  ctime_r(&t, ts);
  ts[strlen(ts) - 1] = '\0';

  FILE *fp;
  if (g_dbgfile[0]) {
    fp = fopen(g_dbgfile, "a");
    if (fp != NULL) {
      if (fprintf(fp, "%s : %s\n", ts, str) < 0) {
	g_hdd_error++;
      }
      if (fclose(fp)) {
	g_hdd_error++;
      }
    } else {
      g_hdd_error++;
    }
  }
#endif
}

void buserror_handler(int signum)
{
  writeDebug("Bus Error");

  exit(EXIT_CODE_BUS_ERROR);
}

void setTZ(char *tzstr)
{
  FILE * fp;
  char buf[128];

  if (tzstr == NULL) {
    // set TZ environment
    if (g_dvr_type == DVR_TYPE_DM500) {
      fp = fopen("/var/run/TZ", "r");
      if (fp != NULL) {
	if (fscanf(fp, "TZ=%s", buf) ==  1) {
	  fprintf(stderr, "TZ:%s\n", buf);
	  setenv("TZ", buf, 1);
	}
	fclose(fp);
      }
    } else {
      fp = fopen("/var/dvr/TZ", "r");
      if (fp != NULL) {
	if (fscanf(fp, "%s", buf) ==  1) {
	  fprintf(stderr, "TZ:%s\n", buf);
	  setenv("TZ", buf, 1);
	}
	fclose(fp);
      }
    }
  } else {
    fprintf(stderr, "TZ:%s\n", tzstr);
    setenv("TZ", tzstr, 1);
  }
}

int videoToK(char *filename) {
  char *ptr = strrchr(filename, '.');
  if (ptr) {
    ptr++;
    if ((*ptr == 'm') || (*ptr == 'M') || (*ptr == '2')) {
      *ptr = 'k';
      ptr++;
      *ptr = '\0';
      return 0;
    }
  }
  return 1;
}

int kToVideo(char *filename, char *extension) {
  char *ptr = strrchr(filename, '.');
  if (ptr) {
    ptr++;
    if ((*ptr == 'k') || (*ptr == 'K')) {
      *ptr = '\0';
      strcat(filename, extension);
      return 0;
    }
  }
  return 1;
}

void read_config()
{
  g_peak_time_in_utc = 0;
  if (g_dvr_type != DVR_TYPE_DM500) {
    char *p;
    if (!cfg_parse_file("/etc/dvr/dvr.conf")) {
#if 0 
      /* now, peak/crash data time stamp is converted to local time */
      p = cfg_get_str("glog", "gforce_log_enable");
      if (p) {
	/* 602 internal gforce has UTC time */
	g_peak_time_in_utc = atoi(p);
	fprintf(stderr, "gforce enable:%d\n", g_peak_time_in_utc);
      }
#endif
      p = cfg_get_str("system", "mss_password");
      if (p) {
	strncpy(g_password, p, sizeof(g_password));
      }

      p = cfg_get_str("io", "gsensor_forward_event");
      if (p) {
	g_peak_tp.fb_pos = atof(p);
      }
      p = cfg_get_str("io", "gsensor_backward_event");
      if (p) {
	g_peak_tp.fb_neg = atof(p);
      }
      p = cfg_get_str("io", "gsensor_right_event");
      if (p) {
	g_peak_tp.lr_pos = atof(p);
      }
      p = cfg_get_str("io", "gsensor_left_event");
      if (p) {
	g_peak_tp.lr_neg = atof(p);
      }
      p = cfg_get_str("io", "gsensor_down_event");
      if (p) {
	g_peak_tp.ud_pos = atof(p);
      }
      p = cfg_get_str("io", "gsensor_up_event");
      if (p) {
	g_peak_tp.ud_neg = atof(p);
      }

      p = cfg_get_str("io", "gsensor_pre_event");
      if (p) {
	g_pre_peak_event = atoi(p);
      }
      p = cfg_get_str("io", "gsensor_post_event");
      if (p) {
	g_post_peak_event = atoi(p);
      }
    }
  }
}

int get_wifi_ip(const char *dev,
		struct in_addr *ip,
		struct in_addr *netmask,
		struct in_addr *broadcast)
{
  int skfd;
  struct ifreq ifr;

  ip->s_addr = 0;
  netmask->s_addr = 0;
  broadcast->s_addr = 0;

  skfd = socket(AF_INET, SOCK_DGRAM, 0);

  /* specify the device name */
  strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

  /* get IPv4 IP address */
  ifr.ifr_addr.sa_family = AF_INET;

  if (ioctl(skfd, SIOCGIFADDR, &ifr) == 0) {
    *ip = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
    fprintf(stderr, "ip:%s\n", inet_ntoa(*ip));

    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    if (ioctl(skfd, SIOCGIFNETMASK, &ifr) == 0) {   
      *netmask = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
      fprintf(stderr, "netmask:%s\n", inet_ntoa(*netmask));
    
      strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
      if (ioctl(skfd, SIOCGIFBRDADDR, &ifr) == 0) {	
	*broadcast = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	fprintf(stderr, "broadcast:%s\n", inet_ntoa(*broadcast));
      }
    }
  }
  close(skfd);

  return 0;
}

/* returns path_id in the list(-1 on error),
 * add item if not found
 */
static int get_path_list_id(char *path)
{
  int highestId = -1;
  struct dirEntry *node;

  node = dir_list;
  while (node) {
    if (!strncmp(node->path.pathStr, path, sizeof(node->path.pathStr))) {
      return node->path.pathId;
    }

    if (node->path.pathId > highestId) {
      highestId = node->path.pathId;
    }

    node = node->next;
  }

  /* not found in the list, add one */
  node = (struct dirEntry *)malloc(sizeof(struct dirEntry));
  if (node) {
    node->path.pathId = highestId + 1;
    strncpy(node->path.pathStr, path, sizeof(node->path.pathStr));
     /* add to the list */
    node->next = dir_list;
    dir_list = node;
    //writeDebug("dir %d,%s added", node->path.pathId, node->path.pathStr);
    return node->path.pathId;
  }

  return -1;
}

/* returns path in the list(NULL if not found),
 */
static char *get_path_for_list_id(int id)
{
  struct dirEntry *node;

  node = dir_list;
  while (node) {
    if (node->path.pathId == id) {
      return node->path.pathStr;
    }

    node = node->next;
  }

  return NULL;
}

void add_time_range_to_head(struct time_range *rml, time_t start, time_t end)
{
  struct time_range *tmp;
  tmp = (struct time_range *)malloc(sizeof(struct time_range));
  tmp->from = start;
  tmp->to = end;
  list_add(&tmp->list, &rml->list);
  fprintf(stderr, "added rm item %ld,%ld to head\n", start, end);
}

void add_time_range_to_tail(struct time_range *rml, time_t start, time_t end)
{
  struct time_range *tmp;
  tmp = (struct time_range *)malloc(sizeof(struct time_range));
  tmp->from = start;
  tmp->to = end;
  list_add_tail(&tmp->list, &rml->list);
  fprintf(stderr, "added rm item %ld,%ld to tail\n", start, end);
}

int add_mark_entry_to_list(struct time_range *rml, time_t from, time_t to)
{
  struct list_head *pos, *n;
  struct time_range *cur;

  if (list_empty(&rml->list)) {
    add_time_range_to_head(rml, from, to);
    return 1;
  } else {
    /* check if we can insert in the begining of the list */
    cur = list_entry(rml->list.next, struct time_range, list);
    /*fprintf(stderr, "comparing rm 1st item %ld,%ld with %ld,%ld\n",
      cur->from, cur->to, from, to);*/
    /* -------|<----NEW---->|--------|<----EVENT---->|------
     * insert new record
     */ 
    if (to < cur->from) {
      add_time_range_to_head(rml, from, to);
      return 1;
    }
    
    list_for_each_safe(pos, n, &rml->list) {
      cur = list_entry(pos, struct time_range, list);
      /*fprintf(stderr, "comparing rm item %ld,%ld with %ld,%ld\n",
	cur->from, cur->to, from, to);*/
      
      
      /* -------|<----NEW---->|
       *             ----|<----EVENT---->|------
       * update the existing start time with the new event       
       */ 
      if ((from < cur->from) &&
	  ((to >= cur->from) && (to < cur->to))) {
	fprintf(stderr, "updating start %ld to %ld\n", cur->from, from);
	cur->from = from;
	return 1;
	
      /*            |<----NEW---->|
       * ------|<----|S--EVENT--E|---->|------ (S:start, E:end)
       * event alread handled: no nothing       
       */ 
      } else if ((from >= cur->from) && (to <= cur->to)) {
	return 0;

      /*                  |<---- NEW ---->|
       * ------|<----EVENT---->|<----
       * update the existing end time with the new event       
       */ 
      } else if (((from >= cur->from) && (from <= cur->to)) &&
		 (to > cur->to)) {
	fprintf(stderr, "updating rm end %ld to %ld\n", cur->to, to);
	cur->to = to;
	return 1;
      }
    }

    /* if came here, we can append at the end of the list */
    /*                                     |<---- NEW ---->|
     * ------|<----LASTEVENT---->|-----
     * append to the end
     */ 
    add_time_range_to_tail(rml, from, to);
  }

  return 1;
}

/* merge peak_event_list(obtained in scanSmartFiles)
 * to remote_mark_list from smartserver */
static void merge_peak_event_list_to_remote_mark_list()
{
  struct peak_event *tmp;
  struct list_head *pos;

  list_for_each(pos, &g_pel.list) {
    tmp = list_entry(pos, struct peak_event, list);
    add_mark_entry_to_list(&g_rml,
			   tmp->start - g_pre_peak_event,
			   tmp->end + g_post_peak_event);
  }
}

static int 
add_smartfile_to_list(char *path,
		      unsigned short year, char month, char day, 
		      char hour, char min, char sec,
		      char type, char lock, 
		      unsigned short fileno,
		      off_t size)
{
  struct sfileEntry *node;

  node = (struct sfileEntry *)malloc(sizeof(struct sfileEntry));
  if (node) {
    node->fi.year = year;
    node->fi.month = month;
    node->fi.day = day;
    node->fi.hour = hour;
    node->fi.min = min;
    node->fi.sec = sec;
    node->fi.type = type;
    node->fi.lock = lock;
    node->fi.fileno = fileno;
    node->fi.size = size;
    node->fi.pathId = get_path_list_id(path);

    /* add to the list */
    node->next = sfile_list;
    sfile_list = node;
  }

  return 0;
}

/* change the recording file(full path) to 'L'/'N' */
int rename_filename_only(char *filename, char type)
{
  char *ptr;
  int ret = 1;

  ptr = strrchr(filename, '/');
  if (ptr) {
    /* look for 1st underbar after CHxx */
    ptr = strchr(ptr + 1, '_');
    if (ptr) {
      /* look for 2nd under bar after YYYYMMDDhhmmss(mmm) */
      ptr = strchr(ptr + 1, '_');
      if (ptr) {
	/* look for 3rd under bar after Length */
	ptr = strchr(ptr + 1, '_');
	if (ptr) {
	  *(ptr + 1) = type;
	  ret = 0;
	}
      }
    }
  }

  return ret;
}

/* rename the recording file(full path) to 'L'/'N' */
static int rename_file(char *filename, char type)
{
  char *ptr, *newname;
  int ret = 1;

  fprintf(stderr, "rename: %s\n", filename);
  newname = strdup(filename);
  if (newname) {
    ptr = strrchr(newname, '/');
    if (ptr) {
      /* look for 1st underbar after CHxx */
      ptr = strchr(ptr + 1, '_');
      if (ptr) {
	/* look for 2nd under bar after YYYYMMDDhhmmss(mmm) */
	ptr = strchr(ptr + 1, '_');
	if (ptr) {
	  /* look for 3rd under bar after Length */
	  ptr = strchr(ptr + 1, '_');
	  if (ptr) {
	    *(ptr + 1) = type;
	    fprintf(stderr, "%s rename to %s\n", filename, newname);
	    if (rename(filename, newname) == -1) {
	      perror("rename_file");
	    } else {
	      ret = 0;
	    }
	  }
	}
      }
    }
    free(newname);
  }

  return ret;
}

static int rename_smartfile(char *pszFile, char type)
{
  int ret = 1;
  char *filename, *ptr;
  
  filename = strdup(pszFile);
  if (filename) {
    ptr = strrchr(filename, '_');
    if (ptr) {
      *(ptr + 1) = type;
      //fprintf(stderr, "filename2:%s\n", filename);
      if (rename(pszFile, filename) == -1)
	perror("rename");
      else
	ret = 0;
    }
    free(filename);
  }
  
  return ret;
}

/* scan USB HD for smart log files. (USB HD & CF already synced)
 * if USB HD not mounted, scan CF instead
 */
static int scanSmartFiles(char *dir_path, char *servername)
{
  DIR *dir;
  struct dirent *de;
  char dirpath[128], dname[128], fullname[256];
  int ret;
  int servernameLen = 0;
  char month = 0, day = 0, hour = 0, min = 0, sec = 0, lock = 0;
  unsigned short year = 0, fileno = 0;
  struct stat st;

  //*usbMounted = 1;
  strncpy(dirpath, dir_path, sizeof(dirpath));

  sprintf(dname, "%s/smartlog", dirpath);
  dir = opendir(dname);
  if (dir == NULL) {
    perror("scanSmartFiles:opendir");
    if (g_dvr_type == DVR_TYPE_DM500) {
      //*usbMounted = 0;
      strncpy(dirpath, smart_dir, sizeof(dirpath));
      sprintf(dname, "%s/smartlog", dirpath);
      dir = opendir(dname);
      if (dir == NULL) {
	perror("scanSmartFiles:opendir");
	return 1;
      }
    } else {
      return 1;
    }
  }

  if (servername) 
    servernameLen = strlen(servername);

  while ((de = readdir(dir)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    
    if (de->d_type != DT_REG)
      continue;

    if (((int)strlen(de->d_name) > servernameLen) &&
	!strncmp(servername, de->d_name, servernameLen)) {
      ret = sscanf(de->d_name + servernameLen,
		   "_%04hd%02hhd%02hhd_%c.%03hd",
		   &year, &month, &day, &lock, &fileno);
      if ((ret == 5) && (fileno == 1)) {
	if (lock == 'L') {
	  snprintf(fullname, sizeof(fullname),
		   "%s/%s",
		   dname, de->d_name);
	  if (stat(fullname, &st)) {
	    perror("scanSmartFiles");
	    continue;
	  }
	  if (g_dvr_type != DVR_TYPE_DM500) {
	    scan_GPS_log(fullname, &g_pel,
			 g_pre_peak_event, g_post_peak_event, &g_peak_tp);
	  }
	  fprintf(stderr, "adding file %s\n", fullname);
	  upload_file_count++;
	  upload_size += st.st_size;
	  add_smartfile_to_list(dirpath,
				year, month, day, 
				0, 0, 0,
				TYPE_SMART, 'L', 
				fileno, st.st_size);
	}
      } else {
	char extension[128];

	ret = sscanf(de->d_name + servernameLen,
		     "_%04hd%02hhd%02hhd%02hhd%02hhd%02hhd_TAB102log_%c.%s",
		     &year, &month, &day,
		     &hour, &min, &sec,
		     &lock, extension);
	if (ret == 8) {
	  if (lock == 'L') {
	    int type = -1;
	    if (!strcmp(extension, "log")) {
	      type = TYPE_IMPACT;
	    } else if (!strcmp(extension, "peak")) {
	      type = TYPE_PEAK;
	    }

	    if ((type == TYPE_IMPACT) || (type == TYPE_PEAK)) {
	      snprintf(fullname, sizeof(fullname),
		       "%s/%s",
		       dname, de->d_name);
	      if (stat(fullname, &st)) {
		perror("scanSmartFiles");
		continue;
	      }
	      if (g_dvr_type != DVR_TYPE_DM500) {
		scan_peak_data(fullname, &g_pel, g_peak_time_in_utc,
			       g_pre_peak_event, g_post_peak_event, &g_peak_tp);
	      }
	      fprintf(stderr, "adding file %s\n", de->d_name);
	      upload_file_count++;
	      upload_size += st.st_size;
	      add_smartfile_to_list(dirpath,
				    year, month, day, 
				    hour, min, sec,
				    type, lock, 
				    0, st.st_size);
	    }
	  }
	}
      }
    }
  }
  
  closedir(dir);

  fprintf(stderr, "scanSmartfiles end\n");

  return 0;
}

int scan_smartlog_disks(char *rootdir, char *servername)
{
  char curdisk[256];
  int r = 0;

  /* we can scan only current recording disk */
  FILE *fp = fopen("/var/dvr/dvrcurdisk", "r");
  if (fp != NULL) {
    r = fread(curdisk, 1, sizeof(curdisk) - 1, fp);
    fclose(fp);
  }
  if (r <= 2)
    return 1;

  curdisk[r] = 0;

  //writeDebug("scaning %s", curdisk);
  if (scanSmartFiles(curdisk, servername)) {
    writeDebug("scanSmartFiles error");
  }

  return 0;
}

static int scan_files(char *dir_path, char *servername,
		      void *arg,
		      int (*doit)(char*, char*, char*, char*, void*))
{
  DIR *dir, *dirCh, *dirFile;
  struct dirent *de, *deCh, *deFile;
  char dname[128];
  
  sprintf(dname, "%s/_%s_", dir_path, servername);
  
  dir = opendir(dname);
  if (dir == NULL) {
    writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
    return 1;
  }
  
  while ((de = readdir(dir)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    
    if (de->d_type != DT_DIR)
      continue;
    
    sprintf(dname, "%s/_%s_/%s", dir_path, servername, de->d_name);
    dirCh = opendir(dname);
    if (dirCh == NULL) {
      writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
      closedir(dir);
      return 1;
    }
    while ((deCh = readdir(dirCh)) != NULL) {
      if (deCh->d_name[0] == '.')
	continue;
      
      if (deCh->d_type != DT_DIR)
	continue;
      
      sprintf(dname, "%s/_%s_/%s/%s",
	      dir_path, servername,
	      de->d_name, deCh->d_name);
      dirFile = opendir(dname);
      if (dirFile == NULL) {
	writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
	closedir(dirCh);
	closedir(dir);
	return 1;
      }
      while ((deFile = readdir(dirFile)) != NULL) {
	if (deFile->d_name[0] == '.') {
	  continue;
	}
	if (deFile->d_type != DT_REG) {
	  continue;
	}
	(*doit)(dir_path, servername, dname, deFile->d_name, arg);
      }
      
      closedir(dirFile);
    }
    
    closedir(dirCh);
  }
  
  closedir(dir);
  
  return 0;
}

#if 0
static int 
where_to_cut_file(struct fileInfo *fi, time_t *cut_head, time_t *cut_tail)
{
  struct time_range *cur;
  struct list_head *pos;
      
  time_t t;
  struct tm tm;
  
  *cut_head = 0;
  *cut_tail = 0;

  tm.tm_isdst = -1;
  tm.tm_year = fi->year - 1900;
  tm.tm_mon = fi->month - 1;
  tm.tm_mday = fi->day;
  tm.tm_hour = fi->hour;
  tm.tm_min = fi->min;
  tm.tm_sec = fi->sec;
  t = mktime(&tm);

  list_for_each(pos, &g_rml.list) {
    cur = list_entry(pos, struct time_range, list);

    unsigned int len_in_sec;
    len_in_sec = ((g_dvr_type == DVR_TYPE_DM500) ? fi->len / 1000 : fi->len);
    printf("*********t:%d length:%d from:%d to:%d\n",t,len_in_sec,cur->from,cur->to);   
    /* don't use >= or <=, as we don't want to copy 1sec video from a file */
    if ((t > (time_t)(cur->from - len_in_sec)) &&
	(t < cur->to)) {
      if (t < cur->from) {
	*cut_head = cur->from;
      }
      if (t + (time_t)len_in_sec > cur->to) {
	*cut_tail = cur->to; /* cut tail */
      }

      /* don't cut for less than 2 seconds */
      if (((t + len_in_sec) - *cut_tail) < 3) {
	fprintf(stderr, "correcting cut_tail from %ld to 0\n", *cut_tail); 
	*cut_tail = 0;
      }
      if ((*cut_head - t) < 3) {
	fprintf(stderr, "correcting cut_head from %ld to 0\n", *cut_head); 
	*cut_head = 0;
      }

      return 0;
    }
  }

  return 1;
}
#endif

static int 
where_to_cut_file(struct fileInfo *fi, time_t *cut_head, time_t *cut_tail)
{
  struct time_range *cur;
  struct list_head *pos;
      
  time_t t;
  struct tm tm;
  
  *cut_head = 0;
  *cut_tail = 0;

  tm.tm_isdst = -1;
  tm.tm_year = fi->year - 1900;
  tm.tm_mon = fi->month - 1;
  tm.tm_mday = fi->day;
  tm.tm_hour = fi->hour;
  tm.tm_min = fi->min;
  tm.tm_sec = fi->sec;
  t = mktime(&tm);
  //printf("inside where_to_cut_files:%d\n",t);
  if(fi->len<2)
    return 1;
  list_for_each(pos, &g_rml.list) {
    cur = list_entry(pos, struct time_range, list);
   // printf("From:%d To:%d\n",cur->from,cur->to);
    unsigned int len_in_sec;
    len_in_sec = ((g_dvr_type == DVR_TYPE_DM500) ? fi->len / 1000 : fi->len);
    /* don't use >= or <=, as we don't want to copy 1sec video from a file */
    if ((t > cur->from - len_in_sec) &&
	(t < cur->to)) {
      if (t < cur->from) {
	*cut_head = cur->from;
      }
      if (t + len_in_sec > cur->to) {
	*cut_tail = cur->to; /* cut tail */
      }
      return 0;
    }
  }

  return 1;
}

int get_filename(char *newname, int size,
		 int ch, unsigned int len, char type, char *servername,
		 struct tm *tm, char *extension)
{
  if (!newname || !servername || !tm)
    return 1;

  snprintf(newname, size,
	   "CH%02d_%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	   ch,
	   tm->tm_year + 1900,
	   tm->tm_mon + 1, tm->tm_mday,
	   tm->tm_hour, tm->tm_min, tm->tm_sec,
	   len,
	   type,
	   servername,
	   extension);

  return 0;
}

time_t get_filetime(struct fileInfo *fi)
{
  struct tm tm;

  tm.tm_isdst = -1;
  tm.tm_year = fi->year - 1900;
  tm.tm_mon = fi->month - 1;
  tm.tm_mday = fi->day;
  tm.tm_hour = fi->hour;
  tm.tm_min = fi->min;
  tm.tm_sec = fi->sec;

  return mktime(&tm);

}

char *create_k_filename(char *filename)
{
  char *kfile = NULL;
  kfile = strdup(filename);
  if (kfile) {
    if (!videoToK(kfile)) {
      return kfile;
    } else {
      free(kfile);
    }
  }

  return NULL;
}

int is_disk_full(char *usbroot)
{
  /* check disk space */
  struct statfs fsInfo;
  long long int freeSpace;
      
  if (!usbroot) return 1;

  if (statfs(usbroot, &fsInfo) == -1) {
    return 1;
  }
      
  long long size = fsInfo.f_bsize;
  freeSpace = fsInfo.f_bavail * size;
  
  if (freeSpace < 1024LL * 1024LL) {
    writeDebug("disk full: %lld", freeSpace);
    return 1; /* disk full */
  }

  return 0;
}

static void
get_filepath(char *fullname, char *path, int path_size)
{
  char *full, *ptr;

  if (!path) return;

  *path = 0;

  full = strdup(fullname);
  if (full) {
    ptr = strrchr(full, '/');
    if (ptr) {
      *ptr = 0;
      strncpy(path, full, path_size);
    }
    free(full);
  }
}

#if 0
int copy_portion_to_new_file(char *dest_dir, char *servername,
			     char *fullname, time_t filetime,
			     char ch, char new_type, 
			     long long from_offset, long long to_offset,
			     long long toffset_start, long long from_toffset,
			     char *new_full_name, int new_full_name_size)
{
  int retval = 0;
  struct tm tm;
  char *newkfile = NULL, *newkfile2 = NULL;
  char *newfile = NULL, *newfile2 = NULL;
  char newname[128];
  FILE *fp = NULL, *fpnew = NULL, *fpnew_k = NULL;
  time_t new_filetime;
  int delete_it = 0;
  time_t new_len;
  int encrypted, bytes;

  // sanity check
  if (!fullname)
    return -1;

  fprintf(stderr, "copy_portion_to_new_file(filetime:%ld)\n", filetime);
  /* set start time for the new file */
  new_filetime = filetime;
  if (from_toffset) {
    new_filetime = filetime + (time_t)(from_toffset / 1000);
  }
  //writeDebug("new_filetime:%ld", new_filetime);
  localtime_r(&new_filetime, &tm);

  /* give a temporary name to the new file */
  get_filename(newname, sizeof(newname),
	       ch, 0, new_type, servername,
	       &tm, g_ext);
  //writeDebug("newname:%s", newname);

  newfile = (char *)malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile) {
    writeDebug("malloc error");
    retval = -1;
    goto err_exit;
  }
  sprintf(newfile, "%s/%s", dest_dir, newname);

  /* open original data file */
  fp = fopen(fullname, "r");
  if (!fp) {
    writeDebug("fopen1:%s(%s)", strerror(errno), fullname);
    retval = -1;
    goto err_exit;
  }

  /* open new file */
  fpnew = fopen(newfile, "w");
  if (!fpnew) {
    writeDebug("fopen2:%s(%s)", strerror(errno), newfile);
    retval = -1;
    goto err_exit;
  }
  //writeDebug("new:%s", newfile);

  newkfile = create_k_filename(newfile);
  if (!newkfile) {
    retval = -1;
    goto err_exit;
  }

  /* open new k file */
  fpnew_k = fopen(newkfile, "w");
  if (!fpnew_k) {
    writeDebug("fopen3:%s(%s)", strerror(errno), newkfile);
    retval = -1;
    goto err_exit;
  }
  //writeDebug("newk:%s", newkfile);

  /* copy file header */
  struct hd_file fileh;
  bytes = fread(&fileh, 1,
		    sizeof(struct hd_file), fp);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fread header(%d, %s)", bytes, fullname);
    delete_it = 1;
    retval = -1;
    goto err_exit;
  }
  
  encrypted = 1;
  if (fileh.flag == 0x484b4834)
    encrypted = 0;

  bytes = fwrite(&fileh, 1, sizeof(struct hd_file), fpnew);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fwrite header(%s)", newfile);
    delete_it = 1;
    retval = -1;
    goto err_exit;
  }

  /* copy video/audio frames to the new file */
  if (copy_frames(fp, fpnew, fpnew_k,
		  from_offset, to_offset, toffset_start,
		  filetime, &new_filetime, &new_len)) {
    delete_it = 1;
    retval = -1;
    goto err_exit;
  }

  /* give the correct file name to the new file */
  localtime_r(&new_filetime, &tm);
  get_filename(newname, sizeof(newname),
	       ch, (unsigned int)new_len, new_type, servername,
	       &tm, g_ext);
  newfile2 = (char *)malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile2) {
    writeDebug("malloc error");
    delete_it = 1;
    retval = -1;
    goto err_exit;
  }
  sprintf(newfile2, "%s/%s", dest_dir, newname);

  writeDebug("extracted %s", newfile2);
  if (rename(newfile, newfile2) == -1) {
    writeDebug("rename error (%s) %s %s", strerror(errno), newfile, newfile2);
  }
  /* save the new filename */
  if (new_full_name && (new_full_name_size > 0)) {
    strncpy(new_full_name, newfile2, new_full_name_size);
  }

  /* give the correct file name to the new k file */
  newkfile2 = create_k_filename(newfile2);
  if (!newkfile2) {
    retval = -1;
    delete_it = 1;
    goto err_exit;
  }
  if (rename(newkfile, newkfile2) == -1) {
    writeDebug("rename error (%s) %s %s", strerror(errno), newkfile, newkfile2);
  }

 err_exit:

  if (fpnew_k) fclose(fpnew_k);
  if (fpnew) fclose(fpnew);
  if (fp) fclose(fp);

  if (delete_it) {
    writeDebug("deleting %s", newkfile);
    unlink(newkfile);
    writeDebug("deleting %s", newfile);
    unlink(newfile);
  }

  if (newkfile2) free(newkfile2);
  if (newfile2) free(newfile2);
  if (newkfile) free(newkfile);
  if (newfile) free(newfile);

  return retval;
}
#else
int copy_portion_into_new_files(char *dir_path,FILE* kfile,char *dest_dir,
                              char *servername,
                              char *fullname,int len, time_t filetime,
                              char ch,long from_toffset,
                              long to_toffset,
                              int kfile_from,int kfile_to)
{
  int retval = 0;
  struct tm tm;
  char *newkfile = NULL;
  char *newfile = NULL;
  char newname[128];
  char newkname[128];
  FILE *fp = NULL, *fpnew = NULL, *fpnew_k = NULL;
  time_t new_filetime;
  time_t new_len;
  int ktime,koffset;
  int ktime_last,koffset_last;
  int ktime_first=0;
  int framesize,frametime;
  char *framebuf=NULL;
  struct hd_file fileh;
  int bytes;
  long posCur=0;
  int  kposCur=0;
  int  readNum=0;
  int  mdelete = 0;
  struct fileInfo newfi;
  // sanity check
  if (!fullname)
    return -1;
 // fprintf(stderr, "copy_portion_into_new_files(filetime:%ld)\n", filetime);
 // printf("fullname:%s\n",fullname);
 // printf("from_toffset:%d to_toffset:%d\n",from_toffset,to_toffset);
 /* set start time for the new file */
  new_filetime = filetime;
  if (from_toffset) {
    new_filetime = filetime + (time_t)(from_toffset / 1000);
  }
  //writeDebug("new_filetime:%ld", new_filetime);
  localtime_r(&new_filetime, &tm);
  new_len=(to_toffset-from_toffset)/1000;
  /* give a name to the new file */
  get_filename(newname, sizeof(newname),
	       ch, new_len, 'L', servername,
	       &tm, "266");
  //writeDebug("newname:%s", newname);

  newfile =(char*)malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile) {
    writeDebug("malloc error");
    retval = -1;
    goto err_exit;
  }
  sprintf(newfile, "%s/%s", dest_dir, newname);

 /* open new file */
  fpnew = fopen(newfile, "w");
  if (!fpnew) {
    writeDebug("fopen:%s(%s)", strerror(errno), newfile);
    retval = -1;
    goto err_exit;
  }
  printf("media file name:%s is created\n",newfile);
 //writeDebug("new:%s", newfile);
  newkfile = create_k_filename(newfile);
  if (!newkfile) {
    retval = -1;
    goto err_exit;
  }
  printf("k file:%s is created\n",newkfile);
  /* open new k file */
  fpnew_k = fopen(newkfile, "w");
  if (!fpnew_k) {
    writeDebug("fopen:%s(%s)", strerror(errno), newkfile);
    retval = -1;
    goto err_exit;
  }
 // printf("k file name:%s is created\n",newkfile);
 /* open original data file */
  fp = fopen(fullname, "r");
  if (!fp) {
    writeDebug("fopen:%s(%s)", strerror(errno), fullname);
    retval = -1;
    mdelete = 1;
    goto err_exit;
  }

 /* copy file header */
  bytes = fread(&fileh, 1,
               sizeof(struct hd_file), fp);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fread header(%d, %s)", bytes, fullname);
    retval = -1;
    mdelete = 1;
    goto err_exit;
  }

  bytes = fwrite(&fileh, 1, sizeof(struct hd_file), fpnew);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fwrite header(%s)", newfile);
    mdelete = 1;
    retval = -1;
    goto err_exit;
  }
  //writeDebug("newk:%s", newkfile);
  fseek(kfile, kfile_from, SEEK_SET);
 // printf("kfile=%d\n",kfile);
  fscanf(kfile,"%d,%d\n",&ktime,&koffset);
//  printf("ktime:%d koffset:%d\n",ktime,koffset);
  ktime_last=ktime;
  koffset_last=koffset;
  ktime_first=ktime;
  posCur=ftell(fpnew);
  while(1){
     readNum=fscanf(kfile,"%d,%d\n",&ktime,&koffset);
     if(readNum!=2)
       break;
     framesize=koffset-koffset_last;
     frametime=ktime_last-ktime_first;

     framebuf=(char*)malloc(framesize);
     fseek(fp,koffset_last,SEEK_SET); 
     bytes=fread(framebuf,1,framesize,fp);
     if(bytes!=framesize){
        free(framebuf);
        break;
     }
     fwrite(framebuf,1,framesize,fpnew);
     fprintf(fpnew_k,"%d,%d\n",frametime,posCur);
     free(framebuf);
     posCur=ftell(fpnew);
     kposCur=ftell(kfile);
     if(kposCur>=kfile_to)
        break;
     ktime_last=ktime;
     koffset_last=koffset;
  }
  
//  printf("file1:%s is done\n",newname);
  new_len=len-to_toffset/1000;
  fclose(fpnew);
  fclose(fpnew_k);
  if(!get_fileinfo_from_fullname(&newfi,newfile,servername)){
     check_and_add_to_list(dir_path,servername,newfile,&newfi);
  }
  if(!get_fileinfo_from_fullname(&newfi,newkfile,servername)){
     check_and_add_to_list(dir_path,servername,newkfile,&newfi);
  }
  if (newkfile){ 
     free(newkfile);
     newkfile=NULL;
  }
  if (newfile){
     free(newfile);
     newfile=NULL;
  }

  if(new_len<5){
     goto err_exit;
  }
  //copy the remaining part;

  new_filetime = filetime + (time_t)(to_toffset / 1000);
  //writeDebug("new_filetime:%ld", new_filetime);
  localtime_r(&new_filetime, &tm); 

  /* give a name to the new file */
  get_filename(newname, sizeof(newname),
	       ch, new_len, 'N', servername,
	       &tm, "266");
  newfile = (char*)malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile) {
    writeDebug("malloc error");
    retval = -1;
    goto err_exit;
  }
  sprintf(newfile, "%s/%s", dest_dir, newname);

 /* open new file */
  fpnew = fopen(newfile, "w");
  if (!fpnew) {
    writeDebug("fopen:%s(%s)", strerror(errno), newfile);
    retval = -1;
    goto err_exit;
  }
 //writeDebug("new:%s", newfile);
  newkfile = create_k_filename(newfile);
  if (!newkfile) {
    retval = -1;
    goto err_exit;
  }

  /* open new k file */
  fpnew_k = fopen(newkfile, "w");
  if (!fpnew_k) {
    writeDebug("fopen:%s(%s)", strerror(errno), newkfile);
    retval = -1;
    goto err_exit;
  }
 /* copy file header */
  fseek(fp,0,SEEK_SET);
  bytes = fread(&fileh, 1,
               sizeof(struct hd_file), fp);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fread header(%d, %s)", bytes, fullname);
    retval = -1;
    mdelete = 1;
    goto err_exit;
  }

  bytes = fwrite(&fileh, 1, sizeof(struct hd_file), fpnew);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fwrite header(%s)", newfile);
    mdelete = 1;
    retval = -1;
    goto err_exit;
  }
  fseek(kfile, kfile_to, SEEK_SET);
  fscanf(kfile,"%d,%d\n",&ktime,&koffset);
  ktime_last=ktime;
  koffset_last=koffset;
  ktime_first=ktime;
  posCur=ftell(fpnew);
  while(1){
     readNum=fscanf(kfile,"%d,%d\n",&ktime,&koffset);
     if(readNum!=2)
       break;
     framesize=koffset-koffset_last;
     frametime=ktime_last-ktime_first;
     framebuf=(char*)malloc(framesize);
     fseek(fp,koffset_last,SEEK_SET); 
     bytes=fread(framebuf,1,framesize,fp);
     if(bytes!=framesize){
        free(framebuf);
        break;
     }
     fwrite(framebuf,1,framesize,fpnew);
     fprintf(fpnew_k,"%d,%d\n",frametime,posCur);
     free(framebuf);
     posCur=ftell(fpnew);
     ktime_last=ktime;
     koffset_last=koffset;
  }

  if (fpnew_k) fclose(fpnew_k);
  if (fpnew) fclose(fpnew);

 err_exit:
  if (fp) fclose(fp);

  if (mdelete) {
    writeDebug("deleting %s", newkfile);
    unlink(newkfile);
    writeDebug("deleting %s", newfile);
    unlink(newfile);
  }

  if (newkfile) free(newkfile);
  if (newfile) free(newfile);
  return retval;
}
#endif
/*
 * fullname: original video file to cut (name changed to the new name)
 * fileInfo structure is also updated to the new length
 * 0: success
 * 1: disk full
 * -1: other errors
 */
#if 0
int cut_file(char *servername, char *fullname, 
	     struct fileInfo *fi, time_t from, time_t to,
	     char *new_L_full_name, int new_L_full_name_size,
	     char *new_N_full_name, int new_N_full_name_size)
{
  int ret, retval = 0;
  char newname[128], filepath[128], buf[128];
  char *kfilename = NULL, *renamedfile = NULL;
  time_t filetime, len_after_cut = 0;
  struct tm tm;
  FILE *fp = NULL, *fpnew = NULL, *fpnew_k = NULL;
  long long toffset, offset; // read from k file
  long long toffset_save = 0, offset_save = 0; // last record saved
  // offset in data file found from k file(from,to)
  long long from_offset = 0, to_offset = 0;
  // toffset(time offset in milsec) found from k file(from,to)
  long long from_toffset = 0, to_toffset = 0;
  long long toffset_start = 0; // 1st ts offset to be used in the new file
  int toffset_start_written = 0;
  off_t pos_before_reading = 0, pos_before_reading_save = 0;
  off_t cut_size = 0;
  int first_portion_is_L;
  
  fprintf(stderr, "cut_file:%s\n", fullname);
  
  if (!from && !to)
    return -1;

  get_filepath(fullname, filepath, sizeof(filepath));
  
  filetime = get_filetime(fi);
  fprintf(stderr, "filetime:%ld\n", filetime);

  kfilename = create_k_filename(fullname);
  if (!kfilename) {
    retval = -1;
    goto error_exit;
  }

  fprintf(stderr, "kfile:%s\n", kfilename);
  /* scan k file for start/end offset */
  fp = fopen(kfilename, "r");
  if (!fp) {
    retval = -1;
    goto error_exit;
  }

  pos_before_reading = ftell(fp);
  while(fgets(buf, sizeof(buf), fp)) {
    if (2 == sscanf(buf, "%lld,%lld\n", &toffset, &offset)) {
      /* save the 1st offset */
      if (!toffset_start_written) {
	toffset_start_written = 1;
	toffset_start = toffset;
	fprintf(stderr, "saved 1st:%lld\n", toffset_start);
      }

      time_t ts = filetime + (time_t)(toffset / 1000LL);
      if (from && !from_offset) {
	if (ts == from) {
	  from_offset = offset;
	  from_toffset = toffset;
	  cut_size = pos_before_reading;
	  //writeDebug("from exact:%lld(%lld)", from_offset, from_toffset);
	} else if (ts > from) {
	  if (offset_save) {
	    from_offset = offset_save;
	    from_toffset = toffset_save;
	    cut_size = pos_before_reading_save;
	    //writeDebug("from back:%lld(%lld)", from_offset, from_toffset);
	  } else {
	    from_offset = offset;
	    from_toffset = toffset;
	    cut_size = pos_before_reading;
	    //writeDebug("from about:%lld(%lld)", from_offset, from_toffset);
	  }
	}
      }
      if (to && !to_offset) {
	if (ts >= to) {
	  to_offset = offset;
	  to_toffset = toffset;
	  if (!cut_size) { /* from == 0 */
	    cut_size = pos_before_reading;
	  }
	  //writeDebug("to about:%lld(%lld)", to_offset, from_toffset);
	}
      }
      offset_save = offset;
      toffset_save = toffset;
      pos_before_reading_save =  pos_before_reading;
   }
    pos_before_reading = ftell(fp);
  }
  fclose(fp); fp = NULL;
  if (cut_size) truncate(kfilename, cut_size);
  if (!from_offset && !to_offset) {
    writeDebug("%s damaged", kfilename);
    retval = -1;
    goto error_exit;
  }

  /* 1st portion L(from:0) or N(from!=0)? */
  first_portion_is_L = 0;
  if (!from_toffset) {
    first_portion_is_L = 1;
    from_offset = to_offset;
    from_toffset = to_toffset;
    to_offset = 0;
    to_toffset = 0;
  }
  len_after_cut = ((from_toffset ? from_toffset : to_toffset)
		   - toffset_start) / 1000;
  /* rename src k file to the newly cut length */
  if (len_after_cut > 2) {
    /* update len to fileInfo */
    fi->len = len_after_cut;
    localtime_r(&filetime, &tm);
    get_filename(newname, sizeof(newname),
		 fi->ch, (unsigned int)len_after_cut,
		 first_portion_is_L ? 'L' : 'N', servername,
		 &tm, "k");
    renamedfile = (char *)malloc(strlen(filepath) + strlen(newname) + 24);
    // 24:len
    if (renamedfile) {
      sprintf(renamedfile, "%s/%s", filepath, newname);
      writeDebug("rename %s %s", kfilename, renamedfile);
      rename(kfilename, renamedfile);
      /* change video file name, too. It'll be truncated later */
      kToVideo(renamedfile, g_ext);
      rename(fullname, renamedfile);
    }
  } else {
    writeDebug("nothing to cut in %s", kfilename);
    retval = -1;
    goto error_exit;
  }

  writeDebug("from:%lld(%lld), to:%lld(%lld)",
	     from_offset, from_toffset, to_offset, to_toffset);
  ret = copy_portion_to_new_file(filepath, servername,
				 renamedfile, filetime,
				 fi->ch,
				 first_portion_is_L ? 'N' : 'L',
				 from_offset, to_offset,
				 toffset_start, from_toffset,
				 first_portion_is_L ?
				 new_N_full_name : new_L_full_name,
				 first_portion_is_L ?
				 new_N_full_name_size : new_L_full_name_size);
  if (!ret && from_offset && to_offset) {
    /* copy the tail portion if necessary
     *       |--------------original file--------------|
     *                 |---new L file-----|
     *       |---N-----|                  |--N(this)---|
     *                 |<--should be truncated here
     */
    copy_portion_to_new_file(filepath, servername,
			     renamedfile, filetime,
			     fi->ch, 'N', 
			     to_offset, 0,
			     toffset_start, to_toffset,
			     new_N_full_name, new_N_full_name_size);
  }

  /* truncate data file according to the diagram above */
  cut_size = (from_offset ? from_offset : to_offset);
  if (cut_size) truncate(renamedfile, cut_size);

  /* make sure L file is added to upload list, not N file */
  if (first_portion_is_L) {
    strncpy(new_L_full_name, renamedfile, new_L_full_name_size);
  }

 error_exit:

  if (fpnew_k) fclose(fpnew_k);
  if (fpnew) fclose(fpnew);
  if (fp) fclose(fp);


  if (renamedfile) free(renamedfile);
  if (kfilename) free(kfilename);

  if (retval && is_disk_full(filepath))
    return 1;

  return retval;
}
#else
int cut_file(char *dir_path,char *servername, char *fullname,
	     struct fileInfo *fi, time_t from, time_t to,
	     char new_type) //, char *new_full_name, int new_full_name_size)
{
  int ret, retval = 0;
  char newname[128], filepath[128], buf[128];
  char *kfilename = NULL, *renamedfile = NULL;
  time_t filetime, len_after_cut = 0;
  struct tm tm;
  FILE *fp = NULL;
  long toffset, offset; // read from k file
  long toffset_save = 0, offset_save = 0; // last record saved
  // offset in data file found from k file(from,to)
  long from_offset = 0, to_offset = 0;
  // toffset(time offset in milsec) found from k file(from,to)
  long from_toffset = 0, to_toffset = 0;
  long toffset_start = 0; // 1st ts offset to be used in the new file
  int toffset_start_written = 0;
  off_t pos_before_reading = 0, pos_before_reading_save = 0;
  off_t cut_size = 0;
  int  mKFromPos=-1;
  int  mkToPos=-1;
  
  //printf("===================================================\n");
  //printf("cut_file:%s\n", fullname);
  //printf("cut from:%d to:%d\n",from,to);
  
  if (!from && !to)
    return -1;

  get_filepath(fullname, filepath, sizeof(filepath));
  
  filetime = get_filetime(fi);
  fprintf(stderr, "filetime:%ld\n", filetime);

  kfilename = create_k_filename(fullname);
  if (!kfilename) {
    retval = -1;
    goto error_exit;
  }

  //fprintf(stderr, "kfile:%s\n", kfilename);
  /* scan k file for start/end offset */
  fp = fopen(kfilename, "r");
  if (!fp) {
    retval = -1;
    goto error_exit;
  }

  pos_before_reading = ftell(fp);
  while(fgets(buf, sizeof(buf), fp)) {
  //  printf("buf:%s\n",buf);
    if (2 == sscanf(buf, "%d,%d\n", &toffset, &offset)) {


     //  printf("==%d,%d\n",toffset,offset);
      /* save the 1st offset */
      if (!toffset_start_written) {
	 toffset_start_written = 1;
	 toffset_start = toffset;
	 fprintf(stderr, "saved 1st:%d\n", toffset_start);
      }
      
      time_t ts = filetime + (time_t)(toffset / 1000LL);
      if (from && !from_offset) {
	if (ts == from) {
	  from_offset = offset;
	  from_toffset = toffset;
	  cut_size = pos_before_reading;
          mKFromPos= pos_before_reading;
	  writeDebug("from exact:%d(%d)", from_offset, from_toffset);
	} else if (ts > from) {
	  if (offset_save) {
	    from_offset = offset_save;
	    from_toffset = toffset_save;
	    cut_size = pos_before_reading_save;
            mKFromPos= pos_before_reading_save;
	    writeDebug("from back:%d(%d)", from_offset, from_toffset);
	  } else {
	    from_offset = offset;
	    from_toffset = toffset;
	    cut_size = pos_before_reading;
            mKFromPos= pos_before_reading;
	    writeDebug("from about:%d(%d)", from_offset, from_toffset);
	  }
	}
      }
      if (to && !to_offset) {
	if (ts >= to) {
	   to_offset = offset;
	   to_toffset = toffset;
	   cut_size = pos_before_reading;
           mkToPos = pos_before_reading;
	   writeDebug("to about:%d(%d)", to_offset, from_toffset);
	}
      }
      offset_save = offset;
      toffset_save = toffset;
      pos_before_reading_save =  pos_before_reading;
    }
    pos_before_reading = ftell(fp);
  }
 // printf("From_toffset:%d To_toffset:%d\n",from_toffset,to_toffset);
 // printf("KFromPos:%d KToPos:%d\n",mKFromPos,mkToPos);
 // if (cut_size) truncate(kfilename, cut_size);
  if (!from_offset && !to_offset) {
      writeDebug("%s damaged", kfilename);
  }
  if(!from_offset||from_toffset<5000){
     fseek(fp,0,SEEK_SET);
     fgets(buf, sizeof(buf), fp);
     sscanf(buf, "%d,%d\n", &toffset, &offset);
     from_offset=offset;
     from_toffset=toffset;
     mKFromPos=0;
  }
  if(!to_offset||(fi->len*1000-to_toffset)<5000){
     fseek(fp,0,SEEK_END);
     mkToPos=ftell(fp);
     to_toffset=fi->len*1000;
  }

 // printf("From_toffset:%d To_toffset:%d\n",from_toffset,to_toffset);
 // printf("KFromPos:%d KToPos:%d\n",mKFromPos,mkToPos);
//  printf("=============================================\n");
  //cut_portion_into_new_files();
  copy_portion_into_new_files(dir_path,fp,filepath,servername,fullname,
                              fi->len, filetime,
                              fi->ch,from_toffset,to_toffset,
                              mKFromPos,mkToPos);
  fclose(fp); fp = NULL;

//  printf("=====================copy new files done\n");
  len_after_cut = (from_toffset
		   - toffset_start) / 1000;
  /* rename src k file to the newly cut length */
  if (len_after_cut > 0) {
    /* update len to fileInfo */
    truncate(kfilename, mKFromPos);
    truncate(fullname,from_offset);
    
    fi->len = len_after_cut;
    localtime_r(&filetime, &tm);
    get_filename(newname, sizeof(newname),
		 fi->ch, (unsigned int)len_after_cut, fi->type, servername,
		 &tm, "k");
    renamedfile = (char*)malloc(strlen(filepath) + strlen(newname) + 24); //24:len
    if (renamedfile) {
      sprintf(renamedfile, "%s/%s", filepath, newname);
      writeDebug("rename %s %s", kfilename, renamedfile);
      rename(kfilename, renamedfile);
      /* change video file name, too. It'll be truncated later */
      kToVideo(renamedfile, g_ext);
      rename(fullname, renamedfile);
    }
  } else {
     remove(fullname);
     remove(kfilename);
  }

 error_exit:

  if (fp) fclose(fp);


  if (renamedfile) free(renamedfile);
  if (kfilename) free(kfilename);

  if (retval && is_disk_full(filepath))
    return 1;

  return retval;
}
#endif
static int 
add_file_to_list(char *path, struct fileInfo *fi)
{
  struct fileEntry *node;

  node = (struct fileEntry *)malloc(sizeof(struct fileEntry));
  if (node) {
    node->fi.ch = fi->ch;
    node->fi.year = fi->year;
    node->fi.month = fi->month;
    node->fi.day = fi->day;
    node->fi.hour = fi->hour;
    node->fi.min = fi->min;
    node->fi.sec = fi->sec;
    node->fi.millisec = fi->millisec;
    node->fi.type = fi->type;
    node->fi.len = fi->len;
    node->fi.prelock = fi->prelock;
    node->fi.ext = fi->ext;
    node->fi.size = fi->size;
    node->fi.pathId = fi->pathId = get_path_list_id(path);
     /* add to the list */
    node->next = file_list;
    file_list = node;
  }

  return 0;
}

void make_smartfile_name(char *dirname, char *servername,
			  struct sfileInfo *fi,
			  char *local, int local_size)
{
  if (fi->type == TYPE_SMART) {      
    snprintf(local, local_size,
	     "%s/smartlog/%s_%04hd%02hhd%02hhd_L.%03hd",
	     dirname,
	     servername,
	     fi->year, fi->month, fi->day,
	     fi->fileno);
  } else {
    snprintf(local, local_size,
	     "%s/smartlog/%s_%04hd%02hhd%02hhd"
	     "%02hhd%02hhd%02hhd_TAB102log_L.%s",
	     dirname,
	     servername,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec,
	     (fi->type == TYPE_IMPACT) ? "log" : "peak");
  }
}

void make_file_name_only(char *servername,
			 struct fileInfo *fi,
			 char *filename, int filename_size)
{
  if (!filename) return;

  if (g_dvr_type == DVR_TYPE_DM500) {
    snprintf(filename, filename_size,
	     "CH%02d_%04d%02d%02d%02d%02d%02d%03d_%u_%c_%s.%s",	       
	     fi->ch,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec, fi->millisec,
	     fi->len, fi->type, servername,
	     (fi->ext == 'k') ? "k" : "mp4");
  } else { /* dvr_type */
    snprintf(filename, filename_size,
	     "CH%02d_%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	     fi->ch,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec,
	     fi->len, fi->type, servername,
	     (fi->ext == 'k') ? "k" : g_ext);
  } /* dvr_type */
}

void make_file_name(char *dirname, char *servername,
		    struct fileInfo *fi,
		    char *filename, int filename_size)
{
  if (g_dvr_type == DVR_TYPE_DM500) {
    if (filename) {
      filename[0] = 0;
      snprintf(filename, filename_size,
	       "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
	       "%04d%02d%02d%02d%02d%02d%03d_%u_%c_%s.%s",
	       dirname, servername,
	       fi->year, fi->month, fi->day,
	       fi->ch, fi->ch,
	       fi->year, fi->month, fi->day,
	       fi->hour, fi->min, fi->sec, fi->millisec,
	       fi->len, fi->type, servername,
	       (fi->ext == 'k') ? "k" : "mp4");
    }
  } else { /* dvr_type */
    if (fi->prelock) { /* not used anymore */
      if (filename) {
	filename[0] = 0;
	snprintf(filename, filename_size,
		 "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
		 "%04d%02d%02d%02d%02d%02d_%u_%c_%u_%s.%s",
		 dirname, servername,
		 fi->year, fi->month, fi->day,
		 fi->ch, fi->ch,
		 fi->year, fi->month, fi->day,
		 fi->hour, fi->min, fi->sec,
		 fi->len, fi->type, fi->prelock, servername,
		 (fi->ext == 'k') ? "k" : g_ext);
      }
    } else { /* prelock */
      if (filename) {
	filename[0] = 0;
	snprintf(filename, filename_size,
		 "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
		 "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
		 dirname, servername,
		 fi->year, fi->month, fi->day,
		 fi->ch, fi->ch,
		 fi->year, fi->month, fi->day,
		 fi->hour, fi->min, fi->sec,
		 fi->len, fi->type, servername,
		 (fi->ext == 'k') ? "k" : g_ext);
      }
    } /* prelock */
  } /* dvr_type */
}

int check_missing_fragments(char *dir_path, char *servername,
			     struct fileInfo *fi)
{
  struct missing_fragment *tmp;
  struct list_head *pos;
  char filepath[256];
  static int all_done = 0;
  int unchecked_file_found = 0;
  struct fileInfo resume_fi;

  /* don't do unnecessary check, as all missing files are checked already */
  if (all_done) return 0;

  list_for_each(pos, &g_mfl.list) {
    tmp = list_entry(pos, struct missing_fragment, list);
	
    if (!get_fileinfo_from_name(&resume_fi, tmp->filename, servername)) {
      if ((resume_fi.ch == fi->ch) &&
	  (resume_fi.year == fi->year) &&
	  (resume_fi.month == fi->month) &&
	  (resume_fi.day == fi->day) &&
	  (resume_fi.hour == fi->hour) &&
	  (resume_fi.min == fi->min) &&
	  (resume_fi.sec == fi->sec) &&
	  (resume_fi.millisec == fi->millisec) &&
	  (resume_fi.len == fi->len) &&
	  (resume_fi.ext == fi->ext) &&
	  (resume_fi.prelock == fi->prelock)) {
	snprintf(filepath, sizeof(filepath),
		 "%s/_%s_/%04d%02d%02d/CH%02d",
		 dir_path, servername,
		 fi->year, fi->month, fi->day,
		 fi->ch);
	tmp->filepath = strdup(filepath);
	if (resume_fi.type != fi->type) {
	  char filename[128], fullname[256];
	  make_file_name_only(servername, fi,
			      filename, sizeof(filename));
	  snprintf(fullname, sizeof(fullname),
		   "%s/%s", filepath, filename);
	  rename_file(fullname, resume_fi.type);
	  fi->type = resume_fi.type;
	} 

	return 1;
      }
    }

    if (tmp->filepath == NULL) 
      unchecked_file_found = 1;
  }

  if (!unchecked_file_found) all_done = 1;

  return 0;
}

void check_and_add_to_list(char *dir_path, char *servername, char *fullname,
			   struct fileInfo *fi)
{
  struct stat st;
  if (fullname) {
    if (stat(fullname, &st)) {
      perror("check_and_add_to_list");
      return;
    }
    fi->size = st.st_size;
  }

  upload_file_count++;
  upload_size += fi->size;
  add_file_to_list(dir_path, fi);
}

int rename_and_add_to_list(char *dir_path, char *servername, char *fullname,
			   struct fileInfo *fi)
{
  struct stat st;
  if (stat(fullname, &st)) {
    perror("rename_and_add_to_list");
    return 1;
  }
  fi->size = st.st_size;

  /* rename it to _L_ file */
  fprintf(stderr, "renaming %s\n", fullname);
  if (!rename_file(fullname, 'L')) {
    fi->type = 'L';
    check_and_add_to_list(dir_path, servername, NULL, fi);
  }

  return 0;
}

static void 
rename_k_and_add_to_list(char *dir_path, char *servername, char *fullname, 
			 struct fileInfo *fi)
{
  fi->ext = 'k';
  char *kfilename = create_k_filename(fullname);
  if (kfilename) {
    rename_and_add_to_list(dir_path, servername, kfilename, fi);
    free(kfilename);
  }
}

/* this function only called on 510x */
static void
delete_file_on_flash(char *fullname)
{
  char sata[64], mountdir[64], diskname[64], filename[256];
  int is_hybrid = 0;
  DIR *dir;
  struct dirent *de;
  char file_to_delete[256];

  //fprintf(stderr, "delete_file_on_flash:%s\n", fullname);
  /* Hybrid? */
  FILE *fp = fopen("/var/dvr/backupdisk", "r");
  if (fp) {
    if (fgets(sata, sizeof(sata), fp)) {
      /* extract diskname (eg. d_sda1) */
      if (!get_filename_from_fullpath(sata, diskname, sizeof(diskname))) {
	is_hybrid = 1;
      }
    }
    fclose(fp);
  }

  if (is_hybrid) {
    /* get only filename part from /_BUS001_/20100611/CH00/... */
    strncpy(filename, fullname + strlen(sata), sizeof(filename));
    fprintf(stderr, "hybrid:%s(%s)\n", filename, sata);
   /* get /var/dvr/disks */
    get_filepath(sata, mountdir, sizeof(mountdir));
    //fprintf(stderr, "opening %s\n", mountdir);
    dir = opendir(mountdir);
    if (dir) {
      while ((de = readdir(dir)) != NULL) {
	if ((de->d_name[0] == '.') || (de->d_type != DT_DIR))
	  continue;
	if (!strcmp(de->d_name, diskname))
	  continue; /* Don't delete files on SATA. */
	snprintf(file_to_delete, sizeof(file_to_delete),
		 "%s/%s%s",
		 mountdir, de->d_name, filename);
	fprintf(stderr, "delete %s\n", file_to_delete);
	if (unlink(file_to_delete))
	  fprintf(stderr, "delete error:%s\n", file_to_delete);
	char *kfilename = create_k_filename(file_to_delete);
	if (kfilename) {
	  unlink(kfilename);
	  free(kfilename);
	}
      }
      closedir(dir);
    }
  }
}

#if 0
static int
check_and_cut_file(char *dir_path, char *servername,
		   char *path, char *filename)
{
  int ret;
  time_t from, to;
  char new_L_fullname[256];
  char new_N_fullname[256];	
  char fullname[256];
  struct fileInfo fi;
	
  if(get_fileinfo_from_name(&fi, filename, servername))
    return 1;

  snprintf(fullname, sizeof(fullname), "%s/%s",
	   path, filename);
  
  ret = where_to_cut_file(&fi, &from, &to);
  if (!ret) { /* file in the remote mark range */
    fprintf(stderr, "where to cut:%ld-%ld\n", from, to);
    if (g_dvr_type == DVR_TYPE_DM500) {
      rename_and_add_to_list(dir_path, servername, fullname, &fi);
      /* K files are skipped here, so add k here */
      rename_k_and_add_to_list(dir_path, servername, fullname, &fi);
    } else {
      if (!from && !to) {
	/* file inside a range: rename(video & k) it to _L_ file */
	rename_and_add_to_list(dir_path, servername, fullname, &fi);
	/* K files are skipped here, so add k here */
	rename_k_and_add_to_list(dir_path, servername, fullname, &fi);
      } else {
	/* cut file, add L file to list(with K), delete on flash */
	new_L_fullname[0] = 0;
	new_N_fullname[0] = 0;
	if (!cut_file(servername, fullname, &fi,
		      from, to,
		      new_L_fullname, sizeof(new_L_fullname),
		      new_N_fullname, sizeof(new_N_fullname))) {
	  if (new_L_fullname[0]) {
	    struct fileInfo newfi;
	    if (!get_fileinfo_from_fullname(&newfi, new_L_fullname,
					    servername)) {
	      check_and_add_to_list(dir_path, servername,
				    new_L_fullname, &newfi);
	      /* add k file */
	      newfi.ext = 'k';
	      char *kfilename = create_k_filename(new_L_fullname);
	      if (kfilename) {
		check_and_add_to_list(dir_path, servername,
				      kfilename, &newfi);
		free(kfilename);
	      }
	    }
	  }
	  if (new_N_fullname[0]) {
	    char Nfile[128];
	    if (!get_filename_from_fullpath(new_N_fullname,
					    Nfile, sizeof(Nfile))) {
	      check_and_cut_file(dir_path, servername,
				 path, Nfile);
	    }
	  }
	  /* delete files on flash(Hybrid) */
	  delete_file_on_flash(fullname);
	  return 0;
	}
      }
    } /* dvr_type */
  } /* where_to_cut */

  return 1;
}
#endif
#if 0
static int 
check_event_files(char *dir_path, char *servername,
		      char *path, char *filename, void *arg)
{
  int ret;
  char fullname[256];
  struct fileInfo fi;
	
  ret = get_fileinfo_from_name(&fi, filename, servername);
  if (!ret) {
    if ((fi.type == 'L') || (fi.type == 'l')) {
      snprintf(fullname, sizeof(fullname), "%s/%s",
	       path, filename);
      if ((fi.ch == 0) && (fi.ext != 'K') && (fi.ext != 'k')) {
	if ((g_dvr_type == DVR_TYPE_DM500) ||
	    ((g_dvr_type != DVR_TYPE_DM500) && !fi.prelock)) {
	  event_count++;
	  event_duration += (g_dvr_type == DVR_TYPE_DM500) ?
	    (fi.len / 1000) : fi.len;
	}
      }
    } /* if (type) */	  
  } /* if (sscanf) */
  
  return 0;
}
#endif

void check_remote_unmark(struct fileInfo *fi, char *fullname)
{
  struct time_range *cur;
  struct list_head *pos;     
  time_t t;
  struct tm tm;

  tm.tm_isdst = -1;
  tm.tm_year = fi->year - 1900;
  tm.tm_mon = fi->month - 1;
  tm.tm_mday = fi->day;
  tm.tm_hour = fi->hour;
  tm.tm_min = fi->min;
  tm.tm_sec = fi->sec;
  t = mktime(&tm);

  list_for_each(pos, &g_ruml.list) {
    cur = list_entry(pos, struct time_range, list);
    unsigned int len_in_sec;
    len_in_sec = ((g_dvr_type == DVR_TYPE_DM500) ? fi->len / 1000 : fi->len);
    /* don't use >= or <=, as we don't want 1sec accuracy */
    if ((t > (time_t)(cur->from - len_in_sec)) &&
	(t < cur->to)) {
      rename_file(fullname, 'N');
      return;
    }
  }
}

#if 0
static int 
create_mark_file_list(char *dir_path, char *servername,
		      char *path, char *filename, void *arg)
{
  int ret;
  char fullname[256];
  struct fileInfo fi;

  ret = get_fileinfo_from_name(&fi, filename, servername);
  if (!ret) {
    if (check_missing_fragments(dir_path, servername, &fi))
      return 0;

    if ((fi.type == 'L') || (fi.type == 'l')) {
      snprintf(fullname, sizeof(fullname), "%s/%s",
	       path, filename);
      /* check for remote unmark */
      check_remote_unmark(&fi, fullname);

      check_and_add_to_list(dir_path, servername, fullname, &fi);
      if ((fi.ch == 0) && (fi.ext != 'K') && (fi.ext != 'k')) {
	if ((g_dvr_type == DVR_TYPE_DM500) ||
	    ((g_dvr_type != DVR_TYPE_DM500) && !fi.prelock)) {
	  event_count++;
	  event_duration += (g_dvr_type == DVR_TYPE_DM500) ?
	    (fi.len / 1000) : fi.len;
	}
      }
    } else {
      if ((fi.ext != 'K') && (fi.ext != 'k')) {
	/* we have only N type video file here */
	check_and_cut_file(dir_path, servername, path, filename);
      } /* (ext!=k) */
    } /* if (type) */	  
  } /* if (sscanf) */
  
  return 0;
}
#else
static int 
create_mark_file_list(char *dir_path, char *servername,
		      char *path, char *filename, void *arg)
{
  int ret;
  char fullname[256];
  struct fileInfo fi;

  ret = get_fileinfo_from_name(&fi, filename, servername);
  if (!ret) {
    if (check_missing_fragments(dir_path, servername, &fi))
      return 0;
    snprintf(fullname, sizeof(fullname), "%s/%s",
	       path, filename);
    if ((fi.type == 'L') || (fi.type == 'l')) {

      /* check for remote unmark */
      check_remote_unmark(&fi, fullname);

      check_and_add_to_list(dir_path, servername, fullname, &fi);
      if ((fi.ch == 0) && (fi.ext != 'K') && (fi.ext != 'k')) {
	if ((g_dvr_type == DVR_TYPE_DM500) ||
	    ((g_dvr_type != DVR_TYPE_DM500) && !fi.prelock)) {
	  event_count++;
	  event_duration += (g_dvr_type == DVR_TYPE_DM500) ?
	    (fi.len / 1000) : fi.len;
	}
      }
    } else {
      if ((fi.ext != 'K') && (fi.ext != 'k')) {
	/* we have only N type video file here */
	time_t from, to;
       // printf("file:%s\n",fullname);
        
	ret = where_to_cut_file(&fi, &from, &to);
	if (!ret) { /* file in the remote mark range */
	  fprintf(stderr, "where to cut:%d-%d\n", from, to);
	  if (g_dvr_type == DVR_TYPE_DM500) {
	    rename_and_add_to_list(dir_path, servername, fullname, &fi);
	    /* K files are skipped here, so add k here */
	    rename_k_and_add_to_list(dir_path, servername, fullname, &fi);
	  } else {
	    if (!from && !to) {
	      /* file inside a range: rename(video & k) it to _L_ file */
	      rename_and_add_to_list(dir_path, servername, fullname, &fi);
	      /* K files are skipped here, so add k here */
	      rename_k_and_add_to_list(dir_path, servername, fullname, &fi);
	    } else {
               cut_file(dir_path,servername, fullname, &fi,from, to, 'L');            
	    }
	  } /* dvr_type */
	} /* where_to_cut */	
      } /* (ext!=k) */
    } /* if (type) */	  
  } /* if (sscanf) */
  
  return 0;
}

#endif
#if 0
int scan_recording_disks(char *rootdir, char *servername,
			 int (*doit)(char*, char*, char*, char*, void*))
{
  DIR *dir;
  struct dirent *de;
  char path[256];
  char curdisk[256];
  int r = 0;

  /* Hybrid: scan only current recording disk */
  FILE *fp = fopen("/var/dvr/backupdisk", "r");
  if (fp != NULL) {
    fclose(fp);
    fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (fp != NULL) {
      r = fread(curdisk, 1, sizeof(curdisk) - 1, fp);
      fclose(fp);
    }
    if (r <= 2)
      return 1;

    curdisk[r] = 0;
  }

  if (r) { /* hybrid */
    writeDebug("scaning %s", curdisk);
    if (scan_files(curdisk, servername, NULL, doit)) {
      writeDebug("scan_files error");
    }
  } else { /* single or multiple */
    dir = opendir(rootdir);
    if (dir) {
      while ((de = readdir(dir)) != NULL) {
	if ((de->d_name[0] == '.') || (de->d_type != DT_DIR))
	  continue;
	
	snprintf(path, sizeof(path), "%s/%s", rootdir, de->d_name);
	writeDebug("scaning %s", path);
	if (scan_files(path, servername, NULL, doit)) {
	  writeDebug("scan_files error");
	}
      }
      closedir(dir);
    }
  }

  return 0;
}
#else
int scan_recording_disks(char *rootdir, char *servername,
			 int (*doit)(char*, char*, char*, char*, void*))
{
  DIR *dir;
  struct dirent *de;
  char path[256];
  char curdisk[256];
  int r = 0;
  struct stat findstat ;
  
  /* Hybrid: scan only current recording disk */
  FILE *fp = fopen("/var/dvr/backupdisk", "r");
  if (fp != NULL) {
    fclose(fp);
    fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (fp != NULL) {
      r = fread(curdisk, 1, sizeof(curdisk) - 1, fp);
      fclose(fp);
    }
    if (r <= 2)
      return 1;

    curdisk[r] = 0;
  }

  if (r) { /* hybrid */
    writeDebug("scaning %s", curdisk);
    if (scan_files(curdisk, servername, NULL, doit)) {
      writeDebug("scan_files error");
    }
  } else { /* single or multiple */
    dir = opendir(rootdir);
    if (dir) {
      while ((de = readdir(dir)) != NULL) {
	if (de->d_name[0] == '.') 
	  continue;
        if(de->d_type==DT_UNKNOWN){
	     snprintf(path, sizeof(path), "%s/%s", rootdir, de->d_name);	  
             if( stat(path, &findstat )==0 ){
                 if( S_ISDIR(findstat.st_mode) ) {
                       de->d_type = DT_DIR ;
                 }
                 else if( S_ISREG(findstat.st_mode) ) {
                     de->d_type = DT_REG ;
                 }
             }
        }
	if (de->d_type != DT_DIR)
          continue;
	
	snprintf(path, sizeof(path), "%s/%s", rootdir, de->d_name);
	writeDebug("scaning %s", path);
	if (scan_files(path, servername, NULL, doit)) {
	  writeDebug("scan_files error");
	}
      }
      closedir(dir);
    }
  }

  return 0;
}
#endif

static void free_peak_event_list()
{
  struct peak_event *tmp;
  struct list_head *pos, *n;

  list_for_each_safe(pos, n, &g_pel.list) {
    tmp = list_entry(pos, struct peak_event, list);
    fprintf(stderr, "freeing pe item %ld,%ld\n", tmp->start, tmp->end);
    list_del(pos);
    free(tmp);
  }
}

static void free_time_range_list(struct time_range *tr)
{
  struct time_range *tmp;
  struct list_head *pos, *n;

  list_for_each_safe(pos, n, &tr->list) {
    tmp = list_entry(pos, struct time_range, list);
    fprintf(stderr, "freeing time range item %ld,%ld\n", tmp->from, tmp->to);
    list_del(pos);
    free(tmp);
  }
}

static void free_todo_list()
{
  struct todo_taskid *tmp;
  struct list_head *pos, *n;

  list_for_each_safe(pos, n, &g_tdl.list) {
    tmp = list_entry(pos, struct todo_taskid, list);
    fprintf(stderr, "freeing todo item %s\n", tmp->id);
    free(tmp->id);
    list_del(pos);
    free(tmp);
  }
}

static void free_missing_fragment_list()
{
  struct missing_fragment *tmp;
  struct list_head *pos, *n;

  list_for_each_safe(pos, n, &g_mfl.list) {
    tmp = list_entry(pos, struct missing_fragment, list);
    fprintf(stderr, "freeing missing fragment item %s\n", tmp->filename);
    if (tmp->filename) free(tmp->filename);
    if (tmp->filepath) free(tmp->filepath);
    list_del(pos);
    free(tmp);
  }
}

static void free_file_list()
{
  struct fileEntry *node;

  while (file_list) {
    node = file_list;
    file_list = node->next;
    free(node);
  }
}

static void free_smartfile_list()
{
  struct sfileEntry *node;

  while (sfile_list) {
    node = sfile_list;
    sfile_list = node->next;
    free(node);
  }
}

static void free_dir_list()
{
  struct dirEntry *node;

  while (dir_list) {
    node = dir_list;
    dir_list = node->next;
    free(node);
  }
}

void check_todo_tasklist(int s)
{
  char task[1024];
  int i_type;

  FILE *fp = fopen(g_taskfile_todo, "r");
  if (fp) {
    while(fgets(task, sizeof(task), fp)) {
      writeDebug("todo(task:%s)", task);
      char *type = parse_xml(task, "type", NULL);
      if (type) {
	i_type = atoi(type);
	char *id = parse_xml(task, "id", NULL);
	if (id) {
	  if (!is_task_done(i_type, id)) {
	    if (i_type == TASKLIST_REMOTEMARK ||
		i_type == TASKLIST_REMOTEUNMARK) {
	      do_remote_mark(s, task, NULL, i_type);
	    } else if (i_type == TASKLIST_MISSINGFILE) {
	      do_missing_fragment(s, task, NULL);
	    }
	  } else {
	    /* will never happen: task in both todo/done list,
	     * but just in case */
	    remove_from_taskfile_todo(i_type, id);
	  }
	  free(id);
	}
	free(type);
      }  
    }
    fclose(fp);
  }
}

void change_todo_to_done(int type)
{
  struct todo_taskid *tmp;
  struct list_head *pos;

  list_for_each(pos, &g_tdl.list) {
    tmp = list_entry(pos, struct todo_taskid, list);
    if (tmp->type == type) {
      tmp->done = 1;
      remove_from_taskfile_todo(type, tmp->id);
      add_to_taskfile(TASKFILE_DONE, type, NULL, tmp->id);
    }
  }
}

struct upload_info {
  int sd;
  char *dir_path;
  struct sockaddr_storage *mss_addr;
};

void *upload_files(void *arg)
{
  char *dirname;
  char path[256], curdisk[256];
  struct fileEntry *node, *temp;
  struct sfileEntry *snode, *stemp;
  int64_t uploaded;
  char *dir_path, *servername;
  struct sockaddr_storage *mss_addr;
  int sd;

  struct upload_info *ui = (struct upload_info *)arg;
  dir_path = ui->dir_path;
  servername = g_busname;
  mss_addr = ui->mss_addr;
  sd = ui->sd;

  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);

  upload_done_size = 0LL;

  if (g_dvr_type == DVR_TYPE_DM500) {
    strncpy(curdisk, dir_path, sizeof(curdisk));
  } else {
    FILE *fp;
    fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (!fp) {
      writeDebug("dvrcurdisk error");
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_DVRCURDISK_ERROR);
    }

    if (!fgets(curdisk, sizeof(curdisk), fp)) {
      fclose(fp);
      writeDebug("dvrcurdisk error2");
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_DVRCURDISK_ERROR);
    }
    fclose(fp);
  }

  /* remove new line if found */
  if (curdisk[strlen(curdisk) - 1] == '\n')
    curdisk[strlen(curdisk) - 1] = '\0';

  /* upload dvrlog.txt file */  
  snprintf(path, sizeof(path),
	   "%s/_%s_/dvrlog.txt",
	   curdisk, servername);

  if (!upload_file(sd, mss_addr, curdisk, path, &uploaded)) {
    g_read_error = g_socket_error = 0;
    if (uploaded > 0) {
      upload_done_size += uploaded;
      writeDebug("uploaded %lld(%lld) bytes", uploaded, upload_done_size);
   }
  }
  if (g_read_error >= 3) {
    upload_thread_end = 1;
    pthread_exit((void *)EXIT_CODE_HD_ERROR);
  }
  if (g_socket_error >= MAX_SOCK_ERROR) {
    upload_thread_end = 1;
    pthread_exit((void *)EXIT_CODE_NO_REPLY);
  }
  if (g_upload_timeout) {
    upload_thread_end = 1;
    pthread_exit((void *)EXIT_CODE_UPLOAD_TIMEOUT);
  }

  /* now upload smartlog files */
  sfile_list = slistsort(sfile_list);
  stemp = sfile_list;
  while (stemp) {    
    snode = stemp;
    stemp = snode->next;
    
    dirname = get_path_for_list_id(snode->fi.pathId);
    if (!dirname)
      continue;

    make_smartfile_name(dirname, servername, &snode->fi,
			path, sizeof(path));
    
    if (!upload_file(sd, mss_addr, curdisk, path, &uploaded, 1)) {
      g_read_error = g_socket_error = 0;
      if (uploaded > 0) {
	upload_done_size += uploaded;
	writeDebug("uploaded %lld(%lld) bytes", uploaded, upload_done_size);
      }
      if (!rename_smartfile(path, 'N')) {
	if (snode->fi.type == TYPE_SMART) {      
	  /* rename 002 file, too (for mDVR500) */
	  path[strlen(path) - 1] = '2';
	  struct stat sb;
	  if (!stat(path, &sb))
	    rename_smartfile(path, 'N');
	}
      }
    }
    if (g_read_error >= 3) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_HD_ERROR);
    }
    if (g_socket_error >= MAX_SOCK_ERROR) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_NO_REPLY);
    }
    if (g_upload_timeout) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_UPLOAD_TIMEOUT);
    }
  }

  struct missing_fragment *tmp;
  struct list_head *pos;
  list_for_each(pos, &g_mfl.list) {
    tmp = list_entry(pos, struct missing_fragment, list);

    if (tmp->filepath == NULL)
      continue;

    if (tmp->to == -1) {
      writeDebug("resuming from fragment %d(%s)", tmp->from, tmp->filename);
    } else {
      writeDebug("missing fragment %d(%s)", tmp->from, tmp->filename);
    }

    snprintf(path, sizeof(path), "%s/%s", tmp->filepath, tmp->filename);
    if (!upload_file(sd, mss_addr, curdisk, path,
		     &uploaded, 1, tmp->from, tmp->to)) {
      g_read_error = g_socket_error = 0;
      if (uploaded > 0) {
	upload_done_size += uploaded;
	writeDebug("uploaded %lld(%lld) bytes", uploaded, upload_done_size);
      }
      rename_file(path, 'N');
    }
    if (g_read_error >= 3) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_HD_ERROR);
    }
    if (g_socket_error >= MAX_SOCK_ERROR) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_NO_REPLY);
    }
    if (g_upload_timeout) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_UPLOAD_TIMEOUT);
    }
  }

  /* if it reached here, missing files are already uploaded */
  change_todo_to_done(TASKLIST_MISSINGFILE);

  file_list = listsort(file_list);
  
  temp = file_list;
  while(temp) {
    node = temp;
    temp = node->next;

    dirname = get_path_for_list_id(node->fi.pathId);
    if (!dirname)
      continue;

    make_file_name(dirname, servername, &node->fi,
		    path, sizeof(path));

    if (!upload_file(sd, mss_addr, curdisk, path, &uploaded, 1)) {
      g_read_error = g_socket_error = 0;
      if (uploaded > 0) {
	upload_done_size += uploaded;
	writeDebug("uploaded %lld(%lld) bytes", uploaded, upload_done_size);
      }
      rename_file(path, 'N');
    }
    if (g_read_error >= 3) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_HD_ERROR);
    }
    if (g_socket_error >= MAX_SOCK_ERROR) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_NO_REPLY);
    }
    if (g_upload_timeout) {
      upload_thread_end = 1;
      pthread_exit((void *)EXIT_CODE_UPLOAD_TIMEOUT);
    }
  } /* while */  
  
  upload_thread_end = 1;
  pthread_exit((void *)0);
}

void scan_resume_info(char *rootdir, char *servername)
{
  DIR *dir;
  struct dirent *de;
  char path[256];

  if (g_dvr_type == DVR_TYPE_DM500) {
    read_resume_info(rootdir, servername);
    return;
  }

  /* mDVR510 */
  dir = opendir(rootdir);
  if (dir) {
    while ((de = readdir(dir)) != NULL) {
      if ((de->d_name[0] == '.') || (de->d_type != DT_DIR))
	continue;

      snprintf(path, sizeof(path), "%s/%s", rootdir, de->d_name);
      if (!read_resume_info(path, servername)) {
	break;
      }
    }
    closedir(dir);
  }
}

void do_login(int *sd, struct sockaddr_storage *addr)
{
  while (1) {
    writeDebug("Trying to Login to mini smartserver");
    int wait = login_to_mss(sd, addr);
    if (wait > 0) {
      sleep(wait);
    } else {
      break;
    }
  }
}

/*
 * arguments:
 *   - 1.wifi_dev_name: device name through which "server discovery" is sent
 *   - 2.bus_name
 *   - 3.password: password for server access (usually 247SECURITY)
 *   - 4.mount_dir: directory name where the usb HD is mounted (DM500)
 *                : or where usb HDs can be found(DM510)
 *   - 5.dvr_type: 500 (mDVR50x), 510 (mDVR51x)
 *   - 6.upload_speed_limimt: bandwidth control
                             (KBytes/sec, 0 or non-numeric: no control)
 *   - 7.reseved: 
 *   - 8.exit_port: TCP port where exit code is sent to. (can be 0)
 *   - 9.anormalie: video lost? 'Y' or 'N'.
 *   - 10.timezone: optional, glibc TZ string
 *                  if omitted, TZ will be read from /var/run/TZ
 */
int main(int argc, char **argv)
{
  int sd = -1;
  struct in_addr ip, netmask, broadcast;
  struct sockaddr_storage mss_addr;
  int retry = 0, ret = 0;

  INIT_LIST_HEAD(&g_pel.list);
  INIT_LIST_HEAD(&g_rml.list);
  INIT_LIST_HEAD(&g_ruml.list); /* remote unmark list */
  INIT_LIST_HEAD(&g_tdl.list);
  INIT_LIST_HEAD(&g_mfl.list);

  if (signal(SIGBUS, buserror_handler) == SIG_IGN)
    signal(SIGBUS, SIG_IGN);

  if (argc < 10) {
    fprintf(stderr,
	    "Usage: %s wifi_dev_name bus_name password mount_dir "
	    "dvr_type[500|510|606|614] res1 res2 port anormalie [timezone]\n",
	    argv[0]);
    exit(EXIT_CODE_BAD_ARGUMENT);
  }


  g_dvr_type = DVR_TYPE_DM500;
  strcpy(g_ext, "mp4");
  if (!strcmp(argv[5], "510")) {
    g_dvr_type = DVR_TYPE_DM510;
    strcpy(g_ext, "266");
  } else if (!strcmp(argv[5], "606")) {
    g_dvr_type = DVR_TYPE_DM606;
    strcpy(g_ext, "mp5");
  } else if (!strcmp(argv[5], "614")) {
    g_dvr_type = DVR_TYPE_DM614;
    strcpy(g_ext, "265");
  }

  int i, all_numeric = 1;
  for (i = 0; i < (int)strlen(argv[6]); i++) {
    if (!isdigit(argv[6][i])) {
      all_numeric = 0;
      break;
    }
  }
  g_uploadrate = 0;
  if (all_numeric) {
    g_uploadrate = atoi(argv[6]) * 1024;
  }

  strncpy(g_busname, argv[2], sizeof(g_busname));
  g_exit_port = atoi(argv[8]);
  g_videolost = ((*argv[9] == 'Y') || (*argv[9] == 'y')) ? 1 : 0;

  if (argc >= 11) {
    setTZ(argv[10]);
  } else {
    setTZ(NULL);
  }

  setDebugFileName(argv[4]);
  setTaskFileName(argv[4]);
  writeDebug("Smartftp started(%d).", g_uploadrate);

  /* get ip address assigned to the specified device
   * to be used in "discover_mss"
   * Do this until DHCP assigns an IP
   */
  while (retry < 10) {
    fprintf(stderr, "Querying IP for %s\n", argv[1]);
    if (get_wifi_ip(argv[1], &ip, &netmask, &broadcast)) {
      writeDebug("can't get ip/netmask");
      exit_out(EXIT_CODE_NETWORK_ERROR);
    }

    if (ip.s_addr)
      break;

    retry++;
    sleep(1);
  }

  if (ip.s_addr == 0) {
    writeDebug("ip address not assigned yet");
    exit_out(EXIT_CODE_NO_DHCP);
  }

  /* setup multicast */
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
	   "route del -net 224.0.0.0 netmask 240.0.0.0;"
	   "route add -net 224.0.0.0 netmask 240.0.0.0 dev %s",
	   argv[1]);
  //fprintf(stderr, "%s\n", cmd);
  system(cmd);

  writeDebug("Trying to find mini smartserver");
  /* send discover message to the specified subnet */
  if (discover_mss(&mss_addr)) {
    writeDebug("Failed to find mini smartserver");
    exit_out(EXIT_CODE_NO_SERVER);
  }

  writeDebug("Found mini smartserver");

  /* login for task list */
  do_login(&sd, &mss_addr);
  if (sd == -1) {
    writeDebug("Login failure");
    exit_out(EXIT_CODE_LOGIN_ERROR);
  }

  /* any pending task? */
  check_todo_tasklist(sd);

  writeDebug("Login success, checking task list");
  get_tasklist(sd);

  /* close connection during file cutting */
  writeDebug("Closing connection after task list");
  close(sd); 
  sd = -1;

  scan_resume_info(argv[4], argv[2]);

  writeDebug("scanning Smartlog files");
  if (g_dvr_type == DVR_TYPE_DM500) {
    scanSmartFiles(argv[4], argv[2]);
  } else {
    scan_smartlog_disks(argv[4], argv[2]);
  }

  merge_peak_event_list_to_remote_mark_list();

  writeDebug("scanning recording files");
  if (g_dvr_type == DVR_TYPE_DM500) {
    scan_files(argv[4], argv[2], NULL, &create_mark_file_list);
  } else {
    scan_recording_disks(argv[4], argv[2], &create_mark_file_list);
  }

  /* if it reached here, remote mark are already marked */
  change_todo_to_done(TASKLIST_REMOTEMARK);
  change_todo_to_done(TASKLIST_REMOTEUNMARK);

  /* login again for real uploading */
  do_login(&sd, &mss_addr);
  if (sd == -1) {
    writeDebug("Login failure");
    exit_out(EXIT_CODE_LOGIN_ERROR);
  }

  ret = send_status_report(sd);
  if (ret == 0) {
    writeDebug("mSS cannot accept uploading");
    goto main_end; /* mSS cannot accept uploading */
  } else if (ret == -1) {
    exit_out(EXIT_CODE_NETWORK_ERROR);
  }

  void *status;
  struct upload_info ui;
  ui.dir_path = argv[4];
  ui.sd = sd;
  ui.mss_addr = &mss_addr;
  pthread_mutex_lock(&mutex);
  if (pthread_create(&upload_thread, NULL, upload_files, &ui)) {
    upload_thread = 0;
  } else {
    pthread_cond_wait(&cond, &mutex);
  }
  pthread_mutex_unlock(&mutex);

  /* wait until upload thread exits with time out */
  while (g_watchdog < 600) { /* 10 min time out for file transfer activity*/
    if (upload_thread_end) {
      pthread_join(upload_thread, &status);
      ret = (int)status;
      break;
    }

    g_watchdog++;

    sleep(1);
  }

  if (g_watchdog >= 600)
    ret = EXIT_CODE_FILE_TRANSFER_TIMEOUT;
  

 main_end:
  if (sd != -1)
    close(sd);

  free_missing_fragment_list();
  free_todo_list();
  free_peak_event_list();
  free_time_range_list(&g_ruml);
  free_time_range_list(&g_rml);
  free_smartfile_list();
  free_file_list();
  free_dir_list();

  writeDebug("mSS exit:%d", ret);
  exit_out(ret);

  return 0;
}
