
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../cfg.h"
#include "../../ioprocess/diomap.h"

char * getquery( const char * qname );

void dio_cmd_calangle()
{
    // wait till rtc_cmd = 0 ;
    int delay ;
    for( delay=0; delay<20; delay++ ) {
        if( p_dio_mmap->rtc_cmd <=0 ) break;
        usleep(100000);
    }
    p_dio_mmap->rtc_cmd = 15 ;          // Enter calibrate gforce angle
}

void dio_cmd_calangle_end()
{
    // wait till rtc_cmd = 0 ;
    int delay ;
    for( delay=0; delay<20; delay++ ) {
        if( p_dio_mmap->rtc_cmd <=0 ) break;
        usleep(100000);
    }
    p_dio_mmap->rtc_cmd = 16 ;          // Leave calibrate gforce angle
}

void dio_wait_cmd()
{
    // wait till rtc_cmd = 0 ;
    int delay ;
    for( delay=0; delay<200; delay++ ) {    // 20 seconds waiting
        if( p_dio_mmap->rtc_cmd <=0 ) break;
        usleep(100000);
    }
}

int main(int argc, char *argv[])
{
    // print header
    printf( "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-cache\r\n\r\n" );
    char * op = getquery( "op" );
    if( op==NULL && argc>1 ) {
        op=argv[1] ;
    }
    if( dio_mmap() && op ) {
        if( strcmp( op, "start" )==0 ) {
            p_dio_mmap->rtc_cmd = 15 ;          // calibrate gforce command
        }
        else if( strcmp( op, "stop")==0 ) {
            // stop cal
            p_dio_mmap->rtc_cmd = 16 ;          // end calibrate gforce command
        }
        float gf, gd, gr, angle ;
        dio_lock();
        gf=p_dio_mmap->gforce_forward ;
        gd=p_dio_mmap->gforce_down ;
        gr=p_dio_mmap->gforce_right ;
        dio_unlock();
        if( gd<0.01 && gd>-0.01 ) {
            gd=0.01 ;               // give it a smallest number, so no divided by zero error
        }
        angle=180.0*atan( gf/gd )/M_PI;
        printf( "{\"gvalue_f\":%.2f,\"gvalue_d\":%.2f,\"gvalue_r\":%.2f,\"gvalue_angle\":%.0f}", gf, gd, gr, angle );
        dio_munmap();
    }
    else {
        printf( "{\"gvalue_f\":0,\"gvalue_d\":0,\"gvalue_r\":0,\"gvalue_angle\":0}");
    }
    return 0 ;
}
