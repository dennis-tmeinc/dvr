
#include <stdio.h>
#include <string.h>

/* typedef a 32 bit type */
typedef unsigned long int UINT4;

/* Data structure for MD5 (Message Digest) computation */
typedef struct {
  UINT4 i[2];                   /* number of _bits_ handled mod 2^64 */
  UINT4 buf[4];                                    /* scratch buffer */
  unsigned char in[64];                              /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
} MD5_CTX;

void MD5Init ( MD5_CTX * mdContext);
void MD5Update (MD5_CTX * mdContext, unsigned char * inBuf, unsigned int inLen);
void MD5Final (MD5_CTX * mdContext);

static unsigned char md5_init_table[16] = 
{
    0xee, 0xca, 0x3e, 0xd5, 0x4c, 0xf7, 0xa6, 0x11, 0x77, 0xe5, 0x25, 0x8a, 0xa0, 0x45, 0x24, 0x8a 
} ;

int md5check( char * checkfilename, int print )
{
    int fileadded=0;
    int res = 0 ;
    MD5_CTX mdctx ;
    unsigned char checksum[16] ;
    char fbuf[512] ;
    FILE * checkfile ;
    int i, j, l ;
    checkfile = fopen( checkfilename, "rb" );
    if( checkfile ) {
        MD5Init( &mdctx );
        MD5Update( &mdctx, md5_init_table, 16 );
        while( fgets(fbuf, 512, checkfile ) ) {
            if( strncasecmp( fbuf, "checksum:", 9 )==0 ) {
                MD5Final( &mdctx );
                if( print && fileadded>0 ) {
                    for( i=0; i<16; i++ ) {
                        printf("%02x", mdctx.digest[i] );
                    }
                    printf("\n");
                    break ;
                }
                j=9 ;
                while( fbuf[j]<=' ' ) j++ ;
                for( i=0; i<16; i++ ) {
                    sscanf( &fbuf[j], "%02x", &l );
                    j+=2 ;
                    checksum[i] = (unsigned char )l ;
                }
                res = ( memcmp(mdctx.digest, checksum, 16 )==0 );
                break;
            }
            
            l = strlen(fbuf) ;
            while( l>0 ) {
                if( fbuf[l-1]==' ' || fbuf[l-1]=='\n' || fbuf[l-1]=='\r' ) {
                    l-- ;
                    fbuf[l]=0 ;
                }
                else {
                    break;
                }
            }
            FILE * mfile = fopen( fbuf, "rb" );
            if( mfile ) {
                while( (l = fread( fbuf, 1, 512, mfile ))>0 ) {
                    MD5Update( &mdctx, (unsigned char *)fbuf, l);
                }
                fileadded++;
                fclose( mfile ) ;
            }
        }
        fclose( checkfile ) ;
    }
    return res ;
}

//
int main( int argc, char * argv[] ) {

    if( argc>2 ) {
        if( strncmp( argv[2], "-gen", 4 )==0 ) {
            md5check( argv[1], 1 );
        }
    }
    else if( argc>1 ) {
        if( md5check( argv[1], 0 ) ) {
            return 0 ;
        }
    }    
    return 1 ;
}
