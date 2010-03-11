#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

// Generate USB id string like: Ven_SanDisk&Prod_Cruzer_Mini&Rev_0.1\20041100531daa107a09

// Ven   :  /sys/block/sda/device/vendor
// Prod  :  /sys/block/sda/device/model
// Rev   :  /sys/block/sda/device/rev
// serial:  cd -P /sys/block/sda/device ; ../../../../serial
//          /sys/block/sda/device/../../../../serial

void trimtail(char * str)
{
    int t = strlen(str);
    while( t>0 ) {
        if( str[t-1]=='\n' || str[t-1]=='\r' || str[t-1]==' ' ) {
            t-- ;
            str[t]=0 ;
        }
        else {
            break;
        }
    }
}

int main(int argc, char * argv[])
{
    char sysfilename[256] ;
    FILE * sysfile ;
    int r ;
    char vendor[64] ;
    char product[64] ;
    char rev[64] ;
    char serial[64] ;

    if( argc<2 ) {
        printf( "Usage : usbid <device>\n");
    }

    // get vendor
    vendor[0]=0 ;
    sprintf( sysfilename, "/sys/block/%s/device/vendor", argv[1] );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(vendor, 64, sysfile );
        fclose( sysfile );
    }
    trimtail( vendor );
    
    // get product
    product[0] = 0 ;
    sprintf( sysfilename, "/sys/block/%s/device/model", argv[1] );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(product, 64, sysfile );
        fclose( sysfile );
    }
    trimtail( product );
        
    // get rev
    rev[0] = 0 ;
    sprintf( sysfilename, "/sys/block/%s/device/rev", argv[1] );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(rev, 64, sysfile );
        fclose( sysfile );
    }
    trimtail( rev );
    
    // get serial no
    serial[0]=0 ;
    sprintf( sysfilename, "/sys/block/%s/device/../../../../serial", argv[1] );
    sysfile = fopen( sysfilename, "r" );
    if( sysfile ) {
        fgets(serial, 64, sysfile );
        fclose( sysfile );
    }
    trimtail( serial );

    sprintf( sysfilename, "Ven_%s&Prod_%s&Rev_%s\\%s", vendor, product, rev, serial ) ;

    r = 0 ;
    while( 1 ) {
        if( sysfilename[r] == 0 ) break ;
        else if( sysfilename[r]==' ' ) {
            sysfilename[r] = '_' ;
        }
        r++ ;
    }

    printf( "%s", sysfilename );
    return 0 ;

}

