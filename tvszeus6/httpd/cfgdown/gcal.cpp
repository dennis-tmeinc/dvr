
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "../../cfg.h"
#include "../../ioprocess/diomap.h"

void dio_cmd_calibrate()
{
    // wait till rtc_cmd = 0 ;
    int delay ;
    for( delay=0; delay<20; delay++ ) {
        if( p_dio_mmap->rtc_cmd <=0 ) break;
        usleep(100000);
    }
    dio_lock();
    p_dio_mmap->rtc_year = 0 ;
    p_dio_mmap->rtc_cmd = 10 ;          // calibrate gforce command
    dio_unlock();
}

void dio_wait_calibrate()
{
    // wait till rtc_cmd = 0 ;
    int delay ;
    for( delay=0; delay<200; delay++ ) {    // 20 seconds waiting
        if( p_dio_mmap->rtc_cmd <=0 ) break;
        usleep(100000);
    }
}

int main()
{
    int res = -1 ;
    FILE * gfile = fopen("gcal.txt", "w");
    if( gfile==NULL ) return 0 ;
    // run in background
    if( fork() ) {
        // CGI output
        fclose( gfile );
        printf( "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-cache\r\n\r\nG-Force Sensor calibration started." );
        return 0 ;
    }
    
    if( dio_mmap() ) {
        fprintf(gfile, "Starting G-Force Sensor calibration...");
        fflush(gfile);
        // wait till rtc_cmd = 0 ;
        int delay ;
        for( delay=0; delay<20; delay++ ) {
            if( p_dio_mmap->rtc_cmd <=0 ) break;
            usleep(100000);
        }
        dio_lock();
        p_dio_mmap->rtc_year = 0 ;
        p_dio_mmap->rtc_cmd = 10 ;          // calibrate gforce command
        dio_unlock();
        for( delay=0; delay<200; delay++ ) {    // 20 seconds waiting
            if( p_dio_mmap->rtc_cmd <=0 ) break;
            usleep(100000);
            fprintf(gfile, ".");
            fflush(gfile);
        }
        dio_lock();
        if( p_dio_mmap->rtc_cmd==0 ) {
            // success
            res = p_dio_mmap->rtc_year ;
        }
        else {
            // failed
            p_dio_mmap->rtc_cmd=0 ;     // finish command
            res = -1 ;
        }
        dio_unlock();
        dio_munmap();
    }
    if( res>0 ) {
        fprintf(gfile, "\nG-Force Sensor calibration success!\n");
    }
    else if( res<0 ) {
        fprintf(gfile, "\nG-Force Sensor calibration failed!\n");
    }
    else {      // ==0 
        fprintf(gfile, "\nG-Force Sensor calibration not supported!\n");
    }
    fclose( gfile );
    return 0 ;
}
