
#include "../../cfg.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/cfg.h"

char dvrconfigfile[]="/etc/dvr/dvr.conf" ;
char tzfile[] = "tz_option" ;
char * dvrworkdir = "/davinci/dvr" ;

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
    system("cp /etc/passwd /davinci/dvr/passwd");
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

int main()
{
    int i, r;
    int ivalue ;
    char * v ;
    FILE * fvalue ;
    char section[20] ;
    int  sensor_number = 31;
    config dvrconfig(dvrconfigfile);
    
    // suspend dvrsvr
    dvrsvr_down();

    // set system_value
    fvalue = fopen( "system_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);

#ifdef  PWII_APP        
        v=getsetvalue("vehicleid");
        if( v ) {
            dvrconfig.setvalue( "system", "id1", v );
            // duplicate first id to hostname
            dvrconfig.setvalue( "system", "hostname", v );
        }

        v=getsetvalue("district");
        if( v ) {
            dvrconfig.setvalue( "system", "id2", v );
        }
        
        v=getsetvalue("unit");
        if( v ) {
            dvrconfig.setvalue( "system", "serial", v );
        }
#endif	// PWII_APP

#ifdef	TVS_APP
        v=getsetvalue("tvs_medallion");
        if( v ) {
            dvrconfig.setvalue( "system", "tvs_medallion", v );
            // duplicate medallion to hostname
            dvrconfig.setvalue( "system", "hostname", v );
        }

        v=getsetvalue("tvs_licenseplate");
        if( v ) {
            dvrconfig.setvalue( "system", "tvs_licenseplate", v );
        }
        
        v=getsetvalue("tvs_ivcs_serial");
        if( v ) {
            dvrconfig.setvalue( "system", "tvs_ivcs_serial", v );
        }
#endif	// TVS_APP
        
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
        
        // pre lock time
        v = getsetvalue ("pre_lock_time");
        if( v ) {
            dvrconfig.setvalue( "system", "prelock", v );
        }
        
        // post lock time
        v = getsetvalue ("post_lock_time");
        if( v ) {
            dvrconfig.setvalue( "system", "postlock", v );
        }

        if( getsetvalue( "bool_norecplayback" )!=NULL ) {
            v = getsetvalue( "norecplayback" );
            if( v ) {
                dvrconfig.setvalueint( "system", "norecplayback", 1 );
            }
            else {
                dvrconfig.setvalueint( "system", "norecplayback", 0 );
            }
        }

        if( getsetvalue( "bool_noreclive" )!=NULL ) {
            v = getsetvalue( "noreclive" );
            if( v ) {
                dvrconfig.setvalueint( "system", "noreclive", 1 );
            }
            else {
                dvrconfig.setvalueint( "system", "noreclive", 0 );
            }
        }
        
        if( getsetvalue( "bool_en_file_encryption" )!=NULL ) {
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
        }

        // gpslog enable
        if( getsetvalue( "bool_en_gpslog" )!=NULL ) {
            v = getsetvalue ("en_gpslog");
            if( v ) {
                dvrconfig.setvalueint( "glog", "gpsdisable", 0 );
            }
            else {
                dvrconfig.setvalueint( "glog", "gpsdisable", 1 );
            }
        }
        
        // gps port
        v = getsetvalue( "gpsport" );
        if( v ) {
            dvrconfig.setvalue( "glog", "serialport", v );
        }

        // gps baud rate
        v = getsetvalue( "gpsbaudrate" );
        if( v ) {
            dvrconfig.setvalue( "glog", "serialbaudrate", v );
        }
        
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
            dvrconfig.setvalue(  "io", "gsensor_forward_trigger", v );
        }
        v = getsetvalue( "gforce_backward_trigger" );
        if( v ) {
            dvrconfig.setvalue(  "io", "gsensor_backward_trigger", v );
        }
        v = getsetvalue( "gforce_right_trigger" );
        if( v ) {
            dvrconfig.setvalue(  "io", "gsensor_right_trigger", v );
        }
        v = getsetvalue( "gforce_left_trigger" );
        if( v ) {
            dvrconfig.setvalue(  "io", "gsensor_left_trigger", v );
        }
        v = getsetvalue( "gforce_downward_trigger" );
        if( v ) {
            dvrconfig.setvalue(  "io", "gsensor_down_trigger", v );
        }
        v = getsetvalue( "gforce_upward_trigger" );
        if( v ) {
            dvrconfig.setvalue(  "io", "gsensor_up_trigger", v );
        }

        // Video output selection
        v = getsetvalue( "videoout" );
        if( v ) {
            dvrconfig.setvalue(  "VideoOut", "startchannel", v );
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
            char sensorkey[30] ;
            sprintf( section,      "sensor%d", i );
            
            sprintf( sensorkey,   "sensor%d_name", i );
            v = getsetvalue(sensorkey);
            if( v ) {
                dvrconfig.setvalue(section, "name", v );
            }

            sprintf( sensorkey, "bool_sensor%d_inverted", i );
            if( getsetvalue(sensorkey)!=NULL ) {
                sprintf( sensorkey, "sensor%d_inverted", i );
                v = getsetvalue(sensorkey);
                if( v && strcmp( v, "on" )==0 ) {
                    dvrconfig.setvalueint(section, "inverted", 1 );
                }
                else {
                    dvrconfig.setvalueint(section, "inverted", 0 );
                }
            }

            sprintf( sensorkey, "bool_sensor%d_eventmarker", i );
            if( getsetvalue(sensorkey)!=NULL ) {
                sprintf( sensorkey, "sensor%d_eventmarker", i );
                v = getsetvalue(sensorkey);
                if( v && strcmp( v, "on" )==0 ) {
                    dvrconfig.setvalueint(section, "eventmarker", 1 );
                }
                else {
                    dvrconfig.setvalueint(section, "eventmarker", 0 );
                }
            }
            
        }
    }

    // set camera_value
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");
    if( camera_number<=0 ) {
        camera_number=2 ;
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
            if(getsetvalue( "bool_enable_camera" )!=NULL ) {
                v=getsetvalue( "enable_camera" );
                if( v ) {
                    dvrconfig.setvalueint(section,"enable",1);
                }
                else {
                    dvrconfig.setvalueint(section,"enable",0);
                }
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
                char trigger[30], osd[30], sensor[30] ;
                int  tr;
                sprintf(trigger, "trigger%d", ivalue );
                sprintf(osd, "sensorosd%d", ivalue);

                // osd
                if( getsetvalue( "bool_sensor_osd" )!=NULL ) {
                    sprintf(sensor, "sensor%d_osd", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                        dvrconfig.setvalueint(section, osd, 1);                    
                    }
                    else {
                        dvrconfig.setvalueint(section, osd, 0);
                    }
                }

                // trigger (MDVR)
                if( getsetvalue( "bool_sensor_trigger" )!=NULL ) {
                    sprintf(sensor, "sensor%d_trigger", ivalue );
                    if( getsetvalue( sensor )!=NULL ) {
                        dvrconfig.setvalueint(section, trigger, 1);    
                    }
                    else {
                        dvrconfig.setvalueint(section, trigger, 0);    
                    }
                }

                // trigger selections (PWII / TVS )
                if( getsetvalue( "bool_sensor_trigger_sel" )!=NULL ) {
                    tr=0 ;
                    sprintf(sensor, "sensor%d_trigger_on", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                        tr|=1 ;
                    }
                    sprintf(sensor, "sensor%d_trigger_off", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                        tr|=2 ;
                    }
                    sprintf(sensor, "sensor%d_trigger_turnon", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                        tr|=4 ;
                    }
                    sprintf(sensor, "sensor%d_trigger_turnoff", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                        tr|=8 ;
                    }

                    dvrconfig.setvalueint(section, trigger, tr);    
                
                    /* 
                     sprintf(sensor, "trigger%d_prerec", ivalue );
                     v=getsetvalue( sensor ) ;
                     if( v ) {
                         dvrconfig.setvalue(section, sensor, v);       
                     }

                     sprintf(sensor, "trigger%d_postrec", ivalue );
                     v=getsetvalue( sensor ) ;
                     if( v ) {
                         dvrconfig.setvalue(section, sensor, v);       
                     }
                     */
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
            if( getsetvalue( "bool_show_gps" )!=NULL ) {
                v=getsetvalue( "show_gps" );
                if( v ) {
                    dvrconfig.setvalueint(section,"showgps",1);
                }
                else {
                    dvrconfig.setvalueint(section,"showgps",0);
                }
            }
            
            // gpsunit
            // speed_display
            v=getsetvalue( "speed_display" );
            if( v ) {
                dvrconfig.setvalue(section,"gpsunit",v);
            }

            // show_gps_coordinate
            if( getsetvalue( "bool_show_gps_coordinate" )!=NULL ) {
                v=getsetvalue( "show_gps_coordinate" );
                if( v ) {
                    dvrconfig.setvalueint(section,"showgpslocation",1);
                }
                else {
                    dvrconfig.setvalueint(section,"showgpslocation",0);
                }
            }

            // show_vri
            if( getsetvalue( "bool_show_vri" )!=NULL ) {
                v=getsetvalue( "show_vri" );
                if( v ) {
                    dvrconfig.setvalueint(section,"show_vri",1);
                }
                else {
                    dvrconfig.setvalueint(section,"show_vri",0);
                }
            }

            // show_policeid
            if( getsetvalue( "bool_show_policeid" )!=NULL ) {
                v=getsetvalue( "show_policeid" );
                if( v ) {
                    dvrconfig.setvalueint(section,"show_policeid",1);
                }
                else {
                    dvrconfig.setvalueint(section,"show_policeid",0);
                }
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
            if( getsetvalue( "bool_disableaudio" )!=NULL ) {
                v = getsetvalue( "disableaudio" );
                if( v ) {
                    dvrconfig.setvalueint( section, "disableaudio", 1);
                }
                else {
                    dvrconfig.setvalueint( section, "disableaudio", 0);
                }
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
        FILE * nfile ;
        
        // eth_ip
        v=getsetvalue("eth_ip");
        if( v ) {
            sprintf( netname, "%s/eth_ip", dvrworkdir );
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
        }

        // eth_mask
        sprintf( netname, "%s/eth_mask", dvrworkdir );
        v=getsetvalue("eth_mask");
        if( v ) {
            nfile = fopen( netname, "w");
            if( nfile ) {
                fprintf(nfile, "%s", v);
                fclose( nfile );
            }
        }
        else {
            remove(netname);
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
        }
        else {
            remove( netname );
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

        // wifi_essid
        v = getsetvalue("wifi_essid");
        if( v ) {
            char essid[128] ;
            sprintf( netname, "%s/wifi_id", dvrworkdir );
            nfile = fopen( netname, "w");
            if( nfile ) {
                strncpy( essid, v, 128 );
                v = getsetvalue( "wifi_key" ) ;
                if( v && strlen(v)>0 ) {
                    fprintf(nfile, " essid %s enc %s", essid, v );
                }
                else {
                    fprintf(nfile, " essid %s ", essid );
                }
                fclose( nfile );
            }
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
        if( getsetvalue( "bool_cycletest" ) {
            v=getsetvalue("cycletest");
            dvrconfig.setvalueint( "debug", "cycletest", (v!=NULL));
        }
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
        if( getsetvalue( "bool_norecord" ) {
            v=getsetvalue("norecord");
            dvrconfig.setvalueint( "system", "norecord", (v!=NULL));
        }
    }
#endif
    
    // save setting
    sync();
    dvrconfig.save();
    sync();
    run( "./getsetup", NULL, 0 );
    
    // reset network settings
    sprintf(buf, "%s/setnetwork", dvrworkdir );
    run( buf, NULL, 1 );

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
   
    return 0;

}

