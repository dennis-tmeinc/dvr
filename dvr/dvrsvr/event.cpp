
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

double g_gpsspeed ;

sensor_t::sensor_t(int n)
{
    config dvrconfig(CFG_FILE);
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
    if( v!=m_value ) {
        m_value = v ;
        m_toggle = 1 ;
        dvr_log("Sensor (%s) triggered to (%d).", (char *)m_name, m_value);
        return 1 ;
    }
    else {
        m_toggle = 0 ;
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

#ifdef TVS_APP        
// value priority order:  2-3-4-5-6-7-1-0
int alarm_t::setvalue(int v)
{
	if( m_value<=0 && v>0 ) {
        m_value = v ;
    }
    else if( m_value == 1 && v > 1 ) {
		m_value = v ;
	}
	else if( m_value > 1 && v>1 && v < m_value ) {
		m_value = v ;
	}
    return m_value ;
}
#else
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
#endif

void alarm_t::update()
{
	if( m_value<=0 || m_value>15 ) {
        dio_output(m_outputpin, 0);
    }
    else if( m_value==1 ) {
        dio_output(m_outputpin, 1);
    }
    else {
        dio_output(m_outputpin, g_timetick&(1<<(m_value+6)) );
    }
}

// time_t poweroffstarttime ;
// int shutdowndelaytime;

static int alarm_suspend_timer = 0 ;
void setdio(int onoff)
{
    int i;
    alarm_suspend_timer=g_timetick+10000 ;        // 10 second io output
    for( i=0; i<num_alarms; i++ ) {
        dio_output(i, onoff);
    }
}

volatile float g_cpu_usage = 0.0 ;

static void cpu_usage()
{
    static float s_cputime=0.0, s_idletime=0.0 ;
    float cputime, idletime ;
    FILE * uptime ;
    uptime = fopen( "/proc/uptime", "r" );
    if( uptime ) {
        fscanf( uptime, "%f %f", &cputime, &idletime );
        fclose( uptime );
        if( cputime>s_cputime ) {
            g_cpu_usage=1.0 -(idletime-s_idletime)/(cputime-s_cputime) ;
            s_cputime=cputime ;
            s_idletime=idletime ;
        }
    }
}

int event_tm ;          // To support PWII Trace Mark

// main program loop here
int event_check()
{
    int i ;
    int videolost, videodata, diskready ;
    static int timer_1s ;
    int ev = dio_check() || screen_io() ;

    if( ev ||
        g_timetick-timer_1s > 1000 ||
        g_timetick<timer_1s )
    {
        // check sensors
        for(i=0; i<num_sensors; i++ ) {
            sensors[i]->check();
        }

        // update decoder screen (OSD)
        for(i=0; i<cap_channels; i++ ) {
            cap_channel[i]->update(1);
        }

        // update recording status
        rec_update();

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

#ifdef TVS_APP                
        // for TVS requirement, no disk flashing 
        disk_alarm();
#endif        

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
					
	printf("channel %d no data! \n", i);
					
                    videodata=0 ;
                }
                if( rec_state(i) ) {
                    ch_state |= 2 ;             // recording
                }
                if( cap_channel[i]->getmotion() ) {
                    ch_state |= 4 ;
                }
            }
            dio_setchstat( i, ch_state );
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
        if( diskready || rec_norecord ) {
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

        if( rec_lockstate(-1) ) {
            dio_setstate (DVR_LOCK);
        }
        else {
            dio_clearstate (DVR_LOCK);
        }

        if( rec_triggered(-1) ) {
            dio_setstate (DVR_SENSOR);
        }
        else {
            dio_clearstate (DVR_SENSOR);
        }

        if( disk_archive_runstate() ) {
            dio_setstate (DVR_ARCH);
        }
        else {
            dio_clearstate(DVR_ARCH);
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
                    sprintf(sysbuf, "cd /home ; "APP_DIR"/tmefile p %s %s ", cyclefile.getstring(), cycleserver.getstring() );
                    system(sysbuf);
                }
            }
        }
#endif

        if( g_timetick<timer_1s || g_timetick-timer_1s > 1000 ) {
            timer_1s = g_timetick ;

            // check if we need to detect smartserver (wifi)
            dio_checkwifi();
            dio_kickwatchdog ();

            // calculate cpu usage
            cpu_usage();

            // check memory availablity
            mem_available();
			if( g_memfree < g_lowmemory ) {
                dvr_log("Memory low. restart DVR.");
                app_state = APPRESTART ;
            }
        }
    }

    // display alarm (LEDs)
    if( g_timetick>alarm_suspend_timer ) {
        for(i=0; i<num_alarms; i++) {
            alarms[i]->update();
        }
    }

    return ev ;
}

void event_init( config &dvrconfig )
{
    int i;

#ifdef  POWERCYCLETEST
    cycletest = dvrconfig.getvalueint("debug", "cycletest" );
    cyclefile = dvrconfig.getvalue("debug", "cyclefile" );
    cycleserver = dvrconfig.getvalue("debug", "cycleserver" );
#endif

    dio_init(dvrconfig);
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
    alarm_suspend_timer = 0 ;
    event_tm = 0 ;
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
