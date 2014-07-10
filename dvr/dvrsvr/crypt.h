#pragma once
// Cryptions for DVR project

// key process

// Convert plain text password to 256 bytes key block
//   password: null terminated password string, maximum 256 bytes
//   k256: key array, 256 bytes
// For RC4 cryption
void key_256( char * password, unsigned char * k256);

// Convert plain text password to 16 bytes (128bits) key block
//   password: null terminated password string, maximum 256 bytes
//   k16: key array, 16 bytes
// For XTEA cryption
void key_16( char * password, unsigned char * k16);


// RC4
//    a very very fast stream cipher

struct RC4_seed {
    unsigned char s[256] ;
    unsigned char i, j ;
} ;

// RC4 key-scheduling algorithm
//     seed: RC4 seed
//     k: key array, 256 bytes
// initialize RC4 seed with key block
void RC4_KSA( struct RC4_seed * seed, unsigned char * k );

// RC4 key-scheduling algorithm, (addition)
//     seed: RC4 seed
//     key: key string
//     klen: key length
void RC4_KSA_A(struct RC4_seed * seed, unsigned char * key, int klen );

// RC4 stream data cryption. 
//     seed: RC4 seed
//     text: data to be encrypt/decrypt
//     textsize: data size
// to decrypt/encrypt text, both seed and text will be changed.
void RC4_crypt(unsigned char * text, int textsize, struct RC4_seed * seed);

// *** RC4 block cryption.
//   Since RC4 is a stream cryption, not a block cryption. 
// So we use RC4 PRGA to generate a block of pesudo random data, encrypt/decrypt 
// by xor original message with this data.

// RC4 PRGA
//   The pseudo-random generation algorithm
unsigned char RC4_PRGA( struct RC4_seed * seed );

// Generate RC4 cryption table
//      crypt_table: cryption table for block encryption
//  table_size:  cryption table size, can be 4096 or 8192
void RC4_crypt_table( unsigned char * crypt_table, int table_size, unsigned char * k);

// RC4 block cryption
//    text: data to be encrypt/decrypt
//    textsize: size of data
//    textoffset: offset of data from start of file (0 for start of file or indipenddant data)
//    crypt_table: cryption table
//    table_size: cryption table size
//void RC4_block_crypt( unsigned char * text, int textsize, int textoffset, unsigned char * crypt_table, int table_size);
void RC4_block_crypt( unsigned char * dest, unsigned char * src, int textsize, int textoffset, const unsigned char * crypt_table, int table_size);

//XTEA

// XTEA encryption
//    num_rounds: 26 to 64
//    v : data to encrypt, 64 bits
//    k : key block, 128 bits   
void XTEA_encipher(unsigned int num_rounds, unsigned int* v, unsigned int* k) ;

// XTEA decryption
//    num_rounds: 26 to 64
//    v : data to decrypt, 64 bits
//    k : key block, 128 bits   
void XTEA_decipher(unsigned int num_rounds, unsigned int* v, unsigned int* k) ;

// XTEA block encryption
//      text:     data to encrypt
//      textsize: data size, should be times of 8
//      k: 128bits key block
// because block size is 8 bytes, remain bytes (<7bytes) are not encrypted
void XTEA_encrypt( unsigned char * text, int textsize, unsigned int * k );

// XTEA block decryption
//      text: data to encrypt
//      textsize: data size, should be times of 8
//      k: 128bits key block
// because block size is 8 bytes, remain bytes (<7bytes) are not encrypted
void XTEA_decrypt( unsigned char * text, int textsize, unsigned int * k );


// convert binary codes into c64 code.
// return size of bytes in c64.
int bin2c64(unsigned char * bin, int binsize, char * c64 );

// convert c64 codes to binary codes.
// return size of bytes in c64.
int c642bin(char * c64, unsigned char * bin, int binsize );

// MD5

/* typedef a 32 bit type */
typedef unsigned int UINT4;

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

