#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

// Generate USB id string like: Ven_SanDisk&Prod_Cruzer_Mini&Rev_0.1\20041100531daa107a09

// Ven   :  /sys/block/sda/device/vendor
// Prod  :  /sys/block/sda/device/model
// Rev   :  /sys/block/sda/device/rev
// serial:  cd -P /sys/block/sda/device ; ../../../../serial
//          /sys/block/sda/device/../../../../serial

void trimtail(char * str)
{
    int t = strlen(str);
    while( t>0 ) {
        if( str[t-1]=='\n' || str[t-1]=='\r' || str[t-1]==' ' ) {
            t-- ;
            str[t]=0 ;
        }
        else {
            break;
        }
    }
}

// dev : sda, sdh...
// idbuf : for output
// return 1 : success, 0: failed
int getdiskid( char * dev , char * idbuf )
{
    FILE * sysfile ;
    int r ;
    char fname[128] ;
    char vendor[64] ;
    char product[64] ;
    char rev[64] ;
    char serial[64] ;

    // get vendor
    vendor[0]=0 ;
    sprintf( fname, "/sys/block/%s/device/vendor", dev );
    sysfile = fopen( fname, "r" );
    if( sysfile ) {
        fgets(vendor, 64, sysfile );
        fclose( sysfile );
    }
    else {
		return 0 ;
	}
    trimtail( vendor );
    
    // get product
    product[0] = 0 ;
    sprintf( fname, "/sys/block/%s/device/model", dev );
    sysfile = fopen( fname, "r" );
    if( sysfile ) {
        fgets(product, 64, sysfile );
        fclose( sysfile );
    }
    else {
		return 0 ;
	}    
    trimtail( product );
        
    // get rev
    rev[0] = 0 ;
    sprintf( fname, "/sys/block/%s/device/rev", dev );
    sysfile = fopen( fname, "r" );
    if( sysfile ) {
        fgets(rev, 64, sysfile );
        fclose( sysfile );
    }
    else {
		return 0 ;
	}       
    trimtail( rev );
    
    // get serial no
    serial[0]=0 ;
    sprintf( fname, "/sys/block/%s/device/../../../../serial", dev );
    sysfile = fopen( fname, "r" );
    if( sysfile ) {
        fgets(serial, 64, sysfile );
        fclose( sysfile );
    }
    else {
		return 0 ;
	}    
    trimtail( serial );

    sprintf( idbuf, "Ven_%s&Prod_%s&Rev_%s\\%s", vendor, product, rev, serial ) ;

    r = 0 ;
    while( 1 ) {
        if( idbuf[r] == 0 ) break ;
        else if( idbuf[r]==' ' ) {
            idbuf[r] = '_' ;
        }
        r++ ;
    }

    return 1 ;

}

// blowfish 
#include "blowfish.h"

static BLOWFISH_CTX ctx ;

void Blowfish_Init(BLOWFISH_CTX *ctx, unsigned char *key, int keyLen);
void Blowfish_Encrypt(BLOWFISH_CTX *ctx, unsigned long *xl, unsigned long *xr);
void Blowfish_Decrypt(BLOWFISH_CTX *ctx, unsigned long *xl, unsigned long *xr);

// init encryption key, salt is 64 bytes random data (include header 'OFID')
void DiskDataEncryptionInit( char * diskid, char * salt )
{
	int len = strlen(diskid);
	unsigned char key[512] ;
	memset( key, 0, 512 );
	memcpy( key, salt, 64 );
	memcpy( key+64, diskid, len );
	Blowfish_Init(&ctx, key, 64+len ) ;
}

// len must be blocks of 64bits (8 bytes)
int DiskDataEncrypt( char * data, int len )
{
	int i ;
	unsigned long * x = (unsigned long *) data ;
	len /= 2*sizeof(unsigned long) ;
	for( i=0; i<len; i++ ) {
		Blowfish_Encrypt( &ctx, x, x+1 );
		x+=2 ;
	}
	return 1 ;
}

int DiskDataDecrypt( char * data, int len )
{
	int i ;
	unsigned long * x = (unsigned long *) data ;
	len /= 2*sizeof(unsigned long) ;
	for( i=0; i<len; i++ ) {
		Blowfish_Decrypt( &ctx, x, x+1 );
		x+=2 ;
	}
	return 1 ;
}

//RC4 encryption 
#include "../dvrsvr/crypt.h" 

struct RC4_seed rc4seed ;

// init encryption key, salt is 64 bytes random data (include header 'OFID')
void DiskDataEncryptionInit_rc4( char * diskid, char * salt )
{
	unsigned char k[256] ;
	key_256( diskid, k );
	RC4_KSA( &rc4seed, k );
	RC4_KSA_A( &rc4seed, NULL, 213 );
	RC4_KSA_A( &rc4seed, (unsigned char *)salt, 64 );
	RC4_KSA_A( &rc4seed, NULL, 731 );
}

// len must be blocks of 64bits (8 bytes)
void DiskDataEncrypt_rc4( char * data, int len )
{
	RC4_crypt( (unsigned char *)data, len, &rc4seed ) ;
}

void DiskDataDecrypt_rc4( char * data, int len )
{
	RC4_crypt( (unsigned char *)data, len, &rc4seed ) ;
}


// data header on disk
struct officerid_header {
	char tag[4] ;				// tag of id block 'OFID"
	long si ;					// size of data ( officerid_t )
	long enctype ;				// 0: 
	long pad ;					// align structure
	char checksum[16] ;
	char salt[64] ;
	long data[1] ;
} ;

// Office ID data format
struct officerid_t {
	char officerid[32] ;
	char firstname[32] ;
	char lastname[32] ;
	char department[64] ;
	char badgenumber[32] ;
	char info[384] ;
	char res[8] ;				// can be more
} ;


// md5 checksum
// generate 16 bytes md5 checksum
void md5_checksum( char * checksum, unsigned char * data, int datalen );
// pre-picked ID data location, in unit of MB
static long IdOffset[8] = { 2, 220, 382, 674, 952, 1142, 1688, 2392 } ;

// dev: device name, key: key string(VEN_???), id: output readed id,  > 32 bytes buffer
// return 1: success, 0: failed
int readid( char * dev, char * key, char *id )
{
	int res = 0 ;
	int rb ;
	char buf[4096] ;
	officerid_header * xhead = (officerid_header *)buf ;
	FILE * f = fopen( dev, "rb" ) ;
	if( f ) {
		int s ;
		for( s=0; s<8; s++ ) {
			long offset = IdOffset[s] * 1024 * 1024 ;
			fseek( f, offset, SEEK_SET );
			rb = fread( buf, 1, 4096, f );
			if( rb==4096 ) {
				if( strncmp( xhead->tag, "OFID", 4 )==0 && xhead->si < 4000 && xhead->si > 96 ) {
					officerid_t * xid = (officerid_t *)xhead->data ;
					
					if( xhead->enctype == 1 ) {
						// decrypt
						DiskDataEncryptionInit( key, xhead->salt ) ;
						DiskDataDecrypt( (char *)xhead->data, xhead->si ) ;
					}
					else if( xhead->enctype == 2 ) {
						// decrypt
						DiskDataEncryptionInit_rc4( key, xhead->salt ) ;
						DiskDataDecrypt_rc4( (char *)xhead->data, xhead->si ) ;
					}
					
					// generate checksum
					char cksum[20] ;
					md5_checksum( cksum, (unsigned char *)xhead->data, xhead->si );
					if( memcmp(cksum, xhead->checksum, 16)==0 ) {		// checksum match?
						memcpy( id, xid->officerid, sizeof(xid->officerid) );
						res = 1 ;
						break ;
					}
				}
			}
			else {
				break ;
			}
		}
		fclose( f );
	}
	return res ;
}

int main(int argc, char *argv[])
{
	char serial[512] ;
	char id[64] ;
	if( getdiskid( argv[1], serial )) {
		char dev[64] ;
		sprintf( dev, "/dev/%s", argv[1] );
		if( readid( dev, serial, id ) ) {
			FILE * f = fopen( "/var/dvr/pwofficerid", "w" ) ;
			if( f ) {
				fputs( id, f );
				fclose( f );
			}
			printf("%s", id );
			return 0 ;
		}
	}
	return 1 ;
}
