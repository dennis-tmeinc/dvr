#include "../../cfg.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

int decode(const char * in, char * out, int osize );
char * getquery( const char * qname );

int savequery( char * valuefile )
{
    char qvalue[1024] ;
    char * qp ;
    char * query ;
    int x ;
    int item=0;
    FILE * sf ;
    sf = fopen( valuefile, "w" ) ;
    if( sf==NULL ) 
        return 0;
    
    // JSON head
    fprintf( sf, "{" );
    
    query = getenv("QUERY_STRING");
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( x>1 ) {
             qp=strchr(qvalue,'=');
            if( qp ) {
                *qp=0 ;
                qp++;
                if( strcmp(qvalue, "page")!=0 ) {
                    fprintf( sf, "\"%s\":\"%s\",", qvalue,  qp );
                    item++;
                }
            }
        }
        if( query[x]=='\0' ) 
            break;
        else {
            query+=x+1;
        }
    }
    query = getenv("POST_STRING" );
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( x>1 ) {
            qp=strchr(qvalue,'=');
            if( qp ) {
                *qp=0 ;
                qp++;
                if( strcmp(qvalue, "page")!=0 ) {
                    fprintf( sf, "\"%s\":\"%s\",", qvalue,  qp );
                    item++;
                }
            }
        }
        if( query[x]=='\0' ) 
            break;
        else {
            query+=x+1;
        }
    }
    fprintf( sf, "\"eobj\":\"\"}" );
    fclose( sf );
    return item ;
}

void system_savevalue()
{
    savequery("system_value");
}

void camera_savevalue()
{
    char * v ;
    char fcam_name[100] ;
    
    v = getquery( "cameraid" );
    if( v== NULL ) 
        return ;
   
    sprintf( fcam_name, "camera_value_%s", v );
    savequery( fcam_name );
    
    v = getquery( "nextcameraid" );
    if( v ) {
        // make a sym link to camera_value
        sprintf(fcam_name, "camera_value_%s", v );
        unlink("camera_value");
        symlink(fcam_name, "camera_value");
    }
}

void sensor_savevalue()
{
    savequery("sensor_value");
}

void network_savevalue()
{
    savequery( "network_value" );
}

#ifdef POWERCYCLETEST         
void cycletest_savevalue()
{
    savequery( "cycletest_value" );
}
#endif
            
int main()
{
    // check originated page
    char * page = getquery("page");
    if( page ) {
        if( strcmp(page, "system" )==0 ) {
            system_savevalue();
        }
        else if( strcmp( page, "camera" )==0 ) {
            camera_savevalue();
        }
        else if( strcmp( page, "sensor" )==0 ) {
            sensor_savevalue();
        }
        else if( strcmp( page, "network" )==0 ) {
            network_savevalue();
       }
#ifdef POWERCYCLETEST         
        else if( strcmp( page, "cycletest" )==0 ) {
            cycletest_savevalue();
       }
#endif        
        else {
            savequery(page);
        }
    }
    return 0;
}
