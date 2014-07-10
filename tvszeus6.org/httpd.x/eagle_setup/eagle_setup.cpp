#include "../../cfg.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <time.h>
#include "../../dvrsvr/dvr.h"
#include "../../ioprocess/diomap.h"
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "../../dvrsvr/crypt.h"

char dvrconfigfile[]="/etc/dvr/dvr.conf" ;
void stoprecording();

int hextoint(int c)
{
    if( c>='0' && c<='9' )
        return c-'0' ;
    if( c>='a' && c<='f' )
        return 10+c-'a' ;
    if( c>='A' && c<='F' )
        return 10+c-'A' ;
    return 0;
}

int decode(const char * in, char * out, int osize )
{
    int i ;
    int o ;
    for(i=0, o=0; o<osize-1; o++ ) {
        if( in[i]=='%' && in[i+1] && in[i+2] ) {
            out[o] = hextoint( in[i+1] ) * 16 + hextoint( in[i+2] );
            i+=3 ;
        }
        else if( in[i]=='+' ) {
            out[o] = ' ' ;
            i++ ;
        }
        else if( in[i]=='&' || in[i]=='\0' ) {
            break ;
        }
        else {
            out[o]=in[i++] ;
        }
    }
    out[o] = 0 ;
    return i;
}

char * getquery( const char * qname )
{
    static char qvalue[1024] ;
    char * query ;
    int l = strlen(qname);
    int x ;
    query = getenv("QUERY_STRING");
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( strncmp(qname, qvalue, l)==0 && qvalue[l]=='=' ) {
            return &qvalue[l+1] ;
        }
        if( query[x]=='\0' ) 
            break;
        else {
            query+=x+1;
        }
    }
    query = getenv("POST_STRING" );
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( strncmp(qname, qvalue, l)==0 && qvalue[l]=='=' ) {
            return &qvalue[l+1] ;
        }
        if( query[x]=='\0' ) 
            break;
        else {
            query+=x+1;
        }
    }
    return NULL ;
}

int savequery( char * valuefile )
{
    char qvalue[1024] ;
    char * qp ;
    char * query ;
    int x ;
    int item=0;
    FILE * sf ;
    sf = fopen( valuefile, "w" ) ;
    if( sf==NULL ) 
        return 0;
    
    // JSON head
    fprintf( sf, "{" );
    
    query = getenv("QUERY_STRING");
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( x>1 ) {
             qp=strchr(qvalue,'=');
            if( qp ) {
                *qp=0 ;
                qp++;
                if( strcmp(qvalue, "page")!=0 ) {
                    fprintf( sf, "\"%s\":\"%s\",", qvalue,  qp );
                    item++;
                }
            }
        }
        if( query[x]=='\0' ) 
            break;
        else {
            query+=x+1;
        }
    }
    query = getenv("POST_STRING" );
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( x>1 ) {
            qp=strchr(qvalue,'=');
            if( qp ) {
                *qp=0 ;
                qp++;
                if( strcmp(qvalue, "page")!=0 ) {
                    fprintf( sf, "\"%s\":\"%s\",", qvalue,  qp );
                    item++;
                }
            }
        }
        if( query[x]=='\0' ) 
            break;
        else {
            query+=x+1;
        }
    }
    fprintf( sf, "\"eobj\":\"\"}" );
    fclose( sf );
    return item ;
}

void system_savevalue()
{
    savequery("system_value");
}

void camera_savevalue()
{
    char * v ;
    char fcam_name[100] ;
    v = getquery( "cameraid" );
    if( v== NULL ) 
        return ;

    sprintf( fcam_name, "camera_value_%s", v );
    savequery( fcam_name );
    
    v = getquery( "nextcameraid" );
    if( v ) {
	// make a sym link to camera_value
        sprintf(fcam_name, "camera_value_%s", v );
        unlink("camera_value");
        symlink(fcam_name, "camera_value");
    }
}

void sensor_savevalue()
{
    savequery("sensor_value");
}

void network_savevalue()
{
    savequery( "network_value" );
}

#ifdef POWERCYCLETEST         
void cycletest_savevalue()
{
    savequery( "cycletest_value" );
}
#endif
            

#include "../../dvrsvr/dvr.h"

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


int net_connect(char *netname, int port)
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

void run( char * cmd, char * argu, int background ) 
{
    pid_t child = fork();
    if( child==0 ) {        // child process
        execlp( cmd, cmd, argu, NULL );
    }
    else {
        if( background==0 && child>0 ) {        // 
            waitpid( child, NULL, 0 );
        }
    }
}

// get channel status
//    return value: totol channels of status returned.
int getchannelstate(struct channelstate * chst, int ch)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    int sockfd ;
    FILE * portfile ;
    int port ;
    int rch = 0;
    portfile = fopen("dvrsvr_port", "r");
    if( portfile==NULL ) {
        return rch;
    }
    port=15112 ;
    fscanf(portfile, "%d", &port);
    sockfd = net_connect ("127.0.0.1", port);
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
        close( sockfd );        
    }
    return rch ;
}

#if 0
int test_mcucdc()
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );              // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return 0;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
        p_dio_mmap->mcu_cdctest=1;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        return 1 ;
    }
    return 0 ;
}


int test_mcuflash()
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );              // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return 0;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
        p_dio_mmap->mcu_flashtest=1;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        return 1 ;
    }
    return 0 ;
}
int test_mcugforce()
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );              // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return 0;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
        p_dio_mmap->gforce_test=1;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        return 1 ;
    }
    return 0 ;
}
#endif
// generate status page
void setrtc()
{
    struct timeval tv ;
    struct tm * ptm ;
    int res=0;   
    gettimeofday( &tv, NULL );
    ptm = gmtime( &tv.tv_sec ); 

    int hrtc = open("/dev/rtc", O_WRONLY );
    if( hrtc>0 ) {
        if( ioctl( hrtc, RTC_SET_TIME, ptm )==0 ) {
            res=1;
        }
        close( hrtc );
    }
    return;   
}

void syncMcuSet()
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );              // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
        p_dio_mmap->synctimestart=1;
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    }
    return;  
}

void status_page()
{
    char * page ;
    char * synctime ;
    int    i;
    FILE * fvalue ;
    char * test_gforce;
    char * test_flash;
    char * test_cdc;
    page = getquery("page");

    if( page && strcmp(page,"status")==0) {     // originate from status page ?
        synctime = getquery("synctime");
        if( synctime ) {
            long long int stime = 0 ;
            sscanf(synctime, "%Ld", &stime );
            if( stime>0 ) {                     // milliseconds since 1970/01/01
                struct timeval tv ;
		struct timeval timenow;
		int diff;
                tv.tv_sec = stime/1000 ;
                tv.tv_usec = (stime%1000)*1000 ;
		
		gettimeofday(&timenow, NULL);
		diff=tv.tv_sec-timenow.tv_sec;
	
		if(abs(diff)>1){  
		  syncMcuSet();
		  settimeofday( &tv, NULL );		  
		  //stoprecording();
		  // kill -USR2 dvrsvr.pid
		  fvalue = fopen( "/var/dvr/dvrsvr.pid", "r" );
		  if( fvalue ) {
		      i=0 ;
		      fscanf(fvalue, "%d", &i) ;
		      fclose( fvalue );
		      if( i> 0 ) {
			  kill( (pid_t)i, SIGUSR2 );      // re-initial dvrsvr
		      }
		  }
		  system( "/mnt/nand/dvr/dvrtime utctomcu > /dev/null" );
		  system( "/mnt/nand/dvr/dvrtime utctortc > /dev/null" );
	       }
            }
        }
#if 0        
        test_flash=getquery("test_flash");
        if(test_flash){
           test_mcuflash();
        }
        test_gforce=getquery("test_gforce");
        if(test_gforce){
           test_mcugforce();
        }
        test_cdc=getquery("test_cdc");
        if(test_cdc){
           test_mcucdc();
        }
#endif        
    }
}
 
void apply_page()
{
    system("./applysetup 2> /dev/null ");
}

#define CFGTAG (0x5f7da992)

struct chunkheader {
    unsigned int tag ;
    int filenamesize ;
    int filesize ;
    int filemode ;
} ;

void cfgupload_page()
{
    int i ;
    int d ;
    struct chunkheader chd ;
    char * cfgfilename ;
    FILE * cfgfile ;
    FILE * fp ;
    FILE * msgfile ;
    int   conf_ok=0;
    char filename[256] ;
    cfgfilename = getenv( "POST_FILE_cfgupload_file" );
    if( cfgfilename==NULL )
        return ;
    cfgfile = fopen( cfgfilename, "r" ) ;
    if( cfgfile ) {
        while( fread(&chd, 1, sizeof(chd), cfgfile )==sizeof(chd) ) {
            if( chd.tag==CFGTAG && chd.filenamesize<255 ) {
                fread( filename, 1, chd.filenamesize, cfgfile );
                filename[chd.filenamesize]=0 ;
                fp = fopen(filename,"w");
                if( fp ) {
                    for( i=0; i<chd.filesize; i++ ) {
                        d=fgetc(cfgfile);
                        if( d==EOF ) break;
                        fputc(d, fp);
                    }
                    fclose( fp );
		    conf_ok=1;
                }                
            }
            else 
                break;
        }
    }
    msgfile=fopen("config_msg", "w");
    if(msgfile){
      if( conf_ok ) {
            fprintf( msgfile, "configure file upload succeed!" );
        }
        else {
            fprintf( msgfile, "configure file upload failed!" );
        }
        fclose(msgfile);      
    }
}

struct file_head {
    uint tag ;
    uint filesize ;
    uint filemode ;
    uint namesize ;
    uint compsize ;
} ;

const uint extract_tag = 0xed3abd05 ;

void stoprecording()
{
    int fd ;
    void * p ;
    struct dio_mmap * p_dio_mmap ;
    
    fd = open("/var/dvr/dvriomap",O_RDWR );
    if( fd>0 ) {
        p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
        close( fd );                         // don't need fd to use memory map.
        if( p==(void *)-1 || p==NULL ) {
            return;
        }
        p_dio_mmap = (struct dio_mmap *)p ;
        p_dio_mmap->dvrcmd = 3; // stop recording
        sleep(5);
    }
    return;
}

// firware uploading page
void firmwareupload_page()
{
    char * firmwarefilename ;
    FILE * firmwarefile ;
    FILE * firmwaremsgfile ;
    int firmwareok = 0 ;
    firmwarefilename = getenv( "POST_FILE_firmware_file" );
    if( firmwarefilename==NULL )
        return ;
    firmwarefile = fopen( firmwarefilename, "r" ) ;
    if( firmwarefile ) {
        // check if it is a firmware file?
        int executesize = -1 ;
        fseek( firmwarefile, -16, SEEK_END );
        if( fscanf( firmwarefile, "%d", &executesize )==1 ) {
            if( executesize<(int)ftell(firmwarefile) && executesize>0 ) {
                struct file_head fhd ;
                char filename[256] ;

                fseek( firmwarefile, executesize, SEEK_SET );
                while( fread( &fhd, 1, sizeof( fhd), firmwarefile ) == sizeof( fhd ) ) {
                    if( fhd.tag !=extract_tag ) {
                        break;
                    }
                    if( fhd.namesize<=0 || fhd.namesize>=sizeof(filename) ) {
                        break;
                    }
                    fread( filename, 1, fhd.namesize, firmwarefile);
                    filename[fhd.namesize]=0;
                    fseek(firmwarefile, fhd.compsize, SEEK_CUR );
                    if( S_ISREG(fhd.filemode) ) {
                        if( strstr( filename, "firmwareid" ) ) {
                            firmwareok=1 ;
                            break;
                        }
                    }
                }
            }
        } 
        
        fclose( firmwarefile );
    }
    firmwaremsgfile = fopen("firmware_msg", "w");
    if( firmwaremsgfile ) {
        if( firmwareok ) {
            fprintf( firmwaremsgfile, "please wait DVR to reboot itself." );
        }
        else {
            fprintf( firmwaremsgfile, "Wrong firmware file, please press back button to try again!" );
        }
        fclose( firmwaremsgfile );
    }

    if( firmwareok ) {
        // install the firmware
        char cmd[256] ;
        mkdir( "firmware_x", 0777 );
        sprintf( cmd, "cp %s firmware_x/fw", firmwarefilename );
        fflush(stdout);
        system( cmd );
        if( fork()==0 ) {
            // disable stdin , stdout
            int fd = open("/dev/null", O_RDWR );
            dup2(fd, 0);    // set dummy stdin stdout, also close old stdin (socket)
            dup2(fd, 1);
            dup2(fd, 2);

            // if dvrsvr running, kill it
            FILE * dvrpidfile ;
            pid_t dvrpid ;
            dvrpidfile=fopen("/var/dvr/dvrsvr.pid", "r");
            if( dvrpidfile ) {
                dvrpid=0 ;
                fscanf(dvrpidfile, "%d", &dvrpid);
                fclose( dvrpidfile );
                if( dvrpid>0 ) {
                   stoprecording();
                  // kill( dvrpid, SIGUSR1 );
                   kill( dvrpid, SIGTERM );
                }
            }

            // update firmware
            chmod( "firmware_x/fw", 0755 );
            system( "mkdir upfirmware; cd upfirmware; ../firmware_x/fw ;sync ; sleep 2" );
            exit(0);
        }
    }
}

void tab102firmwareupload_page()
{
    char * tab102_firmwarefilename ;
    FILE * tab102_firmwaremsgfile ;
    FILE * firmwarefile ;
    char mBuf[20];   
    pid_t  tab102_upd_pid ;

    int  res = 0 ;
    tab102_firmwaremsgfile = fopen("tab102_firmware_msg", "w");
    tab102_firmwarefilename = getenv( "POST_FILE_tab102b_firmware_file" );
    if( tab102_firmwarefilename==NULL ) {
        fprintf( tab102_firmwaremsgfile, "Please check the tab102b firmware file! " );
        fclose( tab102_firmwaremsgfile );
        return;
    }
    firmwarefile = fopen( tab102_firmwarefilename, "r" ) ;
    if(!firmwarefile){
        fprintf( tab102_firmwaremsgfile, "Please check the tab102b firmware file! " );
        fclose( tab102_firmwaremsgfile );
        return;      
    }
    fread(&mBuf[0],1,20,firmwarefile);
    if(mBuf[0]!=0x3a||mBuf[7]!=0x30||mBuf[8]!=0x30){
        fprintf( tab102_firmwaremsgfile, "Please check the tab102b firmware file! " );
        fclose( tab102_firmwaremsgfile );
	fclose(firmwarefile);
	return;
    }
    fclose(firmwarefile);
    
    // install MCU firmware
    fflush(stdout);
    tab102_upd_pid = fork() ;
    if( tab102_upd_pid == 0 ) {
        // disable stdin , stdout
        int fd = open("/dev/null", O_RDWR );
        dup2(fd, 0);       // set dummy stdin stdout, also close old stdin (socket)
        dup2(fd, 1);
        dup2(fd, 2);
        
        // if dvrsvr running, kill it
        FILE * dvrpidfile ;
        pid_t dvrpid ;
        dvrpidfile=fopen("/var/dvr/dvrsvr.pid", "r");
        if( dvrpidfile ) {
            dvrpid=0 ;
            fscanf(dvrpidfile, "%d", &dvrpid);
            fclose( dvrpidfile );
            if( dvrpid>0 ) {
               stoprecording();
               kill( dvrpid, SIGUSR1 );
            }
        }
        dvrpidfile=fopen("/var/dvr/tab102.pid", "r");
        if( dvrpidfile ) {
            dvrpid=0 ;
            fscanf(dvrpidfile, "%d", &dvrpid);
            fclose( dvrpidfile );
            if( dvrpid>0 ) {
               kill( dvrpid, SIGTERM);
            }
        }      
        // updating MCU firmware
        execlp("/mnt/nand/dvr/tab102", "tab102", "-fw", tab102_firmwarefilename, NULL );
        exit(0);
    }
    else if( tab102_upd_pid>0 ) {
        int status=-1 ;
        waitpid( tab102_upd_pid, &status, 0 ) ;
        if( WIFEXITED(status) && WEXITSTATUS(status)==0 ) {
            res=1 ;
        }
    }
    if( res ) {
        fprintf( tab102_firmwaremsgfile, "tab102 firmware update succeed." );
        fflush(stdout);
    }
    else {
        fprintf( tab102_firmwaremsgfile, "tab102 firmware update failed!" );
    }
    fclose( tab102_firmwaremsgfile );
}

// mcu firware uploading page
void mcu_firmwareupload_page()
{
    char * mcu_firmwarefilename ;
    FILE * mcu_firmwaremsgfile ;
    FILE * firmwarefile ;
    char mBuf[20];    
    pid_t  mcu_upd_pid ;

    int res = 0 ;
    mcu_firmwaremsgfile = fopen("mcu_firmware_msg", "w");

    mcu_firmwarefilename = getenv( "POST_FILE_mcu_firmware_file" );
    if( mcu_firmwarefilename==NULL ) {
        fprintf( mcu_firmwaremsgfile, "MCU firmware update failed!" );
        fclose( mcu_firmwaremsgfile );
        return;
    } 

    firmwarefile = fopen( mcu_firmwarefilename, "r" ) ;
    if(!firmwarefile){
        fprintf( mcu_firmwaremsgfile, "Wrong MCU firmware file!" );
        fclose( mcu_firmwaremsgfile );
        return;      
    }
    fread(&mBuf[0],1,20,firmwarefile);
    if(mBuf[0]!=0x3a||mBuf[7]!=0x30||mBuf[8]!=0x30){
        fprintf( mcu_firmwaremsgfile, "Wrong MCU firmware file!" );
        fclose( mcu_firmwaremsgfile ); 
	fclose(firmwarefile);
	return;
    }
    fclose(firmwarefile);
    
    // install MCU firmware
    fflush(stdout);
    mcu_upd_pid = fork() ;
    if( mcu_upd_pid == 0 ) {
        // disable stdin , stdout
        int fd = open("/dev/null", O_RDWR );
        dup2(fd, 0);       // set dummy stdin stdout, also close old stdin (socket)
        dup2(fd, 1);
        dup2(fd, 2);
        
        // if dvrsvr running, kill it
        FILE * dvrpidfile ;
        pid_t dvrpid ;
        dvrpidfile=fopen("/var/dvr/dvrsvr.pid", "r");
        if( dvrpidfile ) {
            dvrpid=0 ;
            fscanf(dvrpidfile, "%d", &dvrpid);
            fclose( dvrpidfile );
            if( dvrpid>0 ) {
               stoprecording();
               kill( dvrpid, SIGUSR1 );
            // kill( dvrpid, SIGTERM );
            }
        }
        // updating MCU firmware
        execlp("/mnt/nand/dvr/ioprocess", "ioprocess", "-fw", mcu_firmwarefilename, NULL );
        exit(0);
    }
    else if( mcu_upd_pid>0 ) {
        int status=-1 ;
        waitpid( mcu_upd_pid, &status, 0 ) ;
        if( WIFEXITED(status) && WEXITSTATUS(status)==0 ) {
            res=1 ;
        }
    }
    if( res ) {
        fprintf( mcu_firmwaremsgfile, "MCU firmware update succeed. <br /> Please un-plug FORCEON cable. <br />System reboot... " );
        fflush(stdout);
        if( fork() == 0 ) {
            sleep(5);
            // disable stdin , stdout
            int fd = open("/dev/null", O_RDWR );
            dup2(fd, 0);     // set dummy stdin stdout, also close old stdin (socket)
            dup2(fd, 1);
            dup2(fd, 2);
            // reboot system
          //  execlp("/bin/reboot", "/bin/reboot", NULL );
            exit(0);
        }
    }
    else {
        fprintf( mcu_firmwaremsgfile, "MCU firmware update failed!" );
    }
    fclose( mcu_firmwaremsgfile );    
}


int main()
{
    // check originated page
    char * page;
/*
    FILE * ftag;
    ftag=fopen( "/var/dvr/system_setup", "r");
    if(ftag){
       fclose(ftag);
       return 0;
    }
    ftag=fopen( "/var/dvr/system_setup", "w");
    if(ftag){
      fclose(ftag);
    }
*/
    page = getquery("page");
    if( page ) {
        if( strcmp(page, "system" )==0 ) {
            system_savevalue();
        }
        else if( strcmp( page, "camera" )==0 ) {
            camera_savevalue();
        }
        else if( strcmp( page, "sensor" )==0 ) {
            sensor_savevalue();
        }
        else if( strcmp( page, "network" )==0 ) {
            network_savevalue();
       }

#ifdef POWERCYCLETEST         
        else if( strcmp( page, "cycletest" )==0 ) {
            cycletest_savevalue();
       }
#endif        
    }
  //  remove("/var/dvr/system_setup");

    page = getenv("REQUEST_URI");
    if( page && strcmp(page, "/status.html")==0 ) {
        status_page();
    }
    else if( page && strcmp(page, "/apply.html")==0 ) {
        apply_page();
    }
    else if( page && strcmp(page, "/cfgupload.html")==0 ) {
        cfgupload_page();
    }
    else if( page && strcmp(page, "/firmware.html")==0 ) {
        firmwareupload_page();
    }
    else if( page && strcmp(page, "/mcufirmware.html")==0 ) {
        mcu_firmwareupload_page();
    }
    else if( page && strcmp(page, "/reboot.html")==0 ) {
        mcu_firmwareupload_page();
    }
    else if(page && strcmp(page,"/tab102firmware.html")==0) {
        tab102firmwareupload_page();
    }
    return 0;
}
