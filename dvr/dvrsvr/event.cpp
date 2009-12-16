
#include "../cfg.h"

#include "dvr.h"
#include "../ioprocess/diomap.h"

#ifdef POWERCYCLETEST

int cycletest ;
string cyclefile ;
string cycleserver ;

#endif

int num_sensors ;
sensor_t ** sensors ;
int num_alarms ;
alarm_t ** alarms ;

int alarm_suspend = 0 ;
static DWORD event_tstamp ;
int event_marker ;

double g_gpsspeed ;

sensor_t::sensor_t(int n) 
{
    config dvrconfig(dvrconfigfile);
    char section[64] ;	
    sprintf(section, "sensor%d", n+1);
    m_name=dvrconfig.getvalue(section, "name");
    if( m_name.length()==0 )
        m_name=section ;
    m_inverted=dvrconfig.getvalueint(section, "inverted");
    m_eventmarker=dvrconfig.getvalueint(section, "eventmarker");

    m_inputpin=n ;
    m_value=0;
    m_toggle=0;
}

int sensor_t::check()
{
    int v ;
    v=dio_input(m_inputpin);
    if( m_inverted )
        v=!v ;
    if( v ) {
        event_marker = 1 ;      // set event_marker
    }
    if( v!=m_value ) {
        m_value = v ;
        m_toggle = 1 ;
        dvr_log("Sensor (%s) triggered to (%d).", m_name.getstring(), m_value);
        return 1 ;
    }
    else {
        return 0 ;
    }
}

alarm_t::alarm_t(int n)
{
    m_outputpin=n;
    m_value=0;
    dio_output(m_outputpin,0);
}

alarm_t::~alarm_t()
{
    dio_output(m_outputpin,0);
}


void alarm_t::clear()
{
    m_value=0;
}

// value priority order:  1-2-3-4-5-6-7-0 
int alarm_t::setvalue(int v)
{
    if( m_value<=0 ) {
        m_value = v ;
    }
    else {
        if( v<m_value ) {
            m_value=v ;
        }
    }
    return m_value ;
}

void alarm_t::update()
{
    if( m_value==1 ) {
        dio_output(m_outputpin, 1);
    }
    else if( m_value>1 ) {
        dio_output(m_outputpin, event_tstamp&(1<<(m_value+2)) );
    }
    else {
        dio_output(m_outputpin, 0);
    }
}

// time_t poweroffstarttime ;
// int shutdowndelaytime;

void setdio(int onoff)
{
    int i;
    alarm_suspend=800 ;
    for( i=0; i<num_alarms; i++ ) {
        dio_output(i, onoff);
    }
}

// called every 0.0125 second
void event_check()
{
    int i ;
    int sensor_toggle=0;
    double gpsspeed ;
    int videolost, videodata, diskready ;
    static int timer_1s ;

    event_tstamp = time_hiktimestamp() ;

    if( dio_check() || g_timetick-timer_1s > 1000 ) {
        timer_1s = g_timetick ;

        // check sensors
        event_marker=0 ;                        // reset event marker
        for(i=0; i<num_sensors; i++ ) {
            sensor_toggle+=sensors[i]->check();
        }

#ifdef    PWII_APP
        extern int pwii_event_marker ;      // rear mirror event marker button
        if( pwii_event_marker ) {
            event_marker = 1 ;
        }
#endif
        
        gpsspeed = gps_speed() ;
        if( gpsspeed >-0.1 ) {
            g_gpsspeed = gpsspeed ;
        }
        else {
            g_gpsspeed = 0.0 ;
        }
        
        // update decoder screen (OSD)
        for(i=0; i<cap_channels; i++ ) {
            cap_channel[i]->update(sensor_toggle || gpsspeed>-0.1);
        }
        
        // update recording status
        rec_update();		

        if( sensor_toggle>0 )
        for(i=0; i<num_sensors; i++ ) {
            sensors[i]->toggle_clear();
        }
            
        // clear alarm value.
        for(i=0; i<num_alarms; i++) {
            alarms[i]->clear();
        }
        
        // update capture channel alarm
        for(i=0; i<cap_channels; i++ ) {
            cap_channel[i]->alarm();
        }

        // update recording alarm
        rec_alarm();		
        
        videolost=0;
        videodata=1 ;
		for(i=0; i<cap_channels; i++ ) {
            int ch_state = 0 ;

			if( cap_channel[i]->enabled() ) {
                if( cap_channel[i]->getsignal() ) {
                    ch_state |= 1 ;             // bit 0: signal avaiable
                }
                else {
                    videolost=1 ;
                }
                if( cap_channel[i]->isworking() ) {
                    ch_state |= 8 ;             // bit 3: video data
                }
                else {
                    videodata=0 ;
                }
                if( rec_state(i) ) {
                    ch_state |= 2 ;             // recording
                }
                if( cap_channel[i]->getmotion() ) {
                    ch_state |= 4 ;
                }
                dio_setchstat( i, ch_state );
			}
	    }
		// set video lost led
        if( videolost ) {
            dio_setstate(DVR_VIDEOLOST) ;
        }
        else {
            dio_clearstate(DVR_VIDEOLOST);
        }
        if( videodata ) {
            dio_clearstate (DVR_NODATA);
        }
        else {
            dio_setstate (DVR_NODATA);
        }
        diskready = rec_basedir.length()>0 ;
        if( diskready ) {
            dio_setstate (DVR_DISKREADY);
        }
        else {
            dio_clearstate (DVR_DISKREADY);
        }

        if( rec_state(-1) ) {
            dio_setstate (DVR_RECORD);
        }
        else {
            dio_clearstate (DVR_RECORD);
        }
        
        if( net_active ) {
            dio_setstate (DVR_NETWORK);
        }
        else {
            dio_clearstate (DVR_NETWORK);
        }

#ifdef POWERCYCLETEST
        if( cycletest ) {
            static int cycle=0 ;
            static int disk=0 ;
            static int video=0 ;
            static struct dvrtime cycletime ;
            FILE * xfile ;
            char sysbuf[512] ;

            if( cycle==0 ) {
                time_now(&cycletime) ;
            }            
            if( cycle==0 || diskready!=disk || videodata!=video ) {
                cycle=1 ;
                disk=diskready ;
                video=videodata ;
                sprintf(sysbuf, "/home/%s",  cyclefile.getstring() );
                xfile=fopen(sysbuf, "w");
                if( xfile ) {
                    fprintf( xfile, "%02d:%02d:%02d \t%d \t%d\r\n", 
                            cycletime.hour, cycletime.minute, cycletime.second,
                            disk,
                            video );
                    fclose( xfile );
                    sync();
                    sprintf(sysbuf, "cd /home ; /davinci/dvr/tmefile p %s %s ", cyclefile.getstring(), cycleserver.getstring() );
                    system(sysbuf);
                }
            }
        }
#endif    
        dio_kickwatchdog ();
        // check memory
        if( mem_available () < g_lowmemory && rec_basedir.length()>1 ) {
            dvr_log("Memory low. reload DVR.");
            app_state = APPRESTART ;
        }
    }

    // display alarm (LEDs)
    if( alarm_suspend>0 ) {
        alarm_suspend-- ;
    }
    else {
        for(i=0; i<num_alarms; i++) {
            alarms[i]->update();
        }
    }
    
}

float cpu_usage()
{
    static float s_cputime=0.0, s_idletime=0.0 ;
    static float s_usage = 0.0 ;
    float cputime, idletime ;
    FILE * uptime ;
    uptime = fopen( "/proc/uptime", "r" );
    if( uptime ) {
        fscanf( uptime, "%f %f", &cputime, &idletime );
        fclose( uptime );
        if( cputime>s_cputime ) {
            s_usage=1.0 -(idletime-s_idletime)/(cputime-s_cputime) ;
            s_cputime=cputime ;
            s_idletime=idletime ;
        }
    }
    return s_usage ;
}

void event_init()
{
    int i;

#ifdef  POWERCYCLETEST
    config dvrconfig(dvrconfigfile);
    cycletest = dvrconfig.getvalueint("debug", "cycletest" );
    cyclefile = dvrconfig.getvalue("debug", "cyclefile" );
    cycleserver = dvrconfig.getvalue("debug", "cycleserver" );
#endif
    
    dio_init();
    num_sensors=dio_inputnum();
    dvr_log("%d sensors detected!", num_sensors );
    if( num_sensors>0 ) {
        sensors = new sensor_t * [num_sensors] ;
        for(i=0;i<num_sensors; i++) 
            sensors[i]=new sensor_t(i);
    }
    
    num_alarms=dio_outputnum();
    dvr_log("%d alarms detected!", num_alarms );
    if( num_alarms>0 ) {
        alarms = new alarm_t * [num_alarms] ;
        for(i=0;i<num_alarms; i++) 
            alarms[i]=new alarm_t(i);
    }
//    config dvrconfig(dvrconfigfile);
//    shutdowndelaytime = dvrconfig.getvalueint( "system", "shutdowndelay");
//    if( shutdowndelaytime < 5 )
//        shutdowndelaytime=5 ;
    // $$$$ pre-set shut down delay time. 
//    dio_lockpower(shutdowndelaytime);
    
    g_gpsspeed = 0 ;
}

void event_uninit()
{
    int i;
    
    if( num_alarms>0 ) {
        for(i=0;i<num_alarms; i++) 
            delete alarms[i] ;
        delete alarms ;
    }
    num_alarms=0 ;
    if( num_sensors>0 ) {
        for(i=0; i<num_sensors; i++)
            delete sensors[i];
        delete sensors ;
    }
    num_sensors=0 ;
    
    // don't want shutdown delay
//    dio_lockpower(0);
    
//    dio_unlockpower();
    dio_uninit();
}
