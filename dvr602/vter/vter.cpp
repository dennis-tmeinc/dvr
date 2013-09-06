/*
 * DVR should support REQ2GETJPEG.
 * make sure to return ANSERROR = 1 for unsupported camera channels.
 * Test DVR with "vter -t 15111 -j" command (change port number).
 *
 * for building libev: check new_hint.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ev.h>
#include "crc.h"
#include "debug.h"
#include "token.h"

enum  {CMD_JPEG_REQ = 1, CMD_JPEG_WRITE, CMD_JPEG_CANCEL};
enum {ACK_NONE, ACK_FAIL, ACK_SUCCESS};
enum dst {DST_AVL = 1, DST_SS, DST_DVR};
enum tod {TOD_COMMAND = 1, TOD_INDICATE, TOD_DATA, TOD_RESPONSE};
enum msg {MSG_NONE, MSG_RS, MSG_EOI, MSG_EOT};
enum status {
  ST_UNINITIALIZED,
  ST_READY, /* ready to receive jpeg reqest from SS */
  ST_JPEG_REQ, /* jpeg request received from SS */
  ST_SENDING_JPEG,
  ST_JPEG_WRITE_ACK,
  ST_JPEG_RESEND,
  ST_DONE_JPEG};
enum reqcode_type { REQ2GETJPEG = 232, REQ2GETGPS = 235};
enum anscode_type { ANSERROR = 1, ANSOK, ANS2JPEG = 217, ANS2GETGPS = 220};
enum { VTER_PORT = 55001 };
enum { MAX_CONN = 16 }; /* max connection allowed from vter, 1 is enough */
#define VTER_BUF_SIZE (4 * 1024)
#define MAX_CAM 32
#define MSGVER (1)
#define SIZE_TIMEOUT (2.0)
#define USE_GPS

short dvrsvr_port = 15111;
char vter_dev[32] = "eth0:1";
char *pidfile;

int b_end, b_restart;

struct dvr_req {
    int reqcode;
    int data;
    int reqsize;
};

struct dvr_ans {
    int anscode;
    int data;
    int anssize;
};

struct gps_t {
  double	gps_speed ;	// knots
  double	gps_direction ; // degree
  double  gps_latitude ;	// degree, + for north, - for south
  double  gps_longitud ;      // degree, + for east,  - for west
  double  gps_gpstime ;	// seconds from 1970-1-1 12:00:00 (utc)
};

/* All values in LSB first(little endian: 0x01234567 = 67 45 23 01). */
struct mss_msg
{
  uint8_t id[2]; // magic number: 'M','S' (id[0]='M',id[1]='S')
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
};

struct vter_msg_t {
  uint8_t magic[4];
  uint8_t dst;
  uint8_t tod;
  uint8_t msg;
  uint8_t reserved;
  uint8_t len[4];
  uint8_t crc[4];
};

typedef struct {
  uint32_t id, offset, size;
} chunk_t;

typedef struct {
  int camid; /* camera number: 1, 2, 3... */
  int jpeg_acquired; /* jpeg image in possession: 0 - none, 1 - 1st image */
  int jpeg_sent; /* jpeg image already sent: 0 - none, 1 - 1st image */
  int cur_chunk; /* chunk seq no to send: starting from 0 */
} caminfo_t;
 
typedef struct {
  ev_timer *data_timeout, *size_timeout, *interval_timeout, *jpeg_timeout;
  ev_io *read_watcher, *write_watcher;
  int status, pending_data, chunk_size, interval_expired;
  int size_timeout_count, close_connection_on_timeout;
  caminfo_t caminfo[MAX_CAM];
  int cur_image; /* starting from 0 */
  int cam_count, image_count, resolution, quality, interval;
  unsigned char *data, *tx_data, *jpeg_data;
  chunk_t *chunks; /* chunk info renewed on each image or on jpeg_write_err */
  chunk_t *err_chunks; /* array of missing chunks from SS */
  size_t n_chunks; /* # of chunks in chunks[] */
  size_t n_err_chunks; /* # of chunks in err_chunks[] */
  size_t chunks_size; /* size of allocated chunks array */
  size_t data_size, tx_data_size, jpeg_data_size, jpeg_data_sent;
} conn_t;

conn_t connections[MAX_CONN];
struct vter_msg_t vter_msg;


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

inline uint32_t GetDWBE(void const *_p)
{
  uint8_t *p = (uint8_t *)_p;
  return ( ((uint32_t)p[0] << 24) | 
	   ((uint32_t)p[1] << 16) |
	   ((uint32_t)p[2] <<  8) |
	   p[3] );
}

inline void SetDWBE(uint8_t *p, uint32_t i_dw)
{
  p[0] = (i_dw >> 24) & 0xff;
  p[1] = (i_dw >> 16) & 0xff;
  p[2] = (i_dw >>  8) & 0xff;
  p[3] = (i_dw      ) & 0xff;
}

int is_valid_message(uint8_t *p, int len)
{
  if (len < 14) return 0;

  if ((p[0] == 'V') &&
      (p[1] == 'T') &&
      (p[2] == 'E') &&
      (p[3] == 'R') &&
      (p[4] == 3) &&
      (p[5] == 0)) {
    uint32_t crc, crc2;
    crc = crc32(0, p, 10);
    crc2 = GetDWBE(p + 10);
    if (crc != crc2) {
      fprintf(stderr, "CRC error:%x (expecting %x)\n", crc2, crc);
      return 0;
    }
    return 1;
  }

  return 0;
}

void create_message(struct mss_msg_set *set, unsigned char type,
		    uint8_t ack_code, uint8_t reason,
		    uint16_t wData, uint32_t dwData, uint64_t qwData,
		    const char *data = NULL, int data_size = 0)
{
  uint32_t crc;

  struct mss_msg *msg = &set->msg;
  memset(set, 0, sizeof(struct mss_msg_set));
  msg->id[0] = 'M';
  msg->id[1] = 'S';
  msg->version = MSGVER;
  msg->command = type;
  msg->ack_code = ack_code;
  msg->reason = reason;
  msg->wData = wData;
  msg->dwData = dwData;
  msg->qwData = qwData;

  if (data && (data_size > 0)) {
    set->ext_data = (char *)data;
    SetDWLE((uint8_t *)&msg->ext_datasize, data_size);
    crc = crc32(0, (unsigned char *)data, data_size);
    SetDWLE((uint8_t *)&msg->ext_crc, crc);
  }

  crc = crc32(0, (unsigned char *)msg, sizeof(struct mss_msg) - sizeof(int));
  SetDWLE((uint8_t *)&msg->crc, crc);
}

/* parse_xml: get xml value for <name> in xml_line
 *            return: value (should be freeed by the caller)
 *                    NULL if not found
 *            next: continue next parsing from here
 */           
char *parse_xml(const char *xml_line, const char *name, char **next)
{
  char *value, *start, *end, *tag, *str_end;

  if (xml_line == NULL)
    return NULL;

  int str_len = strlen(xml_line);
  if (str_len == 0 || str_len < (int)strlen(name))
    return NULL;

  tag = (char *)malloc(str_len + 4);
  if (tag == NULL)
    return NULL;

  value = NULL;
  str_end = (char *)xml_line + str_len;

  sprintf(tag, "<%s>", name);
  start = strstr((char *)xml_line, tag);
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

int create_vter_msg(struct vter_msg_t *hdr, 
		    int dest, int tod, int msg, int payload_len)
{
  uint32_t crc;

  if (hdr == NULL) return 0;

  hdr->magic[0] = 'V';
  hdr->magic[1] = 'T';
  hdr->magic[2] = 'E';
  hdr->magic[3] = 'R';
  hdr->dst = dest; 
  hdr->tod = tod; 
  hdr->msg = msg; 
  hdr->reserved = 0; 
  SetDWBE(hdr->len, payload_len);
  crc = crc32(0, (unsigned char *)hdr, sizeof(struct vter_msg_t) - 4);
  SetDWBE(hdr->crc, crc);

  return sizeof(struct vter_msg_t);
}

/* can be called many times until it returns 0 */
int getOnePacketFromBuffer(conn_t *conn, struct vter_msg_t *msghdr) {
  unsigned char *ptr;
  int found = 0;

  if (conn->data_size < (int)sizeof(struct vter_msg_t))
    return 0;

  /* find the start of one packet */
  ptr = conn->data;
  int to_check = conn->data_size;
  while (to_check >= (int)sizeof(struct vter_msg_t)) {
    if ((ptr[0] != 'V') ||
	(ptr[1] != 'T') ||
	(ptr[2] != 'E') ||
	(ptr[3] != 'R')) {
      /* advance one byte */
      ptr++;
      to_check--;
      continue;
    }

    /* check Dest ID */
    if (ptr[4] != 3) {
      ptr += 5;
      to_check -= 5;
      continue;
    }

    /* check command type */
    if ((ptr[5] < 1) || (ptr[5] > 4)) {
      ptr += 6;
      to_check -= 6;
      continue;
    }

    /* check message */
    if (ptr[6] > 4) {
      ptr += 7;
      to_check -= 7;
      continue;
    }

    /* check checksum */
    uint32_t crc, crc2;
    crc = crc32(0, ptr, 12);
    crc2 = GetDWBE(ptr + 12);
    if (crc != crc2) {
      fprintf(stderr, "CRC error:%x (expecting %x)\n", crc2, crc);
      ptr += 16;
      to_check -= 16;
      continue;
    }

    /* finally we got a valid message */
    found = 1;
    memcpy(msghdr, ptr, sizeof(struct vter_msg_t));
    ptr += 16;
    to_check -= 16;
    break;
  } 

  if (to_check > 0) {
    /* copy remaining bytes to the head of the main buffer */
    memmove(conn->data, ptr, to_check);
  }

  conn->data_size = to_check;
  if (conn->data_size < 0) 
    conn->data_size = 0;

  return found;
}

int change_to_nonblock(int sock)
{
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
      perror("fcntl:F_GETFL");
      return 1;
  }
   
  if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl:F_SETFL");
    return 1;
  }

  return 0;
}

/* Usage: makeServerSocket(SOCK_STREAM or SOCK_DGRAM, port)
 *
 */
int makeServerSocket (int type, unsigned short port)
{
  int sock, yes = 1;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket (PF_INET, type, 0);
  if (sock < 0) {
    perror ("socket");
    return -1;
  }

  if (change_to_nonblock(sock)) {
    close(sock);
    return -1;
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    perror("setsockopt");
    close(sock);
    return -1;
  }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  memset(&(name.sin_zero), '\0', 8);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
    perror ("bind");
    close(sock);
    return -1;
  }

  if (type == SOCK_DGRAM) {
    return sock;
  }

  /* listen */
  if (listen(sock, 20) == -1) {
    perror("listen");
    close(sock);
    return -1;
  }

  return sock;
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
    close(sfd);
    return -1;
  }

  return sfd;
}

int connectNonBlockingTcp(char *addr, short port)
{
  int sfd, flags, ret, xerrno;
  socklen_t len;
  struct sockaddr_in destAddr;
  fd_set rset, wset;
  struct timeval timeout;

  sfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    perror("socket");
    return -1;
  }

  flags = fcntl(sfd, F_GETFL, 0);
  if (flags == -1) {
      perror("fcntl:F_GETFL");
  } else {
    if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1) {
      perror("fcntl:F_SETFL");
      close(sfd);
      return -1;
    }
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
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  ret = select(sfd + 1, &rset, &wset, NULL, &timeout);

  if (ret > 0) {
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
  } else if (ret == -1) {
    perror("connectNonBlockingTcp:select");
  } else if (ret == 0) {
    fprintf(stderr, "connectNonBlockingTcp: select time out\n");
  }
  close(sfd);
  return -1;

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

/*
 * read_nbytes:read at least rx_size and as much data as possible
 * (or at most rx_size) with timeout
 * return:
 *    n: # of bytes received
 *   -1: bad size parameters
 */  

int read_nbytes(int fd, unsigned char *buf, int bufsize,
		int rx_size, struct timeval *timeout_tv,
		int showdata, int showprogress, int receive_exact = 0)
{
  struct timeval tv;
  int total = 0, bytes, to_receive;

  if (bufsize < rx_size)
    return -1;

  if (rx_size == 0)
    return 0;

  while (1) {
    bytes = 0;
    tv = *timeout_tv;
    if (blockUntilReadable(fd, &tv) > 0) {
      to_receive = bufsize - total;
      if (receive_exact) {
	to_receive = rx_size - total;
      }
      bytes = read(fd, buf + total, bufsize - total);
      if (bytes > 0) {
	if (showdata)
	  dump(buf + total, bytes);
	total += bytes;
	if (showprogress) fprintf(stderr, ".");
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

#if 0
/* send discover message to the specified subnet
 * and get the server's ip/port
 */
int discover_vter(struct in_addr broadcast,
		  struct in_addr *serverIp)
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
    
  if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int)) < 0) {
    perror("setsockopt");
    close(sockfd);
    return 1;
  }
  
  hisAddr.sin_family = AF_INET;
  hisAddr.sin_port = htons(VTER_PORT);
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
  
  int found = 0;
  int sendit = 1;
  int msg_len = create_vter_msg(DST_AVL, sendbuf, 0);

  while (tried < 60) { 
    if (sendit) {
      hisAddr.sin_addr = broadcast;
      /* don't use INADDR_BROADCAST,
       * otherwise broadcast goes through eth1 only
       */
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

      if (nbytes >= (int)sizeof(buf)) {
	/* message too big */
	tried++;
	continue;
      }

      buf[nbytes] = 0;
      fprintf(stderr, "rcvd %s,%d\n", buf,nbytes);
      dump(buf, nbytes);

      ret = is_valid_message(buf, nbytes);
      if (ret == 1) {
	break;
      } else {
	if (memcmp(buf, sendbuf, msg_len)) {
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

  if (!found)
    return 1;

  *serverIp = hisAddr.sin_addr;
  fprintf(stderr, "found vter:%s\n", inet_ntoa(hisAddr.sin_addr));

  return 0;
}
#endif

int get_dev_ip(const char *dev,
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

/*
 * ch: 0, 1, 2...
 * jpeg_quality:  0-best, 1-better, 2-average 
 * jpeg_pic    :  0-cif , 1-qcif, 2-4cif 
 */
int get_jpeg(int ch, int jpeg_quality, int jpeg_pic,
	     unsigned char **buf, size_t *len)
{
  int fd, bytes, size, ret = 1;
  struct dvr_req req;
  struct dvr_ans ans;
  unsigned char gbuf[40];
  struct gps_t gps;

  memset(&gps, 0, sizeof(gps));

  //fd = connectNonBlockingTcp("192.168.1.100", dvrsvr_port);
  fd = connectNonBlockingTcp("127.0.0.1", dvrsvr_port);
  if (fd == -1)
    return 1;

  fprintf(stderr, "connected to JPEG server(cam:%d)\n",ch);

  req.reqcode = REQ2GETGPS ;
  req.data = 0;
  req.reqsize = 0;
  size = sizeof(req);
  if (!sendall(fd, (unsigned char *)&req, &size)) {
    struct timeval tv;

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    bytes = read_nbytes(fd, (unsigned char *)&ans, sizeof(ans),
			sizeof(ans), &tv, 1, 0);
    fprintf(stderr, "get_gps reply:%d bytes recved\n", bytes);
    if (bytes == sizeof(ans)) {
      if (ans.anscode == ANS2GETGPS && 
	  ans.anssize >= (int)sizeof(struct gps_t)) {
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	bytes = read_nbytes(fd, gbuf, ans.anssize,
			    ans.anssize, &tv, 0, 0);
	fprintf(stderr, "get_gps data:%d bytes recved\n", bytes);
	if (bytes == ans.anssize) {
#ifdef __arm__
	  struct gps_t *g = (struct gps_t *)gbuf;
	  double speed_in_knots = g->gps_speed;
	  double speed_in_km = speed_in_knots * (double)1.852; // to km/h
	  double *pd = (double *)gbuf;
	  *pd = speed_in_km;
	  fprintf(stderr, "GPS:%.6f(%.6f),%.6f,%.6f,%.6f,%.6f\n",
		  speed_in_km, speed_in_knots,
		  g->gps_direction,
		  g->gps_latitude,
		  g->gps_longitud,
		  g->gps_gpstime);
#endif
	  /* arm is using median endianness for double */
	  int i;
	  unsigned char *x;
	  x = (unsigned char *)&gps;
	  for (i = 0; i < 5; i++) {
	    x[8*i]   = gbuf[8*i+4];
	    x[8*i+1] = gbuf[8*i+5];
	    x[8*i+2] = gbuf[8*i+6];
	    x[8*i+3] = gbuf[8*i+7];
	    x[8*i+4] = gbuf[8*i];
	    x[8*i+5] = gbuf[8*i+1];
	    x[8*i+6] = gbuf[8*i+2];
	    x[8*i+7] = gbuf[8*i+3];
	  }
	  //dump(x, 40);
#ifndef __arm__
	  gps.gps_speed *= (double)1.852; // to km/h
	  fprintf(stderr, "GPS2:%.6f,%.6f,%.6f,%.6f,%.6f\n",
		  gps.gps_speed,
		  gps.gps_direction,
		  gps.gps_latitude,
		  gps.gps_longitud,
		  gps.gps_gpstime);
#endif
	}
      }
    }
  }

  req.reqcode=REQ2GETJPEG ;
  req.data=(ch&0xff)|( (jpeg_quality&0xff)<<8)|((jpeg_pic&0xff)<<16) ;
  req.reqsize=0 ;
  size = sizeof(req);
  if (!sendall(fd, (unsigned char *)&req, &size)) {
    struct timeval tv;

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    bytes = read_nbytes(fd, (unsigned char *)&ans, sizeof(ans),
			sizeof(ans), &tv, 1, 0);
    fprintf(stderr, "get_jpeg reply:%d bytes recved\n", bytes);
    if (bytes == sizeof(ans)) {
      if (ans.anscode == ANS2JPEG && ans.anssize > 0) {
	*buf = (unsigned char *)malloc(ans.anssize + 100);
	if (*buf) {
	  unsigned char *jpeg_start;
	  jpeg_start = *buf;
#ifdef USE_GPS
	  double *d;
	  struct timeval t;

	  memset(*buf, 0 , 100);
	  gettimeofday(&t, NULL);
	  SetDWLE(*buf, t.tv_sec);

	  d = (double *)(*buf + 4);
	  *d = gps.gps_latitude;
	  d++;
	  *d = gps.gps_longitud;
	  d++;
	  *d = gps.gps_speed; // km/h
	  d++;
	  *d = gps.gps_direction;
	  jpeg_start += 100;
#endif
	  tv.tv_sec = 5;
	  tv.tv_usec = 0;
	  bytes = read_nbytes(fd, jpeg_start, ans.anssize,
			      ans.anssize, &tv, 0, 0);
	  fprintf(stderr, "get_jpeg image:%d bytes recved\n", bytes);
	  *len = 0;
	  if (bytes == ans.anssize) {
	    *len = (size_t)bytes;
#ifdef USE_GPS
	    *len += 100;
#endif
	    ret = 0;
	  } else {
	    free(*buf);
	    *buf = NULL;
	  }
	}
      }
    }
  }

  close(fd);
  return ret;
}

int enable_tcp_keepalive(int fd)
{
  int val = 1;
  socklen_t len;

  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(int)) < 0) {
    perror("setsockopt");
    return 1;
  }

  len = sizeof(int);
  if (getsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &val, &len) < 0) {
    perror("setsockopt");
    return 1;
  }

  len = sizeof(int);
  if (getsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &val, &len) < 0) {
    perror("setsockopt");
    return 1;
  }

  len = sizeof(int);
  if (getsockopt(fd, SOL_TCP, TCP_KEEPCNT, &val, &len) < 0) {
    perror("setsockopt");
    return 1;
  }

  val = 60;
  if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &val, sizeof(int)) < 0) {
    perror("setsockopt");
    return 1;
  }

  val = 60;
  if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &val, sizeof(int)) < 0) {
    perror("setsockopt");
    return 1;
  }

  val = 9;
  if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &val, sizeof(int)) < 0) {
    perror("setsockopt");
    return 1;
  }

  return 0;
}

/* stop all timers except data timer */
void stop_all_timer(struct ev_loop *loop, conn_t *conn)
{
  if (conn->size_timeout) {
    ev_timer_stop(loop, conn->size_timeout);
  }
  if (conn->interval_timeout) {
    ev_timer_stop(loop, conn->interval_timeout);
  }
  if (conn->jpeg_timeout) {
    ev_timer_stop(loop, conn->jpeg_timeout);
  }
}

static void remove_connection(struct ev_loop *loop, int c)
{
  conn_t *conn;
  int fd = -1;

  fprintf(stderr, "remove_connection\n");

  if (c >= MAX_CONN) return;

  conn = &connections[c];
  if (conn == NULL) return;

  if (conn->read_watcher) {
    fd = conn->read_watcher->fd;
    ev_io_stop(loop, conn->read_watcher);
    free(conn->read_watcher);
    conn->read_watcher = NULL;
  }

  if (conn->write_watcher) {
    ev_io_stop(loop, conn->write_watcher);
    free(conn->write_watcher);
    conn->write_watcher = NULL;
  }

  if (conn->size_timeout) {
    ev_timer_stop(loop, conn->size_timeout);
    free(conn->size_timeout);
    conn->size_timeout = NULL;
  }

  if (conn->data_timeout) {
    ev_timer_stop(loop, conn->data_timeout);
    free(conn->data_timeout);
    conn->data_timeout = NULL;
  }

  if (conn->interval_timeout) {
    ev_timer_stop(loop, conn->interval_timeout);
    free(conn->interval_timeout);
    conn->interval_timeout = NULL;
  }

  if (conn->jpeg_timeout) {
    ev_timer_stop(loop, conn->jpeg_timeout);
    free(conn->jpeg_timeout);
    conn->jpeg_timeout = NULL;
  }

  if (conn->data) {
    free(conn->data);
    conn->data = NULL;
  }
  conn->data_size = 0;

  if (conn->tx_data) {
    free(conn->tx_data);
    conn->tx_data = NULL;
  }
  conn->tx_data_size = 0;

  if (conn->jpeg_data) {
    free(conn->jpeg_data);
    conn->jpeg_data = NULL;
  }
  conn->jpeg_data_size = 0;

  if (conn->chunks) {
    free(conn->chunks);
    conn->chunks = NULL;
  }
  conn->chunks_size = conn->n_chunks = 0;

  if (conn->err_chunks) {
    free(conn->err_chunks);
    conn->err_chunks = NULL;
  }
  conn->n_err_chunks = 0;

  close(fd);

  connections[c].status = ST_UNINITIALIZED;
}

/* create array of caminfo from string
 * and sort it in ascending order */
int parse_cam_num(caminfo_t *caminfo, char *camno)
{
  int i, j, i_camno = 0;
  caminfo_t temp;

  memset(caminfo, '\0', sizeof(caminfo_t) * MAX_CAM);

  token_t tok = tokinit(camno, ";");
  while (*toknext(&tok)) {
    int cam = atoi(tok.result);
    fprintf(stderr, "cam: %d\n", cam);
    if (i_camno < MAX_CAM) { 
      caminfo[i_camno++].camid = cam;
    }
  }
  tokend(&tok);

  /* sort the array */
  for (i = 0; i < i_camno; i++) {
    for (j = 0; j < (i_camno - 1); j++) {
      if (caminfo[j].camid > caminfo[j + 1].camid) {
	temp = caminfo[j];
	caminfo[j] = caminfo[j + 1];
	caminfo[j + 1] = temp;
      }
    }
  }

  return i_camno;
}

int get_req_type(char * req)
{
  char *tag;

  tag = parse_xml(req, "jpegreq", NULL);
  if (tag) {
    free(tag);
    return CMD_JPEG_REQ;
  }

  tag = parse_xml(req, "jpegwrite", NULL);
  if (tag) {
    free(tag);
    return CMD_JPEG_WRITE;
  }

  tag = parse_xml(req, "jpegcancel", NULL);
  if (tag) {
    free(tag);
    return CMD_JPEG_CANCEL;
  }

  return 0;
}

int parse_jpeg_req(conn_t *conn, char * req)
{
  char *cam_no, *resolution, *images, *interval, *quality;

  cam_no = parse_xml(req, "camera", NULL);
  if (cam_no) {
    caminfo_t caminfo[MAX_CAM];
    conn->cam_count = parse_cam_num(caminfo, cam_no);
    if (conn->cam_count) {
      memcpy(conn->caminfo, caminfo, sizeof(caminfo_t) * MAX_CAM);

      conn->quality = 0;
      quality = parse_xml(req, "quality", NULL);
      if (quality) {
	conn->quality = atoi(quality);
	free(quality);
      }
      conn->resolution = 0;
      resolution = parse_xml(req, "resolution", NULL);
      if (resolution) {
	conn->resolution = atoi(resolution);
	free(resolution);
      }
      conn->image_count = 1;
      images = parse_xml(req, "images", NULL);
      if (images) {
	conn->image_count = atoi(images);
	free(images);
      }
      conn->interval = 0;
      interval = parse_xml(req, "interval", NULL);
      if (interval) {
	conn->interval = atoi(interval);
	free(interval);
      }
    }
    free(cam_no);

    return 0;
  }

  return 1;
}

int verify_chunk_info(conn_t *conn, int offset, int size)
{
  if ((offset < 0) || (size <= 0)) 
    return 1;

  if (((size_t)offset < conn->jpeg_data_size) &&
      (size_t)(offset + size) <= conn->jpeg_data_size)
    return 0;

  return 1;	
}

int parse_jpeg_ack(char *msg, int *ack_code, int *cam_num, int *seq_num,
		   conn_t *conn)
{
  char *ackcode, *camnum, *seqnum, *chunklist;
  size_t chunks_size = 0;

  if (!ack_code || !cam_num || !seq_num || !conn)
    return 1;

  *ack_code = ACK_NONE;
  *cam_num = *seq_num = 0;
  if (conn->err_chunks) {
    free(conn->err_chunks);
    conn->err_chunks = NULL;
  }
  conn->n_err_chunks = 0;


  ackcode = parse_xml(msg, "ack_code", NULL);
  if (ackcode) {
    *ack_code = atoi(ackcode);
    if (*ack_code == 1) {
      camnum = parse_xml(msg, "camnum", NULL);
      if (camnum) {
	*cam_num = atoi(camnum);
	seqnum = parse_xml(msg, "seqnum", NULL);
	if (seqnum) {
	  *seq_num = atoi(seqnum);
	  chunklist = parse_xml(msg, "chunklist", NULL);
	  if (chunklist) {
	    char *next, *item, *start;
	    start = chunklist;
	    while ((item = parse_xml(start, "chunkitem", &next))) {
	      if (chunks_size <= conn->n_err_chunks) {
		chunks_size += 10;
		conn->err_chunks = 
		  (chunk_t *)realloc(conn->err_chunks,
				     sizeof(chunk_t) * chunks_size); 
	      }
	      if (conn->err_chunks) {
		int offset, size;
		if (sscanf(item, "%d;%d", &offset, &size) == 2) {
		  if (!verify_chunk_info(conn, offset, size)) {
		    fprintf(stderr, "missing:%d,%d\n", offset, size);
		    conn->err_chunks[conn->n_err_chunks].id = 0;
		    conn->err_chunks[conn->n_err_chunks].offset = offset;
		    conn->err_chunks[conn->n_err_chunks].size = size;
		    conn->n_err_chunks++;
		  }
		}
	      }
	      free(item);
	      start = next;
	    }
	    free(chunklist);
	  }
	  free(seqnum);
	}
	free(camnum);
      }
    }
    free(ackcode);
    return 0;
  }

  return 1;
}

/* mark currently acquired image sent */
int mark_image_sent(conn_t * conn)
{
  int i;

  for (i = 0; i < conn->cam_count; i++) {
    if (conn->caminfo[i].jpeg_acquired > conn->caminfo[i].jpeg_sent) {
      conn->caminfo[i].jpeg_sent = conn->caminfo[i].jpeg_acquired;
      return i;
    }
  }

  return -1;
}

/* check if all cameras are done for the current image sequence */
int is_all_cam_done(conn_t * conn)
{
  int i;

  for (i = 0; i < conn->cam_count; i++) {
    if (conn->caminfo[i].jpeg_acquired < (conn->cur_image + 1)) {
      return 0;
    }
  }

  return 1;
}

/* get camera idx for the current image sequence */
int get_current_cam(conn_t * conn)
{
  int i;

  for (i = 0; i < conn->cam_count; i++) {
    if (conn->caminfo[i].jpeg_acquired > conn->caminfo[i].jpeg_sent) {
      return i;
    }
  }

  return -1;
}

/* cur_image: starting from 0
 * returns caminfo index  
 */
int need_jpeg(conn_t * conn)
{
  int i;

  if (conn->cur_image < conn->image_count) {
    for (i = 0; i < conn->cam_count; i++) {
      /* current image not obtained for this camera? */
      if (conn->caminfo[i].jpeg_acquired < (conn->cur_image + 1)) {
	return i;
      }

      /* current image not sent yet? */
      if (conn->caminfo[i].jpeg_acquired > conn->caminfo[i].jpeg_sent) {
	return -1;
      }
    }
  }

  return -1;
}

int send_jpeg_ack(conn_t *conn, int cmd)
{
  struct mss_msg_set set;
  struct vter_msg_t msg;
  int ret = 1;

  create_vter_msg(&msg, DST_SS, TOD_DATA, MSG_NONE, sizeof(struct mss_msg));
  create_message(&set, cmd, ACK_SUCCESS, 0, 0, 0, 0, 
		 NULL, 0);
  unsigned char *txbuf = (unsigned char *)malloc(sizeof(msg) +
						 sizeof(struct mss_msg));
  if (txbuf) {
    memcpy(txbuf, (char *)&msg, sizeof(msg));
    memcpy(txbuf + sizeof(msg),
	   (char *)&set.msg, sizeof(struct mss_msg));
    int sent = sizeof(msg) + sizeof(struct mss_msg);
    if (!sendall(conn->read_watcher->fd, txbuf, &sent)) {
      fprintf(stderr, "sent jpeg_ack(%d) to VTER\n", cmd);
      ret = 0;
    }
    free(txbuf);
  }

  return ret;
}

int send_request_to_send(conn_t *conn)
{
  struct vter_msg_t msg;

  fprintf(stderr, "send request_to_send to VTER\n");

  create_vter_msg(&msg, DST_AVL, TOD_COMMAND, MSG_RS, 0);

  int sent = sizeof(msg);
  return sendall(conn->read_watcher->fd, (unsigned char *)&msg, &sent);
}

void send_EOT(struct ev_loop *loop, conn_t *conn)
{
  int sent;
  struct vter_msg_t msg;
  struct timeval tv;

  create_vter_msg(&msg, DST_AVL, TOD_INDICATE, MSG_EOT, 0);
  sent = sizeof(msg);
  sendall(conn->read_watcher->fd, (unsigned char *)&msg, &sent);
  gettimeofday(&tv, NULL);
  fprintf(stderr, "%ld.%06ld: sent EOT to VTER:%d(status:%d)\n",
	  tv.tv_sec, tv.tv_usec, sent, conn->status);
  ev_timer_stop(loop, conn->jpeg_timeout);
  conn->close_connection_on_timeout = 1;
  conn->size_timeout->repeat = SIZE_TIMEOUT;
  ev_timer_again(loop, conn->size_timeout);
  conn->interval_expired = 0;
  conn->status = ST_READY;
}

/* return: 0 - current image sequence not finished
 *         1 - current image sequence done
 */
int check_next_step(struct ev_loop *loop, conn_t *conn)
{
  int request_size = 0, ret = 0;

  if (is_all_cam_done(conn)) {
    /* one image sequence done */
    conn->cur_image++;
    ret = 1;
  } else {
    request_size = 1;
  }
	    
  if (conn->interval_expired) {
    conn->status = ST_SENDING_JPEG;
  }

  if ((conn->cur_image + 1) > conn->image_count) {
    /* close connection */
    send_EOT(loop, conn);
    return ret;
  }
    
  /* if interval already expired, go on to next image */
  if (request_size || conn->interval_expired) {
    conn->status = ST_SENDING_JPEG;
    send_request_to_send(conn);
    ev_timer_again(loop, conn->size_timeout);
  }

  return ret;
}

void go_on_to_next_camera(struct ev_loop *loop, conn_t *conn)
{
  free(conn->jpeg_data);
  conn->jpeg_data = NULL;
  conn->jpeg_data_size = 0;
  conn->status = ST_DONE_JPEG;
  
  /* go on to the next camera */
  mark_image_sent(conn);
  check_next_step(loop, conn);
}

void set_interval_timer(struct ev_loop *loop, conn_t *conn)
{
  /* set timer in case of multiple shots */
  conn->interval_expired = 0;
  if (conn->image_count > 1 && conn->interval > 0) {
    if (conn->interval_timeout) {
      fprintf(stderr, "setting interval timer\n");
      conn->interval_timeout->repeat = conn->interval / 1000.0;
      ev_timer_again(loop, conn->interval_timeout);
      conn->interval_timeout->repeat = 0.0;
    }
  } else {
    conn->interval_expired = 1;
  }
}

void realloc_chunk_buf(conn_t *conn, size_t current_chunk)
{
  if (conn->chunks_size <= current_chunk) {
    conn->chunks_size += 10;
    conn->chunks = (chunk_t *)
      realloc(conn->chunks, 
	      sizeof(chunk_t) * conn->chunks_size); 
  }
}

int serialno;
void send_jpeg_data(struct ev_loop *loop, conn_t *conn)
{
  int cam_idx, sent;
  struct vter_msg_t msg;

  /* jpeg to request from DVR? */
  while ((cam_idx = need_jpeg(conn)) >= 0) {
    conn->jpeg_data_size = conn->jpeg_data_sent = 0;
    conn->n_chunks = 0;
    conn->caminfo[cam_idx].cur_chunk = 0;
    if (get_jpeg(conn->caminfo[cam_idx].camid - 1,
		 conn->quality, conn->resolution,
		 &conn->jpeg_data, &conn->jpeg_data_size)) {
      fprintf(stderr, "get_jpeg error:%d\n",
	      conn->caminfo[cam_idx].camid);
      conn->status = ST_DONE_JPEG;
      /* skip this image for this camera */
      conn->caminfo[cam_idx].jpeg_acquired++;
      conn->caminfo[cam_idx].jpeg_sent++;
      if (check_next_step(loop, conn)) {
	if (!conn->interval_expired) {
	  break;
	}
      }
    } else {
      conn->caminfo[cam_idx].jpeg_acquired++; /* 1, 2, 3... */
      /* set timer only on 1st cam */
      if (cam_idx == 0) { 
	set_interval_timer(loop, conn);
      }
#if 0
      FILE *fp;
      char filename[256];
      sprintf(filename, "/var/dvr/disks/d_sda1/test%06d_%02d.jpg", ++serialno, conn->caminfo[cam_idx].camid);
      fp = fopen(filename, "w");
      if (fp) {
	fwrite(conn->jpeg_data, 1, conn->jpeg_data_size, fp);
	fclose(fp);
	fprintf(stderr, "%s created\n\n", filename);
      }
#endif
      break;
    }
  } /* while */
  
  fprintf(stderr, "jpeg_data_size:%d,sent:%d(%d)\n",
	  conn->jpeg_data_size, conn->jpeg_data_sent, conn->interval_expired);
  /* any data to send? */
  int jpeg_to_send = conn->jpeg_data_size - conn->jpeg_data_sent;
  cam_idx = get_current_cam(conn);
  
  if (cam_idx >= 0 && jpeg_to_send > 0) {
    int to_send;
    /* mss_msg header should be added */
    to_send = jpeg_to_send + (int)sizeof(struct mss_msg);
    if (to_send > conn->chunk_size) {
      jpeg_to_send -= (to_send - conn->chunk_size);
      to_send = conn->chunk_size;
    }
    //fprintf(stderr, "to_send:%d\n", to_send);
    create_vter_msg(&msg, DST_SS, TOD_DATA, MSG_NONE, to_send);
    
    struct mss_msg_set set;
    uint16_t wData = conn->caminfo[cam_idx].camid << 8 |
      ((conn->cur_image + 1) & 0xff);
    uint32_t dwData = conn->jpeg_data_size;
    uint64_t qwData = (uint64_t)conn->jpeg_data_sent << 32 |
      (conn->caminfo[cam_idx].cur_chunk & 0xff);
    create_message(&set, CMD_JPEG_WRITE, 0, 0, wData, dwData, qwData, 
		   (char *)conn->jpeg_data + conn->jpeg_data_sent,
		   jpeg_to_send);
    unsigned char *txbuf = 
      (unsigned char *)malloc(sizeof(msg) +
			      sizeof(struct mss_msg) + jpeg_to_send);
    if (txbuf) {
      memcpy(txbuf, (char *)&msg, sizeof(msg));
      memcpy(txbuf + sizeof(msg),
	     (char *)&set.msg, sizeof(struct mss_msg));
      memcpy(txbuf + sizeof(msg) + sizeof(struct mss_msg),
	     (char *)conn->jpeg_data + conn->jpeg_data_sent,
	     jpeg_to_send);
      sent = to_send + sizeof(msg);
      if (!sendall(conn->read_watcher->fd, txbuf, &sent)) {
	/* save chunk info for error recovery */
	realloc_chunk_buf(conn, (size_t)conn->caminfo[cam_idx].cur_chunk);
	
	if (conn->chunks && 
	    conn->chunks_size >
	    (size_t)conn->caminfo[cam_idx].cur_chunk) {
	  conn->chunks[conn->caminfo[cam_idx].cur_chunk].offset =
	    conn->jpeg_data_sent;
	  conn->chunks[conn->caminfo[cam_idx].cur_chunk].size =
	    jpeg_to_send;
	  conn->n_chunks = conn->caminfo[cam_idx].cur_chunk + 1;
	}
	
	conn->jpeg_data_sent += jpeg_to_send;
	conn->caminfo[cam_idx].cur_chunk++;
	fprintf(stderr, "sent to VTER:%d(%d/%d)\n",
		sent, conn->jpeg_data_sent, conn->jpeg_data_size);
	
	/* set the timer */
	//fprintf(stderr, "starting size timer3\n");
	ev_timer_again(loop, conn->size_timeout);
	
	if (conn->jpeg_data_sent >= conn->jpeg_data_size) {
	  conn->status = ST_JPEG_WRITE_ACK;
	  conn->jpeg_timeout->repeat = 20.0;
	  ev_timer_again(loop, conn->jpeg_timeout);
	  struct timeval tv;
	  gettimeofday(&tv, NULL);
	  fprintf(stderr, "%ld.%06ld: waiting for jpeg_write_ack\n",
		  tv.tv_sec, tv.tv_usec);
	}
      } 
      free(txbuf);
    }
    //conn->tx_data_size = to_send;
    //tcp_write_cb(loop, conn->write_watcher, EV_WRITE);
  }
}

void send_missing_chunks(struct ev_loop *loop, conn_t *conn)
{
  int i, cam_idx, sent, err_chunk_idx = -1;
  struct vter_msg_t msg;

  /* find chunks to send, skip already sent chunks */
  for (i = 0; i < (int)conn->n_err_chunks; i++) {
    if (conn->err_chunks[i].size > 0) {
      err_chunk_idx = i;
      break;
    }
  }

  if (err_chunk_idx == -1)
    return;

  cam_idx = get_current_cam(conn);
  int jpeg_to_send = conn->err_chunks[err_chunk_idx].size;
  int jpeg_offset = conn->err_chunks[err_chunk_idx].offset;
  int new_chunk_id = conn->n_chunks;
  if (jpeg_to_send > 0 && jpeg_offset >= 0) {
    int to_send;
    /* mss_msg header should be added */
    to_send = jpeg_to_send + (int)sizeof(struct mss_msg);
    if (to_send > conn->chunk_size) {
      jpeg_to_send -= (to_send - conn->chunk_size);
      to_send = conn->chunk_size;
    }
    fprintf(stderr, "to_send:%d\n", to_send);
    create_vter_msg(&msg, DST_SS, TOD_DATA, MSG_NONE, to_send);
    
    struct mss_msg_set set;
    uint16_t wData = conn->caminfo[cam_idx].camid << 8 |
      ((conn->cur_image + 1) & 0xff);
    uint32_t dwData = conn->jpeg_data_size;
    uint64_t qwData = (uint64_t)jpeg_offset << 32 | (new_chunk_id & 0xff);
    create_message(&set, CMD_JPEG_WRITE, 0, 0, wData, dwData, qwData, 
		   (char *)conn->jpeg_data + jpeg_offset, jpeg_to_send);
    unsigned char *txbuf = 
      (unsigned char *)malloc(sizeof(msg) +
			      sizeof(struct mss_msg) + jpeg_to_send);
    if (txbuf) {
      memcpy(txbuf, (char *)&msg, sizeof(msg));
      memcpy(txbuf + sizeof(msg),
	     (char *)&set.msg, sizeof(struct mss_msg));
      memcpy(txbuf + sizeof(msg) + sizeof(struct mss_msg),
	     (char *)conn->jpeg_data + jpeg_offset, jpeg_to_send);
      sent = to_send + sizeof(msg);
      if (!sendall(conn->read_watcher->fd, txbuf, &sent)) {
	/* save chunk info for error recovery */
	realloc_chunk_buf(conn, new_chunk_id);
	
	if (conn->chunks && 
	    (int)conn->chunks_size > new_chunk_id) {
	  conn->chunks[new_chunk_id].offset = jpeg_offset;
	  conn->chunks[new_chunk_id].size = jpeg_to_send;
	  conn->n_chunks++;
	}
	
	/* mark already sent chunk portion */
	conn->err_chunks[err_chunk_idx].size -= jpeg_to_send;
	conn->err_chunks[err_chunk_idx].offset += jpeg_to_send;

	fprintf(stderr, "sent chunk %d(offset:%d, size:%d) to VTER:%d\n",
		new_chunk_id, jpeg_offset, jpeg_to_send, sent);
	
	/* set the timer */
	//fprintf(stderr, "starting size timer3\n");
	ev_timer_again(loop, conn->size_timeout);
	
	int sent_all_err_chunks = 1;
	for (i = 0; i < (int)conn->n_err_chunks; i++) {
	  if (conn->err_chunks[i].size > 0) {
	    sent_all_err_chunks = 0;
	    break;
	  }
	}

	if (sent_all_err_chunks) {
	  conn->status = ST_JPEG_WRITE_ACK;
	  free(conn->err_chunks);
	  conn->err_chunks = NULL;
	  conn->n_err_chunks = 0;
	  fprintf(stderr, "waiting for jpeg_write_ack\n");
	}
      } 
      free(txbuf);
    }
  }
}

void handle_message(struct ev_loop *loop, conn_t *conn) 
{
  int cam_idx, remain, payload_len = 0;

  if (vter_msg.tod == TOD_DATA) {
    int msg_type;
    payload_len = GetDWBE(vter_msg.len);
    char *req = (char *)malloc(payload_len + 1);
    if (req) {
      memcpy(req, conn->data, payload_len);
      req[payload_len] = 0;

      fprintf(stderr, "SS req(%d):%s\n", conn->status, req);
      msg_type = get_req_type(req);
      if (msg_type == CMD_JPEG_REQ) {
	/* handle this request even if status != ST_READY */
	if (!parse_jpeg_req(conn, req)) {
	  conn->chunk_size = 0;
	  conn->cur_image = 0;
	  conn->jpeg_data_size = conn->jpeg_data_sent = 0;
	  conn->interval_expired = 0;
	  conn->size_timeout_count = 0;
	  conn->close_connection_on_timeout = 0;
	  conn->status = ST_JPEG_REQ;
	  stop_all_timer(loop, conn);
	  if (conn->size_timeout) {
	    //fprintf(stderr, "starting size timer\n");
	    conn->size_timeout->repeat = SIZE_TIMEOUT;
	    ev_timer_again(loop, conn->size_timeout);
	  }
	}
      } else if (msg_type == CMD_JPEG_WRITE) {
	if (conn->status == ST_JPEG_WRITE_ACK) {
	  int ack_code, cam_num, seq_num;
	  ev_timer_stop(loop, conn->jpeg_timeout);
	  if (!parse_jpeg_ack(req, &ack_code, &cam_num, &seq_num, conn)) {
	    if (ack_code == ACK_SUCCESS) {
	      go_on_to_next_camera(loop, conn);
	    } else if (ack_code == ACK_FAIL) {
	      if (conn->status != ST_READY) { /* SS send this on connection */
		cam_idx = get_current_cam(conn);
		if (conn->err_chunks) {
		  if (conn->n_err_chunks > 0) {
		    if ((cam_num == conn->caminfo[cam_idx].camid) &&
			(seq_num == conn->cur_image + 1)) {
		      conn->status = ST_JPEG_RESEND;
		      send_request_to_send(conn);
		      ev_timer_again(loop, conn->size_timeout);
		    }
		  }
		}
		/* in case something is wrong in the message,
		 * ignore this message and send next one.
		 */
		if (conn->status != ST_JPEG_RESEND) {
		  go_on_to_next_camera(loop, conn);
		}
	      }
	    }
	  }
	}
      } else if (msg_type == CMD_JPEG_CANCEL) {
	send_jpeg_ack(conn, CMD_JPEG_CANCEL);
	if (conn->status != ST_READY) /* SS send this on connection */
	  send_EOT(loop, conn);
      }
      free(req);
    }
  } else if (vter_msg.tod == TOD_INDICATE) {
    if (vter_msg.msg == MSG_RS) {
      if (conn->size_timeout) {
	//fprintf(stderr, "stopping size timer\n");
	ev_timer_stop(loop, conn->size_timeout);
	conn->size_timeout_count = 0;
      }

      payload_len = GetDWBE(vter_msg.len);
      if (payload_len == 2)
	conn->chunk_size = conn->data[0] << 8 | conn->data[1];
      else 
	conn->chunk_size = GetDWBE(conn->data);
      fprintf(stderr, "chunk size:%d(%d)\n", conn->chunk_size, conn->status);

      if (conn->chunk_size <= (int)sizeof(struct mss_msg)) {
	//fprintf(stderr, "starting size timer2\n");
	ev_timer_again(loop, conn->size_timeout);
	return;
      }

      if (conn->status == ST_JPEG_REQ) {
	fprintf(stderr, "sending jpeg_req_ack\n");
	if (!send_jpeg_ack(conn, CMD_JPEG_REQ)) {
	  conn->status = ST_SENDING_JPEG;
	  //fprintf(stderr, "starting size timer after jpeg_ack\n");
	  ev_timer_again(loop, conn->size_timeout);
	}
      } else if (conn->status == ST_SENDING_JPEG) {
	send_jpeg_data(loop, conn);
      } else if (conn->status == ST_JPEG_RESEND) {
	send_missing_chunks(loop, conn);
      }
    }
  }

  /* now clear used data from receive buffer */
  remain = conn->data_size - payload_len;
  conn->data_size = 0;
  if (remain > 0) {
    memmove(conn->data, conn->data + payload_len, remain);
    conn->data_size = remain;
  }
}

static void tcp_read_cb(struct ev_loop *loop, ev_io *w, int revents)
{
  unsigned char buf[2048];
  int bytes, to_read;
  size_t payload_len;
  conn_t *conn = &connections[(int)(w->data)];

  //fprintf(stderr, "tcp_read_cb\n");
  if (conn == NULL) return;

  to_read = sizeof(buf);
  if (conn->pending_data) {
    to_read = conn->pending_data;
  }
  bytes = read(w->fd, buf, sizeof(buf));
  //fprintf(stderr, "read %d\n", bytes);
  if (bytes == 0) {
    fprintf(stderr, "tcp_read_cb: closing\n");
    remove_connection(loop, (int)(w->data));
    return;
  } else if (bytes > 0) {
    /* add received data to the main buffer */
    if (conn->data_size + bytes > VTER_BUF_SIZE) {
      /* we are receiving garbages or not handling fast enough */
      fprintf(stderr, "overflow:giving up %d b\n", conn->data_size);
      conn->data_size = 0;
    }
    
    memcpy(conn->data + conn->data_size, buf, bytes);
    conn->data_size += bytes;

    if (conn->pending_data) {
      payload_len = GetDWBE(vter_msg.len);
      if (conn->data_size >= payload_len) {
	//fprintf(stderr, "got complet data\n");
	conn->pending_data = 0;
	if (conn->data_timeout) {
	  //fprintf(stderr, "stopping data timer\n");
	  ev_timer_stop(loop, conn->data_timeout);
	}
	handle_message(loop, conn);
      }
    }

    while (!conn->pending_data && getOnePacketFromBuffer(conn, &vter_msg)) {
      payload_len = GetDWBE(vter_msg.len);
      fprintf(stderr, "got one message:%d,%d(%d),%d\n",
	      vter_msg.tod, vter_msg.msg, payload_len, conn->data_size);
      if (payload_len > 0) {
	conn->pending_data = payload_len - conn->data_size;
	if (conn->pending_data <= 0) {
	  //fprintf(stderr, "got complet data2\n");
	  conn->pending_data = 0;
	  handle_message(loop, conn);
	}
      }  
    }
  }

  if (conn->pending_data) {
    if (conn->data_timeout) {
      //fprintf(stderr, "setting data timer\n");
      conn->data_timeout->repeat = 2.0;
      ev_timer_again(loop, conn->data_timeout);
      conn->data_timeout->repeat = 0.0;
    }
  }

  //ev_io_start(loop, w);
}

static void tcp_write_cb(struct ev_loop *loop, ev_io *w, int revents)
{
  conn_t *conn = &connections[(int)(w->data)];
  if (conn == NULL) return;

  fprintf(stderr, "tcp_write_cb\n");
  if (conn->tx_data_size > 0) {
    int written = send(w->fd, conn->tx_data, conn->tx_data_size, 0);
    if (written == -1) {
      if (errno != EAGAIN) {
	fprintf(stderr, "send error(%s), closing connection\n",
		strerror(errno));
	remove_connection(loop, (int)(w->data));
	return;
      }
    } else {
      int remain = conn->tx_data_size - written;
      if (remain > 0) {
	memmove(conn->tx_data, conn->tx_data + written, remain);
	conn->tx_data_size = remain;
	ev_io_start(loop, conn->write_watcher);
	return;
      }
      conn->tx_data_size = 0;
      ev_io_stop(loop, conn->write_watcher);
    }
  }
}

static void interval_timeout_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
  conn_t *conn = &connections[(int)(w->data)];
  if (conn == NULL) return;

  fprintf(stderr, "interval timeout_cb:%d\n", conn->status);

  conn->interval_expired = 1;
  if (conn->status == ST_DONE_JPEG) {
    conn->status = ST_SENDING_JPEG;
    send_request_to_send(conn);
    ev_timer_again(loop, conn->size_timeout);
  }
}

static void jpeg_timeout_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
  conn_t *conn = &connections[(int)(w->data)];
  if (conn == NULL) return;

  fprintf(stderr, "jpeg timeout_cb:%d\n", conn->status);

  if (conn->status == ST_JPEG_WRITE_ACK) {
    go_on_to_next_camera(loop, conn);
  }
}

static void data_timeout_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
  conn_t *conn = &connections[(int)(w->data)];
  if (conn == NULL) return;

  fprintf(stderr, "data timeout_cb\n");

  conn->pending_data = 0;
}

static void size_timeout_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
  conn_t *conn = &connections[(int)(w->data)];
  if (conn == NULL) return;

  fprintf(stderr, "size timeout_cb(%d,%d)\n",
	  conn->size_timeout_count,conn->close_connection_on_timeout);
  conn->size_timeout_count++;
  if (conn->size_timeout_count > 30 ||
      conn->close_connection_on_timeout) {
    remove_connection(loop, (int)(w->data));
    return;
  }
  send_request_to_send(conn);
  ev_timer_again(loop, conn->size_timeout);

  /* try send immediately, if not, the handler will start write_watcher */
  //memcpy(conn->tx_data, (char *)&msg, sizeof(msg));
  //conn->tx_data_size = sizeof(msg);
  //tcp_write_cb(loop, conn->write_watcher, EV_WRITE);
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents)
{
  struct sockaddr_in remoteaddr;
  socklen_t addrlen;
  int i, newfd;

  //fprintf(stderr, "accept_cb\n");
  addrlen = sizeof(remoteaddr);
  if ((newfd = accept(w->fd, (struct sockaddr *)&remoteaddr,
		      &addrlen)) == -1) {
    perror("accept");
    return;
  }

  fprintf(stderr,"new connection:%s on %d\n",
	  inet_ntoa(remoteaddr.sin_addr), newfd);

  for (i = 0; i < MAX_CONN; i++) {
    if (connections[i].status == ST_UNINITIALIZED)
      break;
  }

  if (i == MAX_CONN) {
    fprintf(stderr,"Maximum number of connection reached\n");
    close(newfd);
    return;
  }

  if (change_to_nonblock(newfd)) {
    close(newfd);
    return;
  }

  memset(&connections[i], '\0', sizeof(conn_t));
  ev_io *watcher = (ev_io *)malloc(sizeof(ev_io));
  if (watcher) {
    watcher->data = (void *)i;
    ev_io_init(watcher, tcp_read_cb, newfd, EV_READ);
    connections[i].read_watcher = watcher;
    ev_timer *timer = (ev_timer *)malloc(sizeof(ev_timer));
    if (timer) {
      timer->data = (void *)i;
      ev_timer_init(timer, data_timeout_cb, 10.0, 0.0);
      connections[i].data_timeout = timer;
    }
    timer = (ev_timer *)malloc(sizeof(ev_timer));
    if (timer) {
      timer->data = (void *)i;
      ev_timer_init(timer, size_timeout_cb, 10.0, 0.0);
      connections[i].size_timeout = timer;
    }
    timer = (ev_timer *)malloc(sizeof(ev_timer));
    if (timer) {
      timer->data = (void *)i;
      ev_timer_init(timer, interval_timeout_cb, 10.0, 0.0);
      connections[i].interval_timeout = timer;
    }
    timer = (ev_timer *)malloc(sizeof(ev_timer));
    if (timer) {
      timer->data = (void *)i;
      ev_timer_init(timer, jpeg_timeout_cb, 10.0, 0.0);
      connections[i].jpeg_timeout = timer;
    }
    ev_io *writer = (ev_io *)malloc(sizeof(ev_io));
    if (writer) {
      writer->data = (void *)i;
      ev_io_init(writer, tcp_write_cb, newfd, EV_WRITE);
      connections[i].write_watcher = writer;
    }
    connections[i].status = ST_READY;
    connections[i].tx_data = (unsigned char *)malloc(VTER_BUF_SIZE);
    if (connections[i].tx_data == NULL)
      return;
    connections[i].data = (unsigned char *)malloc(VTER_BUF_SIZE);
    if (connections[i].data) {
      ev_io_start(loop, watcher);
    }
  }
}

static void sigusr1_cb (struct ev_loop *loop, ev_signal *w, int revents)
{
  fprintf(stderr, "Got SIGUSR1 or SIGINT - Stopping.\n");
  b_end = 1;
  ev_break(loop, EVBREAK_ALL);
}

static void sigusr2_cb (struct ev_loop *loop, ev_signal *w, int revents)
{
  fprintf(stderr, "Got SIGUSR2 - Restarting.\n");
  b_restart = 1;
  ev_break(loop, EVBREAK_ALL);
}

void closeall(int fd)
{
  int fdlimit = sysconf(_SC_OPEN_MAX);

  while (fd < fdlimit)
    close(fd++);
}

int daemon(int nochdir, int noclose)
{
  switch (fork()) {
  case 0: break;
  case -1: return -1;
  default: _exit(0);
  }

  if (setsid() < 0)
    return -1;

  switch (fork()) {
  case 0: break;
  case -1: return -1;
  default: _exit(0);
  }

  if (!nochdir)
    chdir("/");

  if (!noclose) {
    closeall(0);
    open("/dev/null", O_RDWR);
    dup(0); dup(0);
  }

  return 0;
}

int main(int argc, char *argv[])
{
  int listener;
  int optchar, force_foreground = 0;
  int jpeg_test = 0;

  signal(SIGPIPE, SIG_IGN);

  while ((optchar = getopt(argc, argv, "p:t:fhj?")) != -1) {
    switch (optchar) {
#if 0
    case 'i':
      strncpy(vter_dev, optarg, sizeof(vter_dev));
      break;
#endif
    case 'p':
      pidfile = strdup(optarg);
      break;
    case 't':
      dvrsvr_port = atoi(optarg);
      break;
    case 'f':
      force_foreground = 1;
      break;
    case 'j':
      force_foreground = 1;
      jpeg_test = 1;
      break;
    case 'h':
    case '?':
    default:
      fprintf(stderr, "Usage: %s [-p pidfile] [-t tcp_port] [-f] [-j]\n"
	      "\t pidfile: pid file name with full path\n"
	      "\t tcp_port: tcp port number of the dvrsvr [%d]\n"
	      "\t f: force foreground (useful for debugging)\n"
	      "\t j: do jpeg test (useful for testing DVR).\n",
	      argv[0], dvrsvr_port);
      exit(0);
    }
  }

  if (!force_foreground) {
    if (daemon(0, 0) < 0) {
      perror("daemon");
      exit(1);
    }
  }

  if (jpeg_test) {
    int ch = 0;
    size_t jpeg_data_size;
    unsigned char *jpeg_data;
    while (!get_jpeg(ch, 1, 2, &jpeg_data, &jpeg_data_size)) {
      FILE *fp;
      char filename[256];
      sprintf(filename, "test%d.jpg", ch + 1);
      fp = fopen(filename, "w");
      if (fp) {
	fwrite(jpeg_data, 1, jpeg_data_size, fp);
	fclose(fp);
	fprintf(stderr, "%s created\n\n", filename);
      }
      free(jpeg_data);
      ch++;
   }
    return 0;
  }

  if (pidfile) {
    FILE *pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }
  }

  while ((listener = makeServerSocket (SOCK_STREAM, VTER_PORT)) < 0) {
    sleep(1);
  }

  struct ev_loop *loop = EV_DEFAULT;
  ev_io sock_watcher;
  ev_init(&sock_watcher, accept_cb);
  ev_io_set(&sock_watcher, listener, EV_READ);
  ev_io_start(loop, &sock_watcher);

  ev_signal sigint_watcher, sigusr1_watcher, sigusr2_watcher;
  ev_signal_init(&sigint_watcher, sigusr1_cb, SIGINT);
  ev_signal_start(loop, &sigint_watcher);
  ev_signal_init(&sigusr1_watcher, sigusr1_cb, SIGUSR1);
  ev_signal_start(loop, &sigusr1_watcher);
  ev_signal_init(&sigusr2_watcher, sigusr2_cb, SIGUSR2);
  ev_signal_start(loop, &sigusr2_watcher);

  ev_run(loop, 0);

  ev_signal_stop(loop, &sigusr2_watcher);
  ev_signal_stop(loop, &sigusr1_watcher);
  ev_signal_stop(loop, &sigint_watcher);
  ev_io_stop(loop, &sock_watcher);

  if (pidfile) {
    unlink(pidfile);
  }

  return 0;
}
