/* --- Changes ---
 *
 * 09/23/2009 by Harrison
 *   1. added more wifi encryptions
 *
 * 09/30/2009 by Harrison
 *   1. added enable_buzzer
 *
 */

#include "../../cfg.h"

#include <stdio.h>

#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/cfg.h"

char dvrconfigfile[]="/etc/dvr/dvr.conf" ;
char tzfile[] = "tz_option" ;
char * dvrworkdir = "/dav/dvr" ;

int main()
{
    char * zoneinfobuf ;
    string s ;
    char * p ;
    int l;
    int i;
    config_enum enumkey ;
    config dvrconfig(dvrconfigfile);
    string tzi ;
    string value ;
    int ivalue ;
    char buf[100] ;
    FILE * fvalue ;
    FILE * f_id ;
    int sensor_number ;
    
    // sensor number
    sensor_number=dvrconfig.getvalueint("io", "inputnum");
    if( sensor_number<=0 || sensor_number>32 ) {
        sensor_number=6 ;                   // default
    }
   
    fvalue = fopen("sensor_number", "w");
    if( fvalue ) {
        fprintf(fvalue, "%d", sensor_number );
        fclose( fvalue );
    }
    
    // write system_value
    fvalue = fopen( "system_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

        // dvr_server_name
        value = dvrconfig.getvalue("system","hostname");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"dvr_server_name\":\"%s\",", value.getstring() );
        }
        
        // dummy password
        fprintf(fvalue, "\"password\":\"********\",");
      
        // dvr_time_zone
        value = dvrconfig.getvalue("system", "timezone");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"dvr_time_zone\":\"%s\",", value.getstring() );
            FILE * ftimezone ;
            ftimezone = fopen("timezone", "w");
            if( ftimezone ) {
                fprintf(ftimezone, "%s", value.getstring() );
                fclose( ftimezone );
            }

            ftimezone = fopen("tz_env", "w");
            if( ftimezone ) {

                // time zone enviroment
                tzi=dvrconfig.getvalue( "timezones", value.getstring() );
                if( tzi.length()>0 ) {
                    p=strchr(tzi.getstring(), ' ' ) ;
                    if( p ) {
                        *p=0;
                    }
                    p=strchr(tzi.getstring(), '\t' ) ;
                    if( p ) {
                        *p=0;
                    }
                    fprintf(ftimezone, "%s", tzi.getstring() );
                }
                else {
                    fprintf(ftimezone, "%s", value.getstring() );
                }
                fclose( ftimezone );
            }
        }

        // shutdown_delay
        value = dvrconfig.getvalue("system", "shutdowndelay");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"shutdown_delay\":\"%s\",", value.getstring() );
        }

        // standbytime
        value = dvrconfig.getvalue("system", "standbytime");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"standbytime\":\"%s\",", value.getstring() );
        }

        
        // file_size
        value = dvrconfig.getvalue("system", "maxfilesize");
        if( value.length()>0 ) {
            l=value.length();
            p=value.getstring();
            if( p[l-1]=='M' ) {         // use Mega bytes only
                p[l-1]=0;
            }
            fprintf(fvalue, "\"file_size\":\"%s\",", p );
        }
        
        // file_time
        value = dvrconfig.getvalue("system", "maxfilelength");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"file_time\":\"%s\",", value.getstring() );
        }
        
        // minimun_disk_space
        value = dvrconfig.getvalue("system", "mindiskspace");
        if( value.length()>0 ) {
            l=value.length();
            p=value.getstring();
            if( p[l-1]=='M' ) {         // use Mega bytes only
                p[l-1]=0;
            }
            fprintf(fvalue, "\"minimun_disk_space\":\"%s\",", p );
        }

        // EventMarker
        ivalue = dvrconfig.getvalueint("eventmarker", "eventmarker" );
        fprintf(fvalue, "\"EventMarker\":\"%d\",", ivalue ) ;


        // Max file length
        value = dvrconfig.getvalue("system", "maxfilelength");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"file_time\":\"%s\",", value.getstring() );
        }
        
        // pre_lock_time
        value = dvrconfig.getvalue("eventmarker", "prelock");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"pre_lock_time\":\"%s\",", value.getstring() );
        }
        
        // post_lock_time
        value = dvrconfig.getvalue("eventmarker", "postlock");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"post_lock_time\":\"%s\",", value.getstring() );
        }
        
        // no rec playback
        ivalue = dvrconfig.getvalueint("system", "norecplayback");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"norecplayback\":\"on\"," );
        }

        // no rec liveview
        ivalue = dvrconfig.getvalueint("system", "noreclive");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"noreclive\":\"on\"," );
        }

        
        // en_file_encryption
        int file_encrypt = dvrconfig.getvalueint("system", "fileencrypt");
        if( file_encrypt>0 ) {
            fprintf(fvalue, "\"en_file_encryption\":\"on\"," );
        }
        
        fprintf( fvalue, "\"file_password\":\"********\",");

        
        // gpslog enable
        ivalue = dvrconfig.getvalueint("glog", "gpsdisable");
        if( ivalue == 0 ) {
            fprintf(fvalue, "\"en_gpslog\":\"on\",");
        }
        //tab102 enable
        ivalue = dvrconfig.getvalueint( "glog", "tab102b_enable");
        if( ivalue == 1 ) {
            fprintf(fvalue, "\"en_tab102\":\"on\",");
        }
        
        //tab102 show peak
        ivalue = dvrconfig.getvalueint( "glog", "tab102b_showpeak");
        if( ivalue == 1 ) {
            fprintf(fvalue, "\"tab102_showpeak\":\"on\",");
        }	
        // buzzer enable
        ivalue = dvrconfig.getvalueint("io", "buzzer_enable");
        if( ivalue == 1 ) {
            fprintf(fvalue, "\"en_buzzer\":\"on\",");
        }
        
        //screen number
        value = dvrconfig.getvalue("system", "screen_num");
        if( value.length()>0 ) {
              fprintf(fvalue, "\"screen_num\":\"%s\",", value.getstring() );
        }            

       // smart upload enable
        ivalue = dvrconfig.getvalueint("system", "smartftp_disable");
        if( ivalue == 0 ) {
            fprintf(fvalue, "\"en_upload\":\"on\",");
        }

        //enable external wifi
        ivalue = dvrconfig.getvalueint("system", "ex_wifi_enable");
        if( ivalue == 1 ) {
            fprintf(fvalue, "\"en_wifi_ex\":\"on\",");
        }
        
        //enable HBD recording
        ivalue = dvrconfig.getvalueint("system", "hbdrecording");
        if( ivalue == 1 ) {
            fprintf(fvalue, "\"en_hbd\":\"on\",");
        }        
        // g-force data
        // 

         value = dvrconfig.getvalue( "io", "gsensor_forward");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_forward\":\"%s\",", value.getstring() );
        }
        
        value = dvrconfig.getvalue( "io", "gsensor_upward");	
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_upward\":\"%s\",", value.getstring() );
        }

        value = dvrconfig.getvalue( "io", "gsensor_forward_trigger");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_forward_trigger\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_trigger");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_backward_trigger\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_trigger");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_right_trigger\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_trigger");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_left_trigger\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_trigger");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 1.0) f = 3.0;
	  fprintf(fvalue, "\"gforce_downward_trigger\":\"%g\",", f - 1.0);
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_trigger");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f > 1.0) f = -1.0;
	  fprintf(fvalue, "\"gforce_upward_trigger\":\"%g\",", 1.0 - f);
        }

        value = dvrconfig.getvalue( "io", "gsensor_forward_base");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_forward_base\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_base");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_backward_base\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_base");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_right_base\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_base");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_left_base\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_base");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 1.0) f = 3.0;
	  fprintf(fvalue, "\"gforce_downward_base\":\"%g\",", f - 1.0);
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_base");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f > 1.0) f = -1.0;
	  fprintf(fvalue, "\"gforce_upward_base\":\"%g\",", 1.0 - f);
        }
        
        value = dvrconfig.getvalue( "io", "gsensor_forward_crash");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_forward_crash\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_crash");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_backward_crash\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_crash");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_right_crash\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_crash");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_left_crash\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_crash");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 1.0) f = 5.0;
	  fprintf(fvalue, "\"gforce_downward_crash\":\"%g\",", f - 1.0);
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_crash");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f > 1.0) f = -3.0;
	  fprintf(fvalue, "\"gforce_upward_crash\":\"%g\",", 1.0 - f);
        }
        
        value = dvrconfig.getvalue( "io", "gsensor_forward_event");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_forward_event\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_event");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_backward_event\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_event");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_right_event\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_event");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 0) f = -f;
	  fprintf(fvalue, "\"gforce_left_event\":\"%g\",", f);
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_event");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f < 1.0) f = 5.0;
	  fprintf(fvalue, "\"gforce_downward_event\":\"%g\",", f - 1.0);
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_event");	
        if( value.length()>0 ) {
	  float f = atof(value.getstring());
	  if (f > 1.0) f = -3.0;
	  fprintf(fvalue, "\"gforce_upward_event\":\"%g\",", 1.0 - f);
        }
        
        ivalue=0 ;
        f_id = fopen( "/var/dvr/gsensor", "r" );
        if( f_id ) {
            fscanf( f_id, "%d", &ivalue);
            fclose(f_id);
        }
        if( ivalue ) {
            fprintf( fvalue, "\"gforce_available\":\"1\",");
        }
        else {
            fprintf( fvalue, "\"gforce_available\":\"0\",");
        }
    
        fprintf( fvalue, "\"objname\":\"system_value\" }");
        
        fclose( fvalue );
    }

  
    // write camera_value
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");
    if( camera_number<=0 ) {
        camera_number=4 ;
    }
    fvalue = fopen("camera_number", "w" );
    if( fvalue ) {
        fprintf(fvalue, "%d", camera_number );
        fclose( fvalue );
    }
    
    for( i=1; i<=camera_number; i++ ) {
        sprintf(buf, "camera_value_%d", i);
        fvalue=fopen(buf, "w");
        if(fvalue){
            char section[10] ;
            
            // JSON head
            fprintf(fvalue, "{" );
            
            fprintf(fvalue, "\"cameraid\":\"%d\",", i );
            fprintf(fvalue, "\"nextcameraid\":\"%d\",", i );
            
            sprintf(section, "camera%d", i);
            // enable_camera
            if( dvrconfig.getvalueint(section, "enable") ) {
                fprintf(fvalue, "\"enable_camera\":\"on\"," );
            }
            
            // camera_name
            value = dvrconfig.getvalue(section, "name");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"camera_name\":\"%s\",", value.getstring() );
            }
            
            // recording_mode
            value = dvrconfig.getvalue(section, "recordmode");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"recording_mode\":\"%s\",", value.getstring() );
            }            
            
            // Video Type for Special version 2. (less options)
            ivalue = dvrconfig.getvalueint(section, "videotype");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"videotype\":\"%d\",", ivalue );
                
                // Camera Type
                value = dvrconfig.getvalue(section, "cameratype");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"cameratype\":\"%s\",", value.getstring() );
                }            
                
            }            
            else {
                
                // # resolution, 0:CIF, 1:2CIF, 2:DCIF, 3:4CIF
                // resolution
                value = dvrconfig.getvalue(section, "resolution");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"resolution\":\"%s\",", value.getstring() );
                }            
                
                // frame_rate
                value = dvrconfig.getvalue(section, "framerate");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"frame_rate\":\"%s\",", value.getstring() );
                }            
                
                //          # Bitrate control
                //          bitrateen=1
                //          # Bitrate mode, 0:VBR, 1:CBR
                //          bitratemode=0
                //          bitrate=1200000
                // bit_rate_mode
                if( dvrconfig.getvalueint(section, "bitrateen")) {
                    fprintf(fvalue, "\"bit_rate_mode\":\"%d\",", dvrconfig.getvalueint(section,"bitratemode")+1 );
                }
                else {
                    fprintf(fvalue, "\"bit_rate_mode\":\"0\"," );
                }
                
                // bit_rate
                value = dvrconfig.getvalue(section, "bitrate");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"bit_rate\":\"%s\",", value.getstring() );
                }            
                
                //          # picture quality, 0:lowest, 10:highest
                // picture_quaity
                value = dvrconfig.getvalue(section, "quality");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"picture_quaity\":\"%s\",", value.getstring() );
                }         
            }
            
            // picture controls
            value = dvrconfig.getvalue(section, "brightness");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"brightness\":\"%s\",", value.getstring() );
            }         
            value = dvrconfig.getvalue(section, "contrast");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"contrast\":\"%s\",", value.getstring() );
            }         
            value = dvrconfig.getvalue(section, "saturation");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"saturation\":\"%s\",", value.getstring() );
            }         
            value = dvrconfig.getvalue(section, "hue");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"hue\":\"%s\",", value.getstring() );
            }         
                
            // trigger and osd
            for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
                char trigger[10], osd[10] ;
                int  itrig, iosd ;
                sprintf(trigger, "trigger%d", ivalue );
                sprintf(osd, "sensorosd%d", ivalue);
                itrig = dvrconfig.getvalueint( section, trigger );
                iosd  = dvrconfig.getvalueint( section, osd );
                if( itrig>0 || iosd>0 ) {
                    if( itrig>0 ) {
                        fprintf(fvalue, "\"sensor%d\":\"%d\",", ivalue, itrig );
                        fprintf(fvalue, "\"sensor%d_trigger\":\"on\",", ivalue );
                    }
                    else {
                        fprintf(fvalue, "\"sensor%d\":\"%d\",", ivalue, iosd );
                    }
                    if( iosd>0 ) {
                        fprintf(fvalue, "\"sensor%d_osd\":\"on\",", ivalue );
                    }
                }
                else {
                    fprintf(fvalue, "\"sensor%d\":\"0\",", ivalue );
                }
            }

            // pre_recording_time
            value = dvrconfig.getvalue(section, "prerecordtime");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"pre_recording_time\":\"%s\",", value.getstring() );
            }  

            // post_recording_time
            value = dvrconfig.getvalue(section, "postrecordtime");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"post_recording_time\":\"%s\",", value.getstring() );
            }  

            // show_gps
            ivalue = dvrconfig.getvalueint(section, "showgps");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_gps\":\"on\"," );
            }  
            
            // gpsunit
            // speed_display
            value = dvrconfig.getvalue(section, "gpsunit");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"speed_display\":\"%s\",", value.getstring() );
            }  
    
            ivalue = dvrconfig.getvalueint(section, "showgpslocation");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"show_gps_coordinate\":\"on\"," );
            }  
            
             // record_alarm_mode
            value = dvrconfig.getvalue( section, "recordalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"record_alarm_mode\":\"%s\",", value.getstring() );
            }  
            value = dvrconfig.getvalue( section, "recordalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"record_alarm_led\":\"%s\",", value.getstring() );
            }  
            
             // video_lost_alarm_mode
            value = dvrconfig.getvalue( section, "videolostalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"video_lost_alarm_mode\":\"%s\",", value.getstring() );
            }  
            value = dvrconfig.getvalue( section, "videolostalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"video_lost_alarm_led\":\"%s\",", value.getstring() );
            }  
            
            // motion_alarm_mode
            value = dvrconfig.getvalue( section, "motionalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_alarm_mode\":\"%s\",", value.getstring() );
            }  
            value = dvrconfig.getvalue( section, "motionalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_alarm_led\":\"%s\",", value.getstring() );
            }  

            // motion_sensitivity
            value = dvrconfig.getvalue( section, "motionsensitivity" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_sensitivity\":\"%s\",", value.getstring() );
            }  
            
            // disableaudio
            ivalue = dvrconfig.getvalueint( section, "disableaudio" );
            if( ivalue ) {
                fprintf(fvalue, "\"disableaudio\":\"%s\",", "on" );
            }  

            // key interval
            value = dvrconfig.getvalue( section, "key_interval" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"key_interval\":\"%s\",", value.getstring() );
            }  
            
            // b_frames
            value = dvrconfig.getvalue( section, "b_frames" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"b_frames\":\"%s\",", value.getstring() );
            }  
            
            // p_frames
            value = dvrconfig.getvalue( section, "p_frames" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"p_frames\":\"%s\",", value.getstring() );
            }  

            fprintf(fvalue, "\"objname\":\"camera_value_%d\" }" ,i );
            
            fclose( fvalue );
        }
    }
    
    // make a soft link to camera_value
    remove("camera_value");
    symlink("camera_value_1", "camera_value");
    // write sensor_value
    fvalue = fopen("sensor_value", "w");
    if( fvalue ) {
        char section[20] ;
        
        // JSON head
        fprintf(fvalue, "{" );
        
        // sensor number
        fprintf(fvalue, "\"sensor_number\":\"%d\",", sensor_number );

        for( i=1; i<=sensor_number; i++ ) {
            sprintf( section, "sensor%d", i );
            value = dvrconfig.getvalue(section, "name") ;
            // write sensor value
            if( value.length()>0 ) {
                fprintf( fvalue, "\"%s_name\": \"%s\",", section, value.getstring() );
            }

            // inverted value
            if( dvrconfig.getvalueint(section, "inverted") ) {
                fprintf( fvalue, "\"%s_inverted\":\"on\",", section );
            }
        }
        if(dvrconfig.getvalueint("io","sensor_powercontrol")){
            fprintf(fvalue, "\"sensor_powercontrol\":\"on\",");
        }
        fprintf(fvalue, "\"objname\":\"sensor_value\" }" );
        fclose( fvalue );
    }
    
    // write network_value
    
    fvalue = fopen("network_value", "w");
    if( fvalue ) {
        FILE * ipfile ;
        
        // JSON head
        fprintf(fvalue, "{" );

        // eth_ip
        sprintf(buf, "%s/eth_ip", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"eth_ip\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // eth_mask
        sprintf(buf, "%s/eth_mask", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"eth_mask\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // wifi_ip
        sprintf(buf, "%s/wifi_ip", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"wifi_ip\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // wifi_mask
        sprintf(buf, "%s/wifi_mask", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"wifi_mask\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

        // gateway
        sprintf(buf, "%s/gateway_1", dvrworkdir );
        ipfile = fopen( buf, "r" );
        if( ipfile ) {
            i=fread( buf, 1, sizeof(buf), ipfile );
            if( i>0 ) {
                while( i>0 ) {
                    if( buf[i-1]<' ' ) {
                        i--;
                    }
                    else {
                        break;
                    }
                }
                buf[i]=0;
                fprintf(fvalue, "\"gateway_1\":\"%s\",", buf );
            }
            fclose( ipfile );
        }

	// wifi authentication/encryption type
	ivalue = dvrconfig.getvalueint("network", "wifi_enc");
	if( ivalue<0 || ivalue>7 ) {
	  ivalue=0 ;                   // default
	}
	fprintf(fvalue, "\"wifi_enc\":\"%d\",", ivalue );

	// wifi ssid
	value = dvrconfig.getvalue("network", "wifi_essid");
	if( value.length()>0 ) {
	  fprintf( fvalue, "\"wifi_essid\": \"%s\",", value.getstring() );
	}

	// wifi key
	value = dvrconfig.getvalue("network", "wifi_key");
	if( value.length()>0 ) {
	  fprintf( fvalue, "\"wifi_key\": \"%s\",", value.getstring() );
	}
        //wifi power on
        ivalue = dvrconfig.getvalueint("network", "wifi_poweron");
        if( ivalue==1) {
            fprintf(fvalue, "\"wifi_power_on\":\"on\"," );
        }  

        fprintf(fvalue, "\"objname\":\"network_value\" }" );
        fclose( fvalue );
    }
    

#ifdef POWERCYCLETEST    
    // write cycletest_value
    
    fvalue = fopen("cycletest_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

        // cycletest
        ivalue = dvrconfig.getvalueint("debug", "cycletest");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"cycletest\":\"on\"," );
        }  
        
        // cyclefile
        value = dvrconfig.getvalue("debug", "cyclefile");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cyclefile\":\"%s\",", value.getstring() );
        }
        
        // cycleserver
        value = dvrconfig.getvalue("debug", "cycleserver");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cycleserver\":\"%s\",", value.getstring() );
        }

        // norecord
        ivalue = dvrconfig.getvalueint("system", "norecord");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"norecord\":\"on\"," );
        }  
        
        fprintf(fvalue, "\"objname\":\"cycletest_value\" }" );
        fclose( fvalue );

    }
#endif    

    
    // write tz_option
    fvalue = fopen( "tz_option", "w");
    if( fvalue ) {
        // initialize enumkey
        enumkey.line=0 ;
        while( (p=dvrconfig.enumkey("timezones", &enumkey))!=NULL ) {
            tzi=dvrconfig.getvalue("timezones", p );
            fprintf(fvalue, "<option value=\"%s\">", p );
            if( tzi.length()<=0 ) {
                fprintf(fvalue, "%s", p);
            }
            else {
                zoneinfobuf=tzi.getstring();
                while( *zoneinfobuf != ' ' && 
                      *zoneinfobuf != '\t' &&
                      *zoneinfobuf != 0 ) {
                          zoneinfobuf++ ;
                      }
                fprintf(fvalue, "%s -%s", p, zoneinfobuf );
            }
            fprintf(fvalue, "</option>\n");
        }
        fclose( fvalue );
    }
    
    // write led_number
    ivalue=dvrconfig.getvalueint("io", "outputnum");
    if( ivalue<=0 || ivalue>32 ) {
        ivalue=4 ;
    }
    fvalue = fopen("led_number","w");
    if( fvalue ) {
        fprintf(fvalue, "%d", ivalue );
        fclose(fvalue);
    }

    // write firmware version
    system("cp /dav/dvr/firmwareid ./firmware_version");
    
    // write dvrsvr port number, used by eagle_setup
    value = dvrconfig.getvalue("network", "port") ;
    if( value.length()>0 ){
        fvalue = fopen("dvrsvr_port","w");
        if( fvalue ) {
            fprintf(fvalue, "%s", value.getstring() );
            fclose(fvalue);
        }
    }
    
    remove("savedstat");
    
    return 0;
}
