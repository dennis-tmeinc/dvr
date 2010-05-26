#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <dirent.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/vfs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#include "dvrproto.h"
#include "../dvrsvr/dvr.h"
#include "../dvrsvr/cfg.h"
#include "../ioprocess/diomap.h"

struct dio_mmap * p_dio_mmap ;
char dvriomap[128] = "/var/dvr/dvriomap" ;
char dvrconfigfile[] = "/etc/dvr/dvr.conf" ;


/*
   share memory lock implemented use atomic swap

    operations between lock() and unlock() should be quick enough and only memory access only.
 
*/ 

// atomically swap value
int atomic_swap( int *m, int v)
{
    register int result ;
/*
    result=*m ;
    *m = v ;
*/
    asm volatile (
        "swp  %0, %2, [%1]\n\t"
        : "=r" (result)
        : "r" (m), "r" (v)
    ) ;
    return result ;
}

void dio_lock()
{
    if( p_dio_mmap ) {
        int c=0;
        while( atomic_swap( &(p_dio_mmap->lock), 1 ) ) {
            if( c++<20 ) {
                sched_yield();
            }
            else {
                // yield too many times ?
                usleep(1);
            }
        }
    }
}

void dio_unlock()
{
    if( p_dio_mmap ) {
        p_dio_mmap->lock=0;
    }
}

void dio_iomsg(char * msg=NULL, int suspend=0)
{
    dio_lock();
    if( msg ) {
        strcpy( p_dio_mmap->iomsg, msg );
    }
    else {
        p_dio_mmap->iomsg[0]=0 ;
    }
    if( suspend ) {
        p_dio_mmap->iomode = IOMODE_SUSPEND;
        p_dio_mmap->suspendtimer = 600 ;
    }
    else {
        p_dio_mmap->iomode = IOMODE_RUN;
    }
    dio_unlock();
}

// return 
//        0 : failed
//        1 : success
int dio_start()
{
    int fd ;
    char * p ;
    config dvrconfig(dvrconfigfile);
    string v ;
    p_dio_mmap=NULL ;
    v = dvrconfig.getvalue( "system", "iomapfile");
    if( v.length()>0 ) {
        fd = open(v.getstring(), O_RDWR );
    }
    else {
        fd = open(dvriomap, O_RDWR );
    }
    if( fd<=0 ) {
        printf("Can't open io map file!\n");
        return 0 ;
    }
    p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );								// fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    return 1 ;
}
 
// dio finish, clean up
void dio_finish()
{
    // clean up shared memory
    if( p_dio_mmap ) {
        dio_iomsg(NULL, 0);
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
}


#define MAX_CHANNEL 16
#define DVRPORT 15111

//#define MYDEBUG
//#define NET_DEBUG
//#define SLOW_COPY

enum {RT_SUCCESS,
      RT_UNKNOWN_ERROR,
      RT_BAD_ARGUMENT,
      RT_CONNECT_ERROR,
      RT_BUS_ERROR,
      RT_HOSTNAME_ERROR,
      RT_CFGFILE_ERROR,
      RT_MEMORY_ERROR};

enum {DVR_TYPE_DM500, DVR_TYPE_DM510};

struct range {
  time_t from, to;
};

struct rangeEntry {
  struct rangeEntry *next, *prev;
  struct range range;
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

struct dirPath {
  int pathId;
  char pathStr[128];
};

struct dirEntry {
  struct dirEntry *next, *prev;
  struct dirPath path;
};

int dfd = -1;
int dvr_type = DVR_TYPE_DM510;
/* # of files to upload */
int upload_file_count;
/* total bytes of all files to upload for progress meter */
long long upload_size, upload_done_size;
int event_count;
long long event_duration;
struct rangeEntry *range_list;
struct fileEntry *file_list;
struct dirEntry *dir_list;
char mountdir[128] = "/var/dvr/disks";
char pidfile[128] = "/var/dvr/dvrsvr.pid";
pid_t pid_dvrsvr;

int connectTcp(char *addr, short port);
int connectNonBlockingTcp(char *addr, short port, int timeout_sec);

static unsigned long GetDWLE(void const *_p)
{
  unsigned char *p = (unsigned char *)_p;

  return (((unsigned long)p[3] << 24) |
	  ((unsigned long)p[2] << 16) |
	  ((unsigned long)p[1] <<  8) |
	  p[0]);
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

//int too_fast;
void writeDebug(char *fmt, ...)
{
  va_list vl;
  char str[256];

  va_start(vl, fmt);
  vsprintf(str, fmt, vl);
  va_end(vl);
  /*
  if (too_fast) {
    strcat(str, "------"); // just testing...
    too_fast = 0;
    }*/

  fprintf(stderr, "%s\n", str);

#ifdef MYDEBUG
  if (dfd != -1) {
    if (send(dfd, str, strlen(str) + 1, MSG_NOSIGNAL) == -1) {
      // if (errno == EAGAIN || errno == EWOULDBLOCK)
      //too_fast = 1;
    }
  } else {
    FILE *fp;
    fp = fopen("/var/run/filert", "a");
    if (fp != NULL) {
      fprintf(fp, "%s\n", str);
      fclose(fp);
    }
  }
#endif
}

void connectDebug()
{
#ifdef NET_DEBUG
#if 0
  int connected = 0, retry = 0;

  while (!connected && (retry < 3)) {
    dfd = connectNonBlockingTcp("192.168.1.220", 11330, 3);
    if (dfd == -1) {
      sleep(1);
      retry++;
      continue;
    }
    fprintf(stderr, "connected for debugging\n");
    connected = 1;
  }

  if (connected)
    writeDebug("connected to dvrsvr.");
#endif
  dfd = connectTcp("192.168.1.220", 11330);

#endif
}

static void free_range_list()
{
  struct rangeEntry *node;

  while (range_list) {
    node = range_list;
    range_list = node->next;
    //fprintf(stderr, "freeing %p\n", node);
    free(node);
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

static void free_dir_list()
{
  struct dirEntry *node;

  while (dir_list) {
    node = dir_list;
    dir_list = node->next;
    free(node);
  }
}

int sendall(int s, unsigned char *buf, int *len)
{
  int total = 0;
  int bytesleft = *len;
  int n = 0;

  while (total < *len) {
    n = send(s, buf + total, bytesleft, MSG_NOSIGNAL);
    if (n == -1) { break; }
    total += n;
  }

  *len = total;

  return n == -1 ? -1 : 0;
}

int connectTcp(char *addr, short port)
{
  int sfd;
  struct sockaddr_in destAddr;

  sfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    return -1;

  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(port);
  destAddr.sin_addr.s_addr = inet_addr(addr);
  memset(&(destAddr.sin_zero), '\0', 8);

  if (connect(sfd, (struct sockaddr *)&destAddr,
	      sizeof(struct sockaddr)) == -1) {
    close(sfd);
    return -1;
  }

  return sfd;
}

int connectNonBlockingTcp(char *addr, short port, int timeout_sec)
{
  int sfd, flags, ret, xerrno;
  socklen_t len;
  struct sockaddr_in destAddr;
  fd_set rset, wset;
  struct timeval timeout;

  sfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    return -1;

  flags = fcntl(sfd, F_GETFL, 0);
  if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1) {
    close(sfd);
    return -1;
  }

  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(port);
  destAddr.sin_addr.s_addr = inet_addr(addr);
  memset(&(destAddr.sin_zero), '\0', 8);

  ret = connect(sfd, (struct sockaddr *)&destAddr,
		sizeof(struct sockaddr));
  if (ret == 0) {
    return sfd;
  } else if ((ret == -1) && (errno != EINPROGRESS)) {
    close(sfd);
    return -1;
  }

  FD_ZERO(&rset);
  FD_SET(sfd, &rset);
  wset = rset;
  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = 0;

  select(sfd + 1, &rset, &wset, NULL, &timeout);

  if (FD_ISSET(sfd, &rset) || FD_ISSET(sfd, &wset)) {
    len = sizeof(xerrno);
    if (getsockopt(sfd, SOL_SOCKET, SO_ERROR, &xerrno, &len) < 0) {
      close(sfd);
      return -1;
    }

    if (xerrno != 0) {
      close(sfd);
      return -1;
    }

    return sfd;
  }

  close(sfd);
  return -1;

}

#if 0
static int blockUntilReadable(int socket, struct timeval* timeout) {
  int result = -1;
  do {
    fd_set rd_set;
    FD_ZERO(&rd_set);
    if (socket < 0) break;
    FD_SET((unsigned) socket, &rd_set);
    const unsigned numFds = socket+1;

    result = select(numFds, &rd_set, NULL, NULL, timeout);
    if (timeout != NULL && result == 0) {
      break; // this is OK - timeout occurred
    } else if (result <= 0) {
#if defined(__WIN32__) || defined(_WIN32)
#else
      if (errno == EINTR || errno == EAGAIN) continue;
#endif
      fprintf(stderr, "select() error: \n");
      break;
    }

    if (!FD_ISSET(socket, &rd_set)) {
      fprintf(stderr, "select() error - !FD_ISSET\n");
      break;
    }
  } while (0);

  return result;
}

/* returns handle of the stream opened */
static int stream_open(int sfd, int channel)
{
  struct dvr_req req ;
  struct dvr_ans ans ;
  int ret, size;
    
  writeDebug("stream_open channel %d", channel);

  req.reqcode = REQSTREAMOPEN;
  req.data = channel;
  req.reqsize = 0;
  
  size = sizeof(req);
  ret = sendall(sfd, (unsigned char *)&req, &size);
  if (ret == -1) {
    writeDebug("stream_open send error");
    return 0;
  }

  writeDebug("stream_open sent request");

  struct timeval timeout = {3, 0};
  if (blockUntilReadable(sfd, &timeout) <= 0) {
    writeDebug("stream_open:recv error");
    return 0;
  }
  
  if ((ret = recv(sfd, &ans, sizeof(ans), 0)) <= 0) {
    if (ret == 0) {
      writeDebug("stream_open:dvrsvr closed connection");
    } else {
      writeDebug("stream_open:recv error(%s)", strerror(errno));
    }
    return 0;
  }

  if (ret == sizeof(ans) &&
      ans.anscode == ANSSTREAMOPEN &&
      ans.anssize == 0) {
    writeDebug("stream_open:success(handle:%d)", ans.data);
    return ans.data;
  }
   
  writeDebug("stream_open:failed(code:%d)", ans.anscode);

  return 0;
}

static int stream_close(int sfd, int handle)
{
  struct dvr_req req ;
  struct dvr_ans ans ;
  int ret, size;
    
  writeDebug("stream_close handle %d", handle);

  req.reqcode = REQSTREAMCLOSE;
  req.data = handle;
  req.reqsize = 0;
  
  size = sizeof(req);
  ret = sendall(sfd, (unsigned char *)&req, &size);
  if (ret == -1) {
    writeDebug("stream_close send error");
    return 1;
  }

  writeDebug("stream_close sent request");

  struct timeval timeout = {3, 0};
  if (blockUntilReadable(sfd, &timeout) <= 0) {
    writeDebug("stream_close:recv error");
    return 1;
  }
  
  if ((ret = recv(sfd, &ans, sizeof(ans), 0)) <= 0) {
    if (ret == 0) {
      writeDebug("stream_close:dvrsvr closed connection");
    } else {
      writeDebug("stream_close:recv error(%s)", strerror(errno));
    }
    return 1;
  }

  if (ret == sizeof(ans) &&
      ans.anscode == ANSOK &&
      ans.anssize == 0) {
    writeDebug("stream_close:success(handle:%d)", handle);
    return 0;
  }
   
  return 1;
}

void setTZ(char *tzstr)
{
  FILE * fp;
  char buf[128];

  if (tzstr == NULL) {
    // set TZ environment
    fp = fopen("/var/run/TZ", "r");
    if (fp != NULL) {
      if (fscanf(fp, "TZ=%s", buf) ==  1) {
	fprintf(stderr, "TZ:%s\n", buf);
	setenv("TZ", buf, 1);
      }
      fclose(fp);
    }
  } else {
    setenv("TZ", tzstr, 1);
  }
}
#endif

int add_time_range_to_list(char *range, char *servername)
{
  struct rangeEntry *node = NULL;
  char *str, *end, *start;
  struct tm from, to;



  /* make a local copy, as we'll have to modify it */
  str = strdup(range);

  start = str;
  while ((end = strchr(start, '\n'))) {
    *end = '\0';
    writeDebug("%s", start);
    int servernameLen = strlen(servername);
    if (((int)strlen(start) > servernameLen) &&
	!strncmp(start, servername, servernameLen)) {
      if (12 == sscanf(start + servernameLen,
		       ":%04d%02d%02d%02d%02d%02d-%04d%02d%02d%02d%02d%02d",
		       &from.tm_year, &from.tm_mon, &from.tm_mday,
		       &from.tm_hour, &from.tm_min, &from.tm_sec,
		       &to.tm_year, &to.tm_mon, &to.tm_mday,
		       &to.tm_hour, &to.tm_min, &to.tm_sec)) {
	node = (rangeEntry *)malloc(sizeof(struct rangeEntry));
	if (node) {
	  from.tm_year -= 1900;
	  from.tm_mon -= 1;
	  from.tm_isdst = -1;
	  to.tm_year -= 1900;
	  to.tm_mon -= 1;
	  to.tm_isdst = -1;
	  node->range.from = mktime(&from);
	  node->range.to = mktime(&to);
	  /* add to the list */
	  node->next = range_list;
	  range_list = node;
	  writeDebug("%s added", start);
	}
      }
    }

    /* get the next range */
    start = end + 1;
  }

  if (str) free(str);
  
  return 0;
}

#define RDBLKSIZE 256
char *readConfigFile(char *path, char *fname)
{
  unsigned char *buf;
  char *request = NULL;
  int fd;
  int bufsize, len, bytes;

  bufsize = RDBLKSIZE;
  buf = (unsigned char *)malloc(bufsize);
  if (!buf) {
    writeDebug("malloc error");
    exit(RT_MEMORY_ERROR);
  }

  sprintf((char *)buf, "%s/%s", path, fname);
  writeDebug("opening %s", (char *)buf);
  fd = open((char *)buf, O_RDONLY);
  if (fd == -1) {
    free(buf);
    return NULL;
  }

  len = 0;
  bytes = 0;
  while (1) {
    bytes = read(fd, buf + len, RDBLKSIZE);
    if (bytes <= 0)
      break;

    len += bytes;
    if (bytes == RDBLKSIZE) {
      bufsize += RDBLKSIZE;
      buf = (unsigned char *)realloc(buf, bufsize);
      if (!buf) {
	writeDebug("malloc error");
	exit(RT_MEMORY_ERROR);
      }
    } else {
      break;
    }
  }

  //writeDebug("read %d", len);

  // byte 0 - byte 3 : numRecord (eg. 03 00 00 00 --> 3 records)
  // each record
  //   servername(string) + start(time_t) + end(time_t)
  //   servername: first byte - string length
  //   start, end: time_t (4 bytes)
  
  if (bytes > 5) { // at least 5, but should be bigger
    int numRecords;
    int serverNameLen;
    unsigned char *ptr;
    char *ptrW;
    time_t fromTime, toTime;
    struct tm fromTm, toTm;
    char temp[32];
    
    
    numRecords = *((unsigned int *)buf);
    
    serverNameLen = *(buf + 4);
    
    //writeDebug("numRecords:%d,serverNameLen:%d", numRecords,serverNameLen);

    if (numRecords) {
      request = (char *) malloc((serverNameLen + 31) * numRecords);
      if (!request) {
	writeDebug("malloc error");
	exit(RT_MEMORY_ERROR);
      }
      
      ptr = buf + 4; /* serverNameLen */
      ptrW = request;
      while(ptr < buf + len) {
	fromTime = GetDWLE(ptr + serverNameLen + 1);
	if (!localtime_r(&fromTime, &fromTm)) {
	  writeDebug("fromTime error:%ld", fromTime);
	  free(request);
	  request = NULL;
	  break;
	}

	toTime = GetDWLE(ptr + serverNameLen + 5);
	if (!localtime_r(&toTime, &toTm)) {
	  writeDebug("toTime error:%ld", toTime);
	  free(request);
	  request = NULL;
	  break;
	}
	
	// verified OK, make string now 
	// SERVER:20070515103010-20070715093530 '\n' separated if more than 1
	ptr++;
	memcpy(ptrW, ptr, serverNameLen);
	ptr += serverNameLen;
	ptrW += serverNameLen;
	*ptrW = ':'; ptrW++; // SERVERNAME:
	
	sprintf(temp,
		"%04d%02d%02d%02d%02d%02d-%04d%02d%02d%02d%02d%02d",
		fromTm.tm_year + 1900,
		fromTm.tm_mon + 1,
		fromTm.tm_mday,
		fromTm.tm_hour,
		fromTm.tm_min,
		fromTm.tm_sec,
		toTm.tm_year + 1900,
		toTm.tm_mon + 1,
		toTm.tm_mday,
		toTm.tm_hour,
		toTm.tm_min,
		toTm.tm_sec);
	strcpy(ptrW, temp);
	writeDebug(temp);
	ptr += 8;
	ptrW += 29;
	*ptrW = '\n'; ptrW++;
      } // while      
    }
  }
  
  close(fd);
  free(buf);
  
  return request;
}

char *checkTmeLogFile(char *path)
{
  DIR *dir;
  struct dirent *de;
  char *buf = NULL;

  dir = opendir(path);
  if (dir) {
    while ((de = readdir(dir)) != NULL) {
      if ((de->d_name[0] == '.') || (de->d_type != DT_REG))
	continue;

      if (!strcmp(de->d_name, "tmelogfile.bin")) {
	writeDebug("tmelogfile.bin found");
	buf = readConfigFile(path, de->d_name);
	if (buf) {
	  writeDebug("time range:%s", buf);
	}
      }
    }
    closedir(dir);
  }

  return buf;
}

void buserror_handler(int signum)
{
  writeDebug("--Bus Error--");
  exit(RT_BUS_ERROR);
}

static int scan_files(char *mount_dir, char *servername,
		      void *arg,
		      int (*doit)(char*, char*, char*, char*, void*))
{
  DIR *dir, *dirCh, *dirFile;
  struct dirent *de, *deCh, *deFile;
  char dname[128];
  
  sprintf(dname, "%s/_%s_", mount_dir, servername);
  //writeDebug("scan_files:%s", dname);
  
  dir = opendir(dname);
  if (dir == NULL) {
    writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
    return 1;
  }
  //writeDebug("%s opened", dname);
  
  while ((de = readdir(dir)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    
    if (de->d_type != DT_DIR)
      continue;
    
    sprintf(dname, "%s/_%s_/%s", mount_dir, servername, de->d_name);
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
	      mount_dir, servername,
	      de->d_name, deCh->d_name);
      dirFile = opendir(dname);
      if (dirFile == NULL) {
	writeDebug("scan_files:opendir %s(%s)", dname,strerror(errno));
	closedir(dirCh);
	closedir(dir);
	return 1;
      }
      //writeDebug("%s opened", dname);
      while ((deFile = readdir(dirFile)) != NULL) {
	if (deFile->d_name[0] == '.')
	  continue;
	if (deFile->d_type != DT_REG)
	  continue;
	(*doit)(mount_dir, servername, dname, deFile->d_name, arg);
      }
      
      closedir(dirFile);
    }
    
    closedir(dirCh);
  }
  
  closedir(dir);
  
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

  /* look for prelock duration for mDVR51xx (_L_150_busname) */
  ret = sscanf(afterType, "%d_%s", &duration, str);
  if ((ret == 2) && !strncmp(str, servername, len)) {
    *prelock = duration;
    return 0;
  }

  return 1;
}

/* rename the recording file(full path) to 'L' */
static int rename_file(char *filename, char type)
{
  char *ptr, *newname;
  int ret = 1;

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
	    //fprintf(stderr, "%s renamed to %s\n", filename, newname);
	    if (rename(filename, newname) == -1)
	      perror("rename_file");
	    else
	      ret = 0;
	  }
	}
      }
    }
    free(newname);
  }

  return ret;
}

void read_config()
{
    char *p ;
    config dvrconfig(dvrconfigfile);
    string v ;

    v = dvrconfig.getvalue( "system", "mountdir");
    if( v.length()>0 ) {
        strcpy( mountdir, v.getstring() );
    }

    v = dvrconfig.getvalue( "system", "pidfile" );
    if( v.length()>0 ) {
        strcpy( pidfile, v.getstring() );
    }

    v = dvrconfig.getvalue( "system", "timezone" );
    v = dvrconfig.getvalue( "timezones", v.getstring() );
    if( v.length()>0 ) {
        p = strchr( v.getstring(), ' ' );
        if( p ) {
            *p=0 ;
        }
        setenv( "TZ", v.getstring(), 1 );
    }
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
  node = (dirEntry *)malloc(sizeof(struct dirEntry));
  if (node) {
    node->path.pathId = highestId + 1;
    strncpy(node->path.pathStr, path, sizeof(node->path.pathStr));
     /* add to the list */
    node->next = dir_list;
    dir_list = node;
    writeDebug("dir %d,%s added", node->path.pathId, node->path.pathStr);
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
add_file_to_list(char *path, char ch,
		 unsigned short year, char month, char day,
		 char hour, char min, char sec, unsigned short millisec,
		 unsigned int len, char type, char ext,
		 unsigned int prelock, off_t size)
{
  struct fileEntry *node;

  //fprintf(stderr, "add_file_to_list:%u\n", len);

  node = (fileEntry*)malloc(sizeof(struct fileEntry));
  if (node) {
    node->fi.ch = ch;
    node->fi.year = year;
    node->fi.month = month;
    node->fi.day = day;
    node->fi.hour = hour;
    node->fi.min = min;
    node->fi.sec = sec;
    node->fi.millisec = millisec;
    node->fi.type = type;
    node->fi.len = len;
    node->fi.prelock = prelock;
    node->fi.ext = ext;
    node->fi.size = size;
    node->fi.pathId = get_path_list_id(path);

    if (dvr_type == DVR_TYPE_DM500) {
      writeDebug("adding CH%02d_%04d%02d%02d%02d%02d%02d%03d_%u_%c.%c",
	      ch,year,month,day,hour,min,sec,millisec,len,type,ext);
    } else {
      writeDebug("adding CH%02d_%04d%02d%02d%02d%02d%02d_%u_%c.%c",
	      ch,year,month,day,hour,min,sec,len,type,ext);
    }

     /* add to the list */
    node->next = file_list;
    file_list = node;
    //fprintf(stderr, "file_list:%p,next:%p\n", file_list, file_list->next);
  }

  return 0;
}

static int 
create_mark_file_list(char *mount_dir, char *servername,
	     char *path, char *filename, void *arg)
{
  int ret;
  time_t t;
  struct tm tm;
  unsigned short year, millisec;
  unsigned int len;
  char month, day, hour, min, sec, ch, type, ext;
  struct rangeEntry *node, *temp;
  struct stat st;
  char fullname[256];
  int prelock_510;
	
  //writeDebug("create_mark_file_list:%s,%s", filename,servername);

  /* get the file extension */
  char *ptr = strrchr(filename, '.');
  if (!ptr)
    return 1;
  ext = *(ptr + 1); /* 1st character of the extension */

  millisec = 0;
  if (dvr_type == DVR_TYPE_DM500) {
    ret = sscanf(filename,
		 "CH%02hhd_%04hd%02hhd%02hhd"
		 "%02hhd%02hhd%02hhd%03hd_%u_%c_",
		 &ch,
		 &year,
		 &month,
		 &day,
		 &hour,
		 &min,
		 &sec,
		 &millisec,
		 &len,
		 &type);
  } else {
    ret = sscanf(filename,
		 "CH%02hhd_%04hd%02hhd%02hhd"
		 "%02hhd%02hhd%02hhd_%u_%c_",
		 &ch,
		 &year,
		 &month,
		 &day,
		 &hour,
		 &min,
		 &sec,
		 &len,
		 &type);
  }
  
  if (((dvr_type == DVR_TYPE_DM500) && (ret == 10)) ||
      ((dvr_type == DVR_TYPE_DM510) && (ret == 9))) {
    if (check_servername(filename, servername, type, &prelock_510))
      return 1;

    snprintf(fullname, sizeof(fullname), "%s/%s",
	     path, filename);
  
    if ((type == 'L') || (type == 'l')) {
      if (stat(fullname, &st)) {
	writeDebug("create_mark_file_list:%s", strerror(errno));
	return 1;
      }
      upload_file_count++;
      upload_size += st.st_size;
      if ((ch == 0) && (ext != 'K') && (ext != 'k')) {
	if ((dvr_type == DVR_TYPE_DM500) ||
	    ((dvr_type == DVR_TYPE_DM510) && !prelock_510)) {
	  event_count++;
	  event_duration += (dvr_type == DVR_TYPE_DM500) ? (len / 1000) : len;
	}
      }
      writeDebug("create_mark_file_list:L found(%s)", filename);
      add_file_to_list(mount_dir, ch, year, month, day,
		       hour, min, sec, millisec,
		       len, 'L', ext, prelock_510, st.st_size);
    } else {
      temp = range_list;
      while (temp) {
	node = temp;
	temp = node->next;
	tm.tm_isdst = -1;
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;
	t = mktime(&tm);
	//fprintf(stderr,"t:%ld,%ld,%ld\n",
	//  t,node->range.from,node->range.to);
	unsigned int len_in_sec;
	len_in_sec = ((dvr_type == DVR_TYPE_DM500) ? len / 1000 : len);
	if (((unsigned int)t >= node->range.from - len_in_sec) &&
	    (t <= node->range.to)) {
	  if (stat(fullname, &st)) {
	    writeDebug("create_mark_file_list:%s", strerror(errno));
	    return 1;
	  }
#ifdef RENAME_TIMERANGE_FILES
	  /* rename it to _L_ file */
	  fprintf(stderr, "renaming %s\n", filename);
	  if (!rename_file(fullname, 'L')) {
#endif
	    upload_file_count++;
	    upload_size += st.st_size;
	    writeDebug("create_mark_file_list:time range found");
	    add_file_to_list(mount_dir, ch, year, month, day,
			     hour, min, sec, millisec,
			     len, type, ext, prelock_510, st.st_size);
#ifdef RENAME_TIMERANGE_FILES
	  }
#endif
	  break;
	}
      }
    } /* if (type) */	  
  } /* if (dvr_type) */
  
  return 0;
}

int scan_recording_disks(char *rootdir, char *servername)
{
  DIR *dir;
  struct dirent *de;
  char path[256];

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

  return 0;
}

int where_to_cut_file(struct fileInfo *fi, time_t *cut_head, time_t *cut_tail)
{
  struct rangeEntry *node;
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

  node = range_list;
  while (node) {
    unsigned int len_in_sec;
    len_in_sec = ((dvr_type == DVR_TYPE_DM500) ? fi->len / 1000 : fi->len);
    /* don't use >= or <=, as we don't want to copy 1sec video from a file */
    if (((unsigned int)t > node->range.from - len_in_sec) &&
	(t < node->range.to)) {
      if (t < node->range.from) {
	*cut_head = node->range.from;
      }
      if (t + len_in_sec > (int)(node->range.to)) {
	*cut_tail = node->range.to; /* cut tail */
      }
      return 0;
    }
    node = node->next;
  }

  return 1;
}

void mp4ToK(char *filename) {
  filename[strlen(filename) - 3] = '\0';
  strcat(filename, "k");
}

void kTo264(char *filename) {
  filename[strlen(filename) - 1] = '\0';
  strcat(filename, "264");
}

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
	     (fi->ext == 'k') ? "k" : "264");
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
	     (fi->ext == 'k') ? "k" : "264");
  }

  return 0;
}

int create_dest_directory(char *usbroot,
			  char *path, int size,
			  struct fileInfo *fi, char *servername)
{
  if (!servername || !path || size < 0 || !fi)
    return 1;

  // create _busname_ directory
  snprintf(path, size, "%s/_%s_",
	   usbroot,
	   servername);
  if (mkdir(path, 0777) == -1) {
    if(errno != EEXIST) {
      writeDebug("mkdir %s error(%s)", path, strerror(errno));
      return 1;
    }
  }
  
  // create YYYYMMDD directory
  snprintf(path, size, "%s/_%s_/%04d%02d%02d",
	   usbroot,
	   servername,
	   fi->year, fi->month, fi->day);
  if (mkdir(path, 0777) == -1) {
    if(errno != EEXIST) {
      writeDebug("mkdir %s error(%s)", path, strerror(errno));
      return 1;
    }
  }

  // create CHxx directory
  snprintf(path, size, "%s/_%s_/%04d%02d%02d/CH%02d",
	   usbroot,
	   servername,
	   fi->year, fi->month, fi->day,
	   fi->ch);
  if (mkdir(path, 0777) == -1) {
    if(errno != EEXIST) {
      writeDebug("mkdir %s error(%s)", path, strerror(errno));
      return 1;
    }
  }

  return 0;
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

/* 0: success
 * 1: disk full
 * -1: other errors(e.g. system(), statfs())
 */
int copy_file_to_path(char *usbroot, char *from_fullname, char *to_path)
{
  char cmd[512];

  snprintf(cmd, sizeof(cmd),
	   "cp %s %s",
	   from_fullname, to_path);
  writeDebug("%s", cmd);
  int status = system(cmd);
  if (status == -1) {
    writeDebug("%s failed", cmd);
    return -1;
  }

  if (WIFEXITED(status)) {
    int exit_status = WEXITSTATUS(status);
    writeDebug("exited, status=%d", exit_status);
    if (exit_status) { /* error */
      /* check disk space */
      if (is_disk_full(usbroot))
	return 1;
    } else { /* success */
      return 0;
    }
  }

  return -1;
}
  
int parse_frame_header(struct hd_frame *frameh,
		       unsigned int *ts, int *nsub)
{
  if (frameh && frameh->flag == 1) {
    *ts = frameh->timestamp;
    *nsub = frameh->d_frames & 0xff;
    return 0;
  }

  writeDebug("wrong frame header flag");
  return 1;
}

int parse_subframe_header(struct hd_subframe *frameh,
			  int *flag, int *size)
{
  if (frameh) {
    *flag = frameh->d_flag & 0xffff;
    *size = frameh->framesize;
    return 0;
  }

  writeDebug("null subframe header");
  return 1;
}

int copy_subframes(FILE *fp_src, FILE *fp_dst, int nsub, int *videokey_found)
{
  struct hd_subframe subframeh;
  int f_flag, bytes;
  unsigned int f_size;
  char *buf = NULL;
  int bufsize = 0;
  int total = 0;

  //writeDebug("copy_subframes");
  if (!fp_src || !fp_dst)
    return -1;

  if (nsub <= 0)
    return -1;

  if (!videokey_found)
    return -1;

  *videokey_found = 0;

  while (nsub > 0) {
    /* copy subframe header to the new file */
    bytes = fread(&subframeh, 1,
		  sizeof(struct hd_subframe), fp_src);
    if (bytes != sizeof(struct hd_subframe)) {
      writeDebug("fread subframe header");
      total = -1;
      break;
    }
    
    if (parse_subframe_header(&subframeh, &f_flag, (int*)&f_size)) {
      total = -1;
      break;
    }

    //writeDebug("subframe header(%x,%u)", f_flag, f_size);
    if (f_flag == 0x1003) {
      *videokey_found = 1;
    }
    
    bytes = fwrite(&subframeh, 1, sizeof(struct hd_subframe), fp_dst);
    if (bytes != sizeof(struct hd_subframe)) {
      writeDebug("fwrite subframe header");
      total = -1;
      break;
    }
    total += bytes;

    /* copy subframe data to the new file */
    if (f_size > bufsize) {
      buf = (char *)realloc(buf, f_size);
      bufsize = f_size;
    }
    if (!buf) {
      writeDebug("realloc error");
      total = -1;
      break;
    }

    bytes = fread(buf, 1, f_size, fp_src);
    if (bytes != f_size) {
      writeDebug("fread subframe data");
      total = -1;
      break;
    }

    bytes = fwrite(buf, 1, f_size, fp_dst);
    if (bytes != f_size) {
      writeDebug("fwrite subframe data");
      total = -1;
      break;
    }

    total += bytes;
    nsub--;
  }

  if (buf) free(buf);

  //writeDebug("copy_subframes done(%d)",total);

  return total;
}

int copy_frames(FILE *fp_src, FILE *fp_dst, FILE *fp_dst_k,
		long long from_offset, long long to_offset,
		long long toffset_start,
		time_t filetime, time_t *new_filetime, time_t *new_len)
{
  int bytes;
  int f_nsub = 0;
  unsigned int ts_start = 0, f_ts;
  struct hd_frame frameh;
  long startpos, nowpos;
  long long new_file_offset = 40;
  long long first_toffset = 0, last_toffset = 0;
  int first_toffset_written = 0;
#ifdef SLOW_COPY
  int bytes_to_flush = 0;
#endif

  if (!fp_src || !fp_dst)
    return 1;

  startpos = ftell(fp_src);

  /* read the 1st time stamp */
  bytes = fread(&frameh, 1,
		sizeof(struct hd_frame), fp_src);
  if (bytes != sizeof(struct hd_frame)) {
    writeDebug("fread frame header");
    return 1;
  }

  if (parse_frame_header(&frameh, &f_ts, &f_nsub)) {
    return 1;
  }
  ts_start = f_ts;
  writeDebug("1st ts:%lu", ts_start);

  /* seek to the 1st frame */
  if (from_offset) {
    if (fseek(fp_src, from_offset, SEEK_SET) == -1) {
      writeDebug("fseek error to %lld", from_offset);
      return 1;
    }
  } else {
    fseek(fp_src, startpos, SEEK_SET);
  }

  int retval = 0;
  while (1) {
    nowpos = ftell(fp_src);
    if (to_offset && nowpos >= to_offset) {
      break;
    }

    /* copy frame header to the new file */
    clearerr(fp_src);
    bytes = fread(&frameh, 1,
		  sizeof(struct hd_frame), fp_src);
    if (bytes != sizeof(struct hd_frame)) {
      writeDebug("fread frame header2:%d", bytes);
      if (ferror(fp_src))
	retval = 1;
      break;
    }

    if (parse_frame_header(&frameh, &f_ts, &f_nsub)) {
      retval = 1;
      break;
    }

    int ts_diff = f_ts - ts_start;
    long long ts_diff_in_millisec = ts_diff * 1000LL / 64LL; 
    long long absolute_ts_in_millisec = 
      filetime * 1000LL + toffset_start + ts_diff_in_millisec; 
    long long ts_diff_in_millisec_from_new_filetime = 
      absolute_ts_in_millisec - (*new_filetime * 1000LL); 

    while ((ts_diff_in_millisec_from_new_filetime < 0) &&
	   (new_file_offset == 40)) {
      writeDebug("adjusting new_filetime");
      (*new_filetime)--;
      ts_diff_in_millisec_from_new_filetime = 
	absolute_ts_in_millisec - (*new_filetime * 1000LL);
    }
    //writeDebug("frame header(%d,%u,%lld)", f_nsub, f_ts, ts_diff_in_millisec_from_new_filetime);
    
    if (!first_toffset_written) {
      first_toffset_written = 1;
      first_toffset = ts_diff_in_millisec_from_new_filetime;
      writeDebug("1st_offset:%lld,%ld,%lld,%lld,%lld",toffset_start,
		 ts_diff,ts_diff_in_millisec,
		 absolute_ts_in_millisec,ts_diff_in_millisec_from_new_filetime);
    }
    last_toffset = ts_diff_in_millisec_from_new_filetime;

    bytes = fwrite(&frameh, 1, sizeof(struct hd_frame), fp_dst);
    if (bytes != sizeof(struct hd_frame)) {
      writeDebug("fwrite frame header");
      retval = 1;
      break;
    }

    int videokey_found;
    bytes = copy_subframes(fp_src, fp_dst, f_nsub, &videokey_found);
    if (bytes <= 0) {
      retval = 1;
      break;
    }

#ifdef SLOW_COPY
    bytes_to_flush += bytes;
    if (bytes_to_flush > (100 * 1024)) {
      bytes_to_flush = 0;
      fdatasync(fileno(fp_dst));
    }
#endif

    if (videokey_found) {
      fprintf(fp_dst_k, "%lld,%lld\n",
	      ts_diff_in_millisec_from_new_filetime, new_file_offset);
    }

    new_file_offset += (sizeof(struct hd_frame) + bytes);
  } 

  *new_len = (last_toffset - first_toffset) / 1000;
  if (*new_len < 0) {
    retval = 1;
  }
  writeDebug("newlen:%ld(%lld,%lld)", *new_len, last_toffset, first_toffset);

#ifdef SLOW_COPY
    if (bytes_to_flush) {
      fdatasync(fileno(fp_dst));
    }
#endif

  return retval;
}

int get_filename(char *newname, int size,
		 int ch, unsigned int len, char type, char *servername,
		 struct tm *tm)
{
  if (!newname || !servername || !tm)
    return 1;

  snprintf(newname, size,
	   "CH%02d_%04d%02d%02d%02d%02d%02d_%u_%c_%s.264",
	   ch,
	   tm->tm_year + 1900,
	   tm->tm_mon + 1, tm->tm_mday,
	   tm->tm_hour, tm->tm_min, tm->tm_sec,
	   len,
	   type,
	   servername);

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

/* 0: success
 * 1: disk full
 * -1: other errors
 */
int copy_file_to_usb(char *usbroot, char *servername,
		     struct fileInfo *fi, time_t from, time_t to,
		     char *dest_dir)
{
  int retval = 0;
  char fullname[256], newname[128], buf[128];
  char *newfile = NULL, *newfile2 = NULL;
  time_t filetime, new_filetime;
  struct tm tm;
  FILE *fp = NULL, *fpnew = NULL, *fpnew_k = NULL;
  long long toffset, offset; // read from k file
  long long toffset_save = 0, offset_save = 0; // last record saved
  long long from_offset = 0, to_offset = 0; // offset found from k file(from,to)
  long long from_toffset = 0, to_toffset = 0; // toffset found from k file(from,to)
  long long toffset_start = 0; // 1st ts offset to be used in the new file
  int toffset_start_written = 0;
  int fdelete = 0;
  struct hd_file fileh;
  int bytes ;
  time_t new_len;
  int encrypted ;

  writeDebug("copy_file_to_usb");

  filetime = get_filetime(fi);

  if(get_full_path_for_fileinfo(fullname, sizeof(fullname),
				fi, servername)) {
    return -1;
  }

  dio_iomsg("Copying Video Files", 1) ;
        
  if (!from && !to) {
    /* just copy 264/k files */
    int ret = copy_file_to_path(usbroot, fullname, dest_dir);
    if (!ret) {
      mp4ToK(fullname);
      return copy_file_to_path(usbroot, fullname, dest_dir);
    } else {
      return ret;
    }
  }

  mp4ToK(fullname);
  fp = fopen(fullname, "r");
  if (!fp)
    return -1;

  while(fgets(buf, sizeof(buf), fp)) {
    if (2 == sscanf(buf, "%lld,%lld\n", &toffset, &offset)) {
      /* save the 1st offset */
      if (!toffset_start_written) {
	toffset_start_written = 1;
	toffset_start = toffset;
	writeDebug("saved 1st:%lld", toffset_start);
      }

      time_t ts = filetime + (time_t)(toffset / 1000LL);
      if (from && !from_offset) {
	if (ts == from) {
	  from_offset = offset;
	  from_toffset = toffset;
	  //writeDebug("from exact:%lld(%lld)", from_offset, from_toffset);
	} else if (ts > from) {
	  if (offset_save) {
	    from_offset = offset_save;
	    from_toffset = toffset_save;
	    //writeDebug("from back:%lld(%lld)", from_offset, from_toffset);
	  } else {
	    from_offset = offset;
	    from_toffset = toffset;
	    //writeDebug("from about:%lld(%lld)", from_offset, from_toffset);
	  }
	}
      }
      if (to && !to_offset) {
	if (ts >= to) {
	  to_offset = offset;
	  to_toffset = toffset;
	  //writeDebug("to about:%lld(%lld)", to_offset, from_toffset);
	}
      }
      offset_save = offset;
      toffset_save = toffset;
    }
  }

  fclose(fp); fp = NULL;

  writeDebug("from:%lld(%lld), to:%lld(%lld)", from_offset, from_toffset, to_offset, to_toffset);

  writeDebug("filetime:%ld", filetime);
  /* set start time for the new file */
  new_filetime = filetime;
  if (from_toffset) {
    new_filetime = filetime + (time_t)(from_toffset / 1000);
  }

  writeDebug("new_filetime:%ld", new_filetime);
  localtime_r(&new_filetime, &tm);

  /* do real copy now */
  get_filename(newname, sizeof(newname),
	       fi->ch, 0, fi->type, servername,
	       &tm);
  writeDebug("newname:%s", newname);

  newfile = (char *)malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile) {
    writeDebug("malloc error");
    retval = -1;
    goto error_exit;
  }
  sprintf(newfile, "%s/%s", dest_dir, newname);

  kTo264(fullname);
  fp = fopen(fullname, "r");
  if (!fp) {
    writeDebug("fopen1:%s(%s)", strerror(errno), fullname);
    retval = -1;
    goto error_exit;
  }


  fpnew = fopen(newfile, "w");
  if (!fpnew) {
    writeDebug("fopen2:%s(%s)", strerror(errno), fullname);
    retval = -1;
    goto error_exit;
  }
  writeDebug("new:%s", newfile);

  mp4ToK(newfile);
  fpnew_k = fopen(newfile, "w");
  if (!fpnew_k) {
    writeDebug("fopen3:%s(%s)", strerror(errno), fullname);
    retval = -1;
    goto error_exit;
  }
  writeDebug("newk:%s", newfile);
  kTo264(newfile);

  bytes = fread(&fileh, 1,
		    sizeof(struct hd_file), fp);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fread header(%d, %s)", bytes, fullname);
    fdelete = 1;
    retval = -1;
    goto error_exit;
  }
  
  encrypted = 1;
  if (fileh.flag == 0x484b4834)
    encrypted = 0;

  bytes = fwrite(&fileh, 1, sizeof(struct hd_file), fpnew);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fwrite header(%s)", fullname);
    fdelete = 1;
    retval = -1;
    goto error_exit;
  }

  if (copy_frames(fp, fpnew, fpnew_k,
		  from_offset, to_offset, toffset_start,
		  filetime, &new_filetime, &new_len)) {
    fdelete = 1;
    retval = -1;
    goto error_exit;
  }

  localtime_r(&new_filetime, &tm);
  get_filename(newname, sizeof(newname),
	       fi->ch, (unsigned int)new_len, fi->type, servername,
	       &tm);
  newfile2 = (char *)malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile2) {
    writeDebug("malloc error");
    fdelete = 1;
    retval = -1;
    goto error_exit;
  }
  sprintf(newfile2, "%s/%s", dest_dir, newname);

  writeDebug("rename %s %s", newfile, newfile2);
  if (rename(newfile, newfile2) == -1) {
    writeDebug("rename error (%s) %s %s", strerror(errno), newfile, newfile2);
  }
  mp4ToK(newfile);
  mp4ToK(newfile2);
  if (rename(newfile, newfile2) == -1) {
    writeDebug("rename error (%s) %s %s", strerror(errno), newfile, newfile2);
  }

error_exit:

  if (fpnew_k) fclose(fpnew_k);
  if (fpnew) fclose(fpnew);
  if (fp) fclose(fp);

  if (fdelete) {
    writeDebug("deleting %s", newfile);
    unlink(newfile);
    mp4ToK(newfile);
    writeDebug("deleting %s", newfile);
    unlink(newfile);
  }

  if (newfile2) free(newfile2);
  if (newfile) free(newfile);

  if (retval && is_disk_full(usbroot))
    return 1;

  return retval;
}

int wait_until_stopped() {
  FILE *fp;
  int retry = 0;

  while(retry < 10) {
    sleep(1);
    fp = fopen("/var/dvr/dvrcurdisk", "r");
    if (!fp) {
      return 0;
    }

    retry++;
    fclose(fp);
  }

  return 1;
}

pid_t stop_dvrsvr() {
  FILE *fp;
  char buf[128];
  int pid = -1;

  fp = fopen("/var/dvr/dvrsvr.pid", "r");
  if (fp) {
    if (fgets(buf, sizeof(buf), fp)) {
      pid = atoi(buf);
    }
    fclose(fp);
  }

  if (pid > 0) {
    if (!kill(pid, SIGUSR1)) {      
      wait_until_stopped();
    }
  }

  strncpy(buf, "/bin/ifconfig rausb0 down", sizeof(buf));
  system(buf);  
  strncpy(buf, "/bin/rmmod rt73", sizeof(buf));
  system(buf);  
  strncpy(buf, "/bin/rmmod mos7840", sizeof(buf));
  system(buf);  
  strncpy(buf, "/bin/rmmod usbserial", sizeof(buf));
  system(buf);  

  return pid;
}

int start_dvrsvr(pid_t pid) {
  if (pid > 0) {
    if (kill(pid, SIGUSR2)) {      
      writeDebug("dvrsvr start error(%s)", strerror(errno));
      return 1;
    }
  }

  writeDebug("starting network again");
  char buf[128];
  strncpy(buf, "/davinci/dvr/setnetwork", sizeof(buf));
  system(buf);  

  return 0;
}

int upload_files(char *usbroot, char *servername)
{
  struct fileEntry *node;
  int ret;
  char path[256], path2[256];

  writeDebug("upload_files");

  file_list = listsort(file_list);

  node = file_list;
  while (node) {

// Display io message
  dio_iomsg("Copying Video Files", 1) ;
      
    if (!pid_dvrsvr) {
      pid_dvrsvr = stop_dvrsvr();
      if (pid_dvrsvr > 0)
	writeDebug("dvrsvr stopped");
    }

    if(get_full_path_for_fileinfo(path, sizeof(path),
				  &node->fi, servername)) {
      node = node->next;
      continue;
    }
    
    writeDebug("  file:%s", path);
    if (create_dest_directory(usbroot,
			      path2, sizeof(path2),
			      &node->fi, servername)) {
      break;
    }

    if (node->fi.type == 'L' || node->fi.type == 'l') {
      if (node->fi.prelock) {
	/* ignore k files */
	if (node->fi.ext != 'k') {
	  time_t pre;
	  
	  pre = get_filetime(&node->fi) + node->fi.len - node->fi.prelock;
	  ret = copy_file_to_usb(usbroot, servername,
				 &node->fi,
				 pre,
				 0, path2);
	  if (!ret) {    /* success */
	    rename_file(path, 'N');
	    mp4ToK(path);
	    rename_file(path, 'N');
	  } else if (ret == 1) {
	    break; /* disk full */
	  }
	  /* on other errors, go on to next files */
	}
      } else {
	ret = copy_file_to_path(usbroot, path, path2);
	if (!ret) {    /* success */
	  rename_file(path, 'N');
	} else if (ret == 1) {
	  break; /* disk full */
	}
	/* on other errors, go on to next files */
      }
    } else { /* time range files */
      if (node->fi.ext != 'k') {
	/* cut file and copy */
	time_t from, to;
	ret = where_to_cut_file(&node->fi, &from, &to);
	writeDebug("cut: %d(%ld,%ld),%s", ret, from, to, path);
	ret = copy_file_to_usb(usbroot, servername,
			       &node->fi, from, to, path2);
	/* don't rename these files */
	if (ret == 1) {
	  break; /* disk full */
	}
	/* on other errors, go on to next files */
      }
    }

    node = node->next;
  }

  if (pid_dvrsvr > 0) {
    start_dvrsvr(pid_dvrsvr);
  }

  return 0;
}

int main(int argc, char **argv)
{
  char *time_range = NULL;
  char *usbroot;
  char servername[128];

  connectDebug();

  if (signal(SIGBUS, buserror_handler) == SIG_IGN)
    signal(SIGBUS, SIG_IGN);

  if (gethostname(servername, sizeof(servername))) {
    writeDebug("hostname error:%s", strerror(errno));
    exit(RT_HOSTNAME_ERROR);
  }

  usbroot = getenv("USBROOT");
  /* for local debugging */
  if (!usbroot)
    usbroot = "/var/dvr/xdisk/sdb1";

  writeDebug("busname:%s, usbroot:%s", servername, usbroot);

  if( !dio_start() ) {
      writeDebug("IO map file error!");
      exit( RT_HOSTNAME_ERROR );
  }
      
  read_config();

  dio_iomsg("Copying Video Files",1);
    
  time_range = checkTmeLogFile(usbroot);
  if (time_range) {
    add_time_range_to_list(time_range, servername);
    free(time_range);
  }

  scan_recording_disks(mountdir, servername);
  upload_files(usbroot, servername);

#if 0
  for (ch = 0; ch < MAX_CHANNEL; ch++) {
    int handle = stream_open(sfd, ch);
    if (handle) {
      stream_close(sfd, handle);
    }
  }
#endif

  free_file_list();
  free_dir_list();
  free_range_list();

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "umount -l %s", usbroot);
  system(cmd);  

  writeDebug("filert ends");

  dio_finish();
  sleep(1);

  return 0;
}
