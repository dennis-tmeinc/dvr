
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "crypt.h"

/*
keyfile format

	struct keyfile {
		unsigned char key[256] ;			// random key
		unsigned char keyfilename[64] ;		// key file name
		unsigned char keyfile[2048-256-64] ;	// encrypted key file content
		unsigned char disk[2048] ;			// encrypted disk id, (first 4 sector), all zero means no disk check
	}
*/

struct key_file_t {
	unsigned char key[256] ;			// random key
	char          idfilename[64] ;		// id file name
	unsigned char idfile[2048-256-64] ;	// encrypted id file content
	unsigned char disk[2048] ;			// encrypted disk id, (first 4 sector), all zero means no disk check
} keyfile ;

struct RC4_seed seed ;

unsigned char buffer[2048] ;

// usage:  usbgenkey <idfile> [device] [appid] [keyfilename]
//         <idfile> id file need be check, it is also autorun file
//         [device] should be the device contain hd id. (/dev/sda, /dev/sdb, ... ), if it is "none" then no disk check
//         [appid]  application id. (ex: MDVR5104).
//         [keyfilename] key file, default: usbkey
int main(int argc, char * argv[])
{
    int fd ;
	int r ;
	int checkdisk ;

	char *idfile ;
	char *device="none";
	char *appid="MDVRUSBKEY";
	char *keyfilename="usbkey";

	if( argc<2 ) {
		printf("Usage: usbgenkey <idfile> [device] [appid] [keyfilename]\n");
		printf("\t<idfile> auto run file;\n");
		printf("\t[device] usb disk to check. (/dev/sda, /dev/sdb). \"none\" for no disk check;\n");
		printf("\t[appid] application id.\n");
		printf("\t[keyfilename] generate this key file. (default:usbkey)\n");
		return 1;
	}
	else {
		idfile=argv[1] ;
		if( argc>=3) {
			device=argv[2];
		}
		if( argc>=4) {
			appid=argv[3];
		}
		if( argc>=5) {
			keyfilename=argv[4];
		}
	}

	// generate random key
	fd = open( "/dev/urandom", O_RDONLY );
	if( fd<=0 ) {
		printf("Error! Can't open /dev/urandom.\n");
		return 1;
	}
	read( fd, keyfile.key, sizeof(keyfile.key) );
	close( fd );

	// read id file
	strncpy( keyfile.idfilename, idfile, sizeof(keyfile.idfilename));
	fd = open( keyfile.idfilename, O_RDONLY );
	if( fd<=0 ) {
		printf("Error! Can't open id file :%s\n", keyfile.idfilename );
		return 1 ;
	}
	memset( keyfile.idfile, 0x55, sizeof( keyfile.idfile ) );
	memset( buffer, 0xaa, sizeof( keyfile.idfile ) );
	while( read( fd, buffer, sizeof(keyfile.idfile) )>0 ) {
		for( r=0; r<sizeof(keyfile.idfile); r++ ) {
			keyfile.idfile[r] ^= buffer[r] ;
		}
		memset( buffer, 0xaa, sizeof( keyfile.idfile ) );
	}
	close( fd );

	// read disk id
	if( device[0] == 'n' || device[0] == 'N' || device[0]=='0' ) {
		memset( keyfile.disk, 0, sizeof( keyfile.disk ) );
	}
	else {
		fd = open( device, O_RDONLY ) ;
		if( fd<=0 ) {
			printf("Error! Can't open disk device: %s\n", device );
			return 1;
		}
		r = read( fd, keyfile.disk, sizeof( keyfile.disk) );
		close( fd );
		if( r!=sizeof(keyfile.disk) ) {
			printf("Error! Can't read disk device: %s\n", device );
			return 1;
		}
		// see if disk id content is good?
		checkdisk=0 ;
		for( r=1024; r<1536; r++) {
			if( keyfile.disk[r]!=0 ) {
				checkdisk=1;
				break;
			}
		}
		if( !checkdisk ) {		// 3rd sector is empty
			// put some random data to 3rd sector of disk as disk id
			fd = open( "/dev/urandom", O_RDONLY );
			read( fd, &(keyfile.disk[1038]), 20 );
			close( fd );

			// write back disk id to disk
			fd = open( device, O_WRONLY ) ;
			if( fd<=0 ) {
				printf("Error! Can't open disk device for writing: %s\n", device );
				return 1;
			}
			write(fd, keyfile.disk, sizeof( keyfile.disk) );
			close( fd );
		}
	}

    // generate 256 bytes key array on to 'buffer'
	key_256( appid, buffer );

	// mix app key with keyfile key
	for(r=0; r<=256; r++) {
		buffer[r] ^= keyfile.key[r]; 
	}

	// encrypt key file
	RC4_KSA( &seed, buffer );
	RC4_crypt( (unsigned char *)keyfile.idfilename, sizeof(keyfile)-256, &seed);

	// write key file
	fd = open(keyfilename, O_WRONLY|O_CREAT, 0644);
	if( fd<=0 ) {
		printf("Error! Can't create key file: %s\n", keyfilename );
		return 1;
	}
	r=write(fd, &keyfile, sizeof(keyfile));
	close( fd );
	if( r!=sizeof(keyfile) ){
		printf("Error! Writing keyfile: %s\n", argv[3] );
		return 1;
	}

	printf("Success!\n");
	return 0;
}
		
