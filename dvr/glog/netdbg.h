
#ifndef __NETDBG_H__
#define __NETDBG_H__

// standard headers

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>

void netdbg_init( char * ip, int port );
void netdbg_finish();
void netdbg_print( char * fmt, ... );
void netdbg_dump( void * data, int size );

extern int netdbg_on ;

#endif
