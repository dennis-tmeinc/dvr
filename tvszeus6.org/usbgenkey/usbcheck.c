
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "crypt.h"

struct key_file_t {
	unsigned char key[256] ;			// random key
	unsigned char idfilename[64] ;		// id file name
	unsigned char idfile[2048-256-64] ;	// encrypted id file content
	unsigned char disk[2048] ;			// encrypted disk id, (first 4 sector), all zero means no disk check
} keyfile ;

struct RC4_seed seed ;

unsigned char buffer[2048] ;

// usage:  usbcheck <device> <keyfile> [appid]
//         <device> should be the device contain hd id. (/dev/sda, /dev/sdb, ... )
//         <keyfile> key file to check.
//         [appid]   application id. used to verify system board. (for example: MDVR5104)
// output:
//         output id filename to stdout. "id filename" may used as auto-executing filename
// return: 0 = usb verify good
//         1 = error (no stdout output)
int main(int argc, char * argv[])
{
    int fd ;
	int r ;
	int checkdisk ;
	
	char * device="none" ;
    char * keyfilename="usbkey" ;
    char * appid="MDVRUSBKEY" ;
    
    if( argc>1 ) {
        device=argv[1];
    }
    if( argc>2 ) {
        keyfilename=argv[2];
    }
    if( argc>3 ) {
        appid=argv[3];
    }

    // read keyfile
	fd = open( keyfilename, O_RDONLY ) ;		// open key file
	if( fd<=0 ) {
		return 1 ;
	}
	r = read( fd, &keyfile, sizeof(keyfile) );
	close( fd );
	if( r != (int)sizeof(keyfile) ) {
		return 1 ;
	}

    // generate 256 bytes key into 'buffer'
    key_256( appid, buffer );
        
	// mix app key with keyfile key
	for(r=0; r<=256; r++) {
		keyfile.key[r]^=buffer[r] ;
	}

	// decrypt key file
	RC4_KSA( &seed, keyfile.key );
	RC4_crypt( keyfile.idfilename, sizeof(keyfile)-256, &seed);

	// read id file
	fd = open( keyfile.idfilename, O_RDONLY );
	if( fd<=0 ) {
		return 1 ;
	}
	memset( buffer, 0xaa, sizeof( keyfile.idfile ) );
	while( read( fd, buffer, sizeof(keyfile.idfile) )>0 ) {
		for( r=0; r<sizeof(keyfile.idfile); r++ ) {
			keyfile.idfile[r] ^= buffer[r] ;
		}
		memset( buffer, 0xaa, sizeof( keyfile.idfile ) );
	}
	close( fd );

	// now keyfile.idfile should be all 0x55
	for( r=0; r<sizeof(keyfile.idfile); r++ ) {
		if( keyfile.idfile[r]!=0x55 ) {
			return 1;
		}
	}

	// check disk id
	checkdisk = 0 ;
	for( r=0; r<sizeof(keyfile.disk); r++ ) {
		if( keyfile.disk[r]!=0 ) {
			checkdisk=1;				// need to check disk
			break;
		}
	}
	
	// check disk content
	if( checkdisk ) {
		fd = open( device, O_RDONLY );
		if( fd <=0 ) {
			return 1 ;
		}
		r =	read( fd, buffer, sizeof(keyfile.disk) );
		close( fd );
		if( r!=(int)sizeof(keyfile.disk) ){
			return 1 ;
		}
		if( memcmp( buffer, keyfile.disk, sizeof(keyfile.disk) )!=0 ) {
			return 1 ;
		}
	}

	// every thing goood.
	// output id file name
	printf("%s", keyfile.idfilename);
	return 0;
}

