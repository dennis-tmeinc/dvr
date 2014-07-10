
#include "string.h"
#include "crypt.h"

// Cryptions for DVR project

// key process

// initialize table of key block
static unsigned char key_init[256] = {
	0x98, 0xaa, 0x07, 0xd8, 0x01, 0x5b, 0x83, 0x12, 0xbe, 0x85, 0x31, 0x24, 0xc3, 0x7d, 0x0c, 0x55, 
	0x74, 0x5d, 0xbe, 0x72, 0xfe, 0xb1, 0xde, 0x80, 0xa7, 0x06, 0xdc, 0x9b, 0x74, 0xf1, 0x9b, 0xc1, 
	0x98, 0x2f, 0x8a, 0x42, 0x91, 0x44, 0x37, 0x71, 0xcf, 0xfb, 0xc0, 0xb5, 0xa5, 0xdb, 0xb5, 0xe9, 
	0x5b, 0xc2, 0x56, 0x39, 0xf1, 0x11, 0xf1, 0x59, 0xa4, 0x82, 0x3f, 0x92, 0xd5, 0x5e, 0x1c, 0xab, 
	0x16, 0xc1, 0xa4, 0x19, 0x08, 0x6c, 0x37, 0x1e, 0x4c, 0x77, 0x48, 0x27, 0xb5, 0xbc, 0xb0, 0x34, 
	0xb3, 0x0c, 0x1c, 0x39, 0x4a, 0xaa, 0xd8, 0x4e, 0x4f, 0xca, 0x9c, 0x5b, 0xf3, 0x6f, 0x2e, 0x68, 
	0x52, 0x51, 0x3e, 0x98, 0x6d, 0xc6, 0x31, 0xa8, 0xc8, 0x27, 0x03, 0xb0, 0xc7, 0x7f, 0x59, 0xbf, 
	0xf3, 0x0b, 0xe0, 0xc6, 0x47, 0x91, 0xa7, 0xd5, 0x51, 0x63, 0xca, 0x06, 0x67, 0x29, 0x29, 0x14, 
	0xc1, 0x69, 0x9b, 0xe4, 0x86, 0x47, 0xbe, 0xef, 0xc6, 0x9d, 0xc1, 0x0f, 0xcc, 0xa1, 0x0c, 0x24, 
	0x6f, 0x2c, 0xe9, 0x2d, 0xaa, 0x84, 0x74, 0x4a, 0xdc, 0xa9, 0xb0, 0x5c, 0xda, 0x88, 0xf9, 0x76, 
	0xa1, 0xe8, 0xbf, 0xa2, 0x4b, 0x66, 0x1a, 0xa8, 0x70, 0x8b, 0x4b, 0xc2, 0xa3, 0x51, 0x6c, 0xc7, 
	0x19, 0xe8, 0x92, 0xd1, 0x24, 0x06, 0x99, 0xd6, 0x85, 0x35, 0x0e, 0xf4, 0x70, 0xa0, 0x6a, 0x10, 
	0x85, 0x0a, 0xb7, 0x27, 0x38, 0x21, 0x1b, 0x2e, 0xfc, 0x6d, 0x2c, 0x4d, 0x13, 0x0d, 0x38, 0x53, 
	0x54, 0x73, 0x0a, 0x65, 0xbb, 0x0a, 0x6a, 0x76, 0x2e, 0xc9, 0xc2, 0x81, 0x85, 0x2c, 0x72, 0x92, 
	0xee, 0x82, 0x8f, 0x74, 0x6f, 0x63, 0xa5, 0x78, 0x14, 0x78, 0xc8, 0x84, 0x08, 0x02, 0xc7, 0x8c, 
	0xfa, 0xff, 0xbe, 0x90, 0xeb, 0x6c, 0x50, 0xa4, 0xf7, 0xa3, 0xf9, 0xbe, 0xf2, 0x78, 0x71, 0xc6
} ;

static unsigned long u32_rotate(unsigned long v, int bits)
{
	return (v<<bits)|(v>>(32-bits));
}

// non endian sensitive assignment
static unsigned long u32_get(unsigned char *p)
{
	return (unsigned long)(*p) + 
		   ((unsigned long)(*(p+1))<<8)+
		   ((unsigned long)(*(p+2))<<16)+
		   ((unsigned long)(*(p+3))<<24) ;
}

// non endian sensitive assignment
static void u32_put(unsigned char *p, unsigned long v)
{
	*p=(unsigned char)v ;
	*(p+1)=(unsigned char)(v>>8) ;
	*(p+2)=(unsigned char)(v>>16) ;
	*(p+3)=(unsigned char)(v>>24) ;
}

// Convert plain text password to 256 bytes key block
//   password: null terminated password string, maximum 256 bytes
//   k256: key array, 256 bytes
// For RC4 cryption
void key_256( char * password, unsigned char * k256)
{
	int rounds,i;
	memcpy( k256, key_init, 256);
	// merge password into key table ;
	for( i=0; i<256; i++ ) {
		if( password[i]==0 )
			break;
		k256[i]^=password[i] ;
	}
	for(rounds=0; rounds<26; rounds++) {
		for(i=0; i<256; i+=4) {
			unsigned long l, l1, l2, l3, l4, l5 ;
			int s1, s2, s3, s4, s5 ;
			s1=(i+7*4)%256 ;
			s2=(i+29*4)%256 ;
			s3=(i+11*4)%256 ;
			s4=(i+51*4)%256 ;
			s5=(i+37*4)%256 ;

			l =u32_get(k256+i);
			l1=u32_get(k256+s1);
			l2=u32_get(k256+s2);
			l3=u32_get(k256+s3);
			l4=u32_get(k256+s4);
			l5=u32_get(k256+s5);

			l1+=u32_rotate(l , 2);
			l2+=u32_rotate(l1, 13);
			l3+=u32_rotate(l2, 22);
			l4+=u32_rotate(l3, 19);
			l5+=u32_rotate(l4, 7);
			l+=(l1^l2)+(l3^l4)+l5 ;

			u32_put(k256+i, l);
			u32_put(k256+s1, l1);
			u32_put(k256+s2, l2);
			u32_put(k256+s3, l3);
			u32_put(k256+s4, l4);
			u32_put(k256+s5, l5);
		}
	}
}

// Convert plain text password to 16 bytes (128bits) key block
//   password: null terminated password string, maximum 256 bytes
//   k16: key array, 16 bytes
// For XTEA cryption
void key_16( char * password, unsigned char * k16)
{
	unsigned char k256[256] ;
	key_256( password, k256);
	memcpy(k16,k256,16);
}

// RC4
//		a very very fast stream cipher

// RC4 key-scheduling algorithm
//     seed: RC4 seed
//     k: key array, 256 bytes
// initialize RC4 seed with key block
void RC4_KSA( struct RC4_seed * seed, unsigned char * k )
{
	int i ;
	unsigned char j, swap;
	for( i=0; i<256; i++ )
		seed->s[i]=(unsigned char)i;
    j=0;
	for( i=0; i<256; i++ ) {
		j=j+seed->s[i]+k[i];
		// swap(s[i], s[j])
		swap=seed->s[i];		
		seed->s[i]=seed->s[j];
		seed->s[j]=swap;
	}
	seed->i=seed->j=0;
}

// RC4 stream data cryption. 
//     seed: RC4 seed
//     text: data to be encrypt/decrypt
//     textsize: data size
// to decrypt/encrypt text, both seed and text will be changed.
void RC4_crypt(unsigned char * text, int textsize, struct RC4_seed * seed)
{
	// PRGA
	unsigned char i,j,k ;
	int n ;
	i=seed->i ;
	j=seed->j ;
	for( n=0; n<textsize; n++) {
		i++ ;
		j+=seed->s[i] ;
		// swap( s[i], s[j] )
		k=seed->s[i] ;
		seed->s[i]=seed->s[j] ;
		seed->s[j]=k ;
		text[n]^=k+seed->s[i] ;
	}
	seed->i=i;
	seed->j=j;
}

// *** RC4 block cryption.
//   Since RC4 is a stream cryption, not a block cryption. 
// So we use RC4 PRGA to generate a block of pesudo random data, encrypt/decrypt 
// by xor original message with this data.

// RC4 PRGA
//		The pseudo-random generation algorithm
unsigned char RC4_PRGA( struct RC4_seed * seed )
{
	unsigned char k ;
	seed->i++;
	seed->j+=seed->s[seed->i] ;
	// swap( s[i], s[j])
	k=seed->s[seed->i] ;
	seed->s[seed->i]=seed->s[seed->j];
	seed->s[seed->j]=k ;
	return seed->s[seed->i] + k ;
}

// Generate RC4 cryption table
//	crypt_table: cryption table for block encryption
//  table_size:  cryption table size, can be 4096 or 8192
void RC4_crypt_table( unsigned char * crypt_table, int table_size, unsigned char * k)
{
	int i ;
	struct RC4_seed seed ;
	RC4_KSA(&seed, k);		// generate seed ;
	for(i=0; i<table_size; i++) {
		crypt_table[i]=RC4_PRGA(&seed);
	}
}

// RC4 block cryption
//    text: data to be encrypt/decrypt
//    textsize: size of data
//    textoffset: offset of data from start of file (0 for start of file or independent data)
//    crypt_table: cryption table
//    table_size: cryption table size
void RC4_block_crypt( unsigned char * text, int textsize, int textoffset, unsigned char * crypt_table, int table_size)
{
	int i;
	for(i=0; i<textsize; i++) {
		text[i]^=crypt_table[(i+textoffset)%table_size] ;
	}
}


//XTEA

// XTEA encryption
//    num_rounds: 26 to 64
//    v : data to encrypt, 64 bits
//    k : key block, 128 bits   
void XTEA_encipher(unsigned int num_rounds, unsigned long* v, unsigned long* k) 
{
    unsigned long v0=v[0], v1=v[1], i;
    unsigned long sum=0, delta=0x9E3779B9;
    for(i=0; i<num_rounds; i++) {
       v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]);
    }
    v[0]=v0; v[1]=v1;
}

// XTEA decryption
//    num_rounds: 26 to 64
//    v : data to decrypt, 64 bits
//    k : key block, 128 bits   
void XTEA_decipher(unsigned int num_rounds, unsigned long* v, unsigned long* k) 
{
    unsigned long v0=v[0], v1=v[1], i;
    unsigned long delta=0x9E3779B9, sum=delta*num_rounds;
    for(i=0; i<num_rounds; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
    }
    v[0]=v0; v[1]=v1;
}

// XTEA block encryption
//		text: data to encrypt
//      textsize: data size, should be times of 8
//		k: 128bits key block
// because block size is 8 bytes, remain bytes (<8bytes) are not encrypted
void XTEA_encrypt( unsigned char * text, int textsize, unsigned long * k )
{
	while(textsize>=8) {
		XTEA_encipher(39, (unsigned long *)text, k);
		textsize-=8 ;
		text+=8 ;
	}
}

// XTEA block decryption
//		text: data to encrypt
//      textsize: data size, should be times of 8
//		k: 128bits key block
// because block size is 8 bytes, remain bytes (<8bytes) are not decrypted
void XTEA_decrypt( unsigned char * text, int textsize, unsigned long * k )
{
	while(textsize>=8) {
		XTEA_decipher(39, (unsigned long *)text, k);
		textsize-=8 ;
		text+=8 ;
	}
}

// C64 encoding/decoding. Covert binary to assii codes for save on txt files
// C64 code :
//      0-9 : 0-9
//      A-Z : 10-35
//      a-z : 36-61
//		+   : 62
//      -   : 63

static char _c64(char b)
{
	// convert to C64 code
	if( b<10 ) {
		return b+'0' ;
	}
	else if( b<36 ) {
		return b-10+'A' ;
	}
	else if( b<62 ) {
		return b-36+'a' ;
	}
	else if( b==62 ){
		return '+' ;
	}
	else if( b==63 ){
		return '-' ;
	}
	return 0;
}

static char _b64(char c)
{
	if( c>='0' && c<='9' ) {
		return c-'0' ;
	}
	else if( c>='A' && c<='Z' ) {
		return c-'A'+10 ;
	}
	else if( c>='a' && c<='z' ) {
		return c-'a'+36 ;
	}
	else if( c=='+' ) {
		return 62 ;
	}
	else if( c=='-' ) {
		return 63 ;
	}
	return 0;
}

// convert binary codes into c64 code.
// return size of bytes in c64.
int bin2c64(char * bin, int binsize, char * c64 )
{
	int i, j;
	for( i=0, j=0; i<binsize; i++, j++) {
		if( i%3==0 ) {
			c64[j]=_c64( ((bin[i]>>6)&0x03) |
					((bin[i+1]>>4)&0x0c) |
					((bin[i+2]>>2)&0x30) );
			j++;
		}
		c64[j]=_c64( bin[i]&0x3f );
	}
	c64[j]=0;
	return j;
}

// convert c64 codes to binary codes.
// return size of bytes in bin.
int c642bin(char * c64, char * bin, int binsize )
{
	int i, j;
	char b ;
	for( i=0, j=0; i<binsize; i++, j++) {
		if( c64[j]==0 ) {
			break;
		}
		while( c64[j]==' ' ) {		// skip space
			j++;
		}
		if(i%3==0) {
			b = _b64(c64[j]);
			bin[i]=  (b<<6) & 0xc0 ;
			bin[i+1]=(b<<4) & 0xc0 ;
			bin[i+2]=(b<<2) & 0xc0 ;
			j++;
		}
		bin[i]|=_b64(c64[j]); ;
	}
	return j;
}

