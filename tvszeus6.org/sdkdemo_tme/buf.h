#ifndef __HIK_BUF_H__
#define __HIK_BUF_H__

#define BUF_SIZE 1024*1024

struct buf_obj {
	int r,w;
	char* buf;
	pthread_mutex_t lock;
	pthread_cond_t  cond;
};

#endif
