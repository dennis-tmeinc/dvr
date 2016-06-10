/*
 file serial.cpp,
    serial port functions
 */

#ifndef __SERIAL_H__
#define __SERIAL_H__

#define SERIAL_DELAY	(1000000) //(100000)
#define DEFSERIALBAUD	(115200)

// open serial port
int serial_open(char * device, int buadrate);
int serial_close( int handle );
//
int serial_dataready(int handle, int usdelay, int * usremain);
int serial_read( int handle, char * buffer, int buffersize );
int serial_write( int handle, char *buffer, int buffersize );

#endif // __SERIAL_H__
