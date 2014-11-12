#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/vfs.h>
#include <curl/curl.h>
#include "upload.h"
#include "parseconfig.h"
#include "gforce_peak.h"
#include "dvrproto.h"
//#include "hikfile.h"

//#define NO_SERVER_TEST

#define MAX_RETRY 2
#define SMART_PORT 49954
enum {DVR_TYPE_DM500, DVR_TYPE_DM510};
#define EXIT_CODE_SUCCESS      0
#define EXIT_CODE_BAD_ARGUMENT 1
#define EXIT_CODE_NO_DHCP      2
#define EXIT_CODE_NO_SERVER    3
#define EXIT_CODE_WRONG_FILE_TREE    4
#define EXIT_CODE_LOGIN_ERROR    5
#define EXIT_CODE_FILE_TRANSFER_TIMEOUT      6
#define EXIT_CODE_NO_FTP       CURLE_COULDNT_CONNECT /* 7 */
#define EXIT_CODE_NETWORK_ERROR      8
#define EXIT_CODE_DVRCURDISK_ERROR   9
#define EXIT_CODE_CURLINIT_ERROR    10
#define EXIT_CODE_HD_ERROR    11
#define EXIT_CODE_BUS_ERROR      500

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static const char smart_dir[] = "/mnt/ide";
#define SMART_DIR_LEN (sizeof(smart_dir) / sizeof(char) - 1)
enum {TYPE_SMART, TYPE_IMPACT, TYPE_PEAK};

#define SMARTLOG_SIZE (1024 * 1024) /* 1 MB */
//#define USE_READFUNCTION
#define DBGOUT

#if 0
struct dvrtime {
  /* use get_xxx helper functions */
  unsigned int time; /* MSB to LSB: 4bit(month),5(day),6+6+6(hour,min,sec) */
  unsigned short year, millisec;
};
#endif

struct remote_mark {
  time_t from, to;
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

struct memory_t {
  char *memory;
  size_t size;
};

struct upload_info {
  char *dir_path, *servername, *ftp_user, *ftp_pass;
  int port;
  struct in_addr server_ip;
};

int dvr_type;
struct fileEntry *file_list;
struct sfileEntry *sfile_list;
struct dirEntry *dir_list;

/* # of files to upload */
int upload_file_count;
/* total bytes of all files to upload for progress meter */
long long upload_size, upload_done_size;
int event_count;
long long event_duration;
pthread_t upload_thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
char *resume_info_path; /* path to the resumeinfo file */
char *resume_file; /* file to resume upload */
struct fileInfo resume_fi;;
int resume_offset;
int exit_port;
int write_error_count;
struct remote_mark g_rml;
struct peak_event g_pel;
struct gforce_trigger_point peak_tp = {2.0, -2.0, 2.0, -2.0, 5.0, -3.0};
int pre_peak_event = 30, post_peak_event = 30;
int peak_time_in_utc;
char g_ext[4];

char dbgfile[128], dbgmsg[256];
void setDebugFileName(char *rootdir)
{
#ifdef DBGOUT
  char path[256];
  if (dvr_type == DVR_TYPE_DM500) {
    snprintf(dbgfile, sizeof(dbgfile),
	     "%s/smartlog/smartftp.txt",
	     rootdir);
  } else {
    FILE *fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (fp) {
      if (fscanf(fp, "%s", dbgfile) == 1) {
	snprintf(path, sizeof(path), "%s/smartlog", dbgfile);
	mkdir(path, 0777);	
	strcat(dbgfile, "/smartlog/smartftp.txt");
      }
      fclose(fp);
    }
#if 0
    DIR *dir;
    struct dirent *de;
    char path[256];
    
    dir = opendir(rootdir);
    if (dir) {
      while ((de = readdir(dir)) != NULL) {
	if ((de->d_name[0] == '.') || (de->d_type != DT_DIR))
	  continue;

	/* check if smartlog folder exists */
	snprintf(path, sizeof(path), "%s/%s/smartlog", rootdir, de->d_name);

	DIR *dir1;
	dir1 = opendir(path);
	if (dir1) {
	  snprintf(dbgfile, sizeof(dbgfile),
		   "%s/smartftp.txt",
		   path);
	  closedir(dir1);
	  break;
	}
      }
      closedir(dir);
    }
#endif
  }


  if (dbgfile[0]) {
    struct stat sb;
    if (!stat(dbgfile, &sb)) {
      /* delete file if too big */
      if (sb.st_size > SMARTLOG_SIZE) {
	unlink(dbgfile);
      }
    } 
  }
#endif
}

static int
get_fileinfo_from_fullname(struct fileInfo *fi, char *fullname,
			   char *servername);
			   
void check_and_add_to_list(char *dir_path, char *servername, char *fullname,
			   struct fileInfo *fi);
			   
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
  if (dbgfile[0]) {
    fp = fopen(dbgfile, "a");
    if (fp != NULL) {
      fprintf(fp, "%s : %s\n", ts, str);
      fclose(fp);
    }
  }
#endif
}

void buserror_handler(int signum)
{
  writeDebug("Bus Error");

  exit(EXIT_CODE_BUS_ERROR);
}

static size_t
write_memory(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  struct memory_t *mem = (struct memory_t *)data;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  }
  return realsize;
}

#define WIDTH   16
#define DBUFSIZE 1024
int dump(unsigned char *s, int len)
{
    char buf[DBUFSIZE],lbuf[DBUFSIZE],rbuf[DBUFSIZE];
    unsigned char *p;
    int line,i;

    p =(unsigned char *)s;
    for(line = 1; len > 0; len -= WIDTH, line++) {
      memset(lbuf, '\0', DBUFSIZE);
      memset(rbuf, '\0', DBUFSIZE);
      for(i = 0; i < WIDTH && len > i; i++,p++) {
	sprintf(buf,"%02x ",(unsigned char) *p);
	strcat(lbuf,buf);
	sprintf(buf,"%c",(!iscntrl(*p) && *p <= 0x7f) ? *p : '.');
	strcat(rbuf,buf);
      }
      fprintf(stderr, "%04x: %-*s    %s\n",line - 1, WIDTH * 3, lbuf, rbuf);
    }
    if(!(len%16)) {
      fprintf(stderr, "\n");
    }
    return line;
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

#if 0
static char get_month(struct dvrtime *t)
{
  return t->time >> 28;
}

static char get_day(struct dvrtime *t)
{
  return (t->time >> 23) & 0x1f;
}

static char get_hour(struct dvrtime *t)
{
  return (t->time >> 17) & 0x3f;
}

static char get_min(struct dvrtime *t)
{
  return (t->time >> 11) & 0x3f;
}

static char get_sec(struct dvrtime *t)
{
  return (t->time >> 5) & 0x3f;
}

static void set_month(struct dvrtime *t, char month)
{
  t->time &= ~0xf0000000;
  t->time |= (month << 28);
}

static void set_day(struct dvrtime *t, char day)
{
  t->time &= ~0x0f800000;
  t->time |= ((day & 0x1f) << 23);
}

static void set_hour(struct dvrtime *t, char hour)
{
  t->time &= ~0x007e0000;
  t->time |= ((hour & 0x3f) << 17);
}

static void set_min(struct dvrtime *t, char min)
{
  t->time &= ~0x0001f800;
  t->time |= ((min & 0x3f) << 11);
}

static void set_sec(struct dvrtime *t, char sec)
{
  t->time &= ~0x000007e0;
  t->time |= ((sec & 0x3f) << 5);
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

static void free_remote_mark_list()
{
  struct remote_mark *tmp;
  struct list_head *pos, *n;

  list_for_each_safe(pos, n, &g_rml.list) {
    tmp = list_entry(pos, struct remote_mark, list);
    fprintf(stderr, "freeing rm item %ld,%ld\n", tmp->from, tmp->to);
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

void setTZ(char *tzstr)
{
  FILE * fp;
  char buf[128];

  if (tzstr == NULL) {
    // set TZ environment
    if (dvr_type == DVR_TYPE_DM500) {
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

int connectTcp(char *addr, short port)
{
  int sfd;
  struct sockaddr_in destAddr;

  //fprintf(stderr, "connectTcp(%s,%d)\n", addr,port);
  sfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    return -1;

  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(port);
  destAddr.sin_addr.s_addr = inet_addr(addr);
  memset(&(destAddr.sin_zero), '\0', 8);

  if (connect(sfd, (struct sockaddr *)&destAddr,
	      sizeof(struct sockaddr)) == -1) {
    perror("smartftp:connectTcp");
    close(sfd);
    return -1;
  }

  return sfd;
}

void send_exit_code(int code, short port)
{
  int sfd, bi;
  unsigned char txbuf[4];

  writeDebug("send_exit_code:%d", code);

  while(1) {
    sfd = connectTcp("127.0.0.1", port);
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
  if (exit_port) 
    send_exit_code(code, exit_port);
  else
    exit(code);
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
  node = malloc(sizeof(struct dirEntry));
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

static int 
add_file_to_list(char *path, struct fileInfo *fi)
{
  struct fileEntry *node;

  node = malloc(sizeof(struct fileEntry));
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

static int 
add_smartfile_to_list(char *path,
		      unsigned short year, char month, char day, 
		      char hour, char min, char sec,
		      char type, char lock, 
		      unsigned short fileno,
		      off_t size)
{
  struct sfileEntry *node;

  node = malloc(sizeof(struct sfileEntry));
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


  fprintf(stderr, "scanSmartFiles\n");

  //*usbMounted = 1;
  strncpy(dirpath, dir_path, sizeof(dirpath));

  sprintf(dname, "%s/smartlog", dirpath);
  dir = opendir(dname);
  if (dir == NULL) {
    perror("scanSmartFiles:opendir");
    if (dvr_type == DVR_TYPE_DM500) {
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
    
   // if (de->d_type != DT_REG)
    //  continue;

    if ((strlen(de->d_name) > servernameLen) &&
	!strncmp(servername, de->d_name, servernameLen)) {
      ret = sscanf(de->d_name + servernameLen,
		   "_%04hd%02hhd%02hhd_%c.%03hd",
		   &year, &month, &day, &lock, &fileno);
      if ((ret == 5) && (fileno == 1 || fileno >= 10 )) {
	if (lock == 'L') {
	  snprintf(fullname, sizeof(fullname),
		   "%s/%s",
		   dname, de->d_name);
	  if (stat(fullname, &st)) {
	    perror("scanSmartFiles");
	    continue;
	  }
	  if (dvr_type == DVR_TYPE_DM510) {
	    scan_GPS_log(fullname, &g_pel,
			 pre_peak_event, post_peak_event, &peak_tp);
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
	      if (dvr_type == DVR_TYPE_DM510) {
		scan_peak_data(fullname, &g_pel, peak_time_in_utc,
			       pre_peak_event, post_peak_event, &peak_tp);
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

/* send discover message to the specified subnet
 * and get the server's ip/port
 */
int discover_smart_server(struct in_addr ip,
			  struct in_addr netmask,
			  struct in_addr broadcast,
			  struct in_addr *serverIp, int *port)
{
  int yes = 1, tried = 0;
  int sockfd, nbytes, ret;
  struct sockaddr_in hisAddr;
  fd_set master, readfd;
  struct timeval tv;
  unsigned char buf[256], sendbuf[256];
  socklen_t addrlen;

  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    return 1;
  }
    
  /*
   * For this to work, default gateway should be set.
   * If not set, only directed broadcast (to subnet) will work.
   * You can set it to its own ip address
   * If you don't want this limitation, see the PF_PACKET sample code below.
   * You have to add ip,udp header manually though
   */
  if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int)) < 0) {
    perror("setsockopt");
    close(sockfd);
    return 1;
  }
  
  /* bind to a specific NIC's ip address
   * to send/receive only through that interface
   */
  hisAddr.sin_family = AF_INET;
  hisAddr.sin_port = htons(SMART_PORT);
  //hisAddr.sin_addr = ip;
  hisAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(&hisAddr.sin_zero, '\0', 8);

  fprintf(stderr, "binding to %s\n", inet_ntoa(hisAddr.sin_addr));
  
  if (bind (sockfd, (struct sockaddr *)&hisAddr, sizeof(hisAddr)) < 0) {
    perror ("bind");
    close(sockfd);
    return 1;
  }
  
  FD_ZERO(&master);
  FD_SET(sockfd, &master);
  
  int serverport = 0;
  int sendit = 1;
  strncpy((char *)sendbuf, "lookingforsmartserver", sizeof(sendbuf));
 
  while (tried < 60) { 
    if (sendit) {
      hisAddr.sin_addr = broadcast;
      /* don't use INADDR_BROADCAST,
       * otherwise broadcast goes through eth1 only
       */
      //hisAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
      //writeDebug("discovering smartserver");
      fprintf(stderr, "broadcasting to %s\n", inet_ntoa(hisAddr.sin_addr));
      nbytes = sendto(sockfd, sendbuf, strlen((char *)sendbuf), 0,
		      (struct sockaddr *)&hisAddr, sizeof(hisAddr));
      if (nbytes == -1) {
	perror("discover:sendto");
	break;
      } else {
	fprintf(stderr, "broadcast %d bytes\n", nbytes);
      }
    }
    sendit = 1;
    
    tv.tv_sec = 1;
    tv.tv_usec = 0;//500000;
    readfd = master;
    ret = select(sockfd + 1, &readfd, NULL, NULL, &tv);
    if (ret == -1) {
      perror("select");
      break;
    } else if (!ret) {
      tried++;
      continue;
    } else {
      nbytes = recvfrom(sockfd, buf, sizeof(buf), 0,
			(struct sockaddr *)&hisAddr, &addrlen);
      if (nbytes == -1) {
	perror("recvfrom");
	break;
      }

      if (nbytes >= sizeof(buf)) {
	/* message too big */
	tried++;
	continue;
      }

      buf[nbytes] = 0;
      fprintf(stderr, "rcvd %s,%d\n", buf,nbytes);
      ret = sscanf((char *)buf, "iamserver%d", &serverport);
      if (ret == 1) {
	break;
      } else {
	if (strncmp((char *)buf, (char *)sendbuf, strlen((char *)sendbuf))) {
	  tried++;
	  fprintf(stderr, "trying again %d\n", tried);
	} else {
	  /* got echo from we sent, recv again */
	  sendit = 0;
	}
	continue;
      }
    }
  }
  
  close(sockfd);

  if (!serverport)
    return 1;

  *port = serverport;
  *serverIp = hisAddr.sin_addr;
  fprintf(stderr, "found server:%s(%d)\n", inet_ntoa(hisAddr.sin_addr),
	  serverport);

  return 0;
}

void add_remote_mark_to_head(struct remote_mark *rml, time_t start, time_t end)
{
  struct remote_mark *tmp;
    tmp = (struct remote_mark *)malloc(sizeof(struct remote_mark));
    tmp->from = start;
    tmp->to = end;
    list_add(&tmp->list, &rml->list);
    fprintf(stderr, "added rm item %ld,%ld to head\n", start, end);
}

void add_remote_mark_to_tail(struct remote_mark *rml, time_t start, time_t end)
{
  struct remote_mark *tmp;
    tmp = (struct remote_mark *)malloc(sizeof(struct remote_mark));
    tmp->from = start;
    tmp->to = end;
    list_add_tail(&tmp->list, &rml->list);
    fprintf(stderr, "added rm item %ld,%ld to tail\n", start, end);
}

int add_mark_entry_to_list(struct remote_mark *rml, time_t from, time_t to)
{
  struct list_head *pos, *n;
  struct remote_mark *cur;

  if (list_empty(&rml->list)) {
    add_remote_mark_to_head(rml, from, to);
    return 1;
  } else {
    /* check if we can insert in the begining of the list */
    cur = list_entry(rml->list.next, struct remote_mark, list);
    /*fprintf(stderr, "comparing rm 1st item %ld,%ld with %ld,%ld\n",
      cur->from, cur->to, from, to);*/
    /* -------|<----NEW---->|--------|<----EVENT---->|------
     * insert new record
     */ 
    if (to < cur->from) {
      add_remote_mark_to_head(rml, from, to);
      return 1;
    }
    
    list_for_each_safe(pos, n, &rml->list) {
      cur = list_entry(pos, struct remote_mark, list);
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
    add_remote_mark_to_tail(rml, from, to);
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
			   tmp->start - pre_peak_event,
			   tmp->end + post_peak_event);
  }
}

int add_mark_file_range_to_list(char *range)
{
  char *str, *end, *start;
  char strtime1[256], strtime2[256];
  int sno;
  struct tm from, to;

  /* make a local copy, as we'll have to modify it */
  str = strdup(range);
  fprintf(stderr, "%s", str);

  start = str;
  while ((end = strchr(start, '\n'))) {
    *end = '\0';
    //fprintf(stderr, "%s\n", start);
    if (3 == sscanf(start, "%d,%[^,],%s", &sno, strtime1, strtime2)) {
      if (6 == sscanf(strtime1,
		      "%04d%02d%02d%02d%02d%02d",
		      &from.tm_year, &from.tm_mon, &from.tm_mday,
		      &from.tm_hour, &from.tm_min, &from.tm_sec)) {
	if (6 == sscanf(strtime2,
			"%04d%02d%02d%02d%02d%02d",
			&to.tm_year, &to.tm_mon, &to.tm_mday,
			&to.tm_hour, &to.tm_min, &to.tm_sec)) {
	  from.tm_year -= 1900;
	  from.tm_mon -= 1;
	  from.tm_isdst = -1;
	  to.tm_year -= 1900;
	  to.tm_mon -= 1;
	  to.tm_isdst = -1;
	  add_mark_entry_to_list(&g_rml, mktime(&from), mktime(&to));
	}
      }
    }

    /* get the next range */
    start = end + 1;
  }

  if (str) free(str);

  return 0;
}

int get_mark_info(struct in_addr server_ip, int port,
		  char *busname, char *password)
{
  CURL *curl;
  CURLcode res;
  char *enc, url[128], postfields[256];
  char *enc_busname = NULL, *enc_pass = NULL, *data = NULL;
  struct memory_t chunk;
  int retval = 1, data_size = 0;
  int curl_init = 0;

  chunk.memory = NULL;
  chunk.size = 0;

  /*
   * POST /smart/vlogin_mark_get.jsp HTTP/1.1\r\n
   * Host: 192.168.1.10:8081\r\n
   * Accept: * / *\r\n (no space actually)
   * Content-Length: 29\r\n
   * Content-Type: application/x-www-form-urlencoded\r\n
   * \r\n
   * user=bus%231&pass=247SECURITY
   * Test with curl:
   * curl --data-urlencode "user=bus#1" --data-urlencode "pass=247SECURITY" 
   *      http://192.168.1.10:8081/smart/vlogin_mark_get.jsp
   */
  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "curl_easy_init error\n");
    return 1;
  }
  curl_init = 1;

  /* receive data to memory */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  /* user agent in the HTTP header */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "smartftp");

  /* set URL */
  snprintf(url, sizeof(url),
	   "http://%s:%d/smart/vlogin_mark_get.jsp",
	   inet_ntoa(server_ip), port);
  curl_easy_setopt(curl, CURLOPT_URL, url);

  enc = curl_easy_escape(curl, busname, strlen(busname));
  if (!enc) {
    fprintf(stderr, "curl_easy_escape error\n");
    goto mark_end;
  }

  enc_busname = strdup(enc);
  free(enc);

  /* add '&'(0x26) */
  snprintf(postfields, sizeof(postfields), "user=%s\x26pass=", enc_busname);

  enc = curl_easy_escape(curl, password, strlen(password));
  if (!enc) {
    fprintf(stderr, "curl_easy_escape error\n");
    goto mark_end;
  }

  enc_pass = strdup(enc);
  free(enc);

  strncat(postfields, enc_pass, sizeof(postfields));
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, -1LL);

  fprintf(stderr, "POST %s\n", postfields);
  res = curl_easy_perform(curl);

  if (!chunk.memory) {
    fprintf(stderr, "no data from server\n");
    goto mark_end;
  }

  /* save data */
  data = malloc(chunk.size + 1);
  data_size = chunk.size;
  memcpy(data, chunk.memory, chunk.size);
  data[data_size] = 0;

  free(chunk.memory);
  chunk.memory = NULL;
  chunk.size = 0;

  curl_easy_cleanup(curl);
  curl_init = 0;

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "curl_easy_init error\n");
    goto mark_end;
  }
  curl_init = 1;
  /* receive data to memory */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  /* user agent in the HTTP header */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "smartftp");

  /* set URL */
  snprintf(url, sizeof(url),
	   "http://%s:%d/smart/vlogin_mark_set.jsp",
	   inet_ntoa(server_ip), port);
  curl_easy_setopt(curl, CURLOPT_URL, url);

  int code, entry;
  sscanf(data, "%d,%d", &code, &entry);
  fprintf(stderr, "code:%d,entry:%d\n", code, entry);

  /* we receive:
   *  1. no files to mark: (4,0\n)
   *     20 20 0d 0a 20 20 34 2c 30 0a
   *                       --    --
   *     0d 0a 20 0d 0a 0d 0a 0d 0a
   *  2. 1 range to mark:  (4,1\n)
   *                        6: this number changes...
   *                       (6,20090113154753,20090113155753\n)
   *     20 20 0d 0a 20 20 34 2c 31 0a 36 2c 32 30 30 39 (...) 35 33 0a
   *     0d 0a 20 0d 0a 0d 0a 0d 0a
   *  2. 2 range to mark:  (4,2\n)
   *                       (7,20090108154815,20090108155815\n)
   *                       (8,20090114160354,20090114180354\n)
   */
   if ((code == 4) && (entry > 0)) {
    char *ptr, *newline;

    ptr = strchr(data, ',');
    if (ptr) {
      newline = strchr(ptr, '\n');
      if (newline) {
	ptr = newline + 1;

	/* get rid of the trailing \r\n's */
	newline = strstr(ptr, "\r\n");
	*newline = 0;

	add_mark_file_range_to_list(ptr);
#if 1

	/* post data for ack */
	snprintf(postfields, sizeof(postfields),
		 "user=%s\x26pass=%s\x26jobs=%s",
		 enc_busname, enc_pass, ptr);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, -1LL);

	/* send it */
	fprintf(stderr, "POST %s\n", postfields);
	res = curl_easy_perform(curl);

	if (chunk.memory) {
	  /* response to vlogin_mark_set.jsp(ack) is same as
	   * response to vlogin_mark_get with no files to mark (4,0)
	   */
	  //dump((unsigned char *)chunk.memory, chunk.size);
	  free(chunk.memory);
	}
#endif
	retval = 0;
      }
    }
  }

 mark_end:
  if (data) free(data);
  if (enc_pass) free(enc_pass);
  if (enc_busname) free(enc_busname);

  if (curl_init) curl_easy_cleanup(curl);

  return retval;
}

int login_to_smartserver(struct in_addr server_ip, int port,
			 char *busname, char *password, char *anormalie)
{
  CURL *curl;
  CURLcode res;
  char *enc, url[256], temp[128], postfields[128];
  char *enc_busname = NULL, *enc_pass = NULL;
  struct memory_t chunk = {0, 0}, header = {0, 0};
  int retval = -1, servercode;
  time_t now;
  struct tm tm;
  char issue = 'N';

  if (anormalie) {
    if ((anormalie[0] == 'Y') || (anormalie[0] == 'y')) {
      issue = 'Y';
    }
  }

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "curl_easy_init error\n");
    return -1;
  }

  /* user agent in the HTTP header */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "smartftp");

  /* set URL */
  snprintf(url, sizeof(url),
	   "http://%s:%d/smart/vlogin.jsp?",
	   inet_ntoa(server_ip), port);

  enc = curl_easy_escape(curl, busname, strlen(busname));
  if (!enc) {
    fprintf(stderr, "curl_easy_escape error\n");
    goto login_end;
  }

  enc_busname = strdup(enc);
  free(enc);

  /* add '&'(0x26) */
  snprintf(postfields, sizeof(postfields), "user=%s\x26pass=", enc_busname);

  enc = curl_easy_escape(curl, password, strlen(password));
  if (!enc) {
    fprintf(stderr, "curl_easy_escape error\n");
    goto login_end;
  }

  enc_pass = strdup(enc);
  free(enc);

  strncat(postfields, enc_pass, sizeof(postfields));

  /* &size=0&files=0&time=YYYYMMDDhhmmss&issue=N&eventcount=3&duration=250 */
  now = time(NULL);
  localtime_r(&now, &tm);

  snprintf(temp, sizeof(temp),
	   "&size=%lld&files=%d&time=%04d%02d%02d%02d%02d%02d"
	   "&issue=%c&eventcount=%d&duration=%lld",
	   upload_size, upload_file_count,
	   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	   tm.tm_hour, tm.tm_min, tm.tm_sec,
	   issue, event_count, event_duration);
  strncat(postfields, temp, sizeof(postfields));

  //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
  //curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, -1LL);
  /* using GET now instead of POST */
  strncat(url, postfields, sizeof(url));

  //fprintf(stderr, "POST %s\n", postfields);

  while(1) {
    /* receive data to memory */
    if (chunk.memory) free(chunk.memory);
    chunk.memory = NULL;
    chunk.size = 0;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    /* receive header to memory */
    if (header.memory) free(header.memory);
    header.memory = NULL;
    header.size = 0;
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_memory);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&header);

    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

    fprintf(stderr, "url:%s\n", url);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    res = curl_easy_perform(curl);

    if (!header.memory) {
      fprintf(stderr, "no header from server\n");
      break;
    }

    if (!chunk.memory) {
      fprintf(stderr, "no data from server\n");
      break;
    }

    header.memory[header.size - 1] = 0;
    //fprintf(stderr, "server header:%s\n", header.memory);

    //dump((unsigned char *)chunk.memory, chunk.size);
    chunk.memory[chunk.size - 1] = 0;
    fprintf(stderr, "server data:%s\n", chunk.memory);

    char *ptr = strstr(header.memory, "Location: ");
    if (ptr) {
      ptr += 10; /* http:// starts from here */
      char *ptr2 = strstr(ptr, ".jsp");
      if (ptr2) {
	ptr2 += 4; /* relocated URL ends here */
	*ptr2 = '\0';
	strncpy(url, ptr, sizeof(url));
      } else {
	/* should not happen */
	break;
      }
    } else {
      /* got the final code? */
      if (sscanf(chunk.memory, "%d", &servercode) == 1) {
	writeDebug("servercode:%d", servercode);

	if (servercode == 2) {
	  int sleeptime;
	  if (sscanf(chunk.memory, "%d,%d", &servercode, &sleeptime) == 2) {
	    /* if we get too big sleeptime,
	     * it'll be considered error(negative value)
	     */
	    retval = sleeptime;
	  }
	} else if (servercode == 3) {
	  retval = 0;
	}
      }
      break;
    }
  }

 login_end:
  if (header.memory) free(header.memory);
  if (chunk.memory) free(chunk.memory);
  if (enc_pass) free(enc_pass);
  if (enc_busname) free(enc_busname);

  curl_easy_cleanup(curl);

  return retval;
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

/* rename the recording file(full path) to 'L' */
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

#if 0
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

#endif

static int scan_files(char *dir_path, char *servername,
		      void *arg,
		      int (*doit)(char*, char*, char*, char*, void*))
{
  DIR *dir, *dirCh, *dirFile;
  struct dirent *de, *deCh, *deFile;
  char dname[128];
  struct stat findstat;

  sprintf(dname, "%s/_%s_", dir_path, servername);
  
  dir = opendir(dname);
  if (dir == NULL) {
    writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
    return 1;
  }
  
  while ((de = readdir(dir)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    sprintf(dname, "%s/_%s_/%s", dir_path, servername, de->d_name);
    if(de->d_type==DT_UNKNOWN){
	if( stat(dname, &findstat )==0 ){
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
      sprintf(dname, "%s/_%s_/%s/%s",
	      dir_path, servername,
	      de->d_name, deCh->d_name);

      if(deCh->d_type==DT_UNKNOWN){
             if( stat(dname, &findstat )==0 ){
                 if( S_ISDIR(findstat.st_mode) ) {
                       deCh->d_type = DT_DIR ;
                 }
                 else if( S_ISREG(findstat.st_mode) ) {
                     deCh->d_type = DT_REG ;
                 }
             }
      }
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
	//if (deFile->d_type != DT_REG) {
	 // continue;
	//}
	(*doit)(dir_path, servername, dname, deFile->d_name, arg);
      }
      
      closedir(dirFile);
    }
    
    closedir(dirCh);
  }
  
  closedir(dir);
  
  return 0;
}

/* only for 510x */
int get_full_path_for_fileinfo(char *path, int size,
			       struct fileInfo *fi, char *servername)
{
  if (!servername || !path || size < 0 || !fi)
    return 1;
  
  char *dirname = get_path_for_list_id(fi->pathId);
  if (!dirname)
    return 1;
  
  if (fi->prelock) { /* obsolete */
    snprintf(path, size,
	     "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
	     "%04d%02d%02d%02d%02d%02d_%u_%c_%u_%s.%s",
	     dirname, servername,
	     fi->year, fi->month, fi->day,
	     fi->ch, fi->ch,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec,
	     fi->len, fi->type, fi->prelock, servername,
	     (fi->ext == 'k') ? "k" : "266");
  } else {
    snprintf(path, size,
	     "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
	     "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	     dirname, servername,
	     fi->year, fi->month, fi->day,
	     fi->ch, fi->ch,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec,
	     fi->len, fi->type, servername,
	     (fi->ext == 'k') ? "k" : "266");
  }

  return 0;
}

static int check_servername(char *filename, char *servername,
			    char type, int *prelock)
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

static void 
check_resume_file(char *dir_path, char *servername,
		  struct fileInfo *fi)
{
  if (!resume_file && /* don't check if already found */
      (resume_fi.ch == fi->ch) &&
      (resume_fi.year == fi->year) &&
      (resume_fi.month == fi->month) &&
      (resume_fi.day == fi->day) &&
      (resume_fi.hour == fi->hour) &&
      (resume_fi.min == fi->min) &&
      (resume_fi.sec == fi->sec) &&
      (resume_fi.millisec == fi->millisec) &&
      (resume_fi.len == fi->len) &&
      (resume_fi.type == fi->type) &&
      (resume_fi.ext == fi->ext) &&
      (resume_fi.prelock == fi->prelock) &&
      (resume_fi.size == fi->size)) {
    char path[256];

    if (dvr_type == DVR_TYPE_DM500) {
      snprintf(path, sizeof(path),
	       "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
	       "%04d%02d%02d%02d%02d%02d%03d_%u_%c_%s.%s",
	       dir_path, servername,
	       fi->year, fi->month, fi->day,
	       fi->ch, fi->ch,
	       fi->year, fi->month, fi->day,
	       fi->hour, fi->min, fi->sec, fi->millisec,
	       fi->len, fi->type, servername,
	       (fi->ext == 'k') ? "k" : "mp4");
    } else {
      if (fi->prelock) {
	snprintf(path, sizeof(path),
		 "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
		 "%04d%02d%02d%02d%02d%02d_%u_%c_%u_%s.%s",
		 dir_path, servername,
		 fi->year, fi->month, fi->day,
		 fi->ch, fi->ch,
		 fi->year, fi->month, fi->day,
		 fi->hour, fi->min, fi->sec,
		 fi->len, fi->type, fi->prelock, servername,
		 (fi->ext == 'k') ? "k" : "266");
      } else {
	snprintf(path, sizeof(path),
		 "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
		 "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
		 dir_path, servername,
		 fi->year, fi->month, fi->day,
		 fi->ch, fi->ch,
		 fi->year, fi->month, fi->day,
		 fi->hour, fi->min, fi->sec,
		 fi->len, fi->type, servername,
		 (fi->ext == 'k') ? "k" : "266");
      }
    }

    resume_file = strdup(path);
  }

}

#if 0
static int 
where_to_cut_file(struct fileInfo *fi, time_t *cut_head, time_t *cut_tail)
{
  struct remote_mark *cur;
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
    cur = list_entry(pos, struct remote_mark, list);
    unsigned int len_in_sec;
    len_in_sec = ((dvr_type == DVR_TYPE_DM500) ? fi->len / 1000 : fi->len);
    /* don't use >= or <=, as we don't want to copy 1sec video from a file */
    if ((t > cur->from - len_in_sec) &&
	(t < cur->to)) {
      if (t < cur->from) {
	*cut_head = cur->from;
      }
      if (t + len_in_sec > cur->to) {
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
  struct remote_mark *cur;
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
    cur = list_entry(pos, struct remote_mark, list);
   // printf("From:%d To:%d\n",cur->from,cur->to);
    unsigned int len_in_sec;
    len_in_sec = ((dvr_type == DVR_TYPE_DM500) ? fi->len / 1000 : fi->len);
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

static int
get_filename_from_fullpath(char *fullname, char *name, int name_size)
{
  char *nameWithoutPath = strrchr(fullname, '/');
  if (nameWithoutPath) {
    strncpy(name, ++nameWithoutPath, name_size);
    if (strlen(name) > 0)
      return 0;
  }

  return 1;
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
  int delete = 0;
  time_t new_len;

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
	       &tm, "264");
  //writeDebug("newname:%s", newname);

  newfile = malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
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
  int bytes = fread(&fileh, 1,
		    sizeof(struct hd_file), fp);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fread header(%d, %s)", bytes, fullname);
    delete = 1;
    retval = -1;
    goto err_exit;
  }
  
  int encrypted = 1;
  if (fileh.flag == 0x484b4834)
    encrypted = 0;

  bytes = fwrite(&fileh, 1, sizeof(struct hd_file), fpnew);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fwrite header(%s)", newfile);
    delete = 1;
    retval = -1;
    goto err_exit;
  }

  /* copy video/audio frames to the new file */
  if (copy_frames(fp, fpnew, fpnew_k,
		  from_offset, to_offset, toffset_start,
		  filetime, &new_filetime, &new_len)) {
    delete = 1;
    retval = -1;
    goto err_exit;
  }

  /* give the correct file name to the new file */
  localtime_r(&new_filetime, &tm);
  get_filename(newname, sizeof(newname),
	       ch, (unsigned int)new_len, new_type, servername,
	       &tm, "264");
  newfile2 = malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile2) {
    writeDebug("malloc error");
    delete = 1;
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
    delete = 1;
    goto err_exit;
  }
  if (rename(newkfile, newkfile2) == -1) {
    writeDebug("rename error (%s) %s %s", strerror(errno), newkfile, newkfile2);
  }

 err_exit:

  if (fpnew_k) fclose(fpnew_k);
  if (fpnew) fclose(fpnew);
  if (fp) fclose(fp);

  if (delete) {
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
    renamedfile = malloc(strlen(filepath) + strlen(newname) + 24); //24:len
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
#endif

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

  newfile = malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
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
  newfile = malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
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
  
  printf("===================================================\n");
  fprintf(stderr, "cut_file:%s\n", fullname);
  printf("cut from:%d to:%d\n",from,to);
  
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

 // fprintf(stderr, "kfile:%s\n", kfilename);
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
    renamedfile = malloc(strlen(filepath) + strlen(newname) + 24); //24:len
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
  check_resume_file(dir_path, servername, fi);
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
  if (dvr_type == DVR_TYPE_DM500) {
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
  
  if (((dvr_type == DVR_TYPE_DM500) && (ret == 10)) ||
      ((dvr_type == DVR_TYPE_DM510) && (ret == 9))) {
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
    if (dvr_type == DVR_TYPE_DM500) {
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
create_mark_file_list(char *dir_path, char *servername,
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
      check_and_add_to_list(dir_path, servername, fullname, &fi);
      if ((fi.ch == 0) && (fi.ext != 'K') && (fi.ext != 'k')) {
	if ((dvr_type == DVR_TYPE_DM500) ||
	    ((dvr_type == DVR_TYPE_DM510) && !fi.prelock)) {
	  event_count++;
	  event_duration += (dvr_type == DVR_TYPE_DM500) ?
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
#endif

static int 
create_mark_file_list(char *dir_path, char *servername,
		      char *path, char *filename, void *arg)
{
  int ret;
  struct fileInfo fi;
  char fullname[256], newfullname[256];
	
  ret = get_fileinfo_from_name(&fi, filename, servername);
  if (!ret) {
    snprintf(fullname, sizeof(fullname), "%s/%s",
	     path, filename);
  
    if ((fi.type == 'L') || (fi.type == 'l')) {
      check_and_add_to_list(dir_path, servername, fullname, &fi);
      if ((fi.ch == 0) && (fi.ext != 'K') && (fi.ext != 'k')) {
	if ((dvr_type == DVR_TYPE_DM500) ||
	    ((dvr_type == DVR_TYPE_DM510) && !fi.prelock)) {
	  event_count++;
	  event_duration += (dvr_type == DVR_TYPE_DM500) ?
	    (fi.len / 1000) : fi.len;
	}
      }
    } else {
      if ((fi.ext != 'K') && (fi.ext != 'k')) {
	/* we have only N type video file here */
	time_t from, to;
        //printf("file:%s\n",fullname);
        
	ret = where_to_cut_file(&fi, &from, &to);
	if (!ret) { /* file in the remote mark range */
	  fprintf(stderr, "where to cut:%d-%d\n", from, to);
	  if (dvr_type == DVR_TYPE_DM500) {
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
#if 0	      
	      if (!cut_file(dir_path,servername, fullname, &fi,
			    from, to, 'L')){
                delete_file_on_flash(fullname);
              }
#else
               cut_file(dir_path,servername, fullname, &fi,from, to, 'L');
#endif              
	    }
	  } /* dvr_type */
	} /* where_to_cut */
      } /* (ext!=k) */
    } /* if (type) */	  
  } /* if (sscanf) */
  
  return 0;
}

int send_file_add(char *busname, char *server_ip, int port, char *filepath)
{
  CURL *curl;
  struct memory_t chunk;
  char *enc, url[128], postfields[256];
  int retval = 1;

  chunk.memory = NULL;
  chunk.size = 0;

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "curl_easy_init(web) error\n");
    return 1;
  }

  /* receive data to memory */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  /* user agent in the HTTP header */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "smartftp");

  /* set URL */
  snprintf(url, sizeof(url),
	   "http://%s:%d/smart/fileadd.jsp",
	   server_ip, port);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  enc = curl_easy_escape(curl, busname, strlen(busname));
  if (!enc) {
    fprintf(stderr, "curl_easy_escape error1\n");
    goto fileadd_end;
  }
  /* add '&'(0x26) */
  snprintf(postfields, sizeof(postfields), "user=%s&files=", enc);
  free(enc);

  enc = curl_easy_escape(curl, filepath, strlen(filepath));
  if (!enc) {
    fprintf(stderr, "curl_easy_escape error2\n");
    goto fileadd_end;
  }
  strncat(postfields, enc, sizeof(postfields));
  free(enc);

  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, -1LL);

  fprintf(stderr, "POST %s\n", postfields);
  curl_easy_perform(curl);

  if (!chunk.memory) {
    fprintf(stderr, "no data from server\n");
    goto fileadd_end;
  }

  //dump((unsigned char *)chunk.memory, chunk.size);
  chunk.memory[chunk.size - 1] = 0;
  fprintf(stderr, "%s\n", chunk.memory);
  retval = 0;

  free(chunk.memory);

 fileadd_end:
  curl_easy_cleanup(curl);

  return retval;
}

#ifdef USE_READFUNCTION
static size_t readfunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
  FILE *f = (FILE *)stream;
  size_t n;

  fprintf(stderr, "readfunc:");
  if (ferror(f)) {
    fprintf(stderr, "error\n");
    return CURL_READFUNC_ABORT;
  }

  n = fread(ptr, size, nmemb, f) * size;
  fprintf(stderr, "%d bytes\n", n);

  return n;
}
#endif
 
int read_resume_info(char *dirname, char *servername)
{
  FILE *fp;
  char path[256];
  int ret;
  int ch,yy,mm,dd, hour, min, sec, millisec, size, offset;
  char type, ext;
  unsigned int len, prelock;
  char busname[128];

  snprintf(path, sizeof(path),
	   "%s/_%s_/resumeinfo",
	   dirname, servername);
  fp = fopen(path, "r");
  if (fp) {
    ret = fscanf(fp,
		 "%02d,%04d,%02d,%02d,"
		 "%02d,%02d,%02d,%03d,"
		 "%u,%u,%c,%c,%d,%d,%s",
		 &ch, &yy, &mm, &dd, 
		 &hour, &min, &sec, &millisec,
		 &len, &prelock, &type, &ext,
		 &size, &offset, busname);
    if ((ret == 15) && !strcmp(servername, busname)) {
      resume_fi.ch = ch;
      resume_fi.year = yy;
      resume_fi.month = mm;
      resume_fi.day = dd;
      resume_fi.hour = hour;
      resume_fi.min = min;
      resume_fi.sec = sec;
      resume_fi.millisec = millisec;
      resume_fi.type = type;
      resume_fi.len = len;
      resume_fi.prelock = prelock;
      resume_fi.ext = ext;
      resume_fi.size = size;
      resume_offset = offset;
      writeDebug("resume info:CH%02d,%d,%d,%d,%d,%d,%d(%d,%d)", 
		 ch, yy, mm, dd, hour, min, sec,
		 size, offset);
    }

    fclose(fp);

    resume_info_path = strdup(dirname);

    return 0;
  }

  return 1;
}

 void delete_resume_info(char *dirname, char *servername)
{
  char path[256];

  snprintf(path, sizeof(path),
	   "%s/_%s_/resumeinfo",
	   dirname, servername);
  unlink(path);
}

void make_smartfile_names(char *dirname, char *servername,
			  char *server_ip,
			  struct sfileInfo *fi,
			  char *local, int local_size,
			  char *remote, int remote_size)
{
  if (fi->type == TYPE_SMART) {      
    snprintf(local, local_size,
	     "%s/smartlog/%s_%04hd%02hhd%02hhd_L.%03hd",
	     dirname,
	     servername,
	     fi->year, fi->month, fi->day,
	     fi->fileno);
    snprintf(remote, remote_size,
	     "\\\\%s\\data_d\\smartlog\\%s_%04hd%02hhd%02hhd_L.%03hd",
	     server_ip,
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
    snprintf(remote, remote_size,
	     "\\\\%s\\data_d\\smartlog\\%s_%04hd%02hhd%02hhd"
	     "%02hhd%02hhd%02hhd_TAB102log_L.%s",
	     server_ip,
	     servername,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec,
	     (fi->type == TYPE_IMPACT) ? "log" : "peak");
  }
}

void make_file_names(char *dirname, char *servername,
		     char *server_ip,
		     struct fileInfo *fi,
		     char *local, int local_size,
		     char *winpath, int winpath_size,
		     char *remote, int remote_size)
{
  if (dvr_type == DVR_TYPE_DM500) {
    if (local) {
      local[0] = 0;
      snprintf(local, local_size,
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
    snprintf(winpath, winpath_size,
	     "D:\\_%s_\\%04d%02d%02d\\CH%02d\\CH%02d_"
	     "%04d%02d%02d%02d%02d%02d%03d_%u_%c_%s.%s",
	     servername,
	     fi->year, fi->month, fi->day,
	     fi->ch, fi->ch,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec, fi->millisec,
	     fi->len, fi->type, servername,
	     (fi->ext == 'k') ? "k" : "mp4");
    snprintf(remote, remote_size,
	     "\\\\%s\\data_d\\_%s_\\%04d%02d%02d\\CH%02d\\CH%02d_"
	     "%04d%02d%02d%02d%02d%02d%03d_%u_%c_%s.%s",
	     server_ip, servername,
	     fi->year, fi->month, fi->day,
	     fi->ch, fi->ch,
	     fi->year, fi->month, fi->day,
	     fi->hour, fi->min, fi->sec, fi->millisec,
	     fi->len, fi->type, servername,
	     (fi->ext == 'k') ? "k" : "mp4");
  } else { /* dvr_type */
    if (fi->prelock) { /* not used anymore */
      if (local) {
	local[0] = 0;
	snprintf(local, local_size,
		 "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
		 "%04d%02d%02d%02d%02d%02d_%u_%c_%u_%s.%s",
		 dirname, servername,
		 fi->year, fi->month, fi->day,
		 fi->ch, fi->ch,
		 fi->year, fi->month, fi->day,
		 fi->hour, fi->min, fi->sec,
		 fi->len, fi->type, fi->prelock, servername,
		 (fi->ext == 'k') ? "k" : "266");
      }
      snprintf(winpath, winpath_size,
	       "D:\\_%s_\\%04d%02d%02d\\CH%02d\\CH%02d_"
	       "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	       servername,
	       fi->year, fi->month, fi->day,
	       fi->ch, fi->ch,
	       fi->year, fi->month, fi->day,
	       fi->hour, fi->min, fi->sec,
	       fi->len, fi->type, servername,
	       (fi->ext == 'k') ? "k" : "266");
      snprintf(remote, remote_size,
	       "\\\\%s\\data_d\\_%s_\\%04d%02d%02d\\CH%02d\\CH%02d_"
	       "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	       server_ip, servername,
	       fi->year, fi->month, fi->day,
	       fi->ch, fi->ch,
	       fi->year, fi->month, fi->day,
	       fi->hour, fi->min, fi->sec,
	       fi->len, fi->type, servername,
	       (fi->ext == 'k') ? "k" : "266");
    } else { /* prelock */
      if (local) {
	local[0] = 0;
	snprintf(local, local_size,
		 "%s/_%s_/%04d%02d%02d/CH%02d/CH%02d_"
		 "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
		 dirname, servername,
		 fi->year, fi->month, fi->day,
		 fi->ch, fi->ch,
		 fi->year, fi->month, fi->day,
		 fi->hour, fi->min, fi->sec,
		 fi->len, fi->type, servername,
		 (fi->ext == 'k') ? "k" : "266");
      }
      snprintf(winpath, winpath_size,
	       "D:\\_%s_\\%04d%02d%02d\\CH%02d\\CH%02d_"
	       "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	       servername,
	       fi->year, fi->month, fi->day,
	       fi->ch, fi->ch,
	       fi->year, fi->month, fi->day,
	       fi->hour, fi->min, fi->sec,
	       fi->len, fi->type, servername,
	       (fi->ext == 'k') ? "k" : "266");
      snprintf(remote, remote_size,
	       "\\\\%s\\data_d\\_%s_\\%04d%02d%02d\\CH%02d\\CH%02d_"
	       "%04d%02d%02d%02d%02d%02d_%u_%c_%s.%s",
	       server_ip, servername,
	       fi->year, fi->month, fi->day,
	       fi->ch, fi->ch,
	       fi->year, fi->month, fi->day,
	       fi->hour, fi->min, fi->sec,
	       fi->len, fi->type, servername,
	       (fi->ext == 'k') ? "k" : "266");
    } /* prelock */
  } /* dvr_type */
}

void *upload_files(void *arg)
{
  struct upload_info *ui;
  char *dir_path, *servername, *ftp_user, *ftp_pass;
  char *dirname;
  int port;
  char *server_ip;
  char remote[512], winpath[512], path[256];
  FILE  *hd_src;
  struct fileEntry *node, *temp;
  struct sfileEntry *snode, *stemp;
  int retry;
  struct stat st;
  off_t res;
  int write_error, file_exist;

  ui = (struct upload_info *)arg;
  dir_path = ui->dir_path;
  servername = ui->servername;
  server_ip = strdup(inet_ntoa(ui->server_ip));
  port = ui->port;
  ftp_user = ui->ftp_user;
  ftp_pass = ui->ftp_pass;

  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);


  upload_done_size = 0;

  /* upload dvrlog.txt file */  
  if (dvr_type == DVR_TYPE_DM500) {
    snprintf(path, sizeof(path),
	     "%s/_%s_/dvrlog.txt",
	     dir_path, servername);
  } else {
      FILE *fp;
      fp = fopen("/var/dvr/dvrcurdisk", "r");
      if (!fp) {
	writeDebug("dvrcurdisk error");
	pthread_exit((void *)EXIT_CODE_DVRCURDISK_ERROR);
      }

      char curdisk[64];
      if (!fgets(curdisk, sizeof(curdisk), fp)) {
	fclose(fp);
	writeDebug("dvrcurdisk error2");
	pthread_exit((void *)EXIT_CODE_DVRCURDISK_ERROR);
      }
      snprintf(path, sizeof(path),
	      "%s/_%s_/dvrlog.txt",
	      curdisk, servername);
      fclose(fp);
  }
  snprintf(remote, sizeof(remote),
	   "\\\\%s\\data_d\\_%s_\\dvrlog.txt",
	   server_ip, servername);
  if (!stat(path, &st) && st.st_size) {
    fprintf(stderr, "url: %s(%ld)\n", remote, st.st_size);

    retry = 0;
    while (retry < MAX_RETRY) {
      /* open on every retry to avoid CURLE_FTP_COULDNT_USE_REST error */
      hd_src = fopen(path, "rb");
      if (!hd_src) {
	writeDebug("file:%s open error(%s)", path, strerror(errno));
	break;
      }

      writeDebug("uploading:%s", path);
      res = remotefilecopy(hd_src, remote, 0, NULL, NULL, NULL,
			   &write_error, &file_exist);
      if (write_error) {
	write_error_count++;
	if (write_error_count >= 3) {
	  exit_out(EXIT_CODE_HD_ERROR);
	}
      } else {
	write_error_count = 0;
      }
      if ((res >= st.st_size) || file_exist) {
	/* success */
	retry = 0;
	if (!file_exist) {
	  upload_done_size += res;
	  writeDebug("uploaded %ld(%lld) bytes", res, upload_done_size);
	}
	fclose(hd_src);
	break; /* done: no more retry */
      } else {
	writeDebug("ftp error:%ld, %ld, %d", res, st.st_size, retry);
	
	retry++;
	sleep(1); 
      } /* if (!res) */
      fclose(hd_src);
    } /* retry */

    if (retry >= MAX_RETRY) {
      writeDebug("retry count reached in dvrlog upload");
      if (server_ip) free(server_ip);
      pthread_exit((void *)1);
    }
  } /* if (stat) */  

  /* now upload smartlog files */
  sfile_list = slistsort(sfile_list);
  stemp = sfile_list;
  while (stemp) {    
    snode = stemp;
    stemp = snode->next;
    
    dirname = get_path_for_list_id(snode->fi.pathId);
    if (!dirname)
      continue;

    make_smartfile_names(dirname, servername, server_ip, &snode->fi,
			 path, sizeof(path), remote, sizeof(remote));
    fprintf(stderr, "url: %s(%u)\n", remote, snode->fi.size);
    if (stat(path, &st) == -1) {
      writeDebug("file:%s stat error(%s)", path, strerror(errno));
      continue;
    }
    if (!st.st_size) {
      writeDebug("file size 0");
      continue;
    }
    
    retry = 0;
    while (retry < MAX_RETRY) {
      /* open on every retry to avoid CURLE_FTP_COULDNT_USE_REST error */
      hd_src = fopen(path, "rb");
      if (!hd_src) {
	writeDebug("file:%s open error(%s)", path, strerror(errno));
	break;
      }

      writeDebug("uploading:%s", path);
      res = remotefilecopy(hd_src, remote, 0, NULL, NULL, NULL,
			   &write_error, &file_exist);
      if (write_error) {
	write_error_count++;
	if (write_error_count >= 3) {
	  exit_out(EXIT_CODE_HD_ERROR);
	}
      } else {
	write_error_count = 0;
      }
      if ((res >= st.st_size) || file_exist) {
	/* success */
	retry = 0;
	if (!file_exist) {
	  upload_done_size += res;
	  writeDebug("uploaded %ld(%lld) bytes", res, upload_done_size);
	}
	if (!rename_smartfile(path, 'N')) {
	  /* rename 002 file, too */
	  path[strlen(path) - 1] = '2';
	  rename_smartfile(path, 'N');
	}
	fclose(hd_src);
	break; /* done: no more retry */
      } else {
	writeDebug("ftp error:%ld, %ld, %d", res, st.st_size, retry);
	retry++;
	sleep(1); 
      }
      fclose(hd_src);
    } /* retry */

    if (retry >= MAX_RETRY)
      break;
  }

  /* resume file */
  if (resume_file) {
    make_file_names(NULL, servername, server_ip, &resume_fi,
		    NULL, 0, winpath, sizeof(winpath),
		    remote, sizeof(remote));
    fprintf(stderr, "resume url: %s(%u)\n", remote, resume_fi.size);
    
    res = resume_offset;
    retry = 0;
    while (retry < MAX_RETRY) {
      /* open on every retry to avoid CURLE_FTP_COULDNT_USE_REST error */
      hd_src = fopen(resume_file, "rb");
      if (!hd_src) {
	writeDebug("file:%s open error(%s)", resume_file, strerror(errno));
	break;
      }
      
      writeDebug("resume uploading:%s(offset:%ld)", resume_file, res);
      res = remotefilecopy(hd_src, remote, res, &resume_fi,
			   resume_info_path, servername,
			   &write_error, &file_exist);
      if (write_error) {
	write_error_count++;
	if (write_error_count >= 3) {
	  exit_out(EXIT_CODE_HD_ERROR);
	}
      } else {
	write_error_count = 0;
      }
      if ((res >= resume_fi.size) || file_exist) {
	/* success */
	retry = 0;
	if (!file_exist) {
	  upload_done_size += res;
	  writeDebug("uploaded %ld(%lld) bytes", res, upload_done_size);
	  /* notify server the successful upload */
	  if ((resume_fi.ext != 'K') && (resume_fi.ext != 'k')) {
	    send_file_add(servername, server_ip, port, winpath);
	  }
	}
	rename_file(resume_file, 'N');
	fclose(hd_src);
	delete_resume_info(resume_info_path, servername);
	break; /* done: no more retry */
      } else {
	writeDebug("ftp error:%lld, %ld, %d", res, st.st_size, retry);
	
	retry++;
	sleep(1); 
      } /* if (res) */
      fclose(hd_src);
    } /* retry */
    
    if (retry >= MAX_RETRY) {
      writeDebug("retry count reached in video upload");
      if (server_ip) free(server_ip);
      pthread_exit((void *)1);
    }

    free(resume_file);
    resume_file = NULL;
  }

  file_list = listsort(file_list);
  
  temp = file_list;
  while(temp) {
    node = temp;
    temp = node->next;

    dirname = get_path_for_list_id(node->fi.pathId);
    if (!dirname)
      continue;

    make_file_names(dirname, servername, server_ip, &node->fi,
		    path, sizeof(path), winpath, sizeof(winpath),
		    remote, sizeof(remote));
    fprintf(stderr, "url: %s(%u)\n", remote, node->fi.size);
    if (stat(path, &st) == -1) {
      writeDebug("file:%s stat error(%s)", path, strerror(errno));
      continue;
    }
    if (!st.st_size) {
      writeDebug("file size 0");
      continue;
    }

    retry = 0;
    res = 0;
    while (retry < MAX_RETRY) {
      /* open on every retry to avoid CURLE_FTP_COULDNT_USE_REST error */
      hd_src = fopen(path, "rb");
      if (!hd_src) {
	writeDebug("file:%s open error(%s)", path, strerror(errno));
	break;
      }
      
      writeDebug("uploading:%s", path);
      res = remotefilecopy(hd_src, remote, res, &node->fi,
			   resume_info_path ? resume_info_path : dirname,
			   servername, &write_error, &file_exist);
      if (write_error) {
	write_error_count++;
	if (write_error_count >= 3) {
	  exit_out(EXIT_CODE_HD_ERROR);
	}
      } else {
	write_error_count = 0;
      }
      if ((res >= st.st_size) || file_exist) {
	/* success */
	retry = 0;
	if (!file_exist) {
	  upload_done_size += res;
	  writeDebug("uploaded %ld(%lld) bytes", res, upload_done_size);
	
	  /* notify server the successful upload */
	  if ((node->fi.ext != 'K') && (node->fi.ext != 'k')) {
	    send_file_add(servername, server_ip, port, winpath);
	  }
	}
	
	rename_file(path, 'N');
	fclose(hd_src);
	delete_resume_info(resume_info_path ? resume_info_path : dirname,
			   servername);
	break; /* done: no more retry */
      } else {
	writeDebug("ftp error:%ld, %ld, %d", res, st.st_size, retry);
	
	retry++;
	sleep(1); 
      } /* if (res) */
      fclose(hd_src);
    } /* retry */
    
    if (retry >= MAX_RETRY) {
      writeDebug("retry count reached in video upload");
      if (server_ip) free(server_ip);
      pthread_exit((void *)1);
    }
  } /* while */  
  
  
  
  if (server_ip) free(server_ip);
  
  pthread_exit((void *)0);
}

static void safe_sleep(int ms) {
  struct timespec delayTs, remTs;

  delayTs.tv_sec = ms / 1000;
  delayTs.tv_nsec = (ms % 1000) * 1000000;

  while ((nanosleep(&delayTs, &remTs) == -1) && (errno ==  EINTR)) {
    delayTs = remTs;
  }    
}

///////////////////////////////////////////////////////////
void scan_resume_info(char *rootdir, char *servername)
{
  DIR *dir;
  struct dirent *de;
  char path[256];
  struct stat findstat ;
  if (dvr_type == DVR_TYPE_DM500) {
    read_resume_info(rootdir, servername);
    return;
  }
  
  /* DM510 */
  dir = opendir(rootdir);
  if (dir) {
    while ((de = readdir(dir)) != NULL) {

	if (de->d_name[0] == '.')
	  continue;
 	snprintf(path, sizeof(path), "%s/%s", rootdir, de->d_name);
        if(de->d_type==DT_UNKNOWN){
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
       // printf("path:%s servername:%s\n",path,servername);
        if (!read_resume_info(path, servername)) {
	  break;
      }
    }
    closedir(dir);
  }
}


//////////////////////////////
#if 0
void scan_resume_info(char *rootdir, char *servername)
{
  DIR *dir;
  struct dirent *de;
  char path[256];

  if (dvr_type == DVR_TYPE_DM500) {
    read_resume_info(rootdir, servername);
    return;
  }

  /* DM510 */
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
#endif


#if 0
int scan_recording_disks(char *rootdir, char *servername)
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
    if (scan_files(curdisk, servername, NULL, &create_mark_file_list)) {
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
	if (scan_files(path, servername, NULL, &create_mark_file_list)) {
	  writeDebug("scan_files error");
	}
      }
      closedir(dir);
    }
  }

  return 0;
}
#endif

int scan_recording_disks(char *rootdir, char *servername)
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
    if (scan_files(curdisk, servername, NULL, &create_mark_file_list)) {
      writeDebug("scan_files error");
    }
  } else { /* single or multiple */
    dir = opendir(rootdir);
    if (dir) {
      while ((de = readdir(dir)) != NULL) {
	if (de->d_name[0] == '.')
	  continue;
 	snprintf(path, sizeof(path), "%s/%s", rootdir, de->d_name);
        if(de->d_type==DT_UNKNOWN){
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
	if (scan_files(path, servername, NULL, &create_mark_file_list)) {
	  writeDebug("scan_files error");
	}
      }
      closedir(dir);
    }
  }

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

void read_config()
{
  peak_time_in_utc = 0;
  if (dvr_type == DVR_TYPE_DM510) {
    char *p;
    if (!cfg_parse_file("/etc/dvr/dvr.conf")) {
      p = cfg_get_str("glog", "gforce_log_enable");
      if (p) {
	/* 602 internal gforce has UTC time */
	peak_time_in_utc = atoi(p);
	fprintf(stderr, "gforce enable:%d\n", peak_time_in_utc);
      }

      p = cfg_get_str("io", "gsensor_forward_event");
      if (p) {
	peak_tp.fb_pos = atof(p);
      }
      p = cfg_get_str("io", "gsensor_backward_event");
      if (p) {
	peak_tp.fb_neg = atof(p);
      }
      p = cfg_get_str("io", "gsensor_right_event");
      if (p) {
	peak_tp.lr_pos = atof(p);
      }
      p = cfg_get_str("io", "gsensor_left_event");
      if (p) {
	peak_tp.lr_neg = atof(p);
      }
      p = cfg_get_str("io", "gsensor_down_event");
      if (p) {
	peak_tp.ud_pos = atof(p);
      }
      p = cfg_get_str("io", "gsensor_up_event");
      if (p) {
	peak_tp.ud_neg = atof(p);
      }

      p = cfg_get_str("io", "gsensor_pre_event");
      if (p) {
	pre_peak_event = atoi(p);
      }
      p = cfg_get_str("io", "gsensor_post_event");
      if (p) {
	post_peak_event = atoi(p);
      }
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
 *   - 6.ftp_user: username for ftp access
 *   - 7.ftp_pass: password for ftp access
 *   - 8.exit_port: TCP port where exit code is sent to. (can be 0)
 *   - 9.anormalie: system anormalie? 'Y' or 'N'.
 *   - 10.timezone: optional, glibc TZ string
 *               if omitted, TZ will be read from /var/run/TZ
 */

int main(int argc, char **argv)
{
  struct in_addr ip, netmask, broadcast, server_ip;
  int port;
  void *retval;
  int retry = 0;
  int real_argc;
  int ret;

  if (signal(SIGBUS, buserror_handler) == SIG_IGN)
    signal(SIGBUS, SIG_IGN);

  if (!strcmp(argv[argc - 1], "&")) {
    real_argc = argc - 1;
  } else {
    real_argc = argc;
  }

  if (real_argc < 10) {
    fprintf(stderr,
	    "Usage: %s wifi_dev_name bus_name password mount_dir "
	    "dvr_type[500|510] ftp_user ftp_pass port anormalie [timezone]\n",
	    argv[0]);
    exit(EXIT_CODE_BAD_ARGUMENT);
  }

  INIT_LIST_HEAD(&g_pel.list);
  INIT_LIST_HEAD(&g_rml.list);

  dvr_type = strcmp(argv[5], "500") ? DVR_TYPE_DM510 : DVR_TYPE_DM500;
  if (dvr_type == DVR_TYPE_DM500)
    strcpy(g_ext, "mp4");
  else
    strcpy(g_ext, "266");
  exit_port = atoi(argv[8]);

  if (real_argc == 11) {
    setTZ(argv[10]);
  } else {
    setTZ(NULL);
  }

  setDebugFileName(argv[4]);
  writeDebug("smartftp");

  read_config();

  writeDebug("get_wifi_ip");

  /* get ip address assigned to the specified device
   * to be used in "discover_smart_server"
   * Do this until DHCP assigns a IP
   */
  while (retry < 10) {
      fprintf(stderr, "Querying IP for %s\n", argv[1]);
      if (get_wifi_ip(argv[1], &ip, &netmask, &broadcast)) {
	writeDebug("can't get ip/netmask");
      if (exit_port) 
	send_exit_code(EXIT_CODE_NETWORK_ERROR, exit_port);
      else
	exit(EXIT_CODE_NETWORK_ERROR);
      }

      if (ip.s_addr)
	break;

      retry++;
      sleep(1);
  }

  if (ip.s_addr == 0) {
      writeDebug("ip address not assigned yet");
      if (exit_port) 
	send_exit_code(EXIT_CODE_NO_DHCP, exit_port);
      else
	exit(EXIT_CODE_NO_DHCP);
  }

#ifndef NO_SERVER_TEST
  writeDebug("discover_smart_server");
  /* send discover message to the specified subnet
   * and get the server's ip/port
   */
  if (discover_smart_server(ip, netmask, broadcast, &server_ip, &port)) {
    writeDebug("can't find smart server");
    if (exit_port)
      send_exit_code(EXIT_CODE_NO_SERVER, exit_port);
    else
      exit(EXIT_CODE_NO_SERVER);
  }

  curl_global_init(CURL_GLOBAL_ALL);

  writeDebug("get_mark_info");

  get_mark_info(server_ip, port, argv[2], argv[3]);
#endif

  writeDebug("scan_resume_info");

  scan_resume_info(argv[4], argv[2]);

  writeDebug("scanSmartFile");
  if (dvr_type == DVR_TYPE_DM500) {
    scanSmartFiles(argv[4], argv[2]);
  } else {
    scan_smartlog_disks(argv[4], argv[2]);
  }

  merge_peak_event_list_to_remote_mark_list();

  writeDebug("scan_files");
  if (dvr_type == DVR_TYPE_DM500) {
    scan_files(argv[4], argv[2], NULL, &create_mark_file_list);
  } else {
    scan_recording_disks(argv[4], argv[2]);
  }

#ifndef NO_SERVER_TEST
  writeDebug("login_to_smartserver");

  while (1) {
    ret = login_to_smartserver(server_ip, port, argv[2], argv[3], argv[9]);
    if (ret <= 0)
      break;
    writeDebug("server requested %dms waiting", ret);
    safe_sleep(ret);
  }

  if (ret < 0) {
    writeDebug("login error");
    if (exit_port) 
      send_exit_code(EXIT_CODE_LOGIN_ERROR, exit_port);
    else
      exit(EXIT_CODE_LOGIN_ERROR);
  }

  /* use thread for later support for progress meter */
  //if (file_list || sfile_list) {
  if (1) { /* alway do upload, as there will be at least dvrlog.txt */
    struct upload_info ui;
    ui.dir_path = argv[4];
    ui.servername = argv[2];
    ui.server_ip = server_ip;
    ui.ftp_user = argv[6];
    ui.ftp_pass = argv[7];
    ui.port = port;
    writeDebug("upload_thread");

    pthread_mutex_lock(&mutex);
    if (pthread_create(&upload_thread, NULL, upload_files, &ui)) {
      upload_thread = 0;
    } else {
      pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
  }

  writeDebug("pthread_join");
  if (upload_thread) {
      pthread_join(upload_thread, &retval);
      writeDebug("upload thread joined:%p", retval);
      if (retval) {
	if (exit_port) 
	  send_exit_code(EXIT_CODE_FILE_TRANSFER_TIMEOUT, exit_port);
	else
	  exit(EXIT_CODE_FILE_TRANSFER_TIMEOUT);
      }
  }

  curl_global_cleanup();
#endif
 
  free_peak_event_list();
  free_remote_mark_list();
  free_smartfile_list();
  free_file_list();
  free_dir_list();

  if (exit_port)
    send_exit_code(EXIT_CODE_SUCCESS, exit_port);

  return EXIT_CODE_SUCCESS;
}
