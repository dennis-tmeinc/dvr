#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/* 
 * 
 * ref: https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
 * 
*/

unsigned char sector[1024] ;

int fat32check( char * bootrecord )
{
	return (
		bootrecord[0x0b] == (char)0 &&
		bootrecord[0x0c] == (char)2 &&
		bootrecord[0x15] == (char) 0xf8 &&
		memcmp( bootrecord + 0x52, "FAT32", 5 )==0 &&
		bootrecord[0x1fe] == (char)0x55 &&
		bootrecord[0x1ff] == (char)0xaa &&
		memcmp( bootrecord + 0x200, "RRaA", 4 )==0 &&
		memcmp( bootrecord + 0x3e4, "rrAa", 4 )==0 &&
		bootrecord[0x3fe] == (char)0x55 &&
		bootrecord[0x3ff] == (char)0xaa ) ;
}

int readbyte(void * addr)
{
	return (int) * (unsigned char *)addr ;
}

int readword(void * addr)
{
	return 
		(int)(
			(unsigned short)( *(unsigned char *)addr ) +
			(((unsigned short)( *(((unsigned char *)addr)+1) ))<<8) 
		) ;
}

int readdword(void * addr)
{
	return 
		(int)(
			(unsigned int)( *(unsigned char *)addr ) +
			(((unsigned int)( *(((unsigned char *)addr)+1) ))<<8) +
			(((unsigned int)( *(((unsigned char *)addr)+2) ))<<16) +
			(((unsigned int)( *(((unsigned char *)addr)+3) ))<<24) 
		) ;
}

void labeltrim( char * label )
{
	int l = 11 ;
	while(--l>=0) {
		if( label[l] <= ' ' ) {
			label[l] = 0 ;
		}
		else {
			break ;
		}
	}
}

int main( int argc, char * argv[] ) 
{
	int res = 2 ;			// 0: fat32 detected (labeled or not), 1: other fs, 2: other error
    if( argc<2 ) {
		printf("Usage:\n\t%s <device>\n", argv[0]);
		return res ;
    }
    int i ;
    int root_offset = 0 ;
	int labelidx = -1 ;		// -1 indicate get label from br
	int emptyidx = -1 ;		// empty slot index
	char dentry[32] ;
    char label[12] ;
    FILE * dev ;
    
    // read boot record
    dev = fopen( argv[1], "rb" );
    if( dev ) {
		res = 1 ;
		if( fread( sector, 1, 1024, dev )==1024 && fat32check( sector ) ) {
			// fat32 detected
			res = 0 ;

			// copy default label from boot sector
			memcpy( label, sector+0x47, 11 );
			label[11]=0 ;
			
			// to search volume label from root
			int bytes_persector = readword( sector+0xb );
			//int sectors_per_cluster = readbyte( sector+0d ) ;
			int reserved_sectors = readword(sector+0xe) ;
			int number_of_fats = readbyte(sector+0x10);
			int number_of_sectors_per_fat = readdword(sector+0x24);
			
			root_offset = (reserved_sectors + number_of_fats * number_of_sectors_per_fat ) * bytes_persector ;
			fseek( dev, root_offset, SEEK_SET );
			
			// search for label
			for( i=0 ; i<512; i++ ) {
				if( fread( dentry, 32, 1, dev ) ) {
					if( dentry[0] == 0 ) {
						if( emptyidx==-1 ) {
							emptyidx = i ;
						}
						break;
					}
					else if( dentry[0] == (char)0xe5 ) {
						if( emptyidx==-1 ) {
							emptyidx = i ;
						}
						continue ;
					}
					else if( dentry[0x0b] == (char)8 ) {
						memcpy( label, dentry, 11 );
						label[11]=0 ;
						labelidx = i ;
						break ;
					}
				}
				else {
					break ;
				}
			}
			
			labeltrim(label);
			printf("%s\n", label );
			
		}
		fclose( dev );
		
		// see if we can change it
		if( res == 0 && argc>2 && root_offset>0 ) {			// try new label in arg3
			dev = fopen( argv[1], "wb" );
			if( dev ) {
				memset( dentry, 0, sizeof(dentry) );
				int sp = 0 ;
				for( i=0; i<11; i++ ) {
					if( sp==0 ) {
						if( argv[2][i] == 0 ) {
							sp = 1 ;
							dentry[i] = ' ' ;
						}
						else {
							dentry[i] = argv[2][i]  ;
						}
					}
					else {
						dentry[i] = ' ' ;
					}
				}
				if( labelidx>=0 ) {
					// set label to root dir
					fseek( dev, root_offset + labelidx*32, SEEK_SET );
					fwrite( dentry, 1, 11, dev );
				}
				else if( emptyidx>=0 ) {
					fseek( dev, root_offset + emptyidx*32, SEEK_SET );
					dentry[0xb]=8 ;
					dentry[0x18] = 0x21 ;
					dentry[0x19] = 0x46 ;
					fwrite( dentry, 1, 32, dev );
				}
				else {
					// set label to br
					fseek( dev, 0x47, SEEK_SET );
					fwrite( dentry, 1, 11, dev );
				}
				fclose( dev );
				
				printf("New label : %11s\n", dentry );
			}
		}
		
		return res ;
	}
	return 2 ;
}
