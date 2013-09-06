/* --- Changes ---
 * 08/24/2009 by Harrison
 *   1. don't restart apps if in standby mode 
 *
 * 09/23/2009 by Harrison
 *   1. added more wifi encryptions
 *
 * 09/30/2009 by Harrison
 *   1. added enable_buzzer
 *
 */

#include "../../cfg.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/cfg.h"
#include "../../ioprocess/diomap.h"

char dvrconfigfile[]="/etc/dvr/dvr.conf" ;
char tzfile[] = "tz_option" ;
char * dvrworkdir = "/dav/dvr" ;

pid_t  dvrpid=0 ;

void dvrsvr_down()
{
    dvrpid = 0 ;
    FILE * fdvrpid = fopen( "/var/dvr/dvrsvr.pid", "r");
    if( fdvrpid ) {
        fscanf(fdvrpid, "%d", &dvrpid );
        fclose( fdvrpid );
    }
    if( dvrpid > 0 ) {
        kill( dvrpid, SIGUSR1 );
    }
}

void dvrsvr_up()
{
    if( dvrpid>0 ) {
        kill( dvrpid, SIGUSR2 );
    }
}

char buf[4000] ;

char * getsetvalue( char * name )
{
    static char v[500] ;
    char * h ;
    char * needle ;
    int l = strlen(name);
    int i ;
    h = buf ;
    while( (needle = strstr( h, name ))!=NULL ) {
        h=needle+l+1 ;
        if( *(needle-1)=='\"' && needle[l]=='\"' ) {
            needle=h ;
            // skip white space
            while( *needle<=' ' ) {
                needle++;
            }
            
            if( *needle==':' ) {
                needle++;
                
                for( l=0; l<499; l++ ) {
                    if( needle[l]==',' || needle[l]=='}' || needle[l]==0 ) {
                        v[l]=0 ;
                        break;
                    }
                    else {
                        v[l]=needle[l] ;
                    }
                }
                
                // clean string
                while( l>0 ) {
                    if( v[l-1]<=' ' || v[l-1]=='\"' )  {
                        l--;
                    }
                    else {
                        break;
                    }
                }
                v[l]=0;
                for( i=0; i<l; i++ ) {
                    if( v[i]>' ' && v[i]!='\"' ) 
                        break;
                }
                return &v[i] ;
            }
        }
    }
    return NULL ;
}

// generate a random charactor in [a-zA-Z0-9./]
int randomchar()
{
    int x ;
    FILE * fran ;
    fran = fopen("/dev/urandom", "r");
    x=fgetc(fran)%64 ;
    fclose( fran );
    if( x<26 ) {
        return 'A'+x ;
    }
    else if( x<52 ) {
        return 'a'+x-26 ;
    }
    else if( x<62 ) {
        return '0'+x-52 ;
    }
    else if( x<63 ){
        return '.' ;
    }
    else {
        return '/' ;
    }
}

void setuserpassword()
{
    char * v ;
    char password[100] ;
    char passwdline[200] ;    
    char salt[20] ;
    char * key ;
    int i ;
    FILE * fpasswd;
    password[0]=0;
    v = getsetvalue("password" );
    if( v ) {
        strncpy( password, v, 99 );
        password[99]=0 ;
    }
    else {
        return ;
    }
    
    if( strcmp( password, "********" )==0 ) return ;    // invalid passwd
    
    fpasswd = fopen( "/etc/passwd", "r+");
    if( fpasswd ) {
        fgets(passwdline, sizeof(passwdline), fpasswd); // bypass first line.(root)
        
        // generate passsword line
        strcpy(salt, "$1$12345678$" );
        for( i=3; i<=10; i++ ) {
            salt[i]=randomchar();
        }
        // set user name and password on second line
        key = crypt(password, salt);
        if( key ) {
            fprintf( fpasswd, "admin:%s:101:101:admin:/home/www/:/bin/false", key );
        }
        fclose( fpasswd);
    }
    system("cp /etc/passwd /dav/dvr/passwd");
}

// c64 key should be 400bytes
void fileenckey( char * password, char * c64key ) 
{
    unsigned char filekey256[260] ;
    key_256( password, filekey256 );			// hash password
    bin2c64(filekey256, 256, c64key);				// convert to c64
}

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

int is_standby_mode(char *dvriomap)
{
  int res = 0;
    int fd ;
    char * p ;
    struct dio_mmap * p_dio_mmap ;

    if (!dvriomap)
      return 0;

    fd = open(dvriomap, O_RDWR, S_IRWXU);
    if( fd<=0 ) {
        return 0 ;
    }
    p=(char *)mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd ); // fd no more needed
    if( p==(char *)-1 || p==NULL ) {
        return 0;
    }
    p_dio_mmap = (struct dio_mmap *)p ;
    
    if( p_dio_mmap->iopid<=0 ) {	// IO not started !
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        return 0 ;
    }
    
    if( p_dio_mmap->current_mode == APPMODE_STANDBY ) {
        res=1 ;
    }
    
    munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
    return res ;
}

void getbroadcastaddr(char* ipaddr,char* ipmask)
{
    struct in_addr ip;
    struct in_addr netmask;
    struct in_addr network;
    struct in_addr broadcast; 
    char* ipbroadcast;
    inet_aton(ipaddr, &ip);
    inet_aton(ipmask, &netmask);  
    network.s_addr = ip.s_addr & netmask.s_addr;
    broadcast.s_addr = network.s_addr | ~netmask.s_addr;
    sprintf(ipaddr,"%s",inet_ntoa(broadcast));
    return;
}

int main()
{
    int i, r;
    int ivalue ;
    char * v ;
    FILE * fvalue ;
    FILE * ftag;
    char section[20] ;
    int  sensor_number = 31;
    config dvrconfig(dvrconfigfile);
    ftag=fopen( "/var/dvr/system_apply", "r");
    if(ftag){
       fclose(ftag);
       return 0;
    }
    ftag=fopen( "/var/dvr/system_apply", "w");
    if(ftag){
      fclose(ftag);
    }
    // suspend dvrsvr
    dvrsvr_down();

    // set system_value
    fvalue = fopen( "system_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);
        v=getsetvalue("dvr_server_name");
        if( v ) {
            dvrconfig.setvalue( "system", "hostname", v );
        }
        
        // setup login user and password
        setuserpassword();
        
        v = getsetvalue ("dvr_time_zone");
        if( v ) {
            dvrconfig.setvalue( "system", "timezone", v );
        }
        
        v = getsetvalue ("shutdown_delay");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>36000 ) i=36000 ;
            dvrconfig.setvalueint( "system", "shutdowndelay", i );
        }

        v = getsetvalue ("standbytime");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>36000 ) i=36000 ;
            dvrconfig.setvalueint( "system", "standbytime", i );
        }

        
        v = getsetvalue ("file_size");
        if( v ) {
            i=strlen(v);
            v[i]='M' ;
            v[i+1]=0 ;
            dvrconfig.setvalue( "system", "maxfilesize", v );
        }

        v = getsetvalue ("minimun_disk_space");
        if( v ) {
            i=strlen(v);
            v[i]='M' ;
            v[i+1]=0 ;
            dvrconfig.setvalue( "system", "mindiskspace", v );
        }

        v = getsetvalue ("file_time");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>18000 ) i=18000 ;
            dvrconfig.setvalueint( "system", "maxfilelength", i );
        }
        
        // EventMarker
        v = getsetvalue( "EventMarker" );
        if( v ) {
            i=atoi(v);
            dvrconfig.setvalueint( "eventmarker", "eventmarker", i );
        }

        // pre lock time
        v = getsetvalue ("pre_lock_time");
        if( v ) {
            dvrconfig.setvalue( "eventmarker", "prelock", v );
        }
        
        // post lock time
        v = getsetvalue ("post_lock_time");
        if( v ) {
            dvrconfig.setvalue( "eventmarker", "postlock", v );
        }
               
        v = getsetvalue( "norecplayback" );
        if( v ) {
            dvrconfig.setvalueint( "system", "norecplayback", 1 );
        }
        else {
            dvrconfig.setvalueint( "system", "norecplayback", 0 );
        }
        
        v = getsetvalue( "noreclive" );
        if( v ) {
            dvrconfig.setvalueint( "system", "noreclive", 1 );
        }
        else {
            dvrconfig.setvalueint( "system", "noreclive", 0 );
        }
        
        v = getsetvalue ("en_file_encryption");
        if( v ) {
            dvrconfig.setvalueint( "system", "fileencrypt", 1 );
            v = getsetvalue( "file_password" ) ;
            if( v ) {
                if( strcmp( v, "********" )!=0 ) {      // password changed!
                    // do set password
                    char filec64key[400] ;
                    fileenckey( v, filec64key );
                    dvrconfig.setvalue("system", "filepassword", filec64key);	// save password to config file
                }
            }
        }
        else {
            dvrconfig.setvalueint( "system", "fileencrypt", 0 );
        }
        
        // gpslog enable
        v = getsetvalue ("en_gpslog");
        if( v ) {
            dvrconfig.setvalueint( "glog", "gpsdisable", 0 );
        }
        else {
            dvrconfig.setvalueint( "glog", "gpsdisable", 1 );
        }
        //tab102 enable
        v = getsetvalue ("en_tab102");
        if( v ) {
            dvrconfig.setvalueint( "glog", "tab102b_enable", 1 );
        }
        else {
            dvrconfig.setvalueint( "glog", "tab102b_enable", 0 );
        }

        //tab102 show peak
        v = getsetvalue ("tab102_showpeak");
        if( v ) {
            dvrconfig.setvalueint( "glog", "tab102b_showpeak", 1 );
        }
        else {
            dvrconfig.setvalueint( "glog", "tab102b_showpeak", 0 );
        }	
	
        // buzzer enable
        v = getsetvalue ("en_buzzer");
        if( v ) {
            dvrconfig.setvalueint( "io", "buzzer_enable", 1 );
        }
        else {
            dvrconfig.setvalueint( "io", "buzzer_enable", 0 );
        }
       // smart upload enable
        v = getsetvalue ("en_upload");
        if( v ) {
            dvrconfig.setvalueint( "system", "smartftp_disable", 0 );
        }
        else {
            dvrconfig.setvalueint( "system", "smartftp_disable", 1 );
        }
        //enable external wifi

        v = getsetvalue ("en_wifi_ex");
        if( v ) {
            dvrconfig.setvalueint( "system", "ex_wifi_enable", 1);
        }
        else {
            dvrconfig.setvalueint( "system", "ex_wifi_enable", 0 );
        }
        
        //enable HBD recording
        v = getsetvalue ("en_hbd");
        if( v ) {
            dvrconfig.setvalueint( "system", "hbdrecording", 1);
        }
        else {
            dvrconfig.setvalueint( "system", "hbdrecording", 0 );
        }
                
        //screen number
        v= getsetvalue("screen_num");
        if(v){
            dvrconfig.setvalue( "system", "screen_num", v );
        }

        // g-force data
      // g-force data
        v = getsetvalue( "gforce_forward" );
        if( v ) {
            dvrconfig.setvalue(  "io", "gsensor_forward", v );
        }
        v = getsetvalue( "gforce_upward" );
        if( v ) {
            dvrconfig.setvalue(  "io", "gsensor_upward", v );
        }
        
        v = getsetvalue( "gforce_forward_trigger" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_forward_trigger", str );
        }
        v = getsetvalue( "gforce_backward_trigger" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_backward_trigger", str );
        }
        v = getsetvalue( "gforce_right_trigger" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_right_trigger", str );
        }
        v = getsetvalue( "gforce_left_trigger" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_left_trigger", str );
        }
        v = getsetvalue( "gforce_downward_trigger" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 + f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_down_trigger", str );
        }
        v = getsetvalue( "gforce_upward_trigger" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 - f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_up_trigger", str );
        }

        v = getsetvalue( "gforce_forward_base" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_forward_base", str );
        }
        v = getsetvalue( "gforce_backward_base" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_backward_base", str );
        }
        v = getsetvalue( "gforce_right_base" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_right_base", str );
        }
        v = getsetvalue( "gforce_left_base" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_left_base", str );
        }
        v = getsetvalue( "gforce_downward_base" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 + f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_down_base", str );
        }
        v = getsetvalue( "gforce_upward_base" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 - f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_up_base", str );
        }

        v = getsetvalue( "gforce_forward_crash" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_forward_crash", str );
        }
        v = getsetvalue( "gforce_backward_crash" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_backward_crash", str );
        }
        v = getsetvalue( "gforce_right_crash" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_right_crash", str );
        }
        v = getsetvalue( "gforce_left_crash" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_left_crash", str );
        }
        v = getsetvalue( "gforce_downward_crash" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 + f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_down_crash", str );
        }
        v = getsetvalue( "gforce_upward_crash" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 - f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_up_crash", str );
        }

        v = getsetvalue( "gforce_forward_event" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_forward_event", str );
        }
        v = getsetvalue( "gforce_backward_event" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_backward_event", str );
        }
        v = getsetvalue( "gforce_right_event" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_right_event", str );
        }
        v = getsetvalue( "gforce_left_event" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f > 0) f = -f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_left_event", str );
        }
        v = getsetvalue( "gforce_downward_event" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 + f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_down_event", str );
        }
        v = getsetvalue( "gforce_upward_event" );
        if( v ) {
	  char str[256];
	  float f = atof(v);
	  if (f < 0) f = -f;
	  f = 1.0 - f;
	  snprintf(str, sizeof(str), "%g", f);
	  dvrconfig.setvalue(  "io", "gsensor_up_event", str );
        }
    }
    
    // write sensor_value
    fvalue = fopen( "sensor_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);

        v = getsetvalue ("sensor_number");
        if( v ) {
            sscanf(v, "%d", &sensor_number);
            if( sensor_number<0 || sensor_number>32 ) {
                sensor_number=6 ;
            }
            dvrconfig.setvalueint( "io", "inputnum", sensor_number );
        }
        
        for( i=1; i<=sensor_number; i++ ) {
            char sensorname[20], sensorinvert[30] ;
            sprintf( section,      "sensor%d", i );
            sprintf( sensorname,   "sensor%d_name", i );
            sprintf( sensorinvert, "sensor%d_inverted", i );
            
            v = getsetvalue(sensorname);
            if( v ) {
                dvrconfig.setvalue(section, "name", v );
            }
            
            v = getsetvalue(sensorinvert);
            if( v ) {
                dvrconfig.setvalueint(section, "inverted", 1 );
            }
            else {
                dvrconfig.setvalueint(section, "inverted", 0 );
            }
       }
       v=getsetvalue("sensor_powercontrol");
       if(v){
           dvrconfig.setvalueint("io","sensor_powercontrol",1);
       }
       else {
           dvrconfig.setvalueint("io","sensor_powercontrol",0);
       }
    }

    // set camera_value
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");
    if( camera_number<=0 ) {
        camera_number=4;
    }
    
    for( i=1; i<=camera_number; i++ ) {
        char camfilename[20] ;
        sprintf(camfilename, "camera_value_%d", i);
        fvalue=fopen(camfilename, "r");
        if(fvalue){
            r=fread( buf, 1, sizeof(buf), fvalue );
            buf[r]=0;
            fclose(fvalue);
            sprintf(section, "camera%d", i);

            // enable_camera
            v=getsetvalue( "enable_camera" );
            if( v ) {
                dvrconfig.setvalueint(section,"enable",1);
            }
            else {
                dvrconfig.setvalueint(section,"enable",0);
            }

            // camera_name
            v=getsetvalue( "camera_name" );
            if( v ) {
                dvrconfig.setvalue(section,"name",v);
            }

            // recording_mode
            v=getsetvalue( "recording_mode" );
            if( v ) {
                dvrconfig.setvalue(section,"recordmode",v);
            }
            
            // video type
            v=getsetvalue( "videotype" );
            if( v ) {
                dvrconfig.setvalue(section,"videotype",v);
            }
            else {
                dvrconfig.setvalue(section,"videotype","0");    // should disable video type if not available
            }                

            // Camera Type
            v=getsetvalue( "cameratype" );
            if( v ) {
                dvrconfig.setvalue(section,"cameratype",v);
            }
            
            // # resolution, 0:CIF, 1:2CIF, 2:DCIF, 3:4CIF
            // resolution
            v=getsetvalue( "resolution" );
            if( v ) {
                dvrconfig.setvalue(section,"resolution",v);
            }
            
            // frame_rate
            v=getsetvalue( "frame_rate" );
            if( v ) {
                dvrconfig.setvalue(section,"framerate",v);
            }
 
//          # Bitrate control
//          bitrateen=1
//          # Bitrate mode, 0:VBR, 1:CBR
//          bitratemode=0
//          bitrate=1200000
            // bit_rate_mode
            v=getsetvalue( "bit_rate_mode" );
            if( v ) {
                ivalue=atoi(v);
                if( ivalue==0 ) {
                    dvrconfig.setvalueint(section, "bitrateen", 0);
                }
                else {
                    dvrconfig.setvalueint(section, "bitrateen", 1);
                    dvrconfig.setvalueint(section, "bitratemode", ivalue-1);
                }
            }
 
            // bit_rate
            v=getsetvalue( "bit_rate" );
            if( v ) {
                dvrconfig.setvalue(section,"bitrate",v);
            }

//          # picture quality, 0:lowest, 10:highest
            // picture_quaity
            v=getsetvalue( "picture_quaity" );
            if( v ) {
                dvrconfig.setvalue(section,"quality",v);
            }

            // picture controls
            v=getsetvalue( "brightness" );
            if( v ) {
                dvrconfig.setvalue(section,"brightness",v);
            }
            v=getsetvalue( "contrast" );
            if( v ) {
                dvrconfig.setvalue(section,"contrast",v);
            }
            v=getsetvalue( "saturation" );
            if( v ) {
                dvrconfig.setvalue(section,"saturation",v);
            }
            v=getsetvalue( "hue" );
            if( v ) {
                dvrconfig.setvalue(section,"hue",v);
            }
            
            // trigger and osd
            for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
                char trigger[20], osd[20], sensor[20] ;
                int  isensor ;
                sprintf(sensor, "sensor%d", ivalue);
                sprintf(trigger, "trigger%d", ivalue );
                sprintf(osd, "sensorosd%d", ivalue);
                v = getsetvalue(sensor);
                isensor=0;
                if( v ) {
                    sscanf(v, "%d", &isensor);
                }
                if( isensor<=0 ) {      // disable
                    dvrconfig.setvalueint(section, trigger, 0);                    
                    dvrconfig.setvalueint(section, osd, 0);                    
                }
                else {
                    sprintf(sensor, "sensor%d_trigger", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                            dvrconfig.setvalueint(section, trigger, isensor); 
                    }
                    else {
                            dvrconfig.setvalueint(section, trigger, 0);                    
                    }
                    sprintf(sensor, "sensor%d_osd", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                            dvrconfig.setvalueint(section, osd, isensor);                    
                    }
                    else {
                            dvrconfig.setvalueint(section, osd, 0);                    
                    }
                }
            }

            // pre_recording_time
            v=getsetvalue( "pre_recording_time" );
            if( v ) {
                dvrconfig.setvalue(section,"prerecordtime",v);
            }

            // post_recording_time
            v=getsetvalue( "post_recording_time" );
            if( v ) {
                dvrconfig.setvalue(section,"postrecordtime",v);
            }
            
            // show_gps
            v=getsetvalue( "show_gps" );
            if( v ) {
                dvrconfig.setvalueint(section,"showgps",1);
            }
            else {
                dvrconfig.setvalueint(section,"showgps",0);
            }
            
            // gpsunit
            // speed_display
            v=getsetvalue( "speed_display" );
            if( v ) {
                dvrconfig.setvalue(section,"gpsunit",v);
            }
                
            // show_gps_coordinate
            v=getsetvalue( "show_gps_coordinate" );
            if( v ) {
                dvrconfig.setvalueint(section,"showgpslocation",1);
            }
            else {
                dvrconfig.setvalueint(section,"showgpslocation",0);
            }
            
             // record_alarm_mode
            v = getsetvalue( "record_alarm_mode" );
            if( v ) {
                dvrconfig.setvalue( section, "recordalarmen", v);
                dvrconfig.setvalue( section, "recordalarmpattern", v);
            }
            v = getsetvalue("record_alarm_led");
            if( v ) {
                dvrconfig.setvalue( section, "recordalarm", v);
            }

             // video_lost_alarm_mode
            v = getsetvalue( "video_lost_alarm_mode" );
            if( v ) {
                dvrconfig.setvalue( section, "videolostalarmen", v);
                dvrconfig.setvalue( section, "videolostalarmpattern", v);
            }
            v=getsetvalue("video_lost_alarm_led");
            if( v ) {
                dvrconfig.setvalue( section, "videolostalarm", v);
            }

            // motion_alarm_mode
            v = getsetvalue( "motion_alarm_mode" );
            if( v ) {
                dvrconfig.setvalue( section, "motionalarmen", v);
                dvrconfig.setvalue( section, "motionalarmpattern", v);
            }
            v=getsetvalue("motion_alarm_led");
            if( v ) {
                dvrconfig.setvalue( section, "motionalarm", v);
            }
            
            // motion_sensitivity
            v = getsetvalue("motion_sensitivity");
            if( v ) {
                dvrconfig.setvalue( section, "motionsensitivity", v);
            }
            
            // key interval
            v = getsetvalue("key_interval");
            if( v ) {
                dvrconfig.setvalue( section, "key_interval", v);
            }
            
            // key interval
            v = getsetvalue("b_frames");
            if( v ) {
                dvrconfig.setvalue( section, "b_frames", v);
            }
            
            // key interval
            v = getsetvalue("p_frames");
            if( v ) {
                dvrconfig.setvalue( section, "p_frames", v);
            }
            
            // disableaudio
            v = getsetvalue( "disableaudio" );
            if( v ) {
                dvrconfig.setvalueint( section, "disableaudio", 1);
            }
            else {
                dvrconfig.setvalueint( section, "disableaudio", 0);
            }
        }
    }

    // write network_value
    fvalue = fopen( "network_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);
        
        char netname[30] ;
	char ipmask[30];
	char tempmask[30]="255.255.255.0";
        FILE * nfile ;
	
        // eth_mask
        sprintf( netname, "%s/eth_mask", dvrworkdir );
        v=getsetvalue("eth_mask");
        if( v ) {
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
            sprintf(ipmask,"%s",v);
        }
        else {
            remove(netname);
	    sprintf(ipmask,"%s",tempmask);
        } 
        
        // eth_ip
        v=getsetvalue("eth_ip");
        if( v ) {
            sprintf( netname, "%s/eth_ip", dvrworkdir );
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
            getbroadcastaddr(v,ipmask);           
            sprintf( netname, "%s/eth_broadcast", dvrworkdir );
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }	    
        }

        // wifi_mask
        sprintf( netname, "%s/wifi_mask", dvrworkdir );
        v=getsetvalue("wifi_mask");
        if( v ) {
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
            sprintf(ipmask,"%s",v);            
        }
        else {
            remove( netname );
	    sprintf(ipmask,"%s",tempmask);	    
        }
        
        // wifi_ip
        v=getsetvalue("wifi_ip");
        if( v ) {
            sprintf( netname, "%s/wifi_ip", dvrworkdir );
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
            getbroadcastaddr(v,ipmask);   
            sprintf( netname, "%s/wifi_broadcast", dvrworkdir );
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }	    
        }

        // gateway
        sprintf( netname, "%s/gateway_1", dvrworkdir );
        v=getsetvalue("gateway_1");
        if( v ) {
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
        }
        else {
            remove( netname );
        }
   
        // wifi id
        v=getsetvalue("wifi_ip");
        if( v ) {
            sprintf( netname, "%s/wifi_ip", dvrworkdir );
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
        }

        // wifi authentication/encription
        v=getsetvalue("wifi_enc");
        if( v ) {
	  int value;
	    sscanf(v, "%d", &value);
            if( value<0 || value>7 ) {
                value=0 ;
            }
            dvrconfig.setvalueint( "network", "wifi_enc", value );
        }
        
        v=getsetvalue("wifi_power_on");
        if( v ) {
            dvrconfig.setvalueint( "network", "wifi_poweron", 1 );
        }
        else {
            dvrconfig.setvalueint( "network", "wifi_poweron", 0 );
        }


        // wifi_essid
        v = getsetvalue("wifi_essid");
	if( v ) {
	  dvrconfig.setvalue( "network", "wifi_essid", v);
	}

        // wifi_key
        v = getsetvalue("wifi_key");
	if( v ) {
	  dvrconfig.setvalue( "network", "wifi_key", v);
	}
    }

#ifdef POWERCYCLETEST
    // set cycletest_value
    fvalue = fopen( "cycletest_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);

        // cycletest
        v=getsetvalue("cycletest");
        dvrconfig.setvalueint( "debug", "cycletest", (v!=NULL));

        // cyclefile
        v = getsetvalue ("cyclefile");
        if( v ) {
            dvrconfig.setvalue( "debug", "cyclefile", v );
        }
        
        // cycleserver
        v = getsetvalue ("cycleserver");
        if( v ) {
            dvrconfig.setvalue( "debug", "cycleserver", v );
        }
        
        // norecord
        v=getsetvalue("norecord");
        dvrconfig.setvalueint( "system", "norecord", (v!=NULL));
    }
#endif
    
    // save setting
    dvrconfig.save();
    run( "./getsetup", NULL, 0 );
    
    // reset network settings
    sprintf(buf, "%s/setnetwork", dvrworkdir );
    run( buf, NULL, 1 );

    char dvriomap[256] = "/var/dvr/dvriomap" ;
    string iomapfile = dvrconfig.getvalue( "system", "iomapfile");
    if( iomapfile.length()>0 ) {
      strncpy( dvriomap, iomapfile.getstring(), sizeof(dvriomap));
     // if (is_standby_mode(dvriomap))
	//return 0;
    }


    // resume dvrsvr
    dvrsvr_up();
    
    // re-initialize glog
    fvalue = fopen("/var/dvr/glog.pid", "r");
    if( fvalue ) {
        r=0;
        if( fscanf(fvalue, "%d", &r)==1 ) {
            if(r>0) {
                kill( (pid_t)r, SIGUSR2);
            }
        }
        fclose( fvalue );
    }
    
    // re-initialize ioprocess
    fvalue = fopen("/var/dvr/ioprocess.pid", "r");
    if( fvalue ) {
        r=0;
        if( fscanf(fvalue, "%d", &r)==1 ) {
            if(r>0) {
                kill( (pid_t)r, SIGUSR2);
            }
        }
        fclose( fvalue );
    }
    remove("/var/dvr/system_apply");

    return 0;

}

