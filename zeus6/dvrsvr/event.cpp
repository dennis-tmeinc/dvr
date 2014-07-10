/* --- Changes ---
 *
 * 11/03/2009 by Harrison
 *   1. Update OSD when gps_valid changes from 1 to 0
 *
 */

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
int event_serialno ;

int event_tm = 0;

double g_gpsspeed ;
float g_fb,g_lr,g_ud;
int lastpeakchanged=0;

sensor_t::sensor_t(int n) 
{
    config dvrconfig(dvrconfigfile);
    char section[64] ;	
    sprintf(section, "sensor%d", n+1);
    m_name=dvrconfig.getvalue(section, "name");
    if( m_name.length()==0 )
        m_name=section ;
    m_inverted=dvrconfig.getvalueint(section, "inverted");
    m_inputpin=n ;
    m_xvalue=0;
    m_value=0;
}

int sensor_t::check()
{
    int v ;
    v=dio_input(m_inputpin);
    if( m_inverted )
        v=!v ;
    if( v!=m_value ) {
        m_value = v ;
        dvr_log("Sensor <%s> triggered to <%d>.", m_name.getstring(), m_value);
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
        dio_output(m_outputpin, event_serialno&(1<<(m_value-1)) );
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
    alarm_suspend=80 ;
    for( i=0; i<num_alarms; i++ ) {
        dio_output(i, onoff);
    }
}

// called every 0.125 second
void event_check()
{
    int i ;
    int sensor_toggle=0;
    double gpsspeed ;
    int input ;
    int videolost, videodata, diskready ;

    event_serialno++;

	// reset sensor xvalue
	for(i=0; i<num_sensors; i++ ) {
		sensors[i]->resetxvalue();
	}	

    input = dio_check();    // update dio status,
    
    if( input ) {
        // check sensors
        for(i=0; i<num_sensors; i++ ) {
            sensor_toggle+=sensors[i]->check();
        }
    }
    
    int gpsvalid_changed;
    gpsspeed = gps_speed(&gpsvalid_changed) ;
    if( gpsspeed >-0.1 ) {
        g_gpsspeed = gpsspeed ;
    }
    else {
        g_gpsspeed = 0.0 ;
    }
    
    int gforce_changed = 0;
    float fb, lr, ud;
    get_peak_data( &fb, &lr, &ud );
  //  dvr_log("fb=%0.2f lr=%0.2f ud=%0.2f",fb,lr,ud);
    if ((fb != g_fb) || (lr != g_lr) || (ud != g_ud)) {
      gforce_changed = 1;
      g_fb = fb;
      g_lr = lr;
      g_ud = ud;
    }    
    
    if(lastpeakchanged!=isPeakChanged()){
       lastpeakchanged=isPeakChanged();
       gforce_changed = 1;
    }
    // update decoder screen (OSD)
    for(i=0; i<cap_channels; i++ ) {
      /* gpsvalid_changed: don't show invalid value */
        cap_channel[i]->update(sensor_toggle || gpsspeed>2.0 ||
			       gpsvalid_changed||gforce_changed);
    }
    
    // update recording status
    rec_update();			
    
    event_tm = 0 ;	
    
    if( event_serialno % 8 == 0 ) {     // every one second
        
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
	    if( cap_channel[i]->enabled() ) {
                if( cap_channel[i]->getsignal()==0 ) {
                    videolost=1 ;
                   // printf("channel %d has video lost\n",i);
                }
                if( !cap_channel[i]->isworking() ) {
                    videodata=0 ;
                }
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
	    // update decoder screen (OSD)
	    for(i=0; i<cap_channels; i++ ) {
	      /* gpsvalid_changed: don't show invalid value */
		cap_channel[i]->update(1);
	    }
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
        
        if( net_active>0 ) {
            net_active-- ;
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

		// check shutdown delay
        
// shutdown delay feature moved to ioprocess.
        
/*        
        if( dio_poweroff() ) {
            dio_lockpower(shutdowndelaytime);
            if( poweroffstarttime==0 )  {
                dvr_log("Power switch off, start power turn off delay.");
                poweroffstarttime=time_now(NULL);
            }
            else {
                if( time_now(NULL)-poweroffstarttime > shutdowndelaytime ) {
                    dvr_log("Power delay time out, system shutdown.");
                    app_state=APPQUIT ;
                    system_shutdown=1 ;
                }
            }
        }
        else {
            if( poweroffstarttime!=0 ) {
                dvr_log("Power switch on!");
                poweroffstarttime=0 ;
                dio_unlockpower();
            }
        }
*/
        dio_kickwatchdog ();
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
