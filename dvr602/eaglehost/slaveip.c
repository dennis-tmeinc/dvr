
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <net/if.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

// #define __DBG__

#ifdef __DBG__
#define PDBG( str ) 	printf( str )
#else
#define PDBG( str )
#endif

#define HOSTIDSIZE      32

struct eagleip_message {
    uint32_t    msgsize;                // sizeof eagleip_message
    uint32_t    msgcode;                // 1: setip, 2: slave ip, 3: reboot, 4: wrong ip (duplicated ip)
    uint32_t    ipaddr ;
    uint32_t    eagleid ;               // board #, 0: host, 1,2,3... for slave board
    uint8_t     hostid[HOSTIDSIZE] ;    // mac id for now
} ;

#define HOST_PORT	(15151)
#define SLAVE_PORT	(15152)

// wait for socket ready to read (timeout in micro-seconds)
int net_recvok(int fd, int tout)
{
    struct timeval timeout ;
    timeout.tv_sec = tout/1000000 ;
    timeout.tv_usec = tout%1000000 ;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(fd, &fds);
    } else {
        return 0;
    }
}

int boardnum=1 ;
char * hostidfile="hostid" ;    // host id file name
int boardid = 1 ;               // default board # 1

uint32_t setip( struct eagleip_message * ipmsg )
{
    uint32_t ip ;
    FILE * fhd ;
    char cmd[256] ;
    
    // record host id
    fhd = fopen( hostidfile, "wb" );
    if( fhd ) {
        fwrite( ipmsg, 1, sizeof( *ipmsg ), fhd );
        fclose( fhd );
    }
    
    // set my ip address
    ip = ipmsg->ipaddr + boardid ;
    sprintf( cmd, "ifconfig eth0 %d.%d.%d.%d",
            (ip>>24)&0xff,
            (ip>>16)&0xff,
            (ip>>8)&0xff,
            (ip)&0xff );
    system( cmd );
    
    // update /etc/hosts
    fhd = fopen( "/etc/hosts", "w" );
    if( fhd ) {
        fprintf( fhd, "127.0.0.1    localhost\n%d.%d.%d.%d    eaglehost\n",
                (ipmsg->ipaddr>>24)&0xff,
                (ipmsg->ipaddr>>16)&0xff,
                (ipmsg->ipaddr>>8)&0xff,
                (ipmsg->ipaddr)&0xff );
        fclose( fhd );
    }
    
    return ip ;
}


int main(int argc, char * argv[] )
{
    int i;
    int res ;
    int fd ;
    struct eagleip_message sendmsg, recvmsg, ipmsg ;	
    struct sockaddr_in addr;
    socklen_t fromlen ;
    FILE * fhd ;
    uint32_t ip ;
    int rept = 0 ;
    
    if( argc<3 ) {
        printf("Usage: slaveip boardid hostidfile\n");
        return 2;
    }
    
    boardid = atoi(argv[1])-1 ;
    hostidfile = argv[2] ;
    
    if( boardid <1 )
        boardid=1 ;
    if( boardid >7 ) 
        boardid=7 ;
    
    // read recorded hostid
    fhd = fopen(hostidfile, "r");
    if( fhd ) {
        fread( &ipmsg, 1, sizeof(ipmsg), fhd );
        fclose( fhd );
        ipmsg.msgcode = 10001 ;			// pre-recorded ipmsg
    }
    else {
        // default value
        ipmsg.msgsize = sizeof( ipmsg );
        ipmsg.ipaddr = 0xc0a8f701 ;		// 192.168.247.1
        ipmsg.msgcode = 10002 ;			// no pre-recorded ipmsg
    }
    
    // receiver socket
    fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SLAVE_PORT);
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    
    // first round listening
    while( net_recvok( fd, 20000000 ) ) {		// 20 seconds:w
        fromlen = sizeof( addr );
        res = recvfrom( fd, &recvmsg, sizeof(recvmsg), 0, (struct sockaddr *)&addr, &fromlen  );
        if( res==sizeof(recvmsg) &&
           recvmsg.msgsize == sizeof( struct eagleip_message ) &&
           recvmsg.msgcode == 1 ) {			// setip message
               PDBG("Recv host ip.\n");
               if(	ipmsg.msgcode == 10002 || memcmp( ipmsg.hostid, recvmsg.hostid, sizeof( recvmsg.hostid))==0 ) {
                   ipmsg=recvmsg ;
                   res = 0 ;
                   break ;
               }
               else if( memcmp( &sendmsg, &recvmsg, sizeof(recvmsg) )==0 ) {	// see if we repeatly recveive same setip msg (from same host)
                   rept++ ;
                   if( rept>=10 ) {
                       ipmsg=recvmsg ;
                       break;
                   }
               }
               else {
                   rept=0;
                   memcpy( &sendmsg, &recvmsg, sizeof(recvmsg) );
               }
           }
    }
    
    // set ip address 
    ip = setip( &ipmsg );
    
    if( ipmsg.msgcode == 1 ) {					// received ipmsg
        // responds
        sendmsg=ipmsg ;							// copy from received msg
        sendmsg.msgsize = sizeof( sendmsg );		// sizeof eagleip_message
        sendmsg.msgcode=2 ;							// slave ip
        sendmsg.ipaddr = ip ;
        sendmsg.eagleid = boardid ;
        
        // responds socket
        addr.sin_addr.s_addr = htonl( ipmsg.ipaddr );
        sendto( fd, &sendmsg, sizeof(sendmsg), 0, (struct sockaddr *)&addr, sizeof(addr) );
        PDBG("Sent slave ip.\n");
    }
    
    // turn into a deamon
    if( fork()!=0 ) {
        // exit main process
        if( ipmsg.msgcode == 1 ) {					// received ipmsg
            return 0;
        }
        else {
            return 1;
        }
    }
    
    // now we run as a deamon
    
    for(;;) {
        if( net_recvok( fd, 20000000 ) ) {		// 20 seconds:w
            fromlen = sizeof( addr );
            res = recvfrom( fd, &recvmsg, sizeof(recvmsg), 0, (struct sockaddr *)&addr, &fromlen  );
            if( res<=0 ) {
                printf("Socket Error!");
                break;
            }
            if( res==sizeof(recvmsg) &&
               recvmsg.msgsize == sizeof( struct eagleip_message ) ) {
                   if( memcmp( recvmsg.hostid, ipmsg.hostid, sizeof(ipmsg.hostid) )==0 ) {		// msg from my host
                       if( recvmsg.msgcode == 1 ) {			// set ip
                           // set ip address 
                           ipmsg = recvmsg ;
                           ip = setip( &ipmsg );
                           // responds
                           sendmsg=ipmsg ;							// copy from received msg
                           sendmsg.msgcode=2 ;						// slave ip
                           sendmsg.ipaddr = ip ;
                           sendmsg.eagleid = boardid ;
                           
                           // responds socket
                           addr.sin_addr.s_addr = htonl( recvmsg.ipaddr );
                           sendto( fd, &sendmsg, sizeof(sendmsg), 0, (struct sockaddr *)&addr, sizeof(addr) );
                           PDBG("Sent slave ip.\n");
                       }
                       else if( recvmsg.msgcode == 3 ) {		// reboot!
                           sync();sync();sync();
                           system("reboot");
                       }
                   }
                   else {							// msg from other host
                       if( recvmsg.ipaddr == ipmsg.ipaddr ) {		// ip address conflict!!!!
                           // send a wrong ip msg back
                           sendmsg = recvmsg ;
                           sendmsg.msgcode=4 ;						// duplicated host ip
                           
                           addr.sin_addr.s_addr = htonl( recvmsg.ipaddr );
                           sendto( fd, &sendmsg, sizeof(sendmsg), 0, (struct sockaddr *)&addr, sizeof(addr) );
                           PDBG("Sent duplicated ip.\n");
                       }
                   }
               }
        }
    }
    return 1;
}
