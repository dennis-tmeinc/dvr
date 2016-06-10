/* --- Changes ---
 * 09/25/2009 by Harrison
 *   1. Make this daemon
 *
 */

#include "dvr.h"
#include "sys/file.h"

#define DEAMON
char dvrconfigfile[] = CFG_FILE ;

static char deflogfile[] = "dvrlog.txt";
static string tmplogfile ;
static string logfile ;
static int    logfilesize ;	// maximum logfile size
static string lastrecdirbase;

char g_hostname[128] ;
pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;

static pthread_mutex_t dvr_mutex;
int system_shutdown;

int app_state;				// APPQUIT, APPUP, APPDOWN, APPRESTART

int g_lowmemory ;

// TVS variables
char g_mfid[32] ;
char g_serial[64] ;        // tvs: tvcs, pwii: unit#
char g_id1[64] ;           // tvs: medallion, pwii: vehicle id
char g_id2[64] ;            // tvs: licenseplate#, pwii: district

// pwii
#ifdef PWII_APP
char g_vri[128] ;           // VRI (video recording id) for PWII
string g_policeidlistfile ; // Police ID list filename
char g_policeid[32];        // Police ID for PWII
#endif

#ifdef TVS_APP
int  g_keycheck = 1 ;           // tvs : tvskeycheck
#else
int  g_keycheck = 0 ;           
#endif

void writeDebug(char *fmt, ...)
{
  va_list vl;
  char str[256];

  va_start(vl, fmt);
  vsprintf(str, fmt, vl);
  va_end(vl);

  FILE *fp = fopen("/var/dvr/debug.txt", "a");
  if (fp != NULL) {
    fprintf(fp, "%s\n", str);
    fclose(fp);
  }
}

void dvr_lock()
{
    pthread_mutex_lock(&dvr_mutex);
}

void dvr_unlock()
{
    pthread_mutex_unlock(&dvr_mutex);
}

// lock with lock variable
void dvr_lock( void * lockvar, int delayus )
{
    dvr_lock();
    while (*(int *)lockvar) {
        dvr_unlock();
        usleep( delayus );
        dvr_lock();
    }
    (*(int *)lockvar)++ ;
    dvr_unlock();
}

void dvr_unlock( void * lockvar )
{
    dvr_lock();
    (*(int *)lockvar)-- ;
    dvr_unlock();
}

// get a random number 
unsigned dvr_random()
{
    FILE * fd ;
    unsigned ran ;
    fd = fopen("/dev/urandom", "r");
    if( fd ) {
        fread( &ran, 1, sizeof(ran), fd );
        fclose( fd );
    }
    return ran ;
}

// clean log file
void dvr_cleanlogfile(const char * lfilename, int cutsize)
{
    int posr, posw, rsize ;
    FILE * flog ;
    flog = fopen(lfilename, "r+");
    if( flog==NULL ) {
        return ;
    }
    
    flock( fileno(flog), LOCK_EX );
    
    posw=ftell(flog);
    fseek( flog, 0, SEEK_END );
    posr = (int)ftell(flog) - cutsize ;
    if( posr > (cutsize/2) ) {
        char fbuf[4096] ;
        fseek( flog, posr, SEEK_SET );
        // skip one line
        fgets( fbuf, 4096, flog );
        while( (rsize=fread(fbuf, 1, 4096, flog))>0 ) {
            posr = ftell(flog) ;
            fseek( flog, posw, SEEK_SET );
            fwrite( fbuf, 1, rsize, flog );
            posw = ftell(flog) ;
            if( rsize<4096 ) {
                break;
            }
            fseek( flog, posr, SEEK_SET );
        }
        fseek( flog, posw, SEEK_SET );
        fflush( flog );
        ftruncate( fileno(flog), posw );
    }
    
    fflush( flog );
    flock( fileno(flog), LOCK_UN );
    fclose( flog );
}

const char *logfilelnk=VAR_DIR "/dvrlogfile" ;

// write log to log file.
// return
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(const char *fmt, ...)
{
    char lbuf[512];
    FILE *flog;
    FILE *ftmplog;
    time_t ti;
    int l;
    va_list ap ;

    flog=NULL ;
    // check logfile, it must be a symlink to hard disk
    if( readlink(logfilelnk, lbuf, 500)>10 ) {
        flog = fopen(logfilelnk, "a");
    }
    else {
		char * logdisk = disk_getlogdisk() ;
		if( logdisk != NULL ) {
			l = sprintf(lbuf, "%s/_%s_", (char *)logdisk, g_hostname);
			mkdir( lbuf, 0777 );
			
            sprintf(lbuf+l, "/%s", (char *)logfile);
            unlink(logfilelnk);
            if( symlink(lbuf, logfilelnk)==0 ) {		// success
                flog = fopen(logfilelnk, "a");
            }
        }
    }

    // copy over temperay logs
    if (flog) {
		
		flock( fileno(flog), LOCK_EX );
		    
        ftmplog = fopen(tmplogfile, "r");	// copy temperary log to logfile
        if (ftmplog) {
            while (fgets(lbuf, 512, ftmplog)) {
                fputs(lbuf, flog);
            }
            fclose(ftmplog);
            unlink(tmplogfile);
        }
        
        fflush( flog );
   		flock( fileno(flog), LOCK_UN );

    }

    ti = time(NULL);
    ctime_r(&ti, lbuf);
    l = strlen(lbuf);
    if (lbuf[l - 1] == '\n')
        l-- ;
    lbuf[l]=':' ;
    l++ ;
    va_start( ap, fmt );
    vsprintf( &(lbuf[l]), fmt, ap );
    va_end( ap );
    
    printf("%s\n", lbuf);

    if (flog) {
				
		flock( fileno(flog), LOCK_EX );
				
        fprintf(flog, "%s\n", lbuf);
        
        fflush( flog );
   		flock( fileno(flog), LOCK_UN );
   		
        if( fclose( flog )==0 ) {
            dvr_cleanlogfile(logfilelnk, logfilesize);
            return 1;
        }
    }

    // make sure no erro on link
    unlink(logfilelnk);

    // log to temperary log file
    flog = fopen(tmplogfile, "a");

    if (flog) {
		
		flock( fileno(flog), LOCK_EX );
				
        fprintf(flog, "%s *\n", lbuf);
        
        fflush( flog );
   		flock( fileno(flog), LOCK_UN );        
        fclose( flog ) ;
        dvr_cleanlogfile(tmplogfile, 100000);
    }

    return 0 ;
}

string g_keylogfile ;
char g_usbid[32] ;
static int keylogsettings ;

static FILE * dvr_logkey_file()
{
    static char logfilename[512] ;
    FILE * lfile=NULL ;
    if( logfilename[0]==0 ) {
		char * logdisk = disk_getlogdisk() ;
        if ( logdisk != NULL ) {
            sprintf(logfilename, "%s/_%s_/%s", (char *)logdisk, g_hostname, (char *)g_keylogfile);
            unlink( VAR_DIR "/tvslogfile" );
            symlink(logfilename, VAR_DIR "/tvslogfile" );
            dvr_cleanlogfile(logfilename, logfilesize);
            lfile=fopen(logfilename,"a");
        }
    }
    else {
        lfile=fopen(logfilename,"a");
    }
    if( lfile==NULL ) {
        logfilename[0]=0;
    }
    return lfile ;
}

static void dvr_logkey_settings()
{
    FILE * flog = dvr_logkey_file() ;
    if( flog==NULL ) {
        return ;
    }
    if( g_usbid[0] == 'I' && g_usbid[1] == 'N' ) {		// log installer only
        //			write down some settings changes
        string v ;
        config dvrconfig(dvrconfigfile);

#ifdef TVS_APP
        fprintf(flog, "\nTVS settings\n" );
        fprintf(flog, "\tMedallion #: %s\n", (char *)dvrconfig.getvalue("system", "tvs_medallion") );
        fprintf(flog, "\tController serial #: %s\n", (char *)dvrconfig.getvalue("system", "tvs_ivcs_serial") );
        fprintf(flog, "\tLicense plate #: %s\n", (char *)dvrconfig.getvalue("system", "tvs_licenseplate") );
#endif // TVS_APP

#ifdef PWII_APP
        fprintf(flog, "\n%s settings\n", APPNAME );
        fprintf(flog, "\tVehicle ID: %s\n", (char *)dvrconfig.getvalue("system", "id1") );
        fprintf(flog, "\tDistrict : %s\n", (char *)dvrconfig.getvalue("system", "id2") );
        fprintf(flog, "\tUnit #: %s\n", (char *)dvrconfig.getvalue("system", "serial") );
#endif

        fprintf(flog, "\tTime Zone : %s\n", (char *)dvrconfig.getvalue("system", "timezone") );
        for( int camera=1; camera<=2 ; camera++ ) {
            char cameraid[10] ;
            sprintf( cameraid, "camera%d", camera );
            fprintf(flog, "Camera %d settings\n", camera);
            if( dvrconfig.getvalueint(cameraid, "enable")) {
                fprintf(flog, "\tEnabled\n");
                fprintf(flog, "\tSerial #: %s\n", (char *)dvrconfig.getvalue(cameraid, "name") );
                for( int s=1; s<=6; s++) {
                    char sen[10] ;
                    sprintf(sen, "sensor%d", s);
                    fprintf(flog, "\tSensor (%s) trigger : ", (char *)dvrconfig.getvalue(sen, "name") );
                    sprintf(sen, "trigger%d", s);
                    int t = dvrconfig.getvalueint(cameraid, sen);
                    if( t&1 ) {
                        fprintf(flog, "On\n");
                    }
                    else if( t&2 ) {
                        fprintf(flog, "Off\n");
                    }
                    else if( (t&12)==12 ) {
                        fprintf(flog, "Turn On/Turn Off\n");
                    }
                    else if( t&4 ) {
                        fprintf(flog, "Turn On\n");
                    }
                    else if( t&8 ) {
                        fprintf(flog, "Turn Off\n");
                    }
                    else {
                        fprintf(flog, "None\n");
                    }
                }

            }
            else{
                fprintf(flog, "\tDisabled\n");
            }
        }
        keylogsettings = 0 ;
    }
    fclose( flog );
    return ;
}

void dvr_logkey( int op, struct key_data * key )
{
    static char TVSchecksum[16] ;
    char lbuf[512];
    FILE *flog ;
    time_t ti;
    int l;

    if( op==0  ) {     // disconnect
        if( g_usbid[0] ) {
            g_usbid[0]=0 ;
            unlink( VAR_DIR "/connectid" );
			//unlink( WWWSERIALFILE );			// remove setup serial no, so setup page would failed
            if( keylogsettings ) {
                dvr_logkey_settings();
            }
            flog = dvr_logkey_file();
            if( flog ) {
				// get time string
				ti = time(NULL);
				ctime_r(&ti, lbuf);
				l = strlen(lbuf);
				if (lbuf[l - 1] == '\n')
					lbuf[l - 1] = '\0';				
                fprintf(flog, "%s:Viewer connection closed, key ID: %s\n", lbuf, g_usbid );
                fclose( flog );
            }
        }
    }
    else if( op == 1 && key!=NULL ) {    // connecting
        if( strcmp( g_usbid, key->usbid )!=0 ||
            memcmp( TVSchecksum, key->checksum, sizeof( TVSchecksum ) )!=0 ) {
            memcpy( TVSchecksum, key->checksum, sizeof( TVSchecksum ) );
            strcpy( g_usbid, key->usbid );
            flog = fopen( VAR_DIR "/connectid", "w" ) ;
            if( flog ) {
                fprintf( flog, "%s", g_usbid );
                fclose( flog );
            }
            flog = dvr_logkey_file();
            if( flog ) {
				// get time string
				ti = time(NULL);
				ctime_r(&ti, lbuf);
				l = strlen(lbuf);
				if (lbuf[l - 1] == '\n')
					lbuf[l - 1] = '\0';				
                fprintf(flog, "\n%s:Viewer connected, key ID: %s\n%s",
                    lbuf, key->usbid, ((char *)&(key->size))+key->keyinfo_start );
                fclose( flog );
            }
        }
        if( keylogsettings ) {
            dvr_logkey_settings();
        }
    }
    else if( op == 2 ) {
        keylogsettings = 1 ;
    }

    return ;
}


int dvr_getsystemsetup(struct system_stru * psys)
{
    int i ;
    int x ;
    string tmpstr;
    char buf[40] ;
    string v ;
    config dvrconfig(dvrconfigfile);
    strcpy( psys->IOMod, "DIOTME");
    strncpy(psys->ServerName, g_hostname, 80);
    //	psys->cameranum = dvrconfig.getvalueint("system", "camera_number") ;
    //	psys->alarmnum = dvrconfig.getvalueint("system", "alarm_number") ;
    //	psys->sensornum = dvrconfig.getvalueint("system", "sensor_number") ;
    psys->cameranum = cap_channels;
    psys->alarmnum = num_alarms ;
    psys->sensornum = num_sensors ;
    psys->breakMode = 0 ;
    
    // maxfilelength ;
    v = dvrconfig.getvalue("system", "maxfilelength");
    if (sscanf(v.getstring(), "%d", &x)>0) {
        i = v.length();
        if (v[i - 1] == 'H' || v[i - 1] == 'h')
            x *= 3600;
        else if (v[i - 1] == 'M' || v[i - 1] == 'm')
            x *= 60;
    } else {
        x = DEFMAXFILETIME;
    }
    if (x < 60)
        x = 60;
    if (x > (24 * 3600))
        x = (24 * 3600);
    psys->breakTime = x ;
    
    // maxfilesize
    v = dvrconfig.getvalue("system", "maxfilesize");
    if (sscanf(v.getstring(), "%d", &x)>0) {
        i = v.length();
        if (v[i - 1] == 'M' || v[i - 1] == 'm')
            x *= 1024*1024;
        else if (v[i - 1] == 'K' || v[i - 1] == 'k')
            x *= 1024;
    } else {
        x = DEFMAXFILESIZE;
    }
    if (x < 1024*1024)
        x = 1024*1024;
    if (x > 1024*1024*1024)
        x = 1024*1024*1024;	
    psys->breakSize = x;
    
    // mindiskspace	
    v = dvrconfig.getvalue("system", "mindiskspace");
    if (sscanf(v.getstring(), "%d", &x)) {
        i = v.length();
        if (v[i - 1] == 'G' || v[i - 1] == 'g') {
            x *= 1024*1024*1024;
        } else if (v[i - 1] == 'K' || v[i - 1] == 'k') {
            x *= 1024;
        } else if (v[i - 1] == 'B' || v[i - 1] == 'b') {
            x = x;
        } else {		// M
            x *= 1024*1024;
        }
        if (x < 20*1024*1024)
            x = 20*1024*1024;
    } else {
        x = 100*1024*1024;
    }
    psys->minDiskSpace = x;
    
    psys->shutdowndelay = dvrconfig.getvalueint("system", "shutdowndelay") ;
    psys->autodisk[0]=0 ;
    if( psys->sensornum>16 )
        psys->sensornum=16 ;
    for( i=0; i<psys->sensornum; i++ ) {
        sprintf(buf, "sensor%d", i+1);
        tmpstr=dvrconfig.getvalue(buf, "name");
        if( tmpstr.length()==0 ) {
            strcpy( psys->sensorname[i], buf );
        }
        else {
            strcpy( psys->sensorname[i], tmpstr.getstring() );
        }
        psys->sensorinverted[i] = dvrconfig.getvalueint(buf, "inverted");
    }
    
    // set eventmarker
    psys->eventmarker=dvrconfig.getvalueint("eventmarker", "eventmarker") ;
    psys->eventmarker_enable=(psys->eventmarker>0) ;
    if( psys->eventmarker_enable ) {
        psys->eventmarker=1<<(psys->eventmarker-1) ;
    }
    else {
        psys->eventmarker=1 ;
    }

    psys->eventmarker_prelock=dvrconfig.getvalueint("eventmarker", "prelock" );
    psys->eventmarker_postlock=dvrconfig.getvalueint("eventmarker", "postlock" );
    
    psys->ptz_en=dvrconfig.getvalueint("ptz", "enable");
    tmpstr=dvrconfig.getvalue("ptz", "device");
    if( tmpstr.length()>9 ) {
        i=0 ;
        sscanf(tmpstr.getstring()+9, "%d", &i );
        psys->ptz_port=(char)i ;
    }
    else {
        psys->ptz_port=0 ;
    }
    psys->ptz_baudrate=dvrconfig.getvalueint("ptz", "baudrate" );
    tmpstr=dvrconfig.getvalue("ptz", "protocol");
    if( *tmpstr.getstring()=='P' )
        psys->ptz_protocol=1 ;
    else 
        psys->ptz_protocol=0 ;
    
    psys->videoencrpt_en=dvrconfig.getvalueint("system", "fileencrypt");
    strcpy( psys->videopassword, "********" );
    
    psys->rebootonnoharddrive=0 ;
    
    return 1 ;
}

int dvr_setsystemsetup(struct system_stru * psys)
{
    int i ;
    string tmpstr;
    char buf[40] ;
    char system[]="system" ;
    config dvrconfig(dvrconfigfile);
    
    if( strcmp( g_hostname, psys->ServerName )!=0 ) {	// set hostname
        sethostname(psys->ServerName, strlen(psys->ServerName)+1);
        dvrconfig.setvalue(system, "hostname", psys->ServerName);
    }
    
    dvrconfig.setvalueint(system, "camera_number", psys->cameranum) ;
    dvrconfig.setvalueint(system, "alarm_number", psys->alarmnum) ;
    dvrconfig.setvalueint(system, "sensor_number", psys->sensornum) ;
    dvrconfig.setvalueint(system, "maxfilelength", psys->breakTime) ;
    dvrconfig.setvalueint(system, "maxfilesize", psys->breakSize) ;
    dvrconfig.setvalueint(system, "mindiskspace", psys->minDiskSpace/(1024*1024)) ;
    dvrconfig.setvalueint(system, "shutdowndelay", psys->shutdowndelay) ;
    if( psys->sensornum>16 )
        psys->sensornum=16 ;
    for( i=0; i<psys->sensornum; i++ ) {
        sprintf(buf, "sensor%d", i+1);
        dvrconfig.setvalue(buf, "name", psys->sensorname[i]);
        dvrconfig.setvalueint(buf, "inverted", psys->sensorinverted[i]);
    }

    // event marker
    dvrconfig.setvalueint("eventmarker", "prelock", psys->eventmarker_prelock );
    dvrconfig.setvalueint("eventmarker", "postlock", psys->eventmarker_postlock );

    dvrconfig.setvalueint("eventmarker", "eventmarker",0);
    if( psys->eventmarker_enable ) {
        for( i=0; i< psys->sensornum; i++) {
            if( psys->eventmarker & (1<<i) ) {
                dvrconfig.setvalueint("eventmarker", "eventmarker", i+1 );
                break;
            }
        }
    }

    dvrconfig.setvalueint("ptz", "enable", (int)psys->ptz_en );
    sprintf(buf,"/dev/ttyS%d", (int)psys->ptz_port);
    dvrconfig.setvalue("ptz", "device", buf);
    dvrconfig.setvalueint("ptz", "baudrate", psys->ptz_baudrate );
    if( psys->ptz_protocol==0 ) {
        dvrconfig.setvalue("ptz", "protocol", "D");
    }
    else {
        dvrconfig.setvalue("ptz", "protocol", "P");
    }
    
    if( psys->videoencrpt_en ) {
        if( strcmp(psys->videopassword, "********")!=0 ) {		// if user enable encryption by accident, do change any thing
            // user set a password
            dvrconfig.setvalueint(system, "fileencrypt", 1) ;
            
            unsigned char filekey256[260] ;
            char filekeyc64[512] ;	// size should > 260*4/3
            
            key_256( psys->videopassword, filekey256 );		// hash password
            bin2c64(filekey256, 256, filekeyc64);	// convert to c64
            dvrconfig.setvalue("system", "filepassword", filekeyc64);	// save password to config file
        }
    }
    else {
        dvrconfig.setvalueint(system, "fileencrypt", 0) ;
        dvrconfig.removekey(system,"filepassword");	// remove password
    }
    
    psys->videoencrpt_en=dvrconfig.getvalueint("system", "fileencrypt");
    strcpy( psys->videopassword, "********" );
    
    dvrconfig.save();
    app_state = APPRESTART ;	// restart application
    return 1 ;
}

int app_restart;
unsigned long app_signalmap=0;
int app_signal_ex=0;

void sig_handler(int signum)
{
    if( signum<32 ) {
        app_signalmap |= ((unsigned long)1)<<signum;
    }
    else {
        app_signal_ex=signum ;
    }
}

void sig_check()
{
    unsigned long sigmap = app_signalmap ;
    app_signalmap=0 ;
    if( sigmap == 0 ) {
        return ;
    }
    
    if( sigmap & (1<<SIGTERM) ) 
    {
        dvr_log("Signal <SIGTERM> captured.");
        app_state = APPQUIT ;
    }
    else if( sigmap & (1<<SIGQUIT) ) 
    {
        dvr_log("Signal <SIGQUIT> captured.");
        app_state = APPQUIT ;
    }
    else if( sigmap & (1<<SIGINT) ) 
    {
        dvr_log("Signal <SIGINT> captured.");
        app_state = APPQUIT ;
    }
    else if( sigmap & (1<<SIGUSR1) ) 
    {
		dvr_log("Signal <SIGUSR1> captured.");
        app_state = APPDOWN ;
    }
    else if( sigmap & (1<<SIGUSR2) ) 
    {
		dvr_log("Signal <SIGUSR2> captured.");
       if(!isstandbymode()){
          app_state = APPRESTART ;
       }
    }
    
    if( sigmap & (1<<SIGPIPE) ) 
    {
        dvr_log("Signal <SIGPIPE> captured.");
    }
    
    if( app_signal_ex ) {
        dvr_log("Signal %d captured.", app_signal_ex );
        app_signal_ex=0 ;
    }

}

static char s_pidfile[] = "/var/dvr/dvrsvr.pid" ;

// app init / re-init
void app_init()
{
    static int app_start=0;
    string hostname ;
    string tz ;
    string tzi ;
    char * p ;		// general pointer
    FILE * fid = NULL ;

    config dvrconfig(dvrconfigfile);
    
	// setup log file names
	tmplogfile = dvrconfig.getvalue("system", "temp_logfile");
	if (tmplogfile.length() == 0) {
		tmplogfile = VAR_DIR "/logfile" ;
	}
	logfile = dvrconfig.getvalue("system", "logfile");
	if (logfile.length() == 0) {
		logfile = deflogfile;
	}
	logfilesize = dvrconfig.getvalueint("system", "logfilesize");
	if( logfilesize< (300*1024 )){
		logfilesize = 300*1024 ;
	} 
	else {
		if(logfilesize>(5*1024*1024) ) {
			logfilesize=5*1024*1024;
		}
	}
    
    // set timezone the first time
    tz=dvrconfig.getvalue( "system", "timezone" );
    if( tz.length()>0 ) {
        tzi=dvrconfig.getvalue( "timezones", tz.getstring() );
        if( tzi.length()>0 ) {
            p=strchr(tzi.getstring(), ' ' ) ;
            if( p ) {
                *p=0;
            }
            p=strchr(tzi.getstring(), '\t' ) ;
            if( p ) {
                *p=0;
            }
            setenv("TZ", tzi.getstring(), 1);
        }
        else {
            setenv("TZ", tz.getstring(), 1);
        }
    }
    tzset();   
 
    hostname=dvrconfig.getvalue("system", "hostname" );
    if( hostname.length()>0 ){
        // setup hostname
        sethostname( (char *)hostname, strlen( (char *)hostname )+1);
    }


#ifdef TVS_APP
	string t ;
	
    // TVS related
    t = dvrconfig.getvalue("system", "tvsmfid" );
    if( t.length()>0 ) {
        strncpy( g_mfid, t, sizeof(g_mfid) );
        fid=fopen(VAR_DIR "/tvsmfid", "w");
        if( fid ) {
            fprintf(fid, "%s", g_mfid );
            fclose(fid);
        }
    }
    g_keycheck = dvrconfig.getvalueint( "system", "tvskeycheck" );

    t = dvrconfig.getvalue("system","tvs_licenseplate");
    if( t.length()>0 ) {
         sprintf(g_id2, "%s", (char *)t );
    }

    t = dvrconfig.getvalue("system","tvs_medallion");
    if( t.length()>0 ) {
        sprintf(g_id1, "%s", (char *)t );
        sethostname( (char *)t, strlen( (char *)t )+1);
    }

    t = dvrconfig.getvalue("system","tvs_ivcs_serial");
    if( t.length()>0 ) {
         sprintf(g_serial, "%s%s", &g_mfid[2], (char *)t );
    }

    g_keylogfile = dvrconfig.getvalue("system","keylogfile");
    dvr_logkey( 2, NULL );	// try logdown settings
#endif


#ifdef PWII_APP
    // PWII related
    
    string t ;
    
    t = dvrconfig.getvalue("system", "mfid" );
    if( t.length()>0 ) {
        strncpy( g_mfid, t, sizeof(g_mfid) );
        fid=fopen(VAR_DIR "/mfid", "w");
        if( fid ) {
            fprintf(fid, "%s", g_mfid );
            fclose(fid);
        }
    }
    g_keycheck = dvrconfig.getvalueint( "system", "keycheck" );

    t = dvrconfig.getvalue("system","serial");
    if( t.length()>0 ) {
         sprintf(g_serial, "%s%s", &g_mfid[2], (char *)t );
    }

    t = dvrconfig.getvalue("system","id1");
    if( t.length()>0 ) {
        sprintf(g_id1, "%s", (char *)t );
        strcpy( g_hostname, g_id1 );
        sethostname( g_hostname, strlen( (char *)t )+1);
    }

    t = dvrconfig.getvalue("system","id2");
    if( t.length()>0 ) {
         sprintf(g_id2, "%s", (char *)t );
    }

    g_keylogfile = dvrconfig.getvalue("system","keylogfile");
    dvr_logkey( 2, NULL );	// try logdown settings

    // police id list file format:
    //          first line : current police ID, empty line indicate NO current ID (bypass)
    //          following lines: list of availabe police IDs, one ID perline, ID must start from first colume
    g_policeidlistfile = dvrconfig.getvalue("system", "pwii_policeidlistfile");
    g_policeid[0]=0;
    fid=fopen(g_policeidlistfile, "r");
    if( fid ) {
        fgets(g_policeid,sizeof(g_policeid),fid);
        fclose(fid);
    }

#endif

    gethostname( g_hostname, 128 );
	dvr_log("Setup hostname: %s", g_hostname);

    g_lowmemory=dvrconfig.getvalueint("system", "lowmemory" );
    if( g_lowmemory<10000 ) {
        g_lowmemory=10000 ;
    }

}

// app startup
void app_start() 
{
	config dvrconfig(dvrconfigfile);
	string pidfile ;
	FILE * fpid ;

	pidfile=dvrconfig.getvalue("system", "pidfile");
	if( pidfile.length()<=0 ) {
		pidfile=s_pidfile ;
	}
	fpid=fopen( (char *)pidfile, "w");
	if( fpid ) {
        fprintf(fpid, "%d", (int)getpid());
        fclose(fpid);
	}

	// setup signal handler	
	signal(SIGQUIT, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGPOLL, sig_handler);
	// ignor these signal
	signal(SIGPIPE, SIG_IGN);	  // ignor this
      
	dvr_log("Start DVR.");
	
	// ZEUS board init, moved here so it been call only once
    eagle_init ();
}

void app_exit()
{
	// ZEUS board finishing
	eagle_uninit();
	
	config dvrconfig(dvrconfigfile);
	string pidfile ;

	pidfile=dvrconfig.getvalue("system", "pidfile");
	if( pidfile.length()<=0 ) {
		pidfile=s_pidfile ;
	}
    unlink( pidfile );
    dvr_log("Quit DVR.\n");
}

void do_init()
{
    // initial mutex
    dvr_mutex = mutex_init ;
    
    app_init();			// reload settings
    mem_init();
    time_init();
    event_init();
    file_init();
    play_init();
    disk_init(0);
    cap_init();
    rec_init();
   // ptz_init();
#ifdef SUPPORT_SCREEN   
    screen_init();	
#endif    
    net_init();

    cap_start();	// start capture 
}

void do_uninit()
{
	
    dvr_log("Start un-initializing.");
    cap_stop();		// stop capture

    net_uninit();
#ifdef SUPPORT_SCREEN   
    screen_uninit();  
#endif    
   // ptz_uninit();
    rec_uninit();    
    cap_uninit();
    disk_uninit(0);   
    play_uninit();    
    file_uninit();
    event_uninit();
    time_uninit();
    mem_uninit();
    printf("uninitialized done\n");

    pthread_mutex_destroy(&dvr_mutex);
}

// dvr exit code
#define EXIT_NORMAL		(0)
#define EXIT_NOACT		(100)
#define EXIT_RESTART		(101)
#define EXIT_REBOOT		(102)
#define EXIT_POWEROFF		(103)

int main(int argc, char **argv)
{
    unsigned int serial=0;
    int app_ostate;	// application status. ( APPUP, APPDOWN )
    struct timeval time1, time2 ;
    int    t_diff ;
    
    // app startup
	app_start() ;
    
    app_ostate = APPDOWN ;
    app_state = APPUP ;

    while( app_state!=APPQUIT ) {
        if( app_state == APPUP ) {  // application up
            serial++ ;
            if( app_ostate != APPUP ) {
                do_init();
                app_ostate = APPUP ;
                gettimeofday(&time1, NULL);
            }
            if( (serial%40)==11 ){ // every 5 second
                // check memory
                if( mem_available () < g_lowmemory && disk_getbasedisk(0)!=NULL ) {
                    dvr_log("Memory low. reload DVR.");
                    app_state = APPRESTART ;
                }
				disk_check();
            }
            if( (serial%8)==3 ){	// every 1 second
                gettimeofday(&time2, NULL);
                t_diff = (int)(int)(time2.tv_sec - time1.tv_sec) ;
                if( t_diff<-100 || t_diff>100 )
                {
                    dvr_log("System time changing detected!");
                    rec_break ();
                }
                time1.tv_sec = time2.tv_sec ;
            }
            event_check();
            usleep( 125000 );
        }
        else if (app_state == APPDOWN ) {			// application down
            if( app_ostate == APPUP ) {
                app_ostate = APPDOWN ;
                do_uninit();
            }
            usleep( 12500 );
        }
        else if (app_state == APPRESTART ) {
            if( app_ostate == APPUP ) {
                app_ostate = APPDOWN ;
                do_uninit();
            }
            app_state = APPUP ;
			
			dvr_log("Reload DVR!");
            
            usleep( 100 );
        }
        sig_check();

        if(dio_poweroff()){
            if(fileclosed()){
                 dio_setfileclose(1);
            } else {
                 dio_setfileclose(0);
            }
        }

    }
    
    if( app_ostate==APPUP ) {
        app_ostate=APPDOWN ;
        do_uninit();
    }
    
    app_exit();
    
    if (system_shutdown) {        // requested system shutdown
        // dvr_log("Reboot system.");
        return EXIT_REBOOT ;
    }
    else {
        return EXIT_NOACT ;
    }
}
