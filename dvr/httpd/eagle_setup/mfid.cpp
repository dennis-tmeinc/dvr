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

#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/dvr.h"

char dvrconfigfile[]="/etc/dvr/dvr.conf" ;
char defconfigfile[]="/davinci/dvr/defconf" ;

extern void md5_checksum( char * checksum, unsigned char * data, int datalen );

int checktvskey( char * usbkeyserialid, struct key_data * tvskey, int keysize )
{
	char md5checksum[16] ;
	struct RC4_seed rc4seed ;
	unsigned char k256[256] ;

    key_256( usbkeyserialid, k256 ) ;
	RC4_KSA( &rc4seed, k256 );
	RC4_crypt( (unsigned char *)tvskey, keysize, &rc4seed ) ;

	if( tvskey->size<=0 || tvskey->size>(int)(keysize-sizeof(tvskey->checksum)) ) {
        return 0 ;
	}
	md5_checksum( md5checksum, (unsigned char *)(&(tvskey->size)), tvskey->size );
    if( memcmp( md5checksum, tvskey->checksum, 16 )==0 &&
		strcmp( tvskey->usbkeyserialid, usbkeyserialid )== 0 ) 
    {
		// key file verified
        return 1 ;
	}
    return 0 ;
}        
    
int main()
{
    char * id_filename ;
    FILE * id_file ;
    int res = 0 ;
    int dvrpid ;
    
    id_filename = getenv( "POST_FILE_mfid_file" );
    if( id_filename!=NULL ) {
        id_file = fopen( id_filename, "r" ) ;
        if( id_file ) {
            // now we check file format
            char * usbid = new char [400] ;
            char * c_key = new char [8192] ;
            struct key_data * key = (struct key_data *)c_key ;
            int keysize=0 ;
            fread( usbid, 1, 256, id_file ) ;
            usbid[256]=0;
            keysize = fread( c_key, 1, 8192, id_file ) ;
            if( checktvskey( usbid, key, keysize ) ) {
                if( key->usbid[0] == 'M' && key->usbid[1] == 'F' ) {
                    bin2c64((unsigned char *)(key->videokey), 256, usbid);		// convert to c64
                    
                    config defconfig(defconfigfile);    
#ifdef TVS_APP		                    
                    defconfig.setvalue( "system", "tvsmfid", key->usbid );
#endif // TVS_APP                    

#ifdef PWII_APP		                    
                    defconfig.setvalue( "system", "mfid", key->usbid );
#endif // PWII_APP                    

                    defconfig.setvalue("system", "filepassword", usbid);	// save password to config file
                    defconfig.save();

                    config dvrconfig(dvrconfigfile);    
#ifdef TVS_APP		                    
                    dvrconfig.setvalue( "system", "tvsmfid", key->usbid );
#endif // TVS_APP                    

#ifdef PWII_APP		                    
                    dvrconfig.setvalue( "system", "mfid", key->usbid );
#endif // PWII_APP                    

                    dvrconfig.setvalue("system", "filepassword", usbid);	// save password to config file
                    dvrconfig.save();
                    res=1 ;     // success ;
                }
            }
            delete usbid ;
            delete c_key ;
            fclose( id_file );
        }
    }
    
// output reports
    if( res ) {
        // success
        printf( "Manufacturer ID file upload success!" );
    }
    else {
        // failed
        printf( "Manufacturer ID file upload failed!" );
    }
    if( res ) {
        id_file = fopen( "/var/dvr/dvrsvr.pid", "r" ) ;
        if( id_file ) {
            dvrpid=0;
            fscanf(id_file, "%d", &dvrpid ) ;
            if( dvrpid>0 ) {
                kill( dvrpid, SIGUSR2 );        // reinitialize dvrsvr
            }
            fclose( id_file ) ;
        }
    }

    return 0;
}
