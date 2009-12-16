#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "buf.h"

void init_buf_obj(struct buf_obj* buf)
{
	buf->r = buf->w = 0;
	buf->buf = malloc(BUF_SIZE);
	
	pthread_mutex_init(&buf->lock,NULL);
}

void destroy_buf_obj(struct buf_obj* buf)
{
	free(buf->buf);
	pthread_mutex_destroy(&buf->lock);
}

void write_data_to_buf(void* data, int size,struct buf_obj* buf)
{
	int len1 = 0, len2 = 0;

	pthread_mutex_lock(&buf->lock);

	if ( buf->w < buf->r ) {
		len1 = buf->r - buf->w;
		len2 = 0;
	}
	else {
		len1 = BUF_SIZE - buf->w;
		len2 = buf->r;
	}

	if ( size <= len1 ) {
		memcpy(buf->buf + buf->w,data,size);
	}
	else {
		memcpy(buf->buf + buf->w,data,len1);
		int copylen = size -len1;
		memcpy(buf->buf,data+len1,copylen);
	}

	buf->w = buf->w+len1;
	buf->w = buf->w % BUF_SIZE;
	buf->w = buf->w+len2;

	pthread_mutex_unlock(&buf->lock);
}


