/* --- Changes ---
 *
 * 11/03/2009 by Harrison
 *   1. Update OSD when gps_valid changes from 1 to 0
 *
 */

#include "../cfg.h"

#include "dvr.h"
#include "../ioprocess/diomap.h"

#define VK_QUEUE_SIZE (20)

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

int event_tm_timeout = 300 ;
int event_tm_time = 0 ;
int event_tm = 0;

double g_gpsspeed ;
float g_fb,g_lr,g_ud;
int lastpeakchanged=0;

int    g_cpu_usage ;	// 0-100

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
	int v = 0 ;
    if( m_value==1 ) {
		v = 1 ;
    }
    else if( m_value>1 ) {
		v =  event_serialno&(1<<(m_value-1)) ;
    }
    dio_output(m_outputpin, v);
}

static int vk_queue[VK_QUEUE_SIZE] ;
static int vk_head = 0 ;
static int vk_tail = 0 ;

// mod from screen_key()
void event_key( int keycode, int keydown ) 	// keyboard/keypad event
{
	dvr_lock();
	vk_queue[vk_tail] = keydown? (0xf0000000|keycode) : keycode ;
	if( ++vk_tail >= VK_QUEUE_SIZE )
		vk_tail = 0 ;
	dvr_unlock();
}

static void event_onkey( int key ) 
{
	int keycode = key&0xffff ;
	int keydown = key&0xffff0000 ;
	
	if( keycode == (int) VK_TM ) {
		if( keydown ) {
			dvr_log("TraceMark pressed!");
			event_tm = 1 ;
			event_tm_time = time_gettick()/1000;
		}
		else {
			dvr_log("TraceMark released!");
			// event_tm = 0 ;  // to be cleared by event.cpp after rec_update()	
		 }
	}
	else if( keycode==(int)VK_LP ) {                             // LP key
		if( keydown ) {
#ifdef APP_PWZ8			
			dio_pwii_bw( 1 );			// black/white mode
#else 
			dio_pwii_lpzoomin( 1 );		// zoom in
#endif			
			dvr_log("LP pressed!");
		}
		else {
#ifdef APP_PWZ8			
			dio_pwii_bw( 0 );			// black/white mode off
#else 
			dio_pwii_lpzoomin( 0 );		// zoom in
#endif			
			dvr_log("LP released!");
		}
	}
	else if( keycode>=(int)VK_C1 && keycode<=(int)VK_C8 ) {     // record C1-C8
		if( keydown ) {
			rec_pwii_toggle_rec( keycode-(int)VK_C1 ) ;
			dvr_log("C%d pressed!", keycode-(int)VK_C1+1);
		}
	}
}

static void event_processkey()
{
	if( vk_head == vk_tail ) return ;

	dvr_lock();
	while( vk_head != vk_tail ) {
		int key = vk_queue[vk_head] ;
		if( ++vk_head >= VK_QUEUE_SIZE ) 
			vk_head = 0;
		dvr_unlock();

		event_onkey(key);
		
		dvr_lock();
	}
	dvr_unlock();
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
	
	event_processkey();
	
	// update trace mark
    if( event_tm && ( time_gettick()/1000 - event_tm_time ) > event_tm_timeout ) {
		event_tm = 0 ;	
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
        cap_channel[i]->update();
        
		/* gpsvalid_changed: don't show invalid value */
		/*
        cap_channel[i]->update(
			sensor_toggle || 
			gpsspeed>2.0 ||
			gpsvalid_changed ||
			gforce_changed );
		*/
    }
    
    // update recording status
    rec_update();			
    
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
				}
				if( !cap_channel[i]->isworking() ) {
					videodata=0 ;
				}
			 }
		}

		// update dio camera status
		for(i=0; i<cap_channels; i++ ) {
			if( cap_channel[i]->enabled() ) {
				unsigned int status = 0 ;
				status = rec_state2(i);
				if( cap_channel[i]->getsignal()==0 ) {
					status |= 1 ;
				}
				if( cap_channel[i]->getmotion() ) {
					status |= 2 ;
				}
				dio_set_camera_status(i, status,  cap_channel[i]->streambytes() );
			 }
			 else {
				dio_set_camera_status(i, 0, 0);
			 }
		}
		
		{
			static int mic_triggerstat = 0 ;
			int tstat = 0;
			for(i=0; i<cap_channels; i++ ) {
				if( rec_forcechannel( i )==0 && rec_staterec(i) ) {
					tstat = 1 ;
					break;
				}
			}
			if( tstat != mic_triggerstat ) {
				mic_triggerstat = tstat ;
				if( tstat ) {
					dio_pwii_mic_on();
				}
				else {
					dio_pwii_mic_off();
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
        }
        
        diskready = pw_disk[0].mounted ||  pw_disk[1].mounted ;
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
        
        if( rec_lockstate(-1) ) {
            dio_setstate (DVR_LOCK);
        }
        else {
            dio_clearstate (DVR_LOCK);
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

		// read free mem
		mem_available();
        
        // check cpu usages
		static double x_idle = 0.0 ;
		static double x_total = 0.0 ;
		char cpu[10] ;
		int  v[8] ;
		double total = 0.0 ;
		FILE * procstat = fopen("/proc/stat", "r" );
		fscanf( procstat , "%s %d %d %d %d %d %d %d", 
			cpu, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6] );
		fclose( procstat );
		for( i=0; i<7; i++ ) {
			total += (double) v[i] ;
		}
		if( total > x_total ) {
			g_cpu_usage = (int)( 100.0 - 100.0 * ( (double)v[3] - x_idle ) / (total-x_total)  ) ;
			x_total = total ;
			x_idle = (double)v[3] ;
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

void event_init()
{
    int i;

    config dvrconfig(dvrconfigfile);

#ifdef  POWERCYCLETEST
    cycletest = dvrconfig.getvalueint("debug", "cycletest" );
    cyclefile = dvrconfig.getvalue("debug", "cyclefile" );
    cycleserver = dvrconfig.getvalue("debug", "cycleserver" );
#endif

	event_tm_timeout = dvrconfig.getvalueint( "system", "tracemarktime");
    if( event_tm_timeout <=0 ) event_tm_timeout = 300 ;
    if( event_tm_timeout >7200 ) event_tm_timeout = 7200 ;
    
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
