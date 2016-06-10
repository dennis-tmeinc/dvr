
#ifndef __FIFOSWAP_H__
#define __FIFOSWAP_H__

// return swap position, positive only, return 0 for error
int swap_out( void * mem, int size );
// swap in buffer,  return 1 success
int swap_in( void * mem, int size, int swappos );
// discard swap space
int swap_discard( int swappos );

void swap_init();
void swap_finish();

#endif
