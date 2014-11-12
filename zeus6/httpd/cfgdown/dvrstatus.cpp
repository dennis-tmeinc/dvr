
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "../../cfg.h"
#include "../../dvrsvr/dvr.h"
#include "../../ioprocess/diomap.h"

// #define NETDBG

#ifdef NETDBG

int net_addr(char *netname, int port, struct sockad *addr);

// debugging procedures
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

class dir_find {
    protected:
        DIR * m_pdir ;
        struct dirent * m_pent ;
        struct dirent m_entry ;
        char m_dirname[PATH_MAX] ;
        char m_pathname[PATH_MAX] ;
        struct stat m_statbuf ;
    public:
        dir_find() {
            m_pdir=NULL ;
        }
        // close dir handle
        void close() {
            if( m_pdir ) {
                closedir( m_pdir );
                m_pdir=NULL ;
            }
        }
        // open an dir for reading
        void open( const char * path ) {
            int l ;
            close();
            m_pent=NULL ;
            strncpy( m_dirname, path, sizeof(m_dirname) );
            l=strlen( m_dirname ) ;
            if( l>0 ) {
                if( m_dirname[l-1]!='/' ) {
                    m_dirname[l]='/' ;
                    m_dirname[l+1]='\0';
                }
            }
            m_pdir = opendir(m_dirname);
        }
        ~dir_find() {
            close();
        }
        dir_find( const char * path ) {
            m_pdir=NULL;
            open( path );
        }
        int isopen(){
            return m_pdir!=NULL ;
        }
        // find directory.
        // return 1: success
        //        0: end of file. (or error)
        int find() {
            struct stat findstat ;
            if( m_pdir ) {
                while( readdir_r( m_pdir, &m_entry, &m_pent)==0  ) {
                    if( m_pent==NULL ) {
                        break;
                    }
                    if( (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='\0') || 
                       (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='.' && m_pent->d_name[2]=='\0') ) {
                           continue ;
                       }
                    if( m_pent->d_type == DT_UNKNOWN ) { // d_type not available
                        strcpy( m_pathname, m_dirname );
                        strcat( m_pathname, m_pent->d_name );
                        if( stat( m_pathname, &findstat )==0 ) {
                            if( S_ISDIR(findstat.st_mode) ) {
                                m_pent->d_type = DT_DIR ;
                            }
                            else if( S_ISREG(findstat.st_mode) ) {
                                m_pent->d_type = DT_REG ;
                            }
                        }
                    }
                    return 1 ;
                }
            }
            m_pent=NULL ;
            return 0 ;
        }
        char * pathname()  {
            if(m_pent) {
                strcpy( m_pathname, m_dirname );
                strcat( m_pathname, m_pent->d_name );
                return (m_pathname) ;
            }
            return NULL ;
        }
        char * filename() {
            if(m_pent) {
                return (m_pent->d_name) ;
            }
            return NULL ;
        }
        
        // check if found a dir
        int    isdir() {
            if( m_pent ) {
                return (m_pent->d_type == DT_DIR) ;
            }
            else {
                return 0;
            }
        }
        
        // check if found a regular file
        int    isfile(){
            if( m_pent ) {
                return (m_pent->d_type == DT_REG) ;
            }
            else {
                return 0;
            }
        }
        
        // return file stat
        struct stat * filestat(){
            char * path = pathname();
            if( path ) {
                if( stat( path, &m_statbuf )==0 ) {
                    return &m_statbuf ;
                }
            }
            return NULL ;
        }
};


int disk_usage( int * disk_total, int * disk_free)
{
   *disk_free = 0;
   *disk_total = 0;
    dir_find disks(VAR_DIR "/disks");
    while( disks.find() ) {
    	if( disks.isdir() ) {
		    struct statfs stfs;
	       if (statfs(disks.pathname(), &stfs) == 0) {
	            *disk_free += stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
	            *disk_total += stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
	        }	    	
	     }
    }
    return 1 ;
}


int get_temperature( int * sys_temp, int * disk_temp)
{
    if( p_dio_mmap ) {
        dio_lock();
        *sys_temp = p_dio_mmap->iotemperature ;
        *disk_temp = p_dio_mmap->hdtemperature1 ;
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
    if( systemperature>-125 && systemperature<125 )  {
        printf("\"temperature_system_c\":\"%d\",", systemperature );
        printf("\"temperature_system_f\":\"%d\",", systemperature*9/5+32 );
    }
    else {
        printf("\"temperature_system_c\":\"\"," );
        printf("\"temperature_system_f\":\"\"," );
    }
    if( hdtemperature>-125 && hdtemperature<125 )  {
        printf("\"temperature_disk_c\":\"%d\",", hdtemperature );
        printf("\"temperature_disk_f\":\"%d\",", hdtemperature*9/5+32 );
    }
    else {
        printf("\"temperature_disk_c\":\"\"," );
        printf("\"temperature_disk_f\":\"\"," );
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
                FILE * fvalue = fopen( VAR_DIR "/dvrsvr.pid", "r" );
                if( fvalue ) {
                    int i=0 ;
                    fscanf(fvalue, "%d", &i) ;
                    fclose( fvalue );
                    if( i> 0 ) {
                        kill( (pid_t)i, SIGUSR2 );      // re-initial dvrsvr
                    }
                }

                system( APP_DIR "/dvrtime utctomcu > /dev/null" );
                system( APP_DIR "/dvrtime utctortc > /dev/null" );
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
