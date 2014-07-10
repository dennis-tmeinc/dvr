#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/* ------------> for dos FAT  */
# include <endian.h>
#if __BYTE_ORDER == __BIG_ENDIAN
#include <byteswap.h>
#define CF_LE_W(v) bswap_16(v)
#define CF_LE_L(v) bswap_32(v)
#define CT_LE_W(v) CF_LE_W(v)
#define CT_LE_L(v) CF_LE_L(v)
#else
#define CF_LE_W(v) (v)
#define CF_LE_L(v) (v)
#define CT_LE_W(v) (v)
#define CT_LE_L(v) (v)
#endif /* __BIG_ENDIAN */

typedef unsigned char   __u8;
typedef unsigned short __u16;
typedef unsigned int   __u32;

#define MSDOS_DIR_BITS	5 /* log2(sizeof(struct msdos_dir_entry)) */
#define MSDOS_FAT12 4084 /* maximum number of clusters in a 12 bit FAT */
#define BOOTCODE_SIZE		448
#define BOOTCODE_FAT32_SIZE	420
//#define GET_UNALIGNED_W(f) CF_LE_W( *(unsigned short *)&f )
#define GET_UNALIGNED_W(f) ( (__u16)(((__u8 *)&f)[0]) + (__u16)(((__u8 *)&f)[1])*256 ) 
#define ROUND_TO_MULTIPLE(n,m) ((n) && (m) ? (n)+(m)-1-((n)-1)%(m) : 0)
    /* don't divide by zero */

#define BOOTCODE_SIZE		448
#define BOOTCODE_FAT32_SIZE	420

struct msdos_volume_info {
  __u8		drive_number;	/* BIOS drive number */
  __u8		RESERVED;	/* Unused */
  __u8		ext_boot_sign;	/* 0x29 if fields below exist (DOS 3.3+) */
  __u8		volume_id[4];	/* Volume ID number */
  __u8		volume_label[11];/* Volume label */
  __u8		fs_type[8];	/* Typically FAT12 or FAT16 */
} __attribute__ ((packed));

struct msdos_boot_sector
{
  __u8	        boot_jump[3];	/* Boot strap short or near jump */
  __u8          system_id[8];	/* Name - can be used to special case
				   partition manager volumes */
  __u8          sector_size[2];	/* bytes per logical sector */
  __u8          cluster_size;	/* sectors/cluster */
  __u16         reserved;	/* reserved sectors */
  __u8          fats;		/* number of FATs */
  __u8          dir_entries[2];	/* root directory entries */
  __u8          sectors[2];	/* number of sectors */
  __u8          media;		/* media code (unused) */
  __u16         fat_length;	/* sectors/FAT */
  __u16         secs_track;	/* sectors per track */
  __u16         heads;		/* number of heads */
  __u32         hidden;		/* hidden sectors (unused) */
  __u32         total_sect;	/* number of sectors (if sectors == 0) */
  union {
    struct {
      struct msdos_volume_info vi;
      __u8	boot_code[BOOTCODE_SIZE];
    } __attribute__ ((packed)) _oldfat;
    struct {
      __u32	fat32_length;	/* sectors/FAT */
      __u16	flags;		/* bit 8: fat mirroring, low 4: active fat */
      __u8	version[2];	/* major, minor filesystem version */
      __u32	root_cluster;	/* first cluster in root directory */
      __u16	info_sector;	/* filesystem info sector */
      __u16	backup_boot;	/* backup boot sector */
      __u16	reserved2[6];	/* Unused */
      struct msdos_volume_info vi;
      __u8	boot_code[BOOTCODE_FAT32_SIZE];
    } __attribute__ ((packed)) _fat32;
  } __attribute__ ((packed)) fstype;
  __u16		boot_sign;
} __attribute__ ((packed));
#define fat32	fstype._fat32
#define oldfat	fstype._oldfat
/* <------------ end of dos FAT  */

void encode(__u32 code, __u32 *v)
{
	__u32 k[4] ;
	__u32 delta=0x9e3779b9, n=32 ; /* a key schedule constant */

	k[0]=0x57389183 ;
	k[1]=0x68391832 ;
	k[2]=0x48296410 ;
	k[3]=0x78306731 ;
	v[0]=code;
	v[1]=0x32686708 ;
	__u32 y=v[0],z=v[1], sum=0 ;   	/* set up */
	while (n-->0) {                       	/* basic cycle start */
		sum += delta ;
		y += ( (z<<4)+k[0] ) ^ (z+sum) ^ ((z>>5)+k[1]) ;
		z += ( (y<<4)+k[2] ) ^ (y+sum) ^ ((y>>5)+k[3]) ;   /* end cycle */
	}
	v[0]=y ; v[1]=z ;
}

__u32 readVolumeID(char *device)
{
  int fd;
  unsigned total_sectors;
  unsigned short logical_sector_size, sectors;
  struct msdos_boot_sector b;
  unsigned int cluster_size;
  unsigned long clusters;
  loff_t root_start;
  loff_t data_start;
  unsigned int root_entries;
  unsigned fat_length;
  off_t data_size;
  unsigned int fat_bits; /* size of a FAT entry */
  struct msdos_volume_info *vi;
  unsigned int volume_id;
 
  if ((fd = open(device, O_RDONLY)) < 0) {
    perror("open");
    return 0;
  }

  if (read(fd, &b, sizeof(b)) != sizeof(b)) {
    fprintf(stderr, "error reading FAT\n");
    goto fat_error;
  }

  logical_sector_size = GET_UNALIGNED_W(b.sector_size);
  if (!logical_sector_size) {
    fprintf(stderr, "wrong sector size\n");
    goto fat_error;
  }

  cluster_size = b.cluster_size*logical_sector_size;
  if (!cluster_size) {
    fprintf(stderr, "wrong cluster size\n");
    goto fat_error;
  }

  sectors = GET_UNALIGNED_W(b.sectors);
  total_sectors = sectors ? sectors : CF_LE_L(b.total_sect);

  fat_length = CF_LE_W(b.fat_length) ?
    CF_LE_W(b.fat_length) : CF_LE_L(b.fat32.fat32_length);
  
  root_start = ((off_t)CF_LE_W(b.reserved)+b.fats*fat_length)*
    logical_sector_size;
  root_entries = GET_UNALIGNED_W(b.dir_entries);

  data_start = root_start+ROUND_TO_MULTIPLE(root_entries <<
					    MSDOS_DIR_BITS,
					    logical_sector_size);
  data_size = (off_t)total_sectors*logical_sector_size-data_start;
  clusters = data_size/cluster_size;

  if (!b.fat_length && b.fat32.fat32_length) {
    fat_bits = 32;
  } else {
    fat_bits = (clusters > MSDOS_FAT12) ? 16 : 12;
  }

  vi = (fat_bits == 32 ? &b.fat32.vi : &b.oldfat.vi);

  volume_id = ((vi->volume_id[3] << 24) | 
	       (vi->volume_id[2] << 16) | 
	       (vi->volume_id[1] <<  8) | 
	       vi->volume_id[0]);
  fprintf(stderr, "volume ID:%x\n", volume_id);

  close(fd);

  return volume_id;

 fat_error:
  close(fd);
  return 0;
}

/*
 * parameters
 *   device:      device name (e.g /dev/sda)
 *   mounted_dir: directory where the FAT is mounted

 * return 0 on success
 *        1 on error
 */
int checkUsbKey(char *device, char *mounted_dir)
{
  __u32 v[2],vf[2], volumeID;
  FILE *fp;
  char filename[256];

  snprintf(filename, sizeof(filename), "%s/key", mounted_dir);

  fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "open key file:%s\n", strerror(errno));
    return 1;
  }

  if (fscanf(fp, "%08X%08X", &vf[0], &vf[1]) != 2) {
    fclose(fp);
    fprintf(stderr, "key file wrong data\n");
    return 1;
  }

  fclose(fp);

  volumeID = readVolumeID(device);
  if (!volumeID) {
    return 1;
  }

  fprintf(stderr, "volumeID:%u\n", volumeID);

  encode(volumeID, v);

  if (memcmp(v, vf, sizeof(v))) {
    fprintf(stderr, "wrong volumeID:%08x %08x,%08x %08x\n",v[0],v[1],vf[1],vf[2]);
    return 1;
  }

  return 0;
}

int main( int argc, char * argv[] ) {
    if( argc>2 ) {
        return checkUsbKey( argv[1], argv[2] );
    }
    return 2 ;
}
