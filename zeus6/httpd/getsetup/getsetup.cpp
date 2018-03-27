

#include <stdio.h>

#include "../../cfg.h"
#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/cfg.h"

static char * rtrim( char * s )
{
	while( *s > 1 && *s <= ' ' ) {
		s++ ;
	}
	return s ;
}

static char * ltrim( char * s )
{
	int l = strlen(s);
	while( l>0 ) {
		if( s[l-1] <= ' ' ) 
			l-- ;
		else 
			break ;
	}
	s[l] = 0 ;
	return s ;
}

static char * readfile( char * filename )
{
	static char sbuf[1024] ;
	FILE * f ;
	f = fopen( filename, "r" );
    if( f ) {
		int r=fread( sbuf, 1, sizeof(sbuf)-1, f );
		fclose( f );
		if( r>0 ) {
			sbuf[r]=0 ;
			return ltrim(rtrim(sbuf)) ;
		}
	}
	return NULL ;
}


int main()
{
    char * zoneinfobuf ;
    string s ;
    char * p ;
    int l;
    int i;
    config dvrconfig(CFG_FILE);
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

#ifdef PWII_APP
    // write mf id
    value = dvrconfig.getvalue("system", "mfid" );
    if( value.length()>0 ) {
        fvalue = fopen("manufacture_id", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value );
            fclose( fvalue );
        }
    }
#endif

#ifdef 	TVS_APP
       // write TVS mf id
    value = dvrconfig.getvalue("system", "tvsmfid" );
    if( value.length()>0 ) {
        fvalue = fopen("manufacture_id", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value );
            fclose( fvalue );
        }
        fvalue = fopen("tvs_ivcs_prefix", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value+2 );
            fclose( fvalue );
        }
    }
#endif

    // write system_value
    fvalue = fopen( "system_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

#ifdef MDVR_APP
        // dvr_server_name
        value = dvrconfig.getvalue("system","hostname");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"dvr_server_name\":\"%s\",", value );
        }
#endif

        // pwii
#ifdef PWII_APP
        value = dvrconfig.getvalue("system","id1");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"vehicleid\":\"%s\",", (char *)value );
        }

        value = dvrconfig.getvalue("system","id2");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"district\":\"%s\",", (char *)value );
        }

        value = dvrconfig.getvalue("system","serial");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"unit\":\"%s\",", (char *)value );
        }
        
        // pw recording method
        ivalue = dvrconfig.getvalueint("system","pw_recordmethod");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"pw_recordmethod\":\"on\"," );
        }
        
#endif

#ifdef TVS_APP
        value = dvrconfig.getvalue("system","tvs_licenseplate");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"tvs_licenseplate\":\"%s\",", (char *)value );
        }

        value = dvrconfig.getvalue("system","tvs_medallion");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"tvs_medallion\":\"%s\",", (char *)value );
        }

        value = dvrconfig.getvalue("system","tvs_ivcs_serial");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"tvs_ivcs_serial\":\"%s\",", (char *)value );
        }
#endif
        // dummy password
        fprintf(fvalue, "\"password\":\"********\",");

        // dvr_time_zone
        value = dvrconfig.getvalue("system", "timezone");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"dvr_time_zone\":\"%s\",", (char *)value );
            FILE * ftimezone ;
            ftimezone = fopen("timezone", "w");
            if( ftimezone ) {
                fprintf(ftimezone, "%s", (char *)value );
                fclose( ftimezone );
            }

            ftimezone = fopen("tz_env", "w");
            if( ftimezone ) {

                // time zone enviroment
                tzi=dvrconfig.getvalue( "timezones", (char *)value );
                if( tzi.length()>0 ) {
                    p=strchr(tzi, ' ' ) ;
                    if( p ) {
                        *p=0;
                    }
                    p=strchr(tzi, '\t' ) ;
                    if( p ) {
                        *p=0;
                    }
                    fprintf(ftimezone, "%s", (char *)tzi );
                }
                else {
                    fprintf(ftimezone, "%s", (char *)value );
                }
                fclose( ftimezone );
            }
        }

        // custom timezone string
        value = dvrconfig.getvalue("timezones", "Custom");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"custom_tz\":\"%s\",", (char *)value );
        }

        // shutdown_delay
        value = dvrconfig.getvalue("system", "shutdowndelay");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"shutdown_delay\":\"%s\",", (char *)value );
        }

        // standbytime
        value = dvrconfig.getvalue("system", "standbytime");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"standbytime\":\"%s\",", (char *)value );
        }

        // uploading time
        value = dvrconfig.getvalue( "system", "uploadingtime");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"uploadingtime\":\"%s\",", (char *)value );
        }

        // uploading time
        value = dvrconfig.getvalue( "system", "archivetime");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"archivingtime\":\"%s\",", (char *)value );
        }
        
        // trace mark event time (in seconds)
        ivalue = dvrconfig.getvalueint( "system", "tracemarktime");
        if( ivalue <=0 ) ivalue = 300 ;
		fprintf(fvalue, "\"tracemarktime\":\"%d\",",  ivalue );
        
		// file buffer size
        value = dvrconfig.getvalue("system", "filebuffersize");
        if( value.length()>0 ) {
            l=value.length();
            p=value;
            if( p[l-1]=='k' ) {         // use k bytes only
                p[l-1]=0;
            }
            fprintf(fvalue, "\"filebuffersize\":\"%s\",", p );
        }

        // file_size
        value = dvrconfig.getvalue("system", "maxfilesize");
        if( value.length()>0 ) {
            l=value.length();
            p=value;
            if( p[l-1]=='M' ) {         // use Mega bytes only
                p[l-1]=0;
            }
            fprintf(fvalue, "\"file_size\":\"%s\",", p );
        }

        // file_time
        value = dvrconfig.getvalue("system", "maxfilelength");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"file_time\":\"%s\",", (char *)value );
        }

        // minimun_disk_space
        value = dvrconfig.getvalue("system", "mindiskspace_percent");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"minimun_disk_space\":\"%s\",", (char *)value );
        }

        // Max file length
        value = dvrconfig.getvalue("system", "maxfilelength");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"file_time\":\"%s\",", (char *)value );
        }

        // check and format CF card
        value = dvrconfig.getvalue("system", "fscktimeout");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"fscktimeout\":\"%s\",", (char *)value );
        }

        ivalue = dvrconfig.getvalueint("system", "checkinternalCF");
        if( ivalue ) {
            fprintf(fvalue, "\"checkinternalCF\":\"on\"," );
        }

        ivalue = dvrconfig.getvalueint("system", "formatinternalCF");
        if( ivalue ) {
            fprintf(fvalue, "\"formatinternalCF\":\"on\"," );
        }

        ivalue = dvrconfig.getvalueint("system", "checkexternalCF");
        if( ivalue ) {
            fprintf(fvalue, "\"checkexternalCF\":\"on\"," );
        }

        ivalue = dvrconfig.getvalueint("system", "formatexternalCF");
        if( ivalue ) {
            fprintf(fvalue, "\"formatexternalCF\":\"on\"," );
        }

		// Number of camera
        value = dvrconfig.getvalue("system", "totalcamera");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"totalcamera\":\"%s\",", (char *)value );
        }

        // pre_lock_time
        value = dvrconfig.getvalue("system", "prelock");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"pre_lock_time\":\"%s\",", (char *)value );
        }

        // post_lock_time
        value = dvrconfig.getvalue("system", "postlock");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"post_lock_time\":\"%s\",", (char *)value );
        }

        // no rec playback
        ivalue = dvrconfig.getvalueint("system", "norecplayback");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"bool_norecplayback\":\"on\"," );
            fprintf(fvalue, "\"norecplayback\":\"on\"," );
        }

        // no rec liveview
        ivalue = dvrconfig.getvalueint("system", "noreclive");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"bool_noreclive\":\"on\"," );
            fprintf(fvalue, "\"noreclive\":\"on\"," );
        }

        // disable wifi upload
        ivalue = dvrconfig.getvalueint( "system", "disable_wifiupload");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"bool_nowifiupload\":\"on\"," );
            fprintf(fvalue, "\"nowifiupload\":\"on\"," );
        }

        // disable archiving feature
        ivalue = dvrconfig.getvalueint("system", "disable_archive");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"bool_noarchive\":\"on\"," );
            fprintf(fvalue, "\"noarchive\":\"on\"," );
        }

        // en_file_encryption
        int file_encrypt = dvrconfig.getvalueint("system", "fileencrypt");
        if( file_encrypt>0 ) {
            fprintf(fvalue, "\"en_file_encryption\":\"on\"," );
        }
        
        char * mfkey = readfile( "/davinci/dvr/mfkey" );
        value = dvrconfig.getvalue("system", "filepassword");
        if( strcmp( mfkey, (char *)value ) == 0 ) {
            fprintf(fvalue, "\"en_use_default_password\":\"on\"," );
		}

        fprintf( fvalue, "\"file_password\":\"********\",");

#ifdef APP_PWZ8
        // sd camera
        ivalue = dvrconfig.getvalueint("system", "camsd");
        if( ivalue ) 
			fprintf(fvalue, "\"camsd\":\"on\",");
#endif        

        // gpslog enable
        ivalue = dvrconfig.getvalueint("glog", "gpsdisable");
        if( ivalue == 0 ) {
            fprintf(fvalue, "\"bool_en_gpslog\":\"on\",");
            fprintf(fvalue, "\"en_gpslog\":\"on\",");
        }

        // gps port
        value = dvrconfig.getvalue("glog", "serialport");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gpsport\":\"%s\",", (char *)value );
        }

        // gps baud rate
        value = dvrconfig.getvalue("glog", "serialbaudrate");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gpsbaudrate\":\"%s\",", (char *)value );
        }

        // g-force data
        //
        ivalue = dvrconfig.getvalueint( "glog", "gforce_log_enable");
        if( ivalue ) {
            fprintf(fvalue, "\"gforce_enable\":\"on\"," );
        }

        value = dvrconfig.getvalue( "io", "gsensor_forward");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_forward\":\"%s\",", (char *)value );
        }

        value = dvrconfig.getvalue( "io", "gsensor_upward");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_upward\":\"%s\",", (char *)value );
        }

        ivalue = dvrconfig.getvalueint(  "io", "gsensor_mountangle") ;
        fprintf(fvalue, "\"gforce_mountangle\":\"%d\",", ivalue);

        // testing, let web display angle calibrate option
        if( dvrconfig.getvalueint(  "io", "gsensor_show_mountangle") ) {
            fprintf(fvalue, "\"show_mountangle\":\"1\",");
        }

        if( dvrconfig.getvalueint( "io", "gsensor_crashdata" ) ) {
            fprintf(fvalue, "\"gforce_crashdata\":\"on\",");
        }

        // gforce trigger value (peak value?)
        value = dvrconfig.getvalue( "io", "gsensor_forward_trigger");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_forward_trigger\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_trigger");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_backward_trigger\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_trigger");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_right_trigger\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_trigger");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_left_trigger\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_trigger");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_downward_trigger\":\"%s\",",(char *) value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_trigger");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_upward_trigger\":\"%s\",", (char *)value );
        }

        // base value (what?)
        value = dvrconfig.getvalue( "io", "gsensor_forward_base");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_forward_base\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_base");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_backward_base\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_base");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_right_base\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_base");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_left_base\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_base");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_downward_base\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_base");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_upward_base\":\"%s\",", (char *)value );
        }

        // crash value
        value = dvrconfig.getvalue( "io", "gsensor_forward_crash");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_forward_crash\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_backward_crash");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_backward_crash\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_right_crash");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_right_crash\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_left_crash");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_left_crash\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_down_crash");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_downward_crash\":\"%s\",", (char *)value );
        }
        value = dvrconfig.getvalue( "io", "gsensor_up_crash");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gforce_upward_crash\":\"%s\",", (char *)value );
        }

        ivalue=0 ;
        f_id = fopen( VAR_DIR "/gsensor", "r" );
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

        // Video output selection
        value = dvrconfig.getvalue("VideoOut", "startchannel");
        if( value.length()>0) {
            fprintf( fvalue, "\"videoout\":\"%s\",", (char *)value);
        }

        fprintf( fvalue, "\"objname\":\"system_value\" }");

        fclose( fvalue );
    }


    // write camera_value
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");
    if( camera_number<=0 ) {
        camera_number=2 ;
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
                fprintf(fvalue, "\"bool_enable_camera\":\"on\"," );
                fprintf(fvalue, "\"enable_camera\":\"on\"," );
            }

            // camera_name
            value = dvrconfig.getvalue(section, "name");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"camera_name\":\"%s\",", (char *)value );
            }
            
            // m_enablejicaudio, record audio in JIC mode
            if( dvrconfig.getvalueint(section, "enablejicaudio") ) {
                fprintf(fvalue, "\"enablejicaudio\":\"on\"," );
            }
            
            // camera_type
            ivalue = dvrconfig.getvalueint(section, "type");
            fprintf(fvalue, "\"camera_type\":\"%d\",", ivalue );
            
            // ip camera_url
            value = dvrconfig.getvalue(section, "stream_URL");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"ipcamera_url\":\"%s\",", (char *)value );
            }

            // Physical channel
            value = dvrconfig.getvalue(section, "channel");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"channel\":\"%s\",", (char *)value );
            }

            // recording_mode
            value = dvrconfig.getvalue(section, "recordmode");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"recording_mode\":\"%s\",", (char *)value );
            }
            
#ifdef APP_PWZ5
            // force record channel (camera direction)
            ivalue = dvrconfig.getvalueint(section, "forcerecordchannel");
			fprintf(fvalue, "\"forcerecordchannel\":\"%d\",", ivalue );
#endif

            // Video Type for Special version 2. (less options)
            ivalue = dvrconfig.getvalueint(section, "videotype");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"videotype\":\"%d\",", ivalue );

                // Camera Type
                value = dvrconfig.getvalue(section, "cameratype");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"cameratype\":\"%s\",", (char *)value );
                }

            }
            else {

                // # resolution, 0:CIF, 1:2CIF, 2:DCIF, 3:4CIF
                // resolution
                value = dvrconfig.getvalue(section, "resolution");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"resolution\":\"%s\",", (char *)value );
                }

                // frame_rate
                value = dvrconfig.getvalue(section, "framerate");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"frame_rate\":\"%s\",", (char *)value );
                }

                //          # Bitrate control
                //          bitrateen=1
                //          # Bitrate mode, 0:VBR, 1:CBR
                //          bitratemode=0
                //          bitrate=1200000
                // bit_rate_mode
                if( dvrconfig.getvalueint(section, "bitrateen")) {
					if( dvrconfig.getvalueint(section,"bitratemode") == 0 ) {
						fprintf(fvalue, "\"bit_rate_mode\":\"1\"," );
					}
					else {
						fprintf(fvalue, "\"bit_rate_mode\":\"2\"," );
					}
                }
                else {
                    fprintf(fvalue, "\"bit_rate_mode\":\"0\"," );
                }

                // bit_rate
                value = dvrconfig.getvalue(section, "bitrate");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"bit_rate\":\"%s\",", (char *)value );
                }

                //          # picture quality, 0:lowest, 10:highest
                // picture_quaity
                value = dvrconfig.getvalue(section, "quality");
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"picture_quaity\":\"%s\",", (char *)value );
                }
            }

            // picture controls
            value = dvrconfig.getvalue(section, "brightness");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"brightness\":\"%s\",", (char *)value );
            }
            value = dvrconfig.getvalue(section, "contrast");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"contrast\":\"%s\",", (char *)value );
            }
            value = dvrconfig.getvalue(section, "saturation");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"saturation\":\"%s\",", (char *)value );
            }
            value = dvrconfig.getvalue(section, "hue");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"hue\":\"%s\",", (char *)value );
            }

            // trigger and osd
#ifdef MDVR_APP
            fprintf(fvalue, "\"bool_sensor_trigger\":\"on\",");
#else
            fprintf(fvalue, "\"bool_sensor_trigger_sel\":\"on\"," );
#endif
            fprintf(fvalue, "\"bool_sensor_osd\":\"on\"," );
            for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
                char trigger[30], osd[30] ;
                int  itrig, iosd ;
                sprintf(trigger, "trigger%d", ivalue );
                sprintf(osd, "sensorosd%d", ivalue);
                itrig = dvrconfig.getvalueint( section, trigger );
                iosd  = dvrconfig.getvalueint( section, osd );
                if( iosd>0 ) {
                    fprintf(fvalue, "\"sensor%d_osd\":\"on\",", ivalue );
                }
                if( itrig>0 ) {
#ifdef MDVR_APP
                    fprintf(fvalue, "\"sensor%d_trigger\":\"on\",", ivalue );
#else
                    if( itrig & 1 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_on\":\"on\",", ivalue );
                    }
                    if( itrig & 2 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_off\":\"on\",", ivalue );
                    }
                    if( itrig & 4 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_turnon\":\"on\",", ivalue );
                    }
                    if( itrig & 8 ) {
                        fprintf(fvalue, "\"sensor%d_trigger_turnoff\":\"on\",", ivalue );
                    }
#endif
                }

/*
                sprintf(trigger, "trigger%d_prerec", ivalue );
                value = dvrconfig.getvalue(section, trigger );
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"trigger%d_prerec\":\"%s\",", ivalue, value );
                }

                sprintf(trigger, "trigger%d_postrec", ivalue );
                value = dvrconfig.getvalue(section, trigger );
                if( value.length()>0 ) {
                    fprintf(fvalue, "\"trigger%d_postrec\":\"%s\",", ivalue, value );
                }
*/
            }

            // g-force trigger recording
            
            // gforce_trigger
            ivalue = dvrconfig.getvalueint(section, "gforce_trigger");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"gforce_trigger\":\"on\"," );
            }

            // gforce_trigger_value
            value = dvrconfig.getvalue(section, "gforce_trigger_value");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"gforce_trigger_value\":\"%s\",",(char *)value );
			}

            // speed_trigger
            ivalue = dvrconfig.getvalueint(section, "speed_trigger");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"speed_trigger\":\"on\"," );
            }

            // speed_trigger_value
            value = dvrconfig.getvalue(section, "speed_trigger_value");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"speed_trigger_value\":\"%s\",",(char *)value );
			}

            // pre_recording_time
            value = dvrconfig.getvalue(section, "prerecordtime");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"pre_recording_time\":\"%s\",",(char *)value );
            }

            // post_recording_time
            value = dvrconfig.getvalue(section, "postrecordtime");
            if( value.length()>0 ) {
                fprintf(fvalue, "\"post_recording_time\":\"%s\",",(char *) value );
            }

            // show_gps
            ivalue = dvrconfig.getvalueint(section, "showgps");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_gps\":\"on\"," );
                fprintf(fvalue, "\"show_gps\":\"on\"," );
            }

            // gpsunit
            // speed_display
            value = dvrconfig.getvalue(section, "gpsunit");
            fprintf(fvalue, "\"speed_display\":\"%s\",", (char *)value );

            ivalue = dvrconfig.getvalueint(section, "showgpslocation");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_gps_coordinate\":\"on\"," );
                fprintf(fvalue, "\"show_gps_coordinate\":\"on\"," );
            }

#ifdef TVS_APP

             // Medallion on OSD
            ivalue = dvrconfig.getvalueint(section, "show_medallion");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_medallion\":\"on\"," );
                fprintf(fvalue, "\"show_medallion\":\"on\"," );
            }

            // License plate on OSD
            ivalue = dvrconfig.getvalueint(section, "show_licenseplate");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_licenseplate\":\"on\"," );
                fprintf(fvalue, "\"show_licenseplate\":\"on\"," );
            }

            // IVCS on OSD
            ivalue = dvrconfig.getvalueint(section, "show_ivcs");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_ivcs\":\"on\"," );
                fprintf(fvalue, "\"show_ivcs\":\"on\"," );
            }

            // IVCS on OSD
            ivalue = dvrconfig.getvalueint(section, "show_cameraserial");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_cameraserial\":\"on\"," );
                fprintf(fvalue, "\"show_cameraserial\":\"on\"," );
            }

#endif

#ifdef PWII_APP
            ivalue = dvrconfig.getvalueint(section, "show_vri");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_vri\":\"on\"," );
                fprintf(fvalue, "\"show_vri\":\"on\"," );
            }

            ivalue = dvrconfig.getvalueint(section, "show_policeid");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_policeid\":\"on\"," );
                fprintf(fvalue, "\"show_policeid\":\"on\"," );
            }
#endif

            ivalue = dvrconfig.getvalueint(section, "show_gforce");
            if( ivalue>0 ) {
                fprintf(fvalue, "\"bool_show_gforce\":\"on\"," );
                fprintf(fvalue, "\"show_gforce\":\"on\"," );
            }

             // record_alarm_mode
            value = dvrconfig.getvalue( section, "recordalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"record_alarm_mode\":\"%s\",", (char *)value );
            }
            value = dvrconfig.getvalue( section, "recordalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"record_alarm_led\":\"%s\",", (char *)value );
            }

             // video_lost_alarm_mode
            value = dvrconfig.getvalue( section, "videolostalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"video_lost_alarm_mode\":\"%s\",", (char *)value );
            }
            value = dvrconfig.getvalue( section, "videolostalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"video_lost_alarm_led\":\"%s\",", (char *)value );
            }

            // motion_alarm_mode
            value = dvrconfig.getvalue( section, "motionalarmpattern" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_alarm_mode\":\"%s\",", (char *)value );
            }
            value = dvrconfig.getvalue( section, "motionalarm" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_alarm_led\":\"%s\",", (char *)value );
            }

            // motion_sensitivity
            value = dvrconfig.getvalue( section, "motionsensitivity" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"motion_sensitivity\":\"%s\",", (char *)value );
            }

            // disableaudio
            ivalue = dvrconfig.getvalueint( section, "disableaudio" );
            if( ivalue ) {
                fprintf(fvalue, "\"bool_disableaudio\":\"%s\",", "on" );
                fprintf(fvalue, "\"disableaudio\":\"%s\",", "on" );
            }

            // key interval
            value = dvrconfig.getvalue( section, "key_interval" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"key_interval\":\"%s\",", (char *)value );
            }

            // b_frames
            value = dvrconfig.getvalue( section, "b_frames" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"b_frames\":\"%s\",", (char *)value );
            }

            // p_frames
            value = dvrconfig.getvalue( section, "p_frames" );
            if( value.length()>0 ) {
                fprintf(fvalue, "\"p_frames\":\"%s\",", (char *)value );
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
                fprintf( fvalue, "\"%s_name\": \"%s\",", section, (char *)value );
            }

            // inverted value
            fprintf( fvalue, "\"bool_%s_inverted\":\"on\",", section );
            if( dvrconfig.getvalueint(section, "inverted") ) {
                fprintf( fvalue, "\"%s_inverted\":\"on\",", section );
            }
            else {
                fprintf( fvalue, "\"%s_inverted\":\"off\",", section );
            }

            // event marker
            if( dvrconfig.getvalueint(section, "eventmarker") ) {
                fprintf( fvalue, "\"%s_eventmarker\":\"on\",", section );
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

        // JSON head
        fprintf(fvalue, "{" );

#ifndef APP_PWZ5
        // eth_ip
        value = dvrconfig.getvalue("network","eth_ip");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_ip\":\"%s\",", (char *)value );
        }

        // eth_mask
        value = dvrconfig.getvalue("network","eth_mask");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_mask\":\"%s\",", (char *)value );
        }

        // eth_bcast
        value = dvrconfig.getvalue("network","eth_bcast");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_bcast\":\"%s\",", (char *)value );
        }

        // gateway
        value = dvrconfig.getvalue("network","gateway");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gateway_1\":\"%s\",", (char *)value );
        }

        // wifi_ip
        value = dvrconfig.getvalue("network","wifi_ip");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_ip\":\"%s\",", (char *)value );
        }

        // smartserver
        ivalue = dvrconfig.getvalueint("network", "wifi_dhcp" );
        if( ivalue>0 ) {
            fprintf(fvalue, "\"wifi_dhcp\":\"%s\",", "on" );
        }

        // wifi_mask
        value = dvrconfig.getvalue("network","wifi_mask");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_mask\":\"%s\",", (char *)value );
        }

        // wifi_bcast
        value = dvrconfig.getvalue("network","wifi_bcast");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_bcast\":\"%s\",", (char *)value );
        }

        // wifi_essid
        value = dvrconfig.getvalue("network","wifi_essid");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_essid\":\"%s\",", (char *)value );
        }

        // wifi_key
        value = dvrconfig.getvalue("network","wifi_key");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_key\":\"%s\",", (char *)value );
        }

        // wifi enc type.
        //        0 : Disable (no enc)
        //        1 : WEP open
        //        2 : WEP shared
        //        3 : WEP auto
        //        4 : WPA Personal TKIP
        //        5 : WPA Personal AES
        //        6 : WPA2 Personal TKIP
        //        7 : WPA2 Personal AES
        value = dvrconfig.getvalue("network","wifi_enc");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_enc\":\"%s\",", (char *)value );
        }

        // smartserver
        value = dvrconfig.getvalue( "network", "smartserver" );
        if( value.length()>0 ) {
            fprintf(fvalue, "\"smartserver\":\"%s\",", (char *)value );
        }

#else   // PWZ5

        // eth_ip
        value = readfile( "/davinci/dvr/eth_ip" ) ;
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_ip\":\"%s\",", (char *)value );
        }

        // eth_mask
        value = readfile( "/davinci/dvr/eth_mask" ) ;
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_mask\":\"%s\",", (char *)value );
        }
        
        // enable dhcp client on eth0
        ivalue = dvrconfig.getvalueint("network", "eth_dhcpc");
        if( ivalue ) {
            fprintf(fvalue, "\"eth_dhcpc\":\"on\"," );
		}

        // wifi_ip
        value = readfile( "/davinci/dvr/wifi_ip" ) ;
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_ip\":\"%s\",", (char *)value );
        }
                
        // wifi_mask
        value = readfile( "/davinci/dvr/wifi_mask" ) ;
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_mask\":\"%s\",", (char *)value );
        }

		// wifi essid
		value = dvrconfig.getvalue("network", "wifi_essid");
		if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_essid\":\"%s\",", (char *)value );
        }

		// wifi_key
        value = dvrconfig.getvalue("network","wifi_key");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_key\":\"%s\",", (char *)value );
        }
        
		// smart upload server
        value = dvrconfig.getvalue("network","smartserver");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"smartserver\":\"%s\",", (char *)value );
        }
        
        // gateway
        value = readfile( "/davinci/dvr/gateway_1" ) ;
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gateway_1\":\"%s\",", (char *)value );
        }

		// wifi authentication/encryption type
		ivalue = dvrconfig.getvalueint("network", "wifi_enc");
		if( ivalue<0 || ivalue>7 ) {
		  ivalue=0 ;                   // default
		}
		fprintf(fvalue, "\"wifi_enc\":\"%d\",", ivalue );
		
        // internet access
        ivalue = dvrconfig.getvalueint("system", "nointernetaccess");
        if( ivalue == 0 ) {
			fprintf(fvalue, "\"internetaccess\":\"on\"," );
        }

        // internet key
        value = dvrconfig.getvalue("system", "internetkey") ;
		fprintf(fvalue, "\"internetkey\":\"%s\",", (char *)value );
	
#endif
        // end of network value
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
            fprintf(fvalue, "\"bool_cycletest\":\"on\"," );
            fprintf(fvalue, "\"cycletest\":\"on\"," );
        }

        // cyclefile
        value = dvrconfig.getvalue("debug", "cyclefile");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cyclefile\":\"%s\",", (char *)value );
        }

        // cycleserver
        value = dvrconfig.getvalue("debug", "cycleserver");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cycleserver\":\"%s\",", (char *)value );
        }

        // norecord
        ivalue = dvrconfig.getvalueint("system", "norecord");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"bool_norecord\":\"on\"," );
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
        config_enum enumkey ;
        enumkey.line=0 ;
        while( (p=dvrconfig.enumkey("timezones", &enumkey))!=NULL ) {
            tzi=dvrconfig.getvalue("timezones", p );
            fprintf(fvalue, "<option value=\"%s\">%s ", p, p );
            if( tzi.length()>0 ) {
                zoneinfobuf=tzi;
                while( *zoneinfobuf != ' ' &&
                      *zoneinfobuf != '\t' &&
                      *zoneinfobuf != 0 ) {
                          zoneinfobuf++ ;
                      }
                if( strlen(zoneinfobuf) > 1 ) {
                    fprintf(fvalue, "-%s", zoneinfobuf );
                }
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

    // link firmware version
    symlink( APP_DIR"/firmwareid", "./firmware_version");

    // write dvrsvr port number, used by eagle_setup
    value = dvrconfig.getvalue("network", "port") ;
    if( value.length()>0 ){
        fvalue = fopen("dvrsvr_port","w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value );
            fclose(fvalue);
        }
    }

    remove("savedstat");

    return 0;
}
