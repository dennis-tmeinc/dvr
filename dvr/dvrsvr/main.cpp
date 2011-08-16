
#include "dvr.h"
#include "../ioprocess/diomap.h"

static char deftmplogfile[] = "/tmp/dvrlog.txt";
static char deflogfile[] = "dvrlog.txt";
static string tmplogfile ;
static int    logfilesize ;	// maximum logfile size
string logfile ;
string g_servername ;

// pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;

char g_mfid[32] ;
int  g_keycheck ;           // tvs : tvskeycheck
char g_serial[64] ;        // tvs: tvcs, pwii: unit#
char g_id1[64] ;           // tvs: medallion, pwii: vehicle id
char g_id2[64] ;            // tvs: licenseplate#, pwii: district

// static pthread_mutex_t dvr_mutex;
int system_shutdown;

int app_state;				// APPQUIT, APPUP, APPDOWN, APPRESTART

int g_lowmemory ;

char g_vri[128] ;           // VRI (video recording id) for PWII
string g_policeidlistfile ; // Police ID list filename
char g_policeid[32];        // Police ID for PWII

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
void dvr_cleanlogfile(char * logfilename)
{
    int pos1, pos2, rsize ;
    FILE * flog ;
    flog = fopen(logfilename, "r+");
    if( flog==NULL ) 
        return ;
    fseek( flog, 0, SEEK_END );
    pos1 = ftell(flog);
    if( pos1>logfilesize ) {
        char fbuf[4096] ;
        fseek( flog, pos1-(logfilesize/3)*2, SEEK_SET );         // cut 1/3 log data
        // skip one line
        fgets( fbuf, 4096, flog );	// skip current line.
        pos2 = 0 ;
        while( (rsize=fread(fbuf, 1, 4096, flog))>0 ) {
            pos1 = ftell(flog) ;
            fseek( flog, pos2, SEEK_SET );
            fwrite( fbuf, 1, rsize, flog );
            pos2 = ftell(flog) ;
            if( rsize<4096 ) {
                break;
            }
            fseek( flog, pos1, SEEK_SET );
        }
        fseek( flog, pos2, SEEK_SET );
        fflush( flog );
        ftruncate( fileno(flog), pos2 );
    }
    fclose( flog );
}

// write log to log file. 
// return 
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(const char *fmt, ...)
{
    static char logfilename[512] ;
    int res = 0 ;
    char lbuf[512];
    FILE *flog=NULL;
    FILE *ftmplog=NULL;
    int rectemp=0 ;
    time_t ti;
    int l;
    va_list ap ;

    if( logfilename[0]==0 ) {
        if (rec_basedir.length() > 0) {
            sprintf(logfilename, "%s/_%s_/%s", (char *)rec_basedir, (char *)g_servername, (char *)logfile);
            unlink(VAR_DIR"/dvrlogfile");
            symlink(logfilename, VAR_DIR"/dvrlogfile");
            dvr_cleanlogfile(logfilename);
        }
    }

    flog = fopen(logfilename, "a");
    if (flog) {
        ftmplog = fopen(tmplogfile, "r");	// copy temperary log to logfile        
        if (ftmplog) {
            fputs("\n", flog);
            while (fgets(lbuf, 512, ftmplog)) {
                fputs(lbuf, flog);
            }
            fputs("\n", flog);
            fclose(ftmplog);
            unlink(tmplogfile);
        }
        res=1 ;
    } else {
        logfilename[0]=0 ;
        flog = fopen(tmplogfile, "a");        
        rectemp=1 ;
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
    net_dprint("%s\n", lbuf );
    if (flog) {
        fprintf(flog, lbuf);
        if( rectemp ) {
            fprintf(flog, " *\n");		
        }
        else {
            fprintf(flog, "\n");		
        }
        fclose(flog);
    }
    va_end( ap );
    return res ;
}

static string keylogfile ;
char g_usbid[32] ;
static int keylogsettings ;

static FILE * dvr_logkey_file()
{
    static char logfilename[512] ;
    FILE * lfile=NULL ;
    if( logfilename[0]==0 ) {
        if (rec_basedir.length() > 0) {
            sprintf(logfilename, "%s/_%s_/%s", (char *)rec_basedir, (char *)g_servername, (char *)keylogfile);
            unlink(VAR_DIR"/tvslogfile" );
            symlink(logfilename, VAR_DIR"/tvslogfile" );
            dvr_cleanlogfile(logfilename);
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
        config dvrconfig(CFG_FILE);
        
#ifdef TVS_APP
        fprintf(flog, "\nTVS settings\n" );
        fprintf(flog, "\tMedallion #: %s\n", dvrconfig.getvalue("system", "tvs_medallion") );
        fprintf(flog, "\tController serial #: %s\n", dvrconfig.getvalue("system", "tvs_ivcs_serial") );
        fprintf(flog, "\tLicense plate #: %s\n", dvrconfig.getvalue("system", "tvs_licenseplate") );
#endif // TVS_APP

#ifdef PWII_APP
        fprintf(flog, "\nPWII settings\n" );
        fprintf(flog, "\tVehicle ID: %s\n", dvrconfig.getvalue("system", "id1") );
        fprintf(flog, "\tDistrict : %s\n", dvrconfig.getvalue("system", "id2") );
        fprintf(flog, "\tUnit #: %s\n", dvrconfig.getvalue("system", "serial") );
#endif					

        fprintf(flog, "\tTime Zone : %s\n", dvrconfig.getvalue("system", "timezone") );
        for( int camera=1; camera<=2 ; camera++ ) {
            char cameraid[10] ;
            sprintf( cameraid, "camera%d", camera );
            fprintf(flog, "Camera %d settings\n", camera);
            if( dvrconfig.getvalueint(cameraid, "enable")) {
                fprintf(flog, "\tEnabled\n");
                fprintf(flog, "\tSerial #: %s\n", dvrconfig.getvalue(cameraid, "name") );
                for( int s=1; s<=6; s++) {
                    char sen[10] ;
                    sprintf(sen, "sensor%d", s);
                    fprintf(flog, "\tSensor (%s) trigger : ", dvrconfig.getvalue(sen, "name") );
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
    file_close( flog );
    return ;
}

void dvr_logkey( int op, struct key_data * key ) 
{
    static char TVSchecksum[16] ;
    char lbuf[512];
    FILE *flog ;
    time_t ti;
    int l;

    // get time string
    ti = time(NULL);
    ctime_r(&ti, lbuf);
    l = strlen(lbuf);
    if (lbuf[l - 1] == '\n')
        lbuf[l - 1] = '\0';

    if( op==0  ) {     // disconnect
        if( g_usbid[0] ) {
            g_usbid[0]=0 ;
            unlink( VAR_DIR"/connectid" );
            unlink( WWWSERIALFILE );			// remove setup serial no, so setup page would failed
            if( keylogsettings ) {
                dvr_logkey_settings();
            }
            flog = dvr_logkey_file();
            if( flog ) {
                fprintf(flog, "%s:Viewer connection closed, key ID: %s\n", lbuf, g_usbid );
                file_close( flog );
            }
        }
    }
    else if( op == 1 && key!=NULL ) {    // connecting
        if( strcmp( g_usbid, key->usbid )!=0 ||
            memcmp( TVSchecksum, key->checksum, sizeof( TVSchecksum ) )!=0 ) {
            memcpy( TVSchecksum, key->checksum, sizeof( TVSchecksum ) );
            strcpy( g_usbid, key->usbid );
            flog = fopen(VAR_DIR"/connectid", "w" ) ;
            if( flog ) {
                fprintf( flog, "%s", g_usbid );
                fclose( flog );
            }
            flog = dvr_logkey_file();
            if( flog ) {
                fprintf(flog, "\n%s:Viewer connected, key ID: %s\n%s", 
                    lbuf, key->usbid, ((char *)&(key->size))+key->keyinfo_start );
                file_close( flog );
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
    const char * pv ;
    config dvrconfig(CFG_FILE);

#ifdef MDVR_APP    
    strcpy(  psys->productid, "MDVR");
#endif    

#ifdef TVS_APP    
    strcpy(  psys->productid, "TVS" );
#endif    
   
#ifdef PWII_APP
    // for PWII, this field will be used to store pwii info
    strcpy(  psys->productid, "PWII" );
#endif
    
    strncpy( psys->serial, g_serial, sizeof(psys->serial) );
    strncpy( psys->ID1, g_id1, sizeof(psys->ID1) );
    strncpy( psys->ID2, g_id2, sizeof(psys->ID2) );
    
    strncpy(psys->ServerName, (char *)g_servername, sizeof(psys->ServerName));

    psys->cameranum = cap_channels;
    psys->alarmnum = num_alarms ;
    psys->sensornum = num_sensors ;
    psys->breakMode = 0 ;
    
    // maxfilelength ;
    pv = dvrconfig.getvalue("system", "maxfilelength");
    if (sscanf(pv, "%d", &x)>0) {
        i = strlen(pv);
        if (pv[i - 1] == 'H' || pv[i - 1] == 'h')
            x *= 3600;
        else if (pv[i - 1] == 'M' || pv[i - 1] == 'm')
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
    pv = dvrconfig.getvalue("system", "maxfilesize");
    if (sscanf(pv, "%d", &x)>0) {
        i = strlen(pv);
        if (pv[i - 1] == 'M' || pv[i - 1] == 'm')
            x *= 1024*1024;
        else if (pv[i - 1] == 'K' || pv[i - 1] == 'k')
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
    pv = dvrconfig.getvalue("system", "mindiskspace");
    if (sscanf(pv, "%d", &x)) {
        i = strlen(pv);
        if (pv[i - 1] == 'G' || pv[i - 1] == 'g') {
            x *= 1024*1024*1024;
        } else if (pv[i - 1] == 'K' || pv[i - 1] == 'k') {
            x *= 1024;
        } else if (pv[i - 1] == 'B' || pv[i - 1] == 'b') {
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
            strcpy( psys->sensorname[i], tmpstr );
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
        sscanf((char *)tmpstr+9, "%d", &i );
        psys->ptz_port=(char)i ;
    }
    else {
        psys->ptz_port=0 ;
    }
    psys->ptz_baudrate=dvrconfig.getvalueint("ptz", "baudrate" );
    tmpstr=dvrconfig.getvalue("ptz", "protocol");
    if( *(char *)tmpstr=='P' )
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
#ifdef MDVR_APP    
	
    int i ;
    string tmpstr;
    char buf[40] ;
    char system[]="system" ;
    config dvrconfig(CFG_FILE);
    
    if( strcmp( (char *)g_servername, psys->ServerName )!=0 ) {	// set hostname
		g_servername = psys->ServerName ;
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
            char filekeyc64[512] ;								// size should > 260*4/3
            
            key_256( psys->videopassword, filekey256 );			// hash password
            bin2c64(filekey256, 256, filekeyc64);				// convert to c64
            dvrconfig.setvalue("system", "filepassword", filekeyc64);	// save password to config file
        }
    }
    else {
        dvrconfig.setvalueint(system, "fileencrypt", 0) ;
        dvrconfig.removekey(system,"filepassword");				// remove password
    }
    
    psys->videoencrpt_en=dvrconfig.getvalueint("system", "fileencrypt");
    strcpy( psys->videopassword, "********" );
    
    dvrconfig.save();
    app_state = APPRESTART ;			// restart application	

#endif    		// MDVR
	
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
#ifdef EAGLE34
        app_state = APPDOWN ;       // APPQUIT make system reset in eagle34 board, so we just do APPDOWN
#else	// EAGLE34
        app_state = APPQUIT ;
#endif	// EAGLE34
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
    else if( sigmap & (1<<SIGUSR2) ) 
    {
        app_state = APPRESTART ;
    }
    else if( sigmap & (1<<SIGUSR1) ) 
    {
        app_state = APPDOWN ;
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

static string pidfile ;
// one time application initialization
void app_init( config & dvrconfig )
{
    static int app_start=0;
    string t ;
    string tz ;
    string tzi ;
    char * p ;					// general pointer
    FILE * fid = NULL ;
    
    pid_t mypid ;
    mypid=getpid();
    
    // make var directory ("/var/dvr") if it is not there.
    mkdir( VAR_DIR, 0777 );
    
    pidfile=dvrconfig.getvalue("system", "pidfile");
    if( pidfile.length()<=0 ) {
        pidfile=VAR_DIR"/dvrsvr.pid" ;
    }
    
    fid=fopen(pidfile, "w");
    if( fid ) {
        fprintf(fid, "%d", (int)mypid);
        fclose(fid);
    }
    
    // setup log file names
    tmplogfile = dvrconfig.getvalue("system", "temp_logfile");
    if (tmplogfile.length() == 0)
        tmplogfile = deftmplogfile;
    logfile = dvrconfig.getvalue("system", "logfile");
    if (logfile.length() == 0)
        logfile = deflogfile;
    logfilesize = dvrconfig.getvalueint("system", "logfilesize");
    if( logfilesize< (20*1024 ) || logfilesize>(20*1024*1024) ) {
        logfilesize = 100*1024 ;
    }
    
    // set timezone the first time
    tz=dvrconfig.getvalue( "system", "timezone" );
    if( tz.length()>0 ) {
        tzi=dvrconfig.getvalue( "timezones", tz );
        if( tzi.length()>0 ) {
            p=strchr(tzi, ' ' ) ;
            if( p ) {
                *p=0;
            }
            p=strchr(tzi, '\t' ) ;
            if( p ) {
                *p=0;
            }
            setenv("TZ", tzi, 1);
        }
        else {
            setenv("TZ", tz, 1);
        }
    }

#ifdef MDVR_APP   
    t=dvrconfig.getvalue("system", "hostname" );
    if( t.length()>0 ){
		g_servername=t ;
    }
#endif
    
#ifdef TVS_APP    
    // TVS related
    t = dvrconfig.getvalue("system", "tvsmfid" );
    if( t.length()>0 ) {
        strncpy( g_mfid, t, sizeof(g_mfid) );
        fid=fopen(VAR_DIR"/tvsmfid", "w");
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
		g_servername = t ;
    }
    
    t = dvrconfig.getvalue("system","tvs_ivcs_serial");
    if( t.length()>0 ) {
         sprintf(g_serial, "%s%s", &g_mfid[2], (char *)t );
    }

    keylogfile = dvrconfig.getvalue("system","keylogfile");
	dvr_logkey( 2, NULL );	// try logdown settings
#endif

#ifdef PWII_APP
        // PWII related
    t = dvrconfig.getvalue("system", "mfid" );
    if( t.length()>0 ) {
        strncpy( g_mfid, t, sizeof(g_mfid) );
        fid=fopen(VAR_DIR"/mfid", "w");
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
		g_servername = t ;
    }

    t = dvrconfig.getvalue("system","id2");
    if( t.length()>0 ) {
         sprintf(g_id2, "%s", (char *)t );
    }

    keylogfile = dvrconfig.getvalue("system","keylogfile");
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
        str_trimtail(g_policeid);
    }

#endif

	// setup hostname
	sethostname( (char *)g_servername, g_servername.length()+1);
	dvr_log("Setup hostname: %s", (char *)g_servername);
	
    g_lowmemory=dvrconfig.getvalueint("system", "lowmemory" );
    if( g_lowmemory<10000 ) {
        g_lowmemory=10000 ;
    }
    
    // setup signal handler	
    signal(SIGQUIT, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);
    signal(SIGPOLL, sig_handler);
	// ignor these signal
    signal(SIGPIPE, SIG_IGN);		// ignor this

    if( app_start==0 ) {
        dvr_log("Start DVR.");
        app_start=1 ;
    }
}

void app_exit()
{
    unlink( pidfile );
    dvr_log("Quit DVR.\n");
}

void do_init()
{
    config dvrconfig(CFG_FILE);
    dvr_log("Start initializing.");

    app_init(dvrconfig);
    
    time_init();
    event_init(dvrconfig);
    disk_init(dvrconfig);
    file_init(dvrconfig);
    play_init(dvrconfig);
    cap_init(dvrconfig);
    rec_init(dvrconfig);
    ptz_init(dvrconfig);
    screen_init(dvrconfig);	
    net_init(dvrconfig);

    cap_start();	// start capture 
}

void do_uninit()
{

    dvr_log("Start un-initializing.");
    cap_stop();		// stop capture
    
    net_uninit();
    screen_uninit();
    ptz_uninit();
    rec_uninit();
    cap_uninit();
    play_uninit();
    file_uninit();
    disk_uninit();

    // at this point, memory pool should be clean
    if( g_memused > 100 ) {
        dvr_log("Unsolved memory leak, request to restart system" );
        dio_setstate( DVR_FAILED ) ;
        sync();
        sleep(60);
        app_state = APPQUIT ;
    }
    
    event_uninit();
    time_uninit();

}

// dvr exit code
#define EXIT_NORMAL		(0)
#define EXIT_NOACT		(100)
#define EXIT_RESTART		(101)
#define EXIT_REBOOT		(102)
#define EXIT_POWEROFF		(103)

int main()
{
    int app_ostate;				// application status. ( APPUP, APPDOWN )

    // initial mutex
//    memcpy( &dvr_mutex, &mutex_init, sizeof(mutex_init));
    
    mem_init();
    
    app_ostate = APPDOWN ;
    app_state = APPUP ;
    
    while( app_state!=APPQUIT ) {
        if( app_state == APPUP ) {					// application up
            if( app_ostate != APPUP ) {
                do_init();
                app_ostate = APPUP ;
            }
            time_tick();
            if(event_check()==0) {
                usleep(1000);
            }
        }
        else if (app_state == APPDOWN ) {			// application down
            if( app_ostate == APPUP ) {
                app_ostate = APPDOWN ;
                do_uninit();
            }
            sleep(1);                               // will wake up on signal.
        }
        else if (app_state == APPRESTART ) {
            app_state = APPUP ;
            if( app_ostate == APPUP ) {
                do_uninit();
                app_ostate = APPDOWN ;
            }
        }
        sig_check();
    }
    
    if( app_ostate==APPUP ) {
        app_ostate=APPDOWN ;
        do_uninit();
    }
    
    app_exit();
    mem_uninit ();

//    pthread_mutex_destroy(&dvr_mutex);

    if (system_shutdown) {              // requested system shutdown
        // dvr_log("Reboot system.");
        return EXIT_REBOOT ;
    }
    else {
        return EXIT_NOACT ;
    }
}
