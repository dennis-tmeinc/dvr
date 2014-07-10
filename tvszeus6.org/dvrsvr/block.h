#ifndef _BLOCK_H_
#define _BLOCK_H_

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
//#include "dvr.h"

typedef struct block_sys_t block_sys_t;

struct dvrtime1 {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int milliseconds;
	int tz;
};

struct block_t
{
  block_t     *p_next;
  block_t     *p_prev;
  
  uint32_t    i_flags;
  
  int64_t     i_pts;
  int64_t     i_dts;
  int64_t     i_length;
  
  int         i_samples; /* Used for audio */
  int         i_rate;

  int         i_buffer;
  uint8_t     *p_buffer;
  struct dvrtime1 frame_time; /* to get current time of picture */
  int i_seek_flag; /* to detect invalid frames after seek */ 
  
  int i_osd; /* osd text size in the beginning of frame, used in decode_video to avoid memmove */
  uint32_t    i_format;
  int i_header; /* header length */
  
  block_sys_t *p_sys;
};

inline void block_Release( block_t *p_block )
{
    free( p_block );
}

struct block_fifo_t
{
    pthread_mutex_t         lock;                         /* fifo data lock */
    pthread_cond_t          wait;         /* fifo data conditional variable */

    int                 i_depth;      /* # of blocks in the fifo */
    block_t             *p_first;
    block_t             **pp_last;
    int                 i_size;			/* total bytes in all blocks in the fifo */
};

block_t *block_New( int i_size );

block_fifo_t *block_FifoNew();
void block_FifoRelease( block_fifo_t *p_fifo );
void block_FifoEmpty( block_fifo_t *p_fifo );
int block_FifoPut( block_fifo_t *p_fifo, block_t *p_block );
block_t *block_FifoGet( block_fifo_t *p_fifo );

#endif
