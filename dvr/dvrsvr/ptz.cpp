#include "dvr.h"

static int ptz_enable;
static int ptz_protocol;		// 0 for pelco-D, 1 for pelco-P
static int ptz_handle ;

static struct termios saved_serialattributes;

// return 0:error, 1:ok
int ptz_msg( int channel, DWORD command, int param )
{
    int addr ;
    BYTE msg[8] ;
    if( ptz_handle <= 0 )
        return 0;
    
    addr = cap_channel[channel]->getptzaddr();
    
    msg[2] = (BYTE)(command >> 24) ;
    msg[3] = (BYTE)(command >> 16) ;
    msg[4] = (BYTE)(command >> 8) ;
    msg[5] = (BYTE)(command) ;
    if( (msg[3] & 6)!=0 ) {
        msg[4] += param ;
    }
    if( (msg[3] & 0x19)!=0 ) {
        msg[5] += param ;
    }
    
    if( ptz_protocol == 0 ) {			// PELCO "D" pro
        msg[0] = 0xff ;
        msg[1] = addr ;
        msg[6] = msg[1]+msg[2]+msg[3]+msg[4]+msg[5] ;
        return write( ptz_handle, msg, 7 );
    }
    else if( ptz_protocol == 1 ) {
        // translate from pelco "D" to pelco "P"
        if( (msg[2]&0x88)==0x88 ) {				// camera on
            msg[2]=0x50 ;
        }
        else if( (msg[2]&0x88)==0x08 ) {		// camera off
            msg[2]=0x40 ;
        }
        else if( (msg[2]&0x90) == 0x90 ) {		// autoscan on
            msg[2]=0x20 ;
        }
        else if( (msg[2]&0x90) == 0x10 ) {		// autoscan off
            msg[2]=0x00 ;
        }
        else if( (msg[3]&1)==0 ) {
            msg[2] = (BYTE)(command >> 23) & 0x7f;
            msg[3] = (BYTE)(command >> 16) & 0x7f ;
        }
        msg[0] = 0xa0 ;
        msg[1] = addr ;
        msg[6] = 0xaf ;
        msg[7] = msg[0]^msg[1]^msg[2]^msg[3]^msg[4]^msg[5]^msg[6] ;
        return write( ptz_handle, msg, 8 );
    }
    return 0;
}

void ptz_init(config &dvrconfig)
{
    string t ;
    struct termios serialattributes;
    string ptz_device;
    int ptz_baudrate;
    ptz_enable = dvrconfig.getvalueint("ptz", "enable");
    if( ptz_enable == 0 ) {
        return;
    }
    sscanf((char *)t, "%d", &ptz_enable);
    if (ptz_enable) {
        ptz_device = dvrconfig.getvalue("ptz", "device");
        
        ptz_handle = open( ptz_device, O_WRONLY|O_NONBLOCK );
        if( ptz_handle > 0 ) {
            tcgetattr(ptz_handle, &saved_serialattributes);
            memcpy( &serialattributes, &saved_serialattributes, sizeof( saved_serialattributes ) );
            
            // set serial port attributes
            
            serialattributes.c_cflag = CS8 |CLOCAL | CREAD;
            serialattributes.c_iflag = IGNPAR;
            serialattributes.c_oflag = 0;
            serialattributes.c_lflag = 0;       //ICANON;
            serialattributes.c_cc[VMIN]=1;
            serialattributes.c_cc[VTIME]=0;
            ptz_baudrate = dvrconfig.getvalueint("ptz", "baudrate");
            if( ptz_baudrate==0 )
                ptz_baudrate=4800 ;
            cfsetspeed(&serialattributes, ptz_baudrate);
            tcflush(ptz_handle, TCIFLUSH);
            tcsetattr(ptz_handle, TCSANOW, &serialattributes);
            
            t = dvrconfig.getvalue("ptz", "protocol");
            if( *(char *)t=='P' )
                ptz_protocol = 1 ;
            else
                ptz_protocol = 0 ;
            
            dvr_log("PTZ initialized.");
        }
        else {
            dvr_log("PTZ error open serial port." );
        }
    }
}

void ptz_uninit()
{
    if( ptz_enable ) {
        if( ptz_handle>0 ) {
            tcflush(ptz_handle, TCIFLUSH);
            tcsetattr(ptz_handle, TCSANOW, &saved_serialattributes);
            close( ptz_handle );
            ptz_handle = 0 ;
        }
        dvr_log("PTZ uninitialized.");
    }
}
