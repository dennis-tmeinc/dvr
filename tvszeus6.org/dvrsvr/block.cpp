#include "block.h"

struct block_sys_t
{
    uint8_t     *p_allocated_buffer;
    int         i_allocated_buffer;
};

#define BLOCK_PADDING_SIZE 32

block_t *block_New( int i_size )
{
    /* We do only one malloc
     * TODO bench if doing 2 malloc but keeping a pool of buffer is better
     * 16 -> align on 16
     * 2 * BLOCK_PADDING_SIZE -> pre + post padding
     */
    block_sys_t *p_sys;
    const int i_alloc = i_size + 2 * BLOCK_PADDING_SIZE + 16;
    block_t *p_block =
        (block_t *)malloc( sizeof( block_t ) + sizeof( block_sys_t ) + i_alloc );

    if( p_block == NULL ) return NULL;

    /* Fill opaque data */
    p_sys = (block_sys_t*)( (uint8_t *)p_block + sizeof( block_t ) );
    p_sys->i_allocated_buffer = i_alloc;
    p_sys->p_allocated_buffer = (uint8_t *)p_block + sizeof( block_t ) +
        sizeof( block_sys_t );

    /* Fill all fields */
    p_block->p_next         = NULL;
    p_block->p_prev         = NULL;
    p_block->i_flags        = 0;
    p_block->i_pts          = 0;
    p_block->i_dts          = 0;
    p_block->i_length       = 0;
    p_block->i_rate         = 0;
    p_block->i_buffer       = i_size;
    p_block->p_buffer       =
        &p_sys->p_allocated_buffer[BLOCK_PADDING_SIZE +
            16 - ((uintptr_t)p_sys->p_allocated_buffer % 16 )];

		p_block->i_osd					= 0;
		p_block->i_header				= 0;
    p_block->p_sys          = p_sys;

    return p_block;
}

/*****************************************************************************
 * block_fifo_t management
 *****************************************************************************/
block_fifo_t *block_FifoNew()
{
    block_fifo_t *p_fifo;

    p_fifo = (block_fifo_t *)malloc( sizeof( block_fifo_t ) );
    pthread_mutex_init( &p_fifo->lock, NULL );
    pthread_cond_init( &p_fifo->wait, NULL );
    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;

    return p_fifo;
}

void block_FifoRelease( block_fifo_t *p_fifo )
{
    block_FifoEmpty( p_fifo );
    pthread_cond_destroy( &p_fifo->wait );
    pthread_mutex_destroy( &p_fifo->lock );
    free( p_fifo );
}

void block_FifoEmpty( block_fifo_t *p_fifo )
{
    block_t *b;

    pthread_mutex_lock( &p_fifo->lock );
    for( b = p_fifo->p_first; b != NULL; )
    {
        block_t *p_next;

        p_next = b->p_next;
        block_Release( b );
        b = p_next;
    }

    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    pthread_mutex_unlock( &p_fifo->lock );
}

/* 
 * adds one of more blocks to the fifo
 * i_depth: # of blocks in the fifo
 * return: total bytes in the fifo
 */
int block_FifoPut( block_fifo_t *p_fifo, block_t *p_block )
{
    int i_size = 0;
    pthread_mutex_lock( &p_fifo->lock );

    do
    {
		/* update the total size to return */
        i_size += p_block->i_buffer;

		/* add the block to the end of fifo */
        *p_fifo->pp_last = p_block;
		/* update the pointer to the last block */
        p_fifo->pp_last = &p_block->p_next;
		/* update the # of blocks in the fifo */
        p_fifo->i_depth++;
		/* update the total size in the fifo*/
        p_fifo->i_size += p_block->i_buffer;

        p_block = p_block->p_next;

    } while( p_block );

	/* warn there is data in this fifo */
    pthread_cond_signal( &p_fifo->wait );
    pthread_mutex_unlock( &p_fifo->lock );

    return i_size;
}

block_t *block_FifoGet( block_fifo_t *p_fifo )
{
    block_t *b;

    pthread_mutex_lock( &p_fifo->lock );

    /* We do a while here because there is a race condition in the
     * win32 implementation of tme_cond_wait() (We can't be sure the fifo
     * hasn't been emptied again since we were signaled). */
    while( p_fifo->p_first == NULL )
    {
        pthread_cond_wait( &p_fifo->wait, &p_fifo->lock );
    }

	/* extract one block from the fifo */
	b = p_fifo->p_first;

	/* now that one is out, update the pointer to the first block, # or blocks, total bytes */ 
    p_fifo->p_first = b->p_next;
    p_fifo->i_depth--;
    p_fifo->i_size -= b->i_buffer;

    if( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    pthread_mutex_unlock( &p_fifo->lock );

    b->p_next = NULL;
    return b;
}

