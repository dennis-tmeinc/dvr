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
#include <fcntl.h>
#include <ev.h>
#include "crc.h"
#include "debug.h"

enum reqcode_type { REQ2GETJPEG = 232 };
enum anscode_type { ANSERROR = 1, ANSOK, ANS2JPEG = 217 };
enum { VTER_PORT = 55001 };
short dvrsvr_port = 15111;
char vter_dev[32] = "eth0:1";
char *pidfile;

int b_end, b_restart;
ev_timer timeout_watcher;

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

int create_vter_msg(int dest, unsigned char *sendbuf, int payload_len)
{
  int bi = 0;
  uint32_t crc;

  if (sendbuf == NULL) return 0;

  sendbuf[bi++] = 'V';
  sendbuf[bi++] = 'T';
  sendbuf[bi++] = 'E';
  sendbuf[bi++] = 'R';
  sendbuf[bi++] = dest; /* dest ID */
  sendbuf[bi++] = 0; /* command */
  SetDWBE((uint8_t *)&sendbuf[bi], payload_len);
  bi += 4;
  crc = crc32(0, sendbuf, bi);
  SetDWBE((uint8_t *)&sendbuf[bi], crc);
  bi += 4;

  return bi;
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
  int msg_len = create_vter_msg(1, sendbuf, 0);

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

int get_jpeg()
{
  int fd, bytes, size;
  struct dvr_req req;
  struct dvr_ans ans ;
  int ch, jpeg_quality, jpeg_pic ;

  fd = connectNonBlockingTcp("192.168.1.220", dvrsvr_port);
  if (fd == -1)
    return 1;

  fprintf(stderr, "connected\n");

  ch = 0;
  jpeg_quality = 1; /* 0-best, 1-better, 2-average */
  jpeg_pic = 2; /* 0-cif , 1-qcif, 2-4cif */

  req.reqcode=REQ2GETJPEG ;
  req.data=(ch&0xff)|( (jpeg_quality&0xff)<<8)|((jpeg_pic&0xff)<<16) ;
  req.reqsize=0 ;
  size = sizeof(req);
  if (!sendall(fd, (unsigned char *)&req, &size)) {
    struct timeval tv;

    tv.tv_sec = 10;
    tv.tv_usec = 0;
    bytes = read_nbytes(fd, (unsigned char *)&ans, sizeof(ans),
			sizeof(ans), &tv, 1, 0);
    fprintf(stderr, "get_jpeg reply:%d bytes recved\n", bytes);
    if (bytes == sizeof(ans)) {
      if (ans.anscode == ANS2JPEG && ans.anssize > 0) {
	unsigned char *rxbuf = (unsigned char *)malloc(ans.anssize);
	if (rxbuf) {
	  tv.tv_sec = 10;
	  tv.tv_usec = 0;
	  bytes = read_nbytes(fd, rxbuf, ans.anssize,
			      ans.anssize, &tv, 0, 0);
	  fprintf(stderr, "get_jpeg image:%d bytes recved\n", bytes);
	  if (bytes > 0) {
	    FILE *fp;
	    fp = fopen("test.jpg", "w");
	    if (fp) {
	      fwrite(rxbuf, 1, bytes, fp);
	      fclose(fp);
	    }
	  }
	  free(rxbuf);
	}
      }
    }
  }

  close(fd);
  return 0;
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

static void sock_cb(struct ev_loop *loop, ev_io *w, int revents)
{
  unsigned char buf[2048];
  int bytes;

  fprintf(stderr, "sock_cb\n");
  ev_io_stop(loop, w);
  bytes = read(w->fd, buf, sizeof(buf));
  fprintf(stderr, "read %d\n", bytes);
  if (bytes == 0) {
    fprintf(stderr, "sock_cb: closing\n");
    close(w->fd);
    *(int *)(w->data) = -1;
    ev_break(loop, EVBREAK_ALL);
    return;
  } else if (bytes > 0) {
    if (is_valid_message(buf, bytes)) {
      dump(buf, 14);
      fprintf(stderr, "recved VTER message - payload:%d\n", GetDWBE(&buf[6]));
      dump(buf, GetDWBE(&buf[6]));
    } else {
      dump(buf, bytes);
    }
  }

  if (0) {
    fprintf(stderr, "setting timer\n");
    timeout_watcher.repeat = 1.0;
    ev_timer_again(loop, &timeout_watcher);
    timeout_watcher.repeat = 0.0;
  }

  ev_io_start(loop, w);
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

int xxx;
static void timeout_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
  fprintf(stderr, "timeout_cb\n");
#if 0
  if (!xxx) {
    xxx = 1;
    timeout_watcher.repeat = 1.0;
    ev_timer_again(loop, &timeout_watcher);
    timeout_watcher.repeat = 0.0;
  }
#endif
  int fd = *(int *)(w->data);
  unsigned char buf[2048];
  int len;
  len = create_vter_msg(2, buf, 1024);
  unsigned char *ptr = buf + 14;
  for (size_t i = 0; i < 1024; i++) {
    ptr[i] = 'A' + i % 52;
  }
  len += 1024;

  sendall(fd, buf, &len); 
  fprintf(stderr, "sent:%d\n", len);
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
  struct in_addr ip, netmask, broadcast, server_ip;
  int fd = -1;
  int optchar, force_foreground = 0;

  signal(SIGPIPE, SIG_IGN);

  while ((optchar = getopt(argc, argv, "i:p:t:fh?")) != -1) {
    switch (optchar) {
    case 'i':
      strncpy(vter_dev, optarg, sizeof(vter_dev));
      break;
    case 'p':
      pidfile = strdup(optarg);
      break;
    case 't':
      dvrsvr_port = atoi(optarg);
      break;
    case 'f':
      force_foreground = 1;
      break;
    case 'h':
    case '?':
    default:
      fprintf(stderr, "Usage: %s [-i interface] [-p pidfile] [-t tcp_port] [-f]\n"
	      "\t interface: network interface vter is connected to [%s]\n"
	      "\t pidfile: pid file name with full path\n"
	      "\t tcp_port: tcp port number of the dvrsvr [%d]\n"
	      "\t f: force foreground (useful for debugging).\n",
	      argv[0], vter_dev, dvrsvr_port);
      exit(0);
    }
  }

  if (!force_foreground) {
    if (daemon(0, 0) < 0) {
      perror("daemon");
      exit(1);
    }
  }

  if (pidfile) {
    FILE *pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid() );
        fclose(pidf);
    }
  }

  /* repeated on config change */ 
  while (!b_end) {
    if (get_dev_ip(vter_dev, &ip, &netmask, &broadcast)) {
      fprintf(stderr, "can't get ip/netmask\n");
      sleep(1);
      continue;
    }
    
#if 0
    while (discover_vter(broadcast, &server_ip)) {
      fprintf(stderr, "can't find vter\n");
      sleep(10);
    }
#else
    inet_aton("192.168.42.130", &server_ip);
#endif

    /* repeated on connection lost */ 
    if (fd != -1) {
      close(fd);
      fd = -1;
    }
    b_restart = 0;
    while (!b_end && !b_restart && (fd == -1)) {
      /* reset the restart flag to stay in the loop */
      fprintf(stderr, "connecting to vter\n");
      fd = connectNonBlockingTcp(inet_ntoa(server_ip), VTER_PORT);
      if (fd == -1) {
	sleep(1);
	continue;
      }
      enable_tcp_keepalive(fd);
      fprintf(stderr, "connect OK:%d\n", fd);

      struct ev_loop *loop = EV_DEFAULT;
      ev_io sock_watcher;
      ev_init(&sock_watcher, sock_cb);
      ev_io_set(&sock_watcher, fd, EV_READ);
      sock_watcher.data = &fd;
      ev_io_start(loop, &sock_watcher);

      ev_init(&timeout_watcher, timeout_cb);

  if (1) {
    timeout_watcher.data = &fd;
    fprintf(stderr, "setting timer\n");
    timeout_watcher.repeat = 1.0;
    ev_timer_again(loop, &timeout_watcher);
    timeout_watcher.repeat = 0.0;
  }
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
      //get_jpeg();
    }
  }

  if (pidfile) {
    unlink(pidfile);
  }

  return 0;
}
