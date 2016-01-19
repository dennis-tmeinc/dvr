/* --- Changes ---
 * 09/25/2009 by Harrison
 *   1. Make this daemon
 *
 */

#include "dvr.h"

#define DEAMON
char dvrconfigfile[] = "/etc/dvr/dvr.conf";

static char deftmplogfile[] = "/tmp/dvrlog.txt";
static char deflogfile[] = "dvrlog.txt";
static string tmplogfile ;
static string logfile ;
static int    logfilesize ;	// maximum logfile size
static string lastrecdirbase;

char g_hostname[128] ;
pthread_mutex_t mutex_init=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP ;
char g_mfid[32] ;
int  g_keycheck ;           // tvs : tvskeycheck
char g_serial[64] ;        // tvs: tvcs, pwii: unit#
char g_id1[64] ;           // tvs: medallion, pwii: vehicle id
char g_id2[64] ;            // tvs: licenseplate#, pwii: district

char mac_addr[128];
char ip_addr[128];
char phone_number[128];


static pthread_mutex_t dvr_mutex;
int system_shutdown;

int app_state;				// APPQUIT, APPUP, APPDOWN, APPRESTART

int g_lowmemory ;

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

void dvr_cleanlog1(FILE * flog)
{
    int i ;
    array <string> alog ;
    string line ;
    char lbuf[1024]; 
    int pos = ftell(flog);
  //  printf("current pos:%d\n",pos);
    if( pos>logfilesize ) {
        pos = (pos/4) ;	// cut 1/4 file
        fseek( flog, pos, SEEK_SET );
	//printf("after seek:%d %d\n",pos,ftell(flog));
        fgets(lbuf, 1024, flog );
        while (fgets(lbuf, 1024, flog)) {
	    line=lbuf;
            if( line.length()>2 ) {
                alog.add( line );
            }
        }	        
        fseek( flog, 0, SEEK_SET ) ;
	ftruncate( fileno(flog), ftell(flog) );
	
	//printf("alog.size=%d\n",alog.size());
        for( i=0; i<alog.size(); i++ ) {
            fputs(alog[i].getstring(), flog);
        }
      //  ftruncate( fileno(flog), ftell(flog) );
    }
}

// clean log file
void dvr_cleanlog(char* logfilename)
{
    int i,pos;
    FILE* flog;
    array <string> alog ;
    string line ;
    char lbuf[1024]; 
    flog=fopen(logfilename,"r+");
    if(flog==NULL)
      return;
    fseek(flog,0,SEEK_END);
    pos = ftell(flog);
  //  printf("current pos:%d\n",pos);
    if( pos>logfilesize ) {
        pos = (pos/5) ;	// cut 1/5 file
        fseek( flog, pos, SEEK_SET );
	//printf("after seek:%d %d\n",pos,ftell(flog));
        fgets(lbuf, 1024, flog );
        while (fgets(lbuf, 1024, flog)) {
	    line=lbuf;
            if( line.length()>2 ) {
                alog.add( line );
            }
        }	   
       // printf("size:%d\n",alog.size());
        if(alog.size()>10){
	    ftruncate( fileno(flog), 0);	
	    fseek( flog, 0, SEEK_SET ) ;		
	    for( i=0; i<alog.size(); i++ ) {
		fputs(alog[i].getstring(), flog);
	    }
	}
     //   printf("final length:%d\n",ftell(flog));
    }
    fflush(flog);
    fclose(flog);
}
// write log to log file. 
// return 
//       1: log to recording hd
//       0: log to temperary file
int dvr_log(char *fmt, ...)
{
    int res = 0 ;
    static char logfilename[512] ;
    char lbuf[512];
    FILE *flog=NULL;
    FILE *ftmplog=NULL;
    int rectemp=0 ;
    time_t ti;
    int l;
    va_list ap ;
    tmplogfile="/var/dvr/dvrlog";
    if (rec_basedir.length() > 0) {
        sprintf(logfilename, "%s/%s", rec_basedir.getstring(), logfile.getstring());
        if(strcmp(lastrecdirbase.getstring(),rec_basedir.getstring())!=0){
             if(lastrecdirbase.length()>3)
                  unlink("/var/dvr/dvrlogfile");
             lastrecdirbase= rec_basedir.getstring();
             symlink(logfilename, "/var/dvr/dvrlogfile");
	     dvr_cleanlog(logfilename); 
        }      
    }
    
    flog = fopen(logfilename, "a");   
    if (flog) {
        ftmplog = fopen("/var/dvr/dvrlog1", "r");	// copy temperary log to disk
        if (ftmplog) {	  
            fputs("\n", flog);
            while (fgets(lbuf, 512, ftmplog)) {
                fputs(lbuf, flog);
            }
            fclose(ftmplog);
            unlink("/var/dvr/dvrlog1");
        }            
        ftmplog = fopen(tmplogfile.getstring(), "r");	// copy temperary log to disk
        if (ftmplog) {	  
            fputs("\n", flog);
            while (fgets(lbuf, 512, ftmplog)) {
                fputs(lbuf, flog);
            }
            fputs("\n", flog);
            fclose(ftmplog);
            unlink(tmplogfile.getstring());
        }
        res=1 ;
    } else {
        flog = fopen(tmplogfile.getstring(), "a");
        rectemp=1 ;
    }

    ti = time(NULL);
    ctime_r(&ti, lbuf);
    l = strlen(lbuf);
    if (lbuf[l - 1] == '\n')
        lbuf[l - 1] = '\0';
    va_start( ap, fmt );
    printf("DVR:%s:", lbuf);
    vprintf(fmt, ap );
    printf("\n");
    if (flog) {
        fprintf(flog, "%s:", lbuf);
        vfprintf(flog, fmt, ap );
        if( rectemp ) {
            fprintf(flog, " *\n");		
        }
        else {
            fprintf(flog, "\n");		
        }
        if( fclose(flog)!=0 ) {
            res = 0 ;
        }
    //don't abuse this,system can hang:sync();
    }
    va_end( ap );
    return res ;
}

int  eventmarker_log()
{
    int l,i;
    struct dvrtime tnow ; 
    struct dvrtime motionstart;
    int basecamera;
    char filename[512];
    double lati, longi ;
    double lati_o,longi_o;   
    int east, north ;
    int latDeg,lonDeg;
    int logsendneed=0;
    int firstincluded=0;

    float latmin,lonmin;    
    FILE* fp=NULL;
    char message[512];
    struct DvrChannel_attr m_attr;
    int cap_ch = cap_channels ;
    if(rec_basedir.length()<5){
       return 0;       
    }
    if(!gps_location (&lati, &longi)){
      lati=0.0;
      longi=0.0;
    }
	    
    lati_o=lati;
    longi_o=longi;
    if( lati>=0.0 ) {
	north='N' ;
    }
    else {
	lati=-lati ;
	north='S' ;
    }
    if( longi>=0.0 ) {
	east='E' ;
    }
    else {
	longi=-longi ;
	east='W' ;
    }	
    latDeg=lati;
    lonDeg=longi;
    latmin=(lati-latDeg)*60;
    lonmin=(longi-lonDeg)*60;
    fp=fopen("/var/dvr/modeminfo","r"); 
    if(fp){
      //fgets(phone_number,128,fp);
      fscanf(fp,"%s\n",&phone_number);
      //fgets(ip_addr,128,fp);     
      fscanf(fp,"%s\n",&ip_addr);
      //fgets(mac_addr,128,fp);
      fscanf(fp,"%s\n",&mac_addr);
      fclose(fp);
      fp=NULL;
    } else {
      sprintf(phone_number,"") ;
      sprintf(ip_addr,"");
      sprintf(mac_addr,"");
    }

    sprintf(filename,"%s/event",rec_basedir.getstring());
    mkdir(filename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
    l=strlen(filename);
    time_now(&tnow);    
    sprintf(filename+l,"/%04d%02d%02d",
	    tnow.year,
	    tnow.month,
	    tnow.day );    
    
    mkdir(filename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
    l = strlen(filename);
    sprintf(filename + l, 
	    "/Driveby_%04d%02d%02d%02d%02d%02d_L_%s.emk", 
			    tnow.year,
			    tnow.month,
			    tnow.day,
			    tnow.hour,
			    tnow.minute,
			    tnow.second,
			    g_hostname); 
   // dvr_log("filename:%s",filename);
    fp=fopen(filename,"w+t");
    if(fp){
        fprintf(fp,"GPS:%c%d\xb0%f %c%d\xb0%f\n",north,latDeg,latmin,east,lonDeg,lonmin);
	fprintf(fp,"GPS_D:%f %f\n",lati_o,longi_o);
	fprintf(fp,"Date and Time:%02d/%02d/%04d %02d:%02d:%02d\n",
	         tnow.month,
		 tnow.day,
		 tnow.year,
		 tnow.hour,
		 tnow.minute,
		 tnow.second
	);
	fprintf(fp,"Client Name:%s\n",g_id2);
	fprintf(fp,"BUS ID:%s\n",g_hostname);
	fprintf(fp,"MAC ID:%s\n",mac_addr);
	fprintf(fp,"IP Address:%s\n",ip_addr);
	fprintf(fp,"Tel:%s\n",phone_number);
	//print the channel number with cross by triggering
	fprintf(fp,"Channels:");
	if(cap_channel[0]->enabled()&&cap_channel[0]->eventMarkerRecordingEnabled()){
	      if(cap_channel[0]->emkSendEnabled()){
	         fprintf(fp,"00"); 
		 firstincluded=1;
	      }
	}
        for (i = 1; i < cap_ch; i++) {
	    if(cap_channel[i]->enabled()&&cap_channel[i]->eventMarkerRecordingEnabled()){
	       if(cap_channel[i]->emkSendEnabled()){
		  if(firstincluded)
		    fprintf(fp,",%02d",i);
		  else {
		    fprintf(fp,"%02d",i);
		    firstincluded=1;
		  }
	       }
	    }
        }
        fprintf(fp,"\n");
        for (i = 0; i < cap_ch; i++) {
	    if(cap_channel[i]->enabled()&&cap_channel[i]->eventMarkerRecordingEnabled()){
	          if(cap_channel[i]->emkSendEnabled()){
		      logsendneed=1;
		      cap_channel[i]->getattr(&m_attr);
		      fprintf(fp,"CH%02d:%s %04d%02d%02d%02d%02d%02d%03d\n",i,m_attr.CameraName,
			  tnow.year,
			  tnow.month,
			  tnow.day,
			  tnow.hour,
			  tnow.minute,
			  tnow.second,
			  tnow.milliseconds
		      );	 
		  }
	    }
        }
        for(i=0; i<num_sensors; i++) {
	    if( sensors[i]->value() ) {
		fprintf(fp,"Sensor%d:%s\n",i,sensors[i]->name());
	    }
        }
        fclose(fp);
	if(logsendneed){
	  message[0]=0;
	  strcpy(message+1,filename);
	  send_ipc_message_to_modem(ID_DVRSVR,115,strlen(filename)+1,(unsigned char*)message);
	}
    }    
}

int  gcrash_log()
{
    int l,i;
    struct dvrtime tnow ; 
    struct dvrtime motionstart;
    int basecamera;
    char filename[512];
    double lati, longi ;
    double lati_o,longi_o;   
    int east, north ;
    int latDeg,lonDeg;

    int logsendneed=0;
    int firstincluded=0;
    
    float latmin,lonmin;    
    FILE* fp=NULL;
    char message[512];
    struct DvrChannel_attr m_attr;
    int cap_ch = cap_channels ;
    if(rec_basedir.length()<5){
       return 0;       
    }
    if(!gps_location (&lati, &longi)){
      lati=0.0;
      longi=0.0;
    }
	    
    lati_o=lati;
    longi_o=longi;
    if( lati>=0.0 ) {
	north='N' ;
    }
    else {
	lati=-lati ;
	north='S' ;
    }
    if( longi>=0.0 ) {
	east='E' ;
    }
    else {
	longi=-longi ;
	east='W' ;
    }	
    latDeg=lati;
    lonDeg=longi;
    latmin=(lati-latDeg)*60;
    lonmin=(longi-lonDeg)*60;
    fp=fopen("/var/dvr/modeminfo","r"); 
    if(fp){
      //fgets(phone_number,128,fp);
      fscanf(fp,"%s\n",&phone_number);
      //fgets(ip_addr,128,fp);     
      fscanf(fp,"%s\n",&ip_addr);
      //fgets(mac_addr,128,fp);
      fscanf(fp,"%s\n",&mac_addr);
      fclose(fp);
      fp=NULL;
    } else {
      sprintf(phone_number,"") ;
      sprintf(ip_addr,"");
      sprintf(mac_addr,"");
    }

    sprintf(filename,"%s/event",rec_basedir.getstring());
    mkdir(filename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
    l=strlen(filename);
    time_now(&tnow);    
    sprintf(filename+l,"/%04d%02d%02d",
	    tnow.year,
	    tnow.month,
	    tnow.day );    
    
    mkdir(filename, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH);
    l = strlen(filename);
    sprintf(filename + l, 
	    "/Driveby_%04d%02d%02d%02d%02d%02d_L_%s.crh", 
			    tnow.year,
			    tnow.month,
			    tnow.day,
			    tnow.hour,
			    tnow.minute,
			    tnow.second,
			    g_hostname); 
   // dvr_log("filename:%s",filename);
    fp=fopen(filename,"w+t");
    if(fp){
        fprintf(fp,"GPS:%c%d\xb0%f %c%d\xb0%f\n",north,latDeg,latmin,east,lonDeg,lonmin);
	fprintf(fp,"GPS_D:%f %f\n",lati_o,longi_o);
	fprintf(fp,"Date and Time:%02d/%02d/%04d %02d:%02d:%02d\n",
	         tnow.month,
		 tnow.day,
		 tnow.year,
		 tnow.hour,
		 tnow.minute,
		 tnow.second
	);
	fprintf(fp,"Client Name:%s\n",g_id2);
	fprintf(fp,"BUS ID:%s\n",g_hostname);
	fprintf(fp,"MAC ID:%s\n",mac_addr);
	fprintf(fp,"IP Address:%s\n",ip_addr);
	fprintf(fp,"Tel:%s\n",phone_number);
	//print the channel number with cross by triggering
	fprintf(fp,"Channels:");
	if(cap_channel[0]->enabled()&&cap_channel[0]->gforceCrushRecordingEnabled()){
	     if(cap_channel[0]->gforceSendEnabled()){
		  fprintf(fp,"00");  
		  firstincluded=1;
	     }
	}
        for (i = 1; i < cap_ch; i++) {
	    if(cap_channel[i]->enabled()&&cap_channel[i]->gforceCrushRecordingEnabled()){
	        if(cap_channel[i]->gforceSendEnabled()){
		   if(firstincluded)
		     fprintf(fp,",%02d",i);
		   else{
		     fprintf(fp,",%02d",i);
		     firstincluded=1;
		   }
		}
	    }
        }
        fprintf(fp,"\n");
        for (i = 0; i < cap_ch; i++) {
	    if(cap_channel[i]->enabled()&&cap_channel[i]->gforceCrushRecordingEnabled()){
	          if(cap_channel[i]->gforceSendEnabled()){
		      logsendneed=1;
		      cap_channel[i]->getattr(&m_attr);
		      fprintf(fp,"CH%02d:%s %04d%02d%02d%02d%02d%02d%03d\n",i,m_attr.CameraName,
			  tnow.year,
			  tnow.month,
			  tnow.day,
			  tnow.hour,
			  tnow.minute,
			  tnow.second,
			  tnow.milliseconds
		      );	
		  }
	    }
        }
        for(i=0; i<num_sensors; i++) {
	    if( sensors[i]->value() ) {
		fprintf(fp,"Sensor%d:%s\n",i,sensors[i]->name());
	    }
        }
        fclose(fp);
	if(logsendneed){
	  message[0]=0;
	  strcpy(message+1,filename);
	  send_ipc_message_to_modem(ID_DVRSVR,115,strlen(filename)+1,(unsigned char*)message);
	}
    }    
}

static string keylogfile ;
char g_usbid[32] ;
static int keylogsettings ;

static FILE * dvr_logkey_file()
{
    static char logfilename[512] ;
    FILE * flog ;

    flog = file_open(logfilename, "a");
    if ( flog==NULL ) {
        if (rec_basedir.length() > 0) {
            sprintf(logfilename, "%s/%s", rec_basedir.getstring(), keylogfile.getstring());
            symlink(logfilename, "/var/dvr/tvslogfile" );
        }
    }
    else {
        file_close( flog );
    }

    return file_open(logfilename, "a");

}

static void dvr_logkey_settings()
{
    FILE * flog = dvr_logkey_file() ;
    if( flog==NULL ) {
        return ;
    }
    dvr_lock();
    if( g_usbid[0] == 'I' && g_usbid[1] == 'N' ) {		// log installer only 
        //			write down some settings changes
        string v ;
        config dvrconfig(dvrconfigfile);
        fprintf(flog, "\nTVS settings\n" );
        fprintf(flog, "\tMedallion #: %s\n", dvrconfig.getvalue("system", "tvs_medallion").getstring() );
        fprintf(flog, "\tController serial #: %s\n", dvrconfig.getvalue("system", "tvs_ivcs_serial").getstring() );
        fprintf(flog, "\tLicense plate #: %s\n", dvrconfig.getvalue("system", "tvs_licenseplate").getstring() );
				
        fprintf(flog, "\tTime Zone : %s\n", dvrconfig.getvalue("system", "timezone").getstring() );
        for( int camera=1; camera<=4 ; camera++ ) {
            char cameraid[10] ;
            sprintf( cameraid, "camera%d", camera );
            fprintf(flog, "Camera %d settings\n", camera);
            if( dvrconfig.getvalueint(cameraid, "enable")) {
                fprintf(flog, "\tEnabled\n");
                fprintf(flog, "\tSerial #: %s\n", dvrconfig.getvalue(cameraid, "name").getstring() );
                for( int s=1; s<=6; s++) {
                    char sen[10] ;
                    sprintf(sen, "sensor%d", s);
                    fprintf(flog, "\tSensor (%s) trigger : ", dvrconfig.getvalue(sen, "name").getstring() );
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
    dvr_unlock();
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
            unlink( "/var/dvr/connectid" );
            if( keylogsettings ) {
                dvr_logkey_settings();
            }
            flog = dvr_logkey_file();
            if( flog ) {
                dvr_lock();
                fprintf(flog, "%s:Viewer connection closed, key ID: %s\n", lbuf, g_usbid );
                dvr_unlock();
                file_close( flog );
            }
        }
    }
    else if( op == 1 && key!=NULL ) {    // connecting
        if( strcmp( g_usbid, key->usbid )!=0 ||
            memcmp( TVSchecksum, key->checksum, sizeof( TVSchecksum ) )!=0 ) {
            memcpy( TVSchecksum, key->checksum, sizeof( TVSchecksum ) );
            strcpy( g_usbid, key->usbid );
            flog = fopen("/var/dvr/connectid", "w" ) ;
            if( flog ) {
                fprintf( flog, "%s", g_usbid );
                fclose( flog );
            }
            flog = dvr_logkey_file();
            if( flog ) {
                dvr_lock();
                fprintf(flog, "\n%s:Viewer connected, key ID: %s\n%s", 
                    lbuf, key->usbid, ((char *)&(key->size))+key->keyinfo_start );
                dvr_unlock();
                l = ftell(flog) ;
                if( l>logfilesize ) {
                    dvr_cleanlog1( flog );
                }
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
    config dvrconfig(dvrconfigfile);
    strcpy(  psys->productid, "TVS" );

    strncpy( psys->serial, g_serial, sizeof(psys->serial) );
    strncpy( psys->ID1, g_id1, sizeof(psys->ID1) );
    strncpy( psys->ID2, g_id2, sizeof(psys->ID2) );

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
        FILE * phostname = NULL;
        phostname=fopen("/etc/dvr/hostname", "w");
        if( phostname!=NULL ) {
            fprintf(phostname, "%s", psys->ServerName);
            fclose(phostname);
        }
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
    else if( sigmap & (1<<SIGUSR2) ) 
    {
       if(!isstandbymode()){
          app_state = APPRESTART ;
       }
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
void app_init(int partial)
{
    static int app_start=0;
    string hostname ;
    string tz ;
    string tzi ;
    char * p ;		// general pointer
    FILE * fid = NULL ;
    char mbuf[256];    
    config dvrconfig(dvrconfigfile);
   
    if (!partial) {
      pid_t mypid ;
      mypid=getpid();
      
      pidfile=dvrconfig.getvalue("system", "pidfile");
      if( pidfile.length()<=0 ) {
        pidfile="/var/dvr/dvrsvr.pid" ;
      }
      
      fid=fopen(pidfile.getstring(), "w");
      if( fid ) {
        fprintf(fid, "%d", (int)mypid);
        fclose(fid);
      }
    }
    
    // setup log file names
    tmplogfile = dvrconfig.getvalue("system", "temp_logfile");
    if (tmplogfile.length() == 0)
        tmplogfile = deftmplogfile;
    logfile = dvrconfig.getvalue("system", "logfile");
    if (logfile.length() == 0)
        logfile = deflogfile;
    logfilesize = dvrconfig.getvalueint("system", "logfilesize");
    if( logfilesize< (300*1024 )){
        logfilesize = 300*1024 ;
    } else {
	if(logfilesize>(10*1024*1024) ) {
	   logfilesize=10*1024*1024;
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
 #if 1
    fid=fopen("/mnt/nand/dvr/firmwareid","r");
    if(fid){
       int num;
       num=fread(mbuf,1,256,fid); 
       fclose(fid);
       mbuf[num-1]='\0';
       dvr_log("DVR Firmware:%s",mbuf);
    }

#endif    
#if 0
    hostname=dvrconfig.getvalue("system", "hostname" );
    if( hostname.length()>0 ){
        // setup hostname
        strncpy( g_hostname, hostname.getstring(), 127 );
        sethostname( g_hostname, strlen(g_hostname)+1);
        gethostname( g_hostname, 128 );
	if (!partial) 
	  dvr_log("Setup hostname: %s", g_hostname);
    }
#else
    hostname = dvrconfig.getvalue("system", "tvsmfid" );
    if( hostname.length()>0 ) {
        strncpy( g_mfid, hostname.getstring(), sizeof(g_mfid) );
        fid=fopen("/var/dvr/tvsmfid", "w");
        if( fid ) {
            fprintf(fid, "%s", g_mfid );
            fclose(fid);
        }
    }
    g_keycheck = dvrconfig.getvalueint( "system", "tvskeycheck" );
    
    hostname = dvrconfig.getvalue("system","tvs_licenseplate");
    if( hostname.length()>0 ) {
         sprintf(g_id2, "%s", hostname.getstring() );
    }

    hostname = dvrconfig.getvalue("system","tvs_medallion");
    if( hostname.length()>0 ) {
        sprintf(g_id1, "%s", hostname.getstring() );

        // setup hostname, make hostname same as medallion number
        strncpy( g_hostname, hostname.getstring(), 127 );
        sethostname( g_hostname, strlen(g_hostname)+1);
        gethostname( g_hostname, 128 );
        dvr_log("Setup hostname: %s", g_hostname);
    }
    
    hostname = dvrconfig.getvalue("system","tvs_ivcs_serial");
    if( hostname.length()>0 ) {
         sprintf(g_serial, "%s%s", &g_mfid[2], hostname.getstring() );
    }
   
    keylogfile = dvrconfig.getvalue("system","keylogfile");
    dvr_logkey( 2, NULL );	// try logdown settings
#endif    
    g_lowmemory=dvrconfig.getvalueint("system", "lowmemory" );
    if( g_lowmemory<10000 ) {
        g_lowmemory=10000 ;
    }
    
    if (!partial) {
      // setup signal handler	
      signal(SIGQUIT, sig_handler);
      signal(SIGINT, sig_handler);
      signal(SIGTERM, sig_handler);
      signal(SIGUSR1, sig_handler);
      signal(SIGUSR2, sig_handler);
      signal(SIGPOLL, sig_handler);
      // ignor these signal
      signal(SIGPIPE, SIG_IGN);	  // ignor this
      
      if( app_start==0 ) {
        dvr_log("Start DVR.");
        app_start=1 ;
      }
    }
}

void app_exit(int silent)
{
  //sync();
    unlink( pidfile.getstring() );
    if (!silent)
      dvr_log("Quit DVR.\n");
}

void do_init()
{
    // initial mutex
    memcpy( &dvr_mutex, &mutex_init, sizeof(mutex_init));
    
    dvr_log("Start initializing.");

    app_init(0);
        
    time_init();
    event_init();
    file_init();
    play_init();
    disk_init(0);
    cap_init();
    rec_init();
   // ptz_init();
    screen_init();	
    msg_init();
    net_init();

    cap_start();	// start capture 
}

void do_uninit()
{
    dvr_log("Start un-initializing.");
    cap_stop();		// stop capture

    net_uninit();
    msg_uninit();
    screen_uninit();  
   // ptz_uninit();
    rec_uninit();    
    cap_uninit();
    disk_uninit(0);   
    play_uninit();    
    file_uninit();
    event_uninit();
    time_uninit();
    printf("uninitialized done\n");
    //sleep(30);
    pthread_mutex_destroy(&dvr_mutex);
}

// dvr exit code
#define EXIT_NORMAL		(0)
#define EXIT_NOACT		(100)
#define EXIT_RESTART		(101)
#define EXIT_REBOOT		(102)
#define EXIT_POWEROFF		(103)

void closeall(int fd)
{
  int fdlimit = sysconf(_SC_OPEN_MAX);

  while (fd < fdlimit)
    close(fd++);
}

int daemon(int nochdir, int noclose)
{
  switch (fork()) {
  case 0: break;
  case -1: return -1;
  default: _exit(0);
  }

  if (setsid() < 0)
    return -1;

  switch (fork()) {
  case 0: break;
  case -1: return -1;
  default: _exit(0);
  }

  if (!nochdir)
    chdir("/");

  if (!noclose) {
    closeall(0);
    open("/dev/null", O_RDWR);
    dup(0); dup(0);
  }

  return 0;
}

//int foo=1;
int main(int argc, char **argv)
{
    unsigned int serial=0;
    int app_ostate;	// application status. ( APPUP, APPDOWN )
    struct timeval time1, time2 ;
    int    t_diff ;

    //while(foo)
    //sleep(1);

    if ((argc >= 2) && !strcmp(argv[1], "-f")) {
      // force foreground
    } else if ((argc >= 2) && !strcmp(argv[1], "-d")) {
      app_init(1);
      dio_init();
      int ret = do_disk_check();
      dio_uninit();
      app_exit(1);
      exit(ret);
    } else {
#ifdef DEAMON
      if (daemon(1, 0) < 0) {
	perror("daemon");
	exit(1);
      }
 #endif
    }

    mem_init();
    
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
            if( (serial%400)==119 ){ // every 3 second
                // check memory
                if( mem_available () < g_lowmemory && rec_basedir.length()>1 ) {
                    dvr_log("Memory low. reload DVR.");
                    app_state = APPRESTART ;
                }
            }
            if( (serial%80)==3 ){	// every 1 second
                gettimeofday(&time2, NULL);
                t_diff = (int)(int)(time2.tv_sec - time1.tv_sec) ;
                if( t_diff<-100 || t_diff>100 )
                {
                    dvr_log("System time changing detected!");
                    rec_break ();
                }
                time1.tv_sec = time2.tv_sec ;
		disk_check();
 
            }
            if( serial%10 == 1 ) {
                event_check();
            }
            screen_io(12500);	// do screen io
//            usleep( 12500 );
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
    FiniSystem();      
    app_exit(0);
    mem_uninit ();
    
    if (system_shutdown) {        // requested system shutdown
        // dvr_log("Reboot system.");
        return EXIT_REBOOT ;
    }
    else {
        return EXIT_NOACT ;
    }
}