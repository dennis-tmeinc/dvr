
//#include "../../cfg.h"
#include "../../dvrsvr/dvr.h"
#include "../../ioprocess/diomap.h"
#include <sys/mman.h>
#include <sys/ioctl.h>

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

int net_connect(const char *netname, int port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *ressave;
    int sockfd;
    char service[20];
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    sprintf(service, "%d", port);
    res = NULL;
    if (getaddrinfo(netname, service, &hints, &res) != 0) {
        return -1;
    }
    if (res == NULL) {
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
            if( connect(sockfd, res->ai_addr, res->ai_addrlen)==0 ) {
                break;
            }
            close(sockfd);
            sockfd = -1;
        }
        res = res->ai_next;
    }
    
    freeaddrinfo(ressave);
    
    return sockfd;
}

// send all data
// return 
//       0: failed
//       other: success
int net_send(int sockfd, void * data, int datasize)
{
    int s ;
    char * cbuf=(char *)data ;
    while( (s = send(sockfd, cbuf, datasize, 0))>=0 ) {
        datasize-=s ;
        if( datasize==0 ) {
            return 1;			// success
        }
        cbuf+=s ;
    }
    return 0;
}

// receive all data
// return 
//       0: failed (time out)
//       other: success
int net_recv(int sockfd, void * data, int datasize)
{
    int r ;
    char * cbuf=(char *)data ;
    while( net_recvok(sockfd, 5000000) ) {
        r = recv(sockfd, cbuf, datasize, 0);
        if( r<=0 ) {
            break;				// error
        }
        datasize-=r ;
        if( datasize==0 ) {
            return 1;			// success
        }
        cbuf+=r ;
    }
    return 0;
}

struct channelstate {
    int sig ;
    int rec ;
    int mot ;
} ;

// get channel status
//    return value: totol channels of status returned.
int getchannelstate(struct channelstate * chst, unsigned long * streambytes, int ch)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    int sockfd ;
    int i;
    FILE * portfile ;
    int port ;
    int rch = 0;
    portfile = fopen("dvrsvr_port", "r");
    if( portfile==NULL ) {
        return rch;
    }
    port=15111 ;
    fscanf(portfile, "%d", &port);
    sockfd = net_connect("127.0.0.1", port);
    if( sockfd>0 ) {
        req.reqcode=REQGETCHANNELSTATE ;
        req.data=0 ;
        req.reqsize=0 ;
        if( net_send(sockfd, &req, sizeof(req)) ) {
            if( net_recv(sockfd, &ans, sizeof(ans))) {
                if( ans.anscode==ANSGETCHANNELSTATE && ans.data>0 ) {
                    rch = ans.data ;
                    if( rch>ch ) {
                        rch=ch ;
                    }
                    net_recv( sockfd, (void *)chst, rch*sizeof( struct channelstate ) );
                }
            }
        }
        
        // get stream bytes
        for( i=0; i<rch; i++ ) {
            streambytes[i]=0;
            req.reqcode=REQ2GETSTREAMBYTES ;
            req.data=i ;
            req.reqsize=0 ;
            if( net_send(sockfd, &req, sizeof(req)) ) {
                if( net_recv(sockfd, &ans, sizeof(ans))) {
                    if( ans.anscode==ANS2STREAMBYTES ) {
                        streambytes[i]=ans.data ;
                    }
                }
            }
        }
        close( sockfd );        
    }
    return rch ;
}

int memory_usage( int * mem_total, int * mem_free)
{
    // Parse /proc/meminfo
    int MemTotal, MemFree, Cached, Buffers, SwapTotal, SwapFree;
    FILE * fproc=NULL;
    char buf[256];
    int rnum = 0;
    fproc = fopen("/proc/meminfo", "r");
    if (fproc == NULL)
        return 0;
    while (fgets(buf, 256, fproc)) {
        if (memcmp(buf, "MemTotal:", 9) == 0) {
            rnum += sscanf(buf + 9, "%d", &MemTotal);
        } else if (memcmp(buf, "MemFree:", 8) == 0) {
            rnum += sscanf(buf + 8, "%d", &MemFree);
        } else if (memcmp(buf, "Cached:", 7) == 0) {
            rnum += sscanf(buf + 7, "%d", &Cached);
        } else if (memcmp(buf, "Buffers:", 8) == 0) {
            rnum += sscanf(buf + 8, "%d", &Buffers);
        } else if (memcmp(buf, "SwapTotal:", 10) == 0) {
            rnum += sscanf(buf + 10, "%d", &SwapTotal);
        } else if (memcmp(buf, "SwapFree:", 9) == 0) {
            rnum += sscanf(buf + 9, "%d", &SwapFree);
        }
    }
    fclose(fproc);

    *mem_total = MemTotal ;
    *mem_free = MemFree + Cached + Buffers ;
    return 1;
}

int disk_usage( int * disk_total, int * disk_free)
{
    int res=0;
    char filename[256] ;
    struct statfs stfs;
    filename[0]=0;
    FILE * curdisk = fopen("/var/dvr/dvrcurdisk", "r");
    if( curdisk ) {
        if( fscanf(curdisk, "%s", filename ) ) {
            if (statfs(filename, &stfs) == 0) {
                *disk_free = stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
                *disk_total = stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
                res=1 ;
            }
        }
        fclose( curdisk );
    }
    return res ;
}

int get_temperature( int * sys_temp, int * disk_temp)
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );                                // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return 0;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
        *sys_temp = p_dio_mmap->iotemperature ;
        *disk_temp = p_dio_mmap->hdtemperature ;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        return 1 ;
    }
    return 0 ;
}

struct dvrstat {
    struct timeval checktime ;
    float uptime, idletime ;
    unsigned long streambytes[16] ;
} savedstat ;
    

// generate status page
void print_status()
{
    // generate camera status.

    FILE * statfile ;
    int i;
    int  chno=0 ;
    int camera_number ;
    struct channelstate cs[16] ;
    struct dvrstat stat ;
    
    memset( &savedstat, 0, sizeof( savedstat ) );
    statfile = fopen( "savedstat", "rb");
    if( statfile ) {
        fread( &savedstat, sizeof(savedstat), 1, statfile );
        fclose( statfile );
    }

    statfile = fopen( "camera_number", "r");
    if( statfile ) {
        fscanf(statfile, "%d", &camera_number );
        fclose( statfile );
    }

    // set timezone
    char tz[128] ;
    FILE * ftz = fopen("tz_env", "r");
    if( ftz ) {
        fscanf(ftz, "%s", tz);
        fclose(ftz);
        setenv("TZ", tz, 1);
    }
        
    //JSON head
    printf("{");
        
    // dvr time
    char timebuf[100] ;
    double timeinc ;
    gettimeofday(&stat.checktime, NULL);
    
    time_t ttnow ;
    ttnow = (time_t) stat.checktime.tv_sec ;
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S "
    strftime( timebuf, sizeof(timebuf), RFC1123FMT, localtime( &ttnow ) );  
    printf("\"dvrtime\":\"%s\",", timebuf);
    
    if( savedstat.checktime.tv_sec==0 ) {
        timeinc = 1.0 ;
    }
    else {
        timeinc = (double)(stat.checktime.tv_sec-savedstat.checktime.tv_sec) + 
            (double)(stat.checktime.tv_usec - savedstat.checktime.tv_usec)/1000000.0 ;
    }
    
    // get status from dvrsvr
    memset( cs, 0, sizeof(cs) );
    chno=getchannelstate(cs, stat.streambytes, 16);
    for( i=1; i<=camera_number ; i++ ) 
    {
        if( cs[i-1].sig ) {
            printf("\"camera_%d_signal_lost\":\"on\",", i );
        }
        if( cs[i-1].rec ) {
            printf("\"camera_%d_recording\":\"on\",", i );
        }
        if( cs[i-1].mot ) {
            printf("\"camera_%d_motion\":\"on\",", i );
        }                
        
        double bitrate = (double)(stat.streambytes[i-1] - savedstat.streambytes[i-1]) / (timeinc*125.0) ;
        if( bitrate > 10000.0 ) {
            bitrate = 0.0 ;
        }
        
        printf("\"camera_%d_bitrate\":\"%d\",", i, (int)bitrate );
        
    }
        
    // calculate CPU usage
    FILE * uptimefile = fopen("/proc/uptime", "r" );
    if( uptimefile ) {
        fscanf( uptimefile, "%f %f", &stat.uptime, &stat.idletime ) ;
        fclose( uptimefile );
    }
    
    float cpuusage = stat.uptime - savedstat.uptime ;
    if( cpuusage < 0.1 ) {
        cpuusage = 99.0 ;
    }
    else {
        cpuusage = 100.0 * (cpuusage - (stat.idletime-savedstat.idletime)) / cpuusage ;
    }
    
    printf("\"cpu_usage\":\"%.1f\",", cpuusage );
    
    // print memory usage
    int stfree, sttotal ;
    if( memory_usage(&sttotal, &stfree) ) {
        printf("\"memory_total\":\"%.1f\",", ((float)sttotal)/1000.0 );
        printf("\"memory_free\":\"%.1f\",", ((float)stfree)/1000.0 );
    }
    
    // print disk usage
    if( disk_usage(&sttotal, &stfree) ) {
        printf("\"disk_total\":\"%d\",", sttotal );
        printf("\"disk_free\":\"%d\",", stfree );
    }
    else {
        printf("\"disk_total\":\"%d\",", 0 );
        printf("\"disk_free\":\"%d\",", 0 );
    }
    
    // print system temperature
    int systemperature=-128, hdtemperature=-128 ;
    get_temperature(&systemperature, &hdtemperature) ;
    if( systemperature>-127 && systemperature<127 )  {
        printf("\"temperature_system_c\":\"%d\",", systemperature );
        printf("\"temperature_system_f\":\"%d\",", systemperature*9/5+32 );
    }
    else {
        printf("\"temperature_system_c\":\" \"," );
        printf("\"temperature_system_f\":\" \"," );
    }
    if( hdtemperature>-127 && hdtemperature<127 )  {
        printf("\"temperature_disk_c\":\"%d\",", hdtemperature );
        printf("\"temperature_disk_f\":\"%d\",", hdtemperature*9/5+32 );
    }
    else {
        printf("\"temperature_disk_c\":\" \"," );
        printf("\"temperature_disk_f\":\" \"," );
    }    
    
    printf("\"objname\":\"status_value\"}\r\n" );

    statfile = fopen( "savedstat", "wb");
    if( statfile ) {
        fwrite( &stat, sizeof(stat), 1, statfile );
        fclose( statfile );
    }
    
}
 

int main()
{
    // printf headers
    printf( "HTTP/1.1 200 OK\r\n" );
    printf( "Content-Type: text/plain\r\n" );
    printf( "Cache-Control: no-cache\r\n" );        // no cache on cgi contents
    printf( "\r\n" );
    print_status();
    return 0;
}
