#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/rtc.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#ifdef  DVR_APP

#define  MCU_SUPPORT

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/config.h"
#include "../ioprocess/diomap.h"

char dvriomap[256] = "/var/dvr/dvriomap" ;

#endif

// check http://tf.nist.gov/tf-cgi/servers.cgi for servr ip
char nistserver[]="64.90.182.55" ;

// check http://support.ntp.org/bin/view/Servers/WebHome
char ntpserver[]="64.90.182.55" ;

void inittz()
{
#ifdef DVR_APP
    char * p ;
    config dvrconfig(CFG_FILE);
    string tz ;
    string tzi ;

    tz=dvrconfig.getvalue( "system", "timezone" );
    if( tz.length()>0 ) {
        tzi=dvrconfig.getvalue( "timezones", tz );
        if( tzi.length()>0 ) {
            p=strchr(tzi, ' ' ) ;
            if( p ) {
                *p=0;
            }
            p=strchr(tzi, '\t' ) ;
            if( p ) {
                *p=0;
            }
            setenv("TZ", tzi, 1);
        }
        else {
            setenv("TZ", tz, 1);
        }
    }
#endif
}

int readrtc(struct tm * ptm)
{
    int res=0;
    int hrtc = open("/dev/rtc", O_RDONLY );
    if( hrtc>0 ) {
        memset( ptm, 0, sizeof(struct tm));
        if( ioctl( hrtc, RTC_RD_TIME, ptm )==0 ) {
            res=1 ;
        }
        close( hrtc );
    }
    return res;
}

int setrtc(struct tm * ptm)
{
    int res=0;
    int hrtc = open("/dev/rtc", O_WRONLY );
    if( hrtc>0 ) {
        if( ioctl( hrtc, RTC_SET_TIME, ptm )==0 ) {
            res=1;
        }
        close( hrtc );
    }
    return res;
}

static char wday_name[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static char mon_name[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// return month
int getmonth(char * monthname) {
    int i;
    for( i=0; i<12; i++ ) {
        if( strcmp(monthname, mon_name[i])==0 ) {
            return i+1 ;
        }
    }
    return 0 ;
}

int printlocaltime()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    struct tm * ptm ;
    ptm = localtime( &tv.tv_sec );
    printf( "%.3s %.3s%3d %.2d:%.2d:%.2d.%03d %s %d\n",
           wday_name[ptm->tm_wday],
           mon_name[ptm->tm_mon],
           ptm->tm_mday, ptm->tm_hour,
           ptm->tm_min, ptm->tm_sec,
           int(tv.tv_usec/1000),
           ptm->tm_zone,
           1900 + ptm->tm_year);
    return 1;
}

int printutc()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    struct tm * ptm ;
    ptm = gmtime( &tv.tv_sec );
    printf( "%.3s %.3s%3d %.2d:%.2d:%.2d.%03d %s %d\n",
           wday_name[ptm->tm_wday],
           mon_name[ptm->tm_mon],
           ptm->tm_mday, ptm->tm_hour,
           ptm->tm_min, ptm->tm_sec,
           int(tv.tv_usec/1000),
           ptm->tm_zone,
           1900 + ptm->tm_year);
    return 1 ;
}

int printrtc()
{
    struct tm rtctm ;
    if( readrtc(&rtctm) ) {
        printf( "%.3s %.3s%3d %.2d:%.2d:%.2d %s %d\n",
               wday_name[rtctm.tm_wday],
               mon_name[rtctm.tm_mon],
               rtctm.tm_mday, rtctm.tm_hour,
               rtctm.tm_min, rtctm.tm_sec,
               "RTC",
               1900 + rtctm.tm_year);
        return 1;
    }
    else {
        return 0;       // error !
    }
}

int setlocaltime(char * datetime)
{
    struct timeval tv ;
    struct tm tmtime ;
    int year, month, day, hour, minute, second ;
    if( datetime==NULL || strlen(datetime)<12 ) {
        return 0 ;
    }
    else {
        year=month=day=hour=minute=second=0;
        sscanf(datetime, "%02d%02d%02d%02d%02d%02d",
               &year, &month, &day, &hour, &minute, &second );
        memset( &tmtime, 0, sizeof(tmtime));
        tmtime.tm_year=year+2000-1900 ;
        tmtime.tm_mon=month-1 ;
        tmtime.tm_mday=day ;
        tmtime.tm_hour=hour ;
        tmtime.tm_min=minute ;
        tmtime.tm_sec=second ;
        tmtime.tm_isdst=-1;
        tv.tv_usec=0;
        tv.tv_sec=(time_t)mktime(&tmtime);
        return settimeofday( &tv, NULL )==0 ;
    }
}

int utctortc()
{
    struct timeval tv ;
    struct tm * ptm ;
    gettimeofday( &tv, NULL );
    ptm = gmtime( &tv.tv_sec );
    return setrtc(ptm);
}

int rtctoutc()
{
    struct timeval tv ;
    struct tm rtctm ;
    if( readrtc(&rtctm) ) {
        tv.tv_usec=0 ;
        tv.tv_sec=(time_t)timegm(&rtctm);
        return settimeofday( &tv, NULL )==0 ;
    }
    return 0 ;
}

int localtortc()
{
    struct timeval tv ;
    struct tm * ptm ;
    gettimeofday( &tv, NULL );
    ptm = localtime( &tv.tv_sec );
    return setrtc(ptm);

}

int rtctolocal()
{
    struct timeval tv ;
    struct tm rtctm ;
    if( readrtc(&rtctm) ) {
        rtctm.tm_isdst=-1 ;					// adjust dst automatically
        tv.tv_usec=0 ;
        tv.tv_sec=(time_t)mktime(&rtctm);
        return settimeofday( &tv, NULL )==0 ;
    }
    return 0;
}

#ifdef	MCU_SUPPORT

// return 1: success
//        0: failed
int readmcu(struct tm * t)
{
    int res=0;
    int i;
    config dvrconfig(CFG_FILE);
    string iomapfile ;
    iomapfile = dvrconfig.getvalue( "system", "iomapfile");
    if( iomapfile.length()>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    if( dio_mmap( dvriomap )==NULL ) {
        return 0 ;
    }
    if( p_dio_mmap->iopid<=0 ) {	// IO not started !
        dio_munmap();
        return 0 ;
    }

    // wait
    for( i=0;i<1000; i++ ) {
        if( p_dio_mmap->rtc_cmd==0 ) break;
        if( p_dio_mmap->rtc_cmd<0 ) {
            p_dio_mmap->rtc_cmd=0;
            break;
        }
        usleep(1000);
    }
    p_dio_mmap->rtc_cmd = 1 ;		// read rtc command
    // wait
    for( i=0;i<1000; i++ ) {
        if( p_dio_mmap->rtc_cmd!=1 )
            break;
        usleep(1000);
    }

    if( p_dio_mmap->rtc_cmd==0 ) {
        dio_lock();
        t->tm_year=p_dio_mmap->rtc_year-1900 ;
        t->tm_mon =p_dio_mmap->rtc_month-1 ;
        t->tm_mday=p_dio_mmap->rtc_day ;
        t->tm_hour=p_dio_mmap->rtc_hour ;
        t->tm_min =p_dio_mmap->rtc_minute ;
        t->tm_sec =p_dio_mmap->rtc_second ;
        dio_unlock();
        mktime(t);
        res=1 ;
    }
    dio_munmap();
    return res ;
}

// return 1: success
//        0: failed
int writemcu(struct tm * t)
{
    int res=0;
    int i;
    config dvrconfig(CFG_FILE);
    string iomapfile( dvrconfig.getvalue( "system", "iomapfile") ) ;
    if( iomapfile.length()>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    if( dio_mmap( dvriomap )==NULL ) {
        return 0 ;
    }
    if( p_dio_mmap->iopid<=0 ) {	// IO not started !
        dio_munmap();
        return res ;
    }

    // wait
    for( i=0;i<1000; i++ ) {
        if( p_dio_mmap->rtc_cmd==0 ) break;
        if( p_dio_mmap->rtc_cmd<0 ) {
            p_dio_mmap->rtc_cmd=0;
            break;
        }
        usleep(1000);
    }
    dio_lock();
    p_dio_mmap->rtc_year=t->tm_year+1900 ;
    p_dio_mmap->rtc_month=t->tm_mon+1;
    p_dio_mmap->rtc_day=t->tm_mday;
    p_dio_mmap->rtc_hour=t->tm_hour;
    p_dio_mmap->rtc_minute=t->tm_min;
    p_dio_mmap->rtc_second=t->tm_sec;
    p_dio_mmap->rtc_cmd = 2 ;		// set rtc command
    dio_unlock();
    // wait
    for( i=0;i<1000; i++ ) {
        if( p_dio_mmap->rtc_cmd!=2 )
            break;
        usleep(1000);
    }

    if( p_dio_mmap->rtc_cmd==0 ) {
        res=1 ;
    }
    dio_munmap();
    return res ;
}

int mcu()
{
    struct tm rtctm ;
    if( readmcu(&rtctm) ) {
        printf( "%.3s %.3s%3d %.2d:%.2d:%.2d %s %d\n",
               wday_name[rtctm.tm_wday],
               mon_name[rtctm.tm_mon],
               rtctm.tm_mday, rtctm.tm_hour,
               rtctm.tm_min, rtctm.tm_sec,
               "MCU",
               1900 + rtctm.tm_year);
        return 1 ;
    }
    else {
        printf("No mcu detected!\n");
        return 0 ;
    }
}

int utctomcu()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    struct tm * ptm ;
    ptm = gmtime( &tv.tv_sec );
    if( writemcu(ptm) ) {
        return 1 ;
    }
    else {
        return 0 ;
    }
}

int mcutoutc()
{
    struct timeval tv ;
    struct tm rtctm ;
    if( readmcu(&rtctm) ) {
        tv.tv_usec=0 ;
        tv.tv_sec=(time_t)timegm(&rtctm);
        return settimeofday( &tv, NULL )==0 ;
    }
    return 0;
}

int localtomcu()
{
    struct timeval tv ;
    gettimeofday( &tv, NULL );
    struct tm * ptm ;
    ptm = localtime( &tv.tv_sec );
    return writemcu(ptm);
}

int mcutolocal()
{
    struct timeval tv ;
    struct tm rtctm ;
    if( readmcu(&rtctm) ) {
        rtctm.tm_isdst=-1 ;					// adjust dst automatically
        tv.tv_usec=0 ;
        tv.tv_sec=(time_t)mktime(&rtctm);
        return settimeofday( &tv, NULL )==0 ;
    }
    return 0 ;
}

// return 1: success
//        0: failed
int readgps(struct tm * t)
{
    int res=0;
    config dvrconfig(CFG_FILE);
    string iomapfile( dvrconfig.getvalue( "system", "iomapfile")) ;
    if( iomapfile.length()>0 ) {
        strncpy( dvriomap, iomapfile, sizeof(dvriomap));
    }
    if( dio_mmap( dvriomap )==NULL ) {
        return 0 ;
    }
    if( p_dio_mmap->iopid<=0 ) {	// IO not started !
        dio_munmap() ;
        return res ;
    }

    if( p_dio_mmap->gps_valid ) {
        time_t tt=(time_t)p_dio_mmap->gps_gpstime ;
        gmtime_r( &tt, t );
        res=1 ;
    }
    dio_munmap();
    return res ;
}

int gps()
{
    struct tm ttm ;
    if( readgps(&ttm) ) {
        printf( "%.3s %.3s%3d %.2d:%.2d:%.2d %s %d\n",
               wday_name[ttm.tm_wday],
               mon_name[ttm.tm_mon],
               ttm.tm_mday, ttm.tm_hour,
               ttm.tm_min, ttm.tm_sec,
               "GPS",
               1900 + ttm.tm_year);
        return 1 ;
    }
    else {
        printf("No valid GPS time!\n");
        return 0 ;
    }
}

int gpstoutc()
{
    struct timeval tv ;
    struct tm rtctm ;
    if( readgps(&rtctm) ) {
        tv.tv_usec=0 ;
        tv.tv_sec=(time_t)timegm(&rtctm);
        if( settimeofday( &tv, NULL )==0 ) {
            printf("Ok!\n");
            return 1 ;
        }
    }
    printf("Failed!\n");
    return 0;
}

#endif 	// MCU_SUPPORT

struct my_sockaddr {
    socklen_t addrlen ;
    struct sockaddr addr ;
    char pad[256] ;
} ;

// connect to
//  type : SOCK_STREAM or SOCK_DGRAM
int net_connect(const char *netname, int port, int type, struct my_sockaddr * ad)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    int sockfd;
    char service[20];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = type;

    sprintf(service, "%d", port);
    res = NULL;
    if (getaddrinfo(netname, service, &hints, &res) != 0) {
        printf("Error:netsvr:net_connect!");
        return -1;
    }
    if (res == NULL) {
        printf("Error:netsvr:net_connect!");
        return -1;
    }
    ressave = res;

    /*
     Try open socket with each address getaddrinfo returned,
     until getting a valid socket.
     */
    sockfd = -1;
    while (res) {
        sockfd = socket(res->ai_family,
                        res->ai_socktype, res->ai_protocol);

        if (sockfd != -1) {
            if( ad!=NULL ) {
                ad->addrlen = res->ai_addrlen ;
                memcpy( &(ad->addr), res->ai_addr, res->ai_addrlen);
            }
            if( type==SOCK_STREAM ) {
                if( connect(sockfd, res->ai_addr, res->ai_addrlen)==0 ) {
                    break;
                }
                close(sockfd);
                sockfd = -1;
            }
            else {
                break;
            }
        }
        res = res->ai_next;
    }

    freeaddrinfo(ressave);

    if (sockfd == -1) {
        printf("Error:netsvr:net_connect!");
        return -1;
    }

    return sockfd;
}

// wait for network ready
int net_recvok(int fd, int tout)
{
    struct timeval timeout = { tout, 1 };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (select(fd + 1, &fds, NULL, NULL, &timeout) > 0) {
        return FD_ISSET(fd, &fds);
    } else {
        return 0;
    }
}

int  nist_daytime(char * server)
{
    char buf[512] ;
    int fd ;
    int n ;
    int res=0 ;
    if( server==NULL ) {
        fd = net_connect(nistserver, 13, SOCK_STREAM, NULL);
    }
    else {
        fd = net_connect(server, 13, SOCK_STREAM, NULL);
    }
    if( fd>0 ) {
        if( net_recvok( fd, 3 ) ) {
            n=recv(fd, buf, 512,0);
            buf[n]=0 ;
            printf("%s", buf);
            res=1 ;
        }
        else {
            printf("Time out!\n");
        }
        close(fd);
    }
    else {
        printf("Can't connect to NIST server.\n");
    }
    return res ;
}

struct ntp_block {
    // first word
    unsigned	Mode:3 ;
    unsigned	VN:3 ;
    unsigned	LI:2 ;
    unsigned	Stratum: 8 ;
    unsigned	Poll:8 ;
    int			Precision:8 ;
    int			Root_Delay ;
    unsigned int	Root_Dispersion ;
    unsigned int	Reference_ID ;
    unsigned int	Reference_Timestamp  ;
    unsigned int	Reference_Timestamp_f  ;		// time fraction
    unsigned int	Originate_Timestamp ;
    unsigned int	Originate_Timestamp_f ;			// timestamp fraction
    unsigned int	Receive_Timestamp ;
    unsigned int	Receive_Timestamp_f ;			// timestamp fraction
    unsigned int	Transmit_Timestamp ;
    unsigned int	Transmit_Timestamp_f ;			// timestamp fraction
};

int ntp_gettime(char * server, struct ntp_block * ntpb)
{
    struct my_sockaddr ad ;
    struct ntp_block sntpb ;
    int fd ;
    int n ;
    int res=0;
    if( server==NULL ) {
        fd = net_connect(ntpserver, 123, SOCK_DGRAM, &ad );
    }
    else {
        fd = net_connect(server, 123, SOCK_DGRAM, &ad);
    }
    if( fd>0 ) {
        memset(&sntpb, 0, sizeof(sntpb));
        sntpb.VN=3 ;
        sntpb.Mode=3 ;		// client
        sntpb.Stratum=0 ;
        sntpb.Originate_Timestamp=htonl((unsigned)time(NULL)+2208988800u);
        sendto(fd, &sntpb, sizeof(sntpb), 0, &ad.addr, ad.addrlen);
        if( net_recvok( fd, 3 ) ) {
            n=recvfrom(fd, ntpb, sizeof(sntpb), 0, &ad.addr, &ad.addrlen);
            if( n>0 ) {
                res=1;
            }
            else {
                printf("recv error!\n");
            }
        }
        else {
            printf("Time out!\n");
        }
        close(fd);
    }
    else {
        printf("Can't connect to NTP server.\n");
    }
    return res ;
}


// return ntp time offset in seconds (float point)
double ntp_offset(char * server)
{
    struct my_sockaddr ad ;
    struct ntp_block ntpb ;
    struct timeval tv_send ;
    struct timeval tv_recv ;

    int fd ;
    int n ;
    double res=0.0;
    if( server==NULL ) {
        fd = net_connect(ntpserver, 123, SOCK_DGRAM, &ad);
    }
    else {
        fd = net_connect(server, 123, SOCK_DGRAM, &ad);
    }
    if( fd>0 ) {
        memset(&ntpb, 0, sizeof(ntpb));
        ntpb.VN=3 ;
        ntpb.Mode=3 ;		// client
        ntpb.Stratum=0 ;
        gettimeofday( &tv_send, NULL );
        ntpb.Originate_Timestamp=htonl((unsigned)tv_send.tv_sec+2208988800u);
        sendto(fd, &ntpb, sizeof(ntpb), 0, &ad.addr, ad.addrlen);
        if( net_recvok( fd, 3 ) ) {
            n=recvfrom(fd, &ntpb, sizeof(ntpb), 0, &ad.addr, &ad.addrlen);
            if( n>0 ) {
                gettimeofday( &tv_recv, NULL );
                double t1, t2, t3, t4 ;
                t1 = (double)tv_send.tv_sec + ((double)tv_send.tv_usec)/1000000.0 ;
                t4 = (double)tv_recv.tv_sec + ((double)tv_recv.tv_usec)/1000000.0 ;
                t2 = (double)ntohl(ntpb.Receive_Timestamp)-2208988800. + (double)ntohl(ntpb.Receive_Timestamp_f)*pow(2., -32 ) ;
                t3 = (double)ntohl(ntpb.Transmit_Timestamp)-2208988800. + (double)ntohl(ntpb.Transmit_Timestamp_f)*pow(2., -32 ) ;
                res = -(t4-t3+t1-t2)/2 ;
            }
            else {
                printf("recv error!\n");
            }
        }
        else {
            printf("Time out!\n");
        }
        close(fd);
    }
    else {
        printf("Can't connect to NTP server.\n");
    }
    return res ;
}


int ntp(char * server)
{
    struct ntp_block ntpb ;
    struct tm rtctm ;
    unsigned int ui ;
    int i;
    int point ;
    struct timeval tv ;

    memset( &ntpb, 0, sizeof(ntpb));
    if( ntp_gettime( server, &ntpb) ) {
        ui = ntohl(ntpb.Transmit_Timestamp);
        ui-=2208988800u ;
        tv.tv_sec = (time_t) ui ;
        ui = ntohl( ntpb.Transmit_Timestamp_f ) ;
        tv.tv_usec = 0 ;
        point=500000 ;
        for( i=0; i<10 ; i++ ) {
            if( ui & 0x80000000 ) {
                tv.tv_usec+=point ;
            }
            ui<<=1 ;
            point>>=1 ;
        }
        gmtime_r( (time_t *)&tv.tv_sec, &rtctm ) ;
        printf( "%.3s %.3s%3d %.2d:%.2d:%.2d.%03d %s %d\n",
               wday_name[rtctm.tm_wday],
               mon_name[rtctm.tm_mon],
               rtctm.tm_mday, rtctm.tm_hour,
               rtctm.tm_min, rtctm.tm_sec,
               (int)(tv.tv_usec/1000),
               "NTP",
               1900 + rtctm.tm_year);
        return 1 ;
    }
    return 0 ;
}

int ntptoutc(char * server)
{
    struct ntp_block ntpb ;
    unsigned int ui ;
    int i;
    int point ;
    struct timeval tv ;

    memset( &ntpb, 0, sizeof(ntpb));
    if( ntp_gettime( server, &ntpb) ) {
        ui = ntohl(ntpb.Transmit_Timestamp);
        ui-=2208988800u ;
        tv.tv_sec = (time_t) ui ;
        ui = ntohl( ntpb.Transmit_Timestamp_f ) ;
        tv.tv_usec = 0 ;
        point=500000 ;
        for( i=0; i<10 ; i++ ) {
            if( ui & 0x80000000 ) {
                tv.tv_usec+=point ;
            }
            ui<<=1 ;
            point>>=1 ;
        }
        settimeofday( &tv, NULL );
        return 1 ;
    }
    return 0;
}

#define HTTPRSIZE (512)

int htp(char * server)
{
    struct my_sockaddr ad ;
    int fd ;
    int n ;
    char buf[HTTPRSIZE] ;
    char * pbuf ;
    char * kbuf ;
    int res=0 ;
    if( server==NULL ) {
        fd = net_connect("www.ntp.org", 80, SOCK_STREAM, &ad );
        sprintf(buf, "GET / HTTP/1.1\nHost: %s \n\n", "www.ntp.org" );
    }
    else {
        fd = net_connect(server, 80, SOCK_STREAM, &ad);
        sprintf(buf, "GET / HTTP/1.1\nHost: %s \n\n", server );
    }
    if( fd>0 ) {
        send(fd, buf, strlen(buf), 0);
        if( net_recvok( fd, 3 ) ) {
            n=recv(fd, buf, HTTPRSIZE, 0);
            if( n>0 ) {
                if( n==HTTPRSIZE ) n-- ;
                buf[n]=0 ;
                pbuf=buf ;
                while(( pbuf=strchr(pbuf, '\n') )!=NULL ) {
                    pbuf++;
                    if( strncmp(pbuf, "Date: ", 6 )==0 ) {
                        if( (kbuf=strchr(pbuf, '\n')) ) {
                            *kbuf=0;
                        }
                        printf("%s\n", pbuf+6 );
                        break;
                    }
                }
                res=1 ;
            }
        }
        close( fd );
    }
    return res;
}

// return 1: success
//        0: failed
int htp_gettime(char * server, struct tm * t)
{
    struct my_sockaddr ad ;
    int fd ;
    int res=0 ;
    int n ;
    char buf[HTTPRSIZE] ;
    char * pbuf ;
    if( server==NULL ) {
        fd = net_connect("www.ntp.org", 80, SOCK_STREAM, &ad );
        sprintf(buf, "GET / HTTP/1.1\nHost: %s \n\n", "www.ntp.org" );
    }
    else {
        fd = net_connect(server, 80, SOCK_STREAM, &ad);
        sprintf(buf, "GET / HTTP/1.1\nHost: %s \n\n", server );
    }
    if( fd>0 ) {
        send(fd, buf, strlen(buf), 0);
        if( net_recvok( fd, 3 ) ) {
            n=recv(fd, buf, HTTPRSIZE, 0);
            if( n>0 ) {
                if( n==HTTPRSIZE ) n-- ;
                buf[n]=0 ;
                pbuf=buf ;
                while(( pbuf=strchr(pbuf, '\n') )!=NULL ) {
                    pbuf++;
                    if( strncmp(pbuf, "Date: ", 6 )==0 ) {
                        char wday[10], month[10] ;
                        int mday, year, hour, minute, second ;
                        n=sscanf( pbuf, "Date: %4s %02d %3s %d %02d:%02d:%02d",
                                 wday,
                                 &mday,
                                 month,
                                 &year,
                                 &hour,
                                 &minute,
                                 &second );
                        if( n==7 ) {
                            t->tm_year=year-1900 ;
                            t->tm_mon=getmonth(month)-1;
                            t->tm_mday=mday;
                            t->tm_hour=hour;
                            t->tm_min=minute;
                            t->tm_sec=second;
                            t->tm_isdst=-1 ;
                            res=1 ;
                        }
                        break;
                    }
                }
            }
        }
        close( fd );
    }
    return res;
}


int htptoutc(char * server)
{
    struct tm t ;
    struct timeval tv ;
    struct timeval rem ;
    time_t thtp ;
    int diffs ;
    if( htp_gettime(server, &t) ) {
        thtp = timegm(&t);
        gettimeofday(&tv, NULL);
        diffs = (int)thtp - (int)tv.tv_sec ;
        if( diffs>2 || diffs<-2 ) {
            tv.tv_sec = thtp ;
            tv.tv_usec = 0 ;
            settimeofday( &tv, NULL );
        }
        else {
            tv.tv_sec = diffs ;
            tv.tv_usec = 0 ;
            adjtime(&tv, &rem);
        }
        return 1 ;
    }
    return 0 ;
}

int cool(char * arg)
{
    double offset=ntp_offset (arg);
    struct timeval tv ;
    struct timeval rem ;
    tv.tv_sec=(time_t)offset ;
    tv.tv_usec=(int)((offset-(double)tv.tv_sec)*1000000.0) ;
    adjtime(&tv, &rem);
    printf("offset: %f, tv: %d-%d, rem : %d-%d\n", offset,
           (int) tv.tv_sec, (int)tv.tv_usec,
           (int) rem.tv_sec, (int) rem.tv_usec );
    return 1 ;
}

int main(int argc, char * argv[])
{
    int res=1 ;
    inittz();
    if( argc<=1 ) {
        res=printlocaltime();
    }
    else if( strcmp(argv[1], "rtc" )==0 ) {
        res=printrtc();
    }
    else if( strcmp(argv[1], "utc" )==0 ) {
        res=printutc();
    }
    else if( strcmp(argv[1], "set" )==0 ) {
        setlocaltime(argv[2]);
        res=printlocaltime();
    }
    else if( strcmp(argv[1], "utctortc" )==0 ) {
        res=utctortc();
    }
    else if( strcmp(argv[1], "rtctoutc" )==0 ) {
        res=rtctoutc();
    }
    else if( strcmp(argv[1], "localtortc" )==0 ) {
        res=localtortc();
    }
    else if( strcmp(argv[1], "rtctolocal" )==0 ) {
        res=rtctolocal();
    }
    else if( strcmp(argv[1], "tz" )==0 ) {
        printf("TZ=%s\n", getenv("TZ"));
    }
#ifdef	MCU_SUPPORT
    else if( strcmp(argv[1], "mcu" )==0 ) {
        res=mcu();
    }
    else if( strcmp(argv[1], "utctomcu" )==0 ) {
        res=utctomcu();
    }
    else if( strcmp(argv[1], "mcutoutc" )==0 ) {
        res=mcutoutc();
    }
    else if( strcmp(argv[1], "localtomcu" )==0 ) {
        res=localtomcu();
    }
    else if( strcmp(argv[1], "mcutolocal" )==0 ) {
        res=mcutolocal();
    }
#endif	// MCU_SUPPORT
    else if( strcmp(argv[1], "nist" )==0 ) {
        if( argc>2 ) {
            res=nist_daytime(argv[2]);
        }
        else {
            res=nist_daytime(NULL);
        }
    }
    else if( strcmp(argv[1], "ntp" )==0 ) {
        if( argc>2 ) {
            res=ntp(argv[2]);
        }
        else {
            res=ntp(NULL);
        }
    }
    else if( strcmp(argv[1], "ntptoutc" )==0 ) {
        if( argc>2 ) {
            res=ntptoutc(argv[2]);
        }
        else {
            res=ntptoutc(NULL);
        }
    }
    else if( strcmp(argv[1], "htp" )==0 ) {
        res=htp(argv[2]);
    }
    else if( strcmp(argv[1], "htptoutc" )==0 ) {
        res=htptoutc(argv[2]);
    }
#ifdef MCU_SUPPORT
    else if( strcmp(argv[1], "gps" )==0 ) {
        res=gps();
    }
    else if( strcmp(argv[1], "gpstoutc" )==0 ) {
        res=gpstoutc();
    }
#endif
    else if( strcmp(argv[1], "cool" )==0 ) {
        res=cool(argv[2]);
    }
    else {
        printlocaltime();
        printf("\nUsage:\n%s [cmd] [YYMMDDhhmmss]\n", argv[0] );
        printf("\tcmd: utc, rtc, set, \n");
        printf("\t     rtctolocal, rtctoutc, localtortc, utctortc, tz\n");
        printf("\t     mcu, mcutolocal, mcutoutc, localtomcu, utctomcu\n");
        printf("\t     nist, ntp, ntptoutc, htp, htptoutc,\n");
        printf("\t     gps, gpstoutc.\n");
    }
    if( res ) {
        return 0 ;
    }
    else {
        return 1 ;
    }
}


