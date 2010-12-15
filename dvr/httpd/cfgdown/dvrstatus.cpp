
//#include "../../cfg.h"
#include "../../dvrsvr/dvr.h"
#include "../../ioprocess/diomap.h"
#include <sys/mman.h>
#include <sys/ioctl.h>

// #define NETDBG

#ifdef NETDBG

// debugging procedures

int net_addr(char *netname, int port, struct sockad *addr)
{
    struct addrinfo hints;
    struct addrinfo *res;
    char service[20];
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    sprintf(service, "%d", port);
    res = NULL;
    if (getaddrinfo(netname, service, &hints, &res) != 0) {
        return -1;
    }
    addr->addrlen = res->ai_addrlen;
    memcpy(&addr->addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return 0;
}

int msgfd ;

// send out UDP message use msgfd
int net_sendmsg( char * dest, int port, const void * msg, int msgsize )
{
    struct sockad destaddr ;
	if( msgfd==0 ) {
		msgfd = socket( AF_INET, SOCK_DGRAM, 0 ) ;
	}
    net_addr(dest, port, &destaddr);
    return (int)sendto( msgfd, msg, (size_t)msgsize, 0, &(destaddr.addr), destaddr.addrlen );
}

void net_dprint( char * fmt, ... ) 
{
    char msg[1024] ;
    va_list ap ;
    va_start( ap, fmt );
    vsprintf(msg, fmt, ap );
    net_sendmsg( "192.168.152.61", 15333, msg, strlen(msg) );
    va_end( ap );
}
#endif

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
    while( net_recvok(sockfd, 2000000) ) {
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

#ifdef NETDBG
	net_dprint( "getchannelstate: 1.\n"); 
#endif	
	
    port=15111 ;
    fscanf(portfile, "%d", &port);

#ifdef NETDBG
	net_dprint( "getchannelstate: 2. port = %d\n", port); 
#endif		
	
    sockfd = net_connect("127.0.0.1", port);

#ifdef NETDBG
	net_dprint( "getchannelstate: 3. connectted, sockfd = %d\n", sockfd ); 
#endif		
	
	
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

#ifdef NETDBG
	net_dprint( "getchannelstate: 4. state\n" ); 
#endif				
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
	
#ifdef NETDBG
	net_dprint( "getchannelstate: 5. finish\n" ); 
#endif			
    return rch ;
}
 
int memory_usage( int * mem_total, int * mem_free)
{
    char buf[256];
    * mem_total = 0 ;
    * mem_free = 0 ;
    FILE * fproc = fopen("/proc/meminfo", "r");
    if (fproc) {
        while (fgets(buf, 256, fproc)) {
            char header[20] ;
            int  v ;
            if( sscanf( buf, "%19s%d", header, &v )==2 ) {
                if( strcmp( header, "MemTotal:")==0 ) {
                     *mem_total=v ;
                }
                else if( strcmp( header, "MemFree:")==0 ) {
                    * mem_free+=v ;
                }
                else if( strcmp( header, "Inactive:")==0 ) {
                    * mem_free+=v ;
                }
            }
        }
        fclose(fproc);
    }
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
    if( p_dio_mmap ) {
        dio_lock();
        *sys_temp = p_dio_mmap->iotemperature ;
        *disk_temp = p_dio_mmap->hdtemperature ;
        dio_unlock();
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
    struct channelstate cs[16] ;
    struct dvrstat stat ;

    memset( &savedstat, 0, sizeof( savedstat ) );
    statfile = fopen( "savedstat", "rb");
    if( statfile ) {
        fread( &savedstat, sizeof(savedstat), 1, statfile );
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
    for( i=1; i<=chno ; i++ ) 
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

    // information from dio_map
    dio_mmap();
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

    dio_munmap();

    printf("\"objname\":\"status_value\"}\r\n" );

    statfile = fopen( "savedstat", "wb");
    if( statfile ) {
        fwrite( &stat, sizeof(stat), 1, statfile );
        fclose( statfile );
    }
    
}

//  function from getquery
int decode(const char * in, char * out, int osize );
char * getquery( const char * qname );

void check_synctime()
{ 
    char * synctime = getquery("synctime");
    if( synctime ) {
            long long int stime = 0 ;
            sscanf(synctime, "%Ld", &stime );
            if( stime>0 ) {                     // milliseconds since 1970/01/01
                struct timeval tv ;
                tv.tv_sec = stime/1000 ;
                tv.tv_usec = (stime%1000)*1000 ;
                settimeofday( &tv, NULL );

                // kill -USR2 dvrsvr.pid
                FILE * fvalue = fopen( "/var/dvr/dvrsvr.pid", "r" );
                if( fvalue ) {
                    int i=0 ;
                    fscanf(fvalue, "%d", &i) ;
                    fclose( fvalue );
                    if( i> 0 ) {
                        kill( (pid_t)i, SIGUSR2 );      // re-initial dvrsvr
                    }
                }
                
                system( "/davinci/dvr/dvrtime utctomcu > /dev/null" );
                system( "/davinci/dvr/dvrtime utctortc > /dev/null" );
            }
    }
    return ;
}

// return 0: for keep alive
int main()
{
    check_synctime();
	
    // printf headers
    printf( "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-cache\r\n\r\n" );
    print_status();

	return 1;
}
