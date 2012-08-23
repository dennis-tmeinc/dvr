
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include "../dvrsvr/dvr.h"

/* available environment varialbes:

    USBROOT : usb disk device mounted directory, etc: /dvr/var/xdisk/sda1
    EXEDIR  : autorun executable path, etc: /dvr/var/xdisk/sda1/510
*/

int main()
{
    struct usbkey_plugin_st {
        char device[8] ;       // "sda", "sdb" etc.
        char mountpoint[128] ;
    } key_plugin ;

    // get usb key device name from environment $USBROOT
    char * usbroot ;
    char * p ;
    usbroot = getenv( "USBROOT" );
    if( usbroot == NULL ) {
        //printf( "No $USBROOT!\n");
        return 0 ;
    }
    p = strrchr( usbroot, '/' );
    if( p ) {
        key_plugin.device[0]= *(p+1);
        key_plugin.device[1]= *(p+2);
        key_plugin.device[2]= *(p+3);
        key_plugin.device[3]= 0;
        strncpy( key_plugin.mountpoint, usbroot, 128 );

        int dvrsocket = net_connect("127.0.0.1", DVRPORT);
        if( dvrsocket>0 ) {
            struct dvr_req req ;
            struct dvr_ans ans ;
            req.reqcode=REQUSBKEYPLUGIN ;
            req.data=0 ;
            req.reqsize=sizeof(key_plugin) ;
            if( net_send(dvrsocket, &req, sizeof(req)) ) {
                net_send(dvrsocket, &key_plugin, sizeof(key_plugin));
                if( net_recv(dvrsocket, &ans, sizeof(ans))) {
                    if( ans.anscode==ANSOK ) {
                        printf("Key event sent!\n");
                    }
                }
            }
            close( dvrsocket );
        }
    }
    return 0 ;
}
