/*
 * (C) Copyright 2000-2008
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2008
 * Guennadi Liakhovetski, DENX Software Engineering, lg@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef MTD_OLD
# include <stdint.h>
# include <linux/mtd/mtd.h>
#else
# define  __user	/* nothing */
# include <mtd/mtd-user.h>
#endif

#include "fw_env.h"

/* --------------------  CRC32 -----------------------------*/

/* 
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
static const unsigned int crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};
/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */

static unsigned int 
crc32(unsigned int crc, const unsigned char* buf, unsigned int len)
{
    if (buf == 0) 
	return 0L;

    crc = crc ^ 0xffffffffL;
    while (len >= 8)
    {
      DO8(buf);
      len -= 8;
    }
    if (len) do {
      DO1(buf);
    } while (--len);
    return crc ^ 0xffffffffL;
}
/* -------------------- end CRC32 -----------------------------*/

#define	CMD_GETENV	"fw_printenv"
#define	CMD_SETENV	"fw_setenv"

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

struct envdev_s {
	char devname[16];		/* Device name */
	ulong devoff;			/* Device offset */
	ulong env_size;			/* environment size */
	ulong erase_size;		/* device erase size */
	ulong env_sectors;		/* number of environment sectors */
	uint8_t mtd_type;		/* type of the MTD device */
};

static struct envdev_s envdevices[2] =
{
	{
		.mtd_type = MTD_ABSENT,
	}, {
		.mtd_type = MTD_ABSENT,
	},
};
static int dev_current;

#define DEVNAME(i)    envdevices[(i)].devname
#define DEVOFFSET(i)  envdevices[(i)].devoff
#define ENVSIZE(i)    envdevices[(i)].env_size
#define DEVESIZE(i)   envdevices[(i)].erase_size
#define ENVSECTORS(i) envdevices[(i)].env_sectors
#define DEVTYPE(i)    envdevices[(i)].mtd_type

#define CONFIG_ENV_SIZE ENVSIZE(dev_current)

#define ENV_SIZE      getenvsize()

struct env_image_single {
	uint32_t	crc;	/* CRC32 over data bytes    */
	char		data[];
};

struct env_image_redundant {
	uint32_t	crc;	/* CRC32 over data bytes    */
	unsigned char	flags;	/* active or obsolete */
	char		data[];
};

enum flag_scheme {
	FLAG_NONE,
	FLAG_BOOLEAN,
	FLAG_INCREMENTAL,
};

struct environment {
	void			*image;
	uint32_t		*crc;
	unsigned char		*flags;
	char			*data;
	enum flag_scheme	flag_scheme;
};

static struct environment environment = {
	.flag_scheme = FLAG_NONE,
};

static int HaveRedundEnv = 0;

static unsigned char active_flag = 1;
/* obsolete_flag must be 0 to efficiently set it on NOR flash without erasing */
static unsigned char obsolete_flag = 0;


#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

static char default_environment[] = {
#if defined(CONFIG_BOOTARGS)
	"bootargs=" CONFIG_BOOTARGS "\0"
#endif
#if defined(CONFIG_BOOTCOMMAND)
	"bootcmd=" CONFIG_BOOTCOMMAND "\0"
#endif
#if defined(CONFIG_RAMBOOTCOMMAND)
	"ramboot=" CONFIG_RAMBOOTCOMMAND "\0"
#endif
#if defined(CONFIG_NFSBOOTCOMMAND)
	"nfsboot=" CONFIG_NFSBOOTCOMMAND "\0"
#endif
#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
	"bootdelay=" MK_STR (CONFIG_BOOTDELAY) "\0"
#endif
#if defined(CONFIG_BAUDRATE) && (CONFIG_BAUDRATE >= 0)
	"baudrate=" MK_STR (CONFIG_BAUDRATE) "\0"
#endif
#ifdef	CONFIG_LOADS_ECHO
	"loads_echo=" MK_STR (CONFIG_LOADS_ECHO) "\0"
#endif
#ifdef	CONFIG_ETHADDR
	"ethaddr=" MK_STR (CONFIG_ETHADDR) "\0"
#endif
#ifdef	CONFIG_ETH1ADDR
	"eth1addr=" MK_STR (CONFIG_ETH1ADDR) "\0"
#endif
#ifdef	CONFIG_ETH2ADDR
	"eth2addr=" MK_STR (CONFIG_ETH2ADDR) "\0"
#endif
#ifdef	CONFIG_ETH3ADDR
	"eth3addr=" MK_STR (CONFIG_ETH3ADDR) "\0"
#endif
#ifdef	CONFIG_ETH4ADDR
	"eth4addr=" MK_STR (CONFIG_ETH4ADDR) "\0"
#endif
#ifdef	CONFIG_ETH5ADDR
	"eth5addr=" MK_STR (CONFIG_ETH5ADDR) "\0"
#endif
#ifdef	CONFIG_ETHPRIME
	"ethprime=" CONFIG_ETHPRIME "\0"
#endif
#ifdef	CONFIG_IPADDR
	"ipaddr=" MK_STR (CONFIG_IPADDR) "\0"
#endif
#ifdef	CONFIG_SERVERIP
	"serverip=" MK_STR (CONFIG_SERVERIP) "\0"
#endif
#ifdef	CONFIG_SYS_AUTOLOAD
	"autoload=" CONFIG_SYS_AUTOLOAD "\0"
#endif
#ifdef	CONFIG_ROOTPATH
	"rootpath=" MK_STR (CONFIG_ROOTPATH) "\0"
#endif
#ifdef	CONFIG_GATEWAYIP
	"gatewayip=" MK_STR (CONFIG_GATEWAYIP) "\0"
#endif
#ifdef	CONFIG_NETMASK
	"netmask=" MK_STR (CONFIG_NETMASK) "\0"
#endif
#ifdef	CONFIG_HOSTNAME
	"hostname=" MK_STR (CONFIG_HOSTNAME) "\0"
#endif
#ifdef	CONFIG_BOOTFILE
	"bootfile=" MK_STR (CONFIG_BOOTFILE) "\0"
#endif
#ifdef	CONFIG_LOADADDR
	"loadaddr=" MK_STR (CONFIG_LOADADDR) "\0"
#endif
#ifdef	CONFIG_PREBOOT
	"preboot=" CONFIG_PREBOOT "\0"
#endif
#ifdef	CONFIG_CLOCKS_IN_MHZ
	"clocks_in_mhz=" "1" "\0"
#endif
#if defined(CONFIG_PCI_BOOTDELAY) && (CONFIG_PCI_BOOTDELAY > 0)
	"pcidelay=" MK_STR (CONFIG_PCI_BOOTDELAY) "\0"
#endif
#ifdef  CONFIG_EXTRA_ENV_SETTINGS
	CONFIG_EXTRA_ENV_SETTINGS
#endif
	"\0"		/* Termimate struct environment data with 2 NULs */
};

static int flash_io (int mode);
static char *envmatch (char * s1, char * s2);
static int env_init (void);
static int parse_config (void);

#if defined(CONFIG_FILE)
static int get_config (char *);
#endif
static inline ulong getenvsize (void)
{
	ulong rc = CONFIG_ENV_SIZE - sizeof (long);

	if (HaveRedundEnv)
		rc -= sizeof (char);
	return rc;
}

/*
 * Search the environment for a variable.
 * Return the value, if found, or NULL, if not found.
 */
char *fw_getenv (char *name)
{
	char *env, *nxt;

	if (env_init ())
		return NULL;

	for (env = environment.data; *env; env = nxt + 1) {
		char *val;

		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= &environment.data[ENV_SIZE]) {
				fprintf (stderr, "## Error: "
					"environment not terminated\n");
				return NULL;
			}
		}
		val = envmatch (name, env);
		if (!val)
			continue;
		return val;
	}
	return NULL;
}

/*
 * Print the current definition of one, or more, or all
 * environment variables
 */
int fw_printenv (int argc, char *argv[])
{
	char *env, *nxt;
	int i, n_flag;
	int rc = 0;

	if (env_init ())
		return -1;

	if (argc == 1) {		/* Print all env variables  */
		for (env = environment.data; *env; env = nxt + 1) {
			for (nxt = env; *nxt; ++nxt) {
				if (nxt >= &environment.data[ENV_SIZE]) {
					fprintf (stderr, "## Error: "
						"environment not terminated\n");
					return -1;
				}
			}

			printf ("%s\n", env);
		}
		return 0;
	}

	if (strcmp (argv[1], "-n") == 0) {
		n_flag = 1;
		++argv;
		--argc;
		if (argc != 2) {
			fprintf (stderr, "## Error: "
				"`-n' option requires exactly one argument\n");
			return -1;
		}
	} else {
		n_flag = 0;
	}

	for (i = 1; i < argc; ++i) {	/* print single env variables   */
		char *name = argv[i];
		char *val = NULL;

		for (env = environment.data; *env; env = nxt + 1) {

			for (nxt = env; *nxt; ++nxt) {
				if (nxt >= &environment.data[ENV_SIZE]) {
					fprintf (stderr, "## Error: "
						"environment not terminated\n");
					return -1;
				}
			}
			val = envmatch (name, env);
			if (val) {
				if (!n_flag) {
					fputs (name, stdout);
					putc ('=', stdout);
				}
				puts (val);
				break;
			}
		}
		if (!val) {
			fprintf (stderr, "## Error: \"%s\" not defined\n", name);
			rc = -1;
		}
	}

	return rc;
}

/*
 * Deletes or sets environment variables. Returns -1 and sets errno error codes:
 * 0	  - OK
 * EINVAL - need at least 1 argument
 * EROFS  - certain variables ("ethaddr", "serial#") cannot be
 *	    modified or deleted
 *
 */
int fw_setenv (int argc, char *argv[])
{
	int i, len;
	char *env, *nxt;
	char *oldval = NULL;
	char *name;

	if (argc < 2) {
		errno = EINVAL;
		return -1;
	}

	if (env_init ())
		return -1;

	name = argv[1];

	/*
	 * search if variable with this name already exists
	 */
	for (nxt = env = environment.data; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= &environment.data[ENV_SIZE]) {
				fprintf (stderr, "## Error: "
					"environment not terminated\n");
				errno = EINVAL;
				return -1;
			}
		}
		if ((oldval = envmatch (name, env)) != NULL)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval) {
		/*
		 * Ethernet Address and serial# can be set only once
		 */
		if ((strcmp (name, "ethaddr") == 0) ||
			(strcmp (name, "serial#") == 0)) {
			fprintf (stderr, "Can't overwrite \"%s\"\n", name);
			errno = EROFS;
			return -1;
		}

		if (*++nxt == '\0') {
			*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	/* Delete only ? */
	if (argc < 3)
		goto WRITE_FLASH;

	/*
	 * Append new definition at the end
	 */
	for (env = environment.data; *env || *(env + 1); ++env);
	if (env > environment.data)
		++env;
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > CONFIG_ENV_SIZE - (env-environment)
	 */
	len = strlen (name) + 2;
	/* add '=' for first arg, ' ' for all others */
	for (i = 2; i < argc; ++i) {
		len += strlen (argv[i]) + 1;
	}
	if (len > (&environment.data[ENV_SIZE] - env)) {
		fprintf (stderr,
			"Error: environment overflow, \"%s\" deleted\n",
			name);
		return -1;
	}
	while ((*env = *name++) != '\0')
		env++;
	for (i = 2; i < argc; ++i) {
		char *val = argv[i];

		*env = (i == 2) ? '=' : ' ';
		while ((*++env = *val++) != '\0');
	}

	/* end is marked with double '\0' */
	*++env = '\0';

  WRITE_FLASH:

	/*
	 * Update CRC
	 */
	*environment.crc = crc32 (0, (uint8_t *) environment.data, ENV_SIZE);

	/* write environment back to flash */
	if (flash_io (O_RDWR)) {
		fprintf (stderr, "Error: can't write fw_env to flash\n");
		return -1;
	}

	return 0;
}

/*
 * Test for bad block on NAND, just returns 0 on NOR, on NAND:
 * 0	- block is good
 * > 0	- block is bad
 * < 0	- failed to test
 */
static int flash_bad_block (int fd, uint8_t mtd_type, loff_t *blockstart)
{
	if (mtd_type == MTD_NANDFLASH) {
		int badblock = ioctl (fd, MEMGETBADBLOCK, blockstart);

		if (badblock < 0) {
			perror ("Cannot read bad block mark");
			return badblock;
		}

		if (badblock) {
#ifdef DEBUG
			fprintf (stderr, "Bad block at 0x%llx, "
				 "skipping\n", *blockstart);
#endif
			return badblock;
		}
	}

	return 0;
}

/*
 * Read data from flash at an offset into a provided buffer. On NAND it skips
 * bad blocks but makes sure it stays within ENVSECTORS (dev) starting from
 * the DEVOFFSET (dev) block. On NOR the loop is only run once.
 */
static int flash_read_buf (int dev, int fd, void *buf, size_t count,
			   off_t offset, uint8_t mtd_type)
{
	size_t blocklen;	/* erase / write length - one block on NAND,
				   0 on NOR */
	size_t processed = 0;	/* progress counter */
	size_t readlen = count;	/* current read length */
	off_t top_of_range;	/* end of the last block we may use */
	off_t block_seek;	/* offset inside the current block to the start
				   of the data */
	loff_t blockstart;	/* running start of the current block -
				   MEMGETBADBLOCK needs 64 bits */
	int rc;

	/*
	 * Start of the first block to be read, relies on the fact, that
	 * erase sector size is always a power of 2
	 */
	blockstart = offset & ~(DEVESIZE (dev) - 1);

	/* Offset inside a block */
	block_seek = offset - blockstart;

	if (mtd_type == MTD_NANDFLASH) {
		/*
		 * NAND: calculate which blocks we are reading. We have
		 * to read one block at a time to skip bad blocks.
		 */
		blocklen = DEVESIZE (dev);

		/*
		 * To calculate the top of the range, we have to use the
		 * global DEVOFFSET (dev), which can be different from offset
		 */
		top_of_range = (DEVOFFSET (dev) & ~(blocklen - 1)) +
			ENVSECTORS (dev) * blocklen;

		/* Limit to one block for the first read */
		if (readlen > blocklen - block_seek)
			readlen = blocklen - block_seek;
	} else {
		blocklen = 0;
		top_of_range = offset + count;
	}

	/* This only runs once on NOR flash */
	while (processed < count) {
		rc = flash_bad_block (fd, mtd_type, &blockstart);
		if (rc < 0)		/* block test failed */
			return -1;

		if (blockstart + block_seek + readlen > top_of_range) {
			/* End of range is reached */
			fprintf (stderr,
				 "Too few good blocks within range\n");
			return -1;
		}

		if (rc) {		/* block is bad */
			blockstart += blocklen;
			continue;
		}

		/*
		 * If a block is bad, we retry in the next block at the same
		 * offset - see common/env_nand.c::writeenv()
		 */
		lseek (fd, blockstart + block_seek, SEEK_SET);

		rc = read (fd, buf + processed, readlen);
		if (rc != readlen) {
			fprintf (stderr, "Read error on %s: %s\n",
				 DEVNAME (dev), strerror (errno));
			return -1;
		}
#ifdef DEBUG
		fprintf (stderr, "Read 0x%x bytes at 0x%llx\n",
			 rc, blockstart + block_seek);
#endif
		processed += readlen;
		readlen = min (blocklen, count - processed);
		block_seek = 0;
		blockstart += blocklen;
	}

	return processed;
}

/*
 * Write count bytes at offset, but stay within ENVSETCORS (dev) sectors of
 * DEVOFFSET (dev). Similar to the read case above, on NOR we erase and write
 * the whole data at once.
 */
static int flash_write_buf (int dev, int fd, void *buf, size_t count,
			    off_t offset, uint8_t mtd_type)
{
	void *data;
	struct erase_info_user erase;
	size_t blocklen;	/* length of NAND block / NOR erase sector */
	size_t erase_len;	/* whole area that can be erased - may include
				   bad blocks */
	size_t erasesize;	/* erase / write length - one block on NAND,
				   whole area on NOR */
	size_t processed = 0;	/* progress counter */
	size_t write_total;	/* total size to actually write - excludinig
				   bad blocks */
	off_t erase_offset;	/* offset to the first erase block (aligned)
				   below offset */
	off_t block_seek;	/* offset inside the erase block to the start
				   of the data */
	off_t top_of_range;	/* end of the last block we may use */
	loff_t blockstart;	/* running start of the current block -
				   MEMGETBADBLOCK needs 64 bits */
	int rc;

	blocklen = DEVESIZE (dev);

	/* Erase sector size is always a power of 2 */
	top_of_range = (DEVOFFSET (dev) & ~(blocklen - 1)) +
		ENVSECTORS (dev) * blocklen;

	erase_offset = offset & ~(blocklen - 1);

	/* Maximum area we may use */
	erase_len = top_of_range - erase_offset;

	blockstart = erase_offset;
	/* Offset inside a block */
	block_seek = offset - erase_offset;

	/*
	 * Data size we actually have to write: from the start of the block
	 * to the start of the data, then count bytes of data, and to the
	 * end of the block
	 */
	write_total = (block_seek + count + blocklen - 1) & ~(blocklen - 1);

	/*
	 * Support data anywhere within erase sectors: read out the complete
	 * area to be erased, replace the environment image, write the whole
	 * block back again.
	 */
	if (write_total > count) {
		data = malloc (erase_len);
		if (!data) {
			fprintf (stderr,
				 "Cannot malloc %u bytes: %s\n",
				 erase_len, strerror (errno));
			return -1;
		}

		rc = flash_read_buf (dev, fd, data, write_total, erase_offset,
				     mtd_type);
		if (write_total != rc)
			return -1;

		/* Overwrite the old environment */
		memcpy (data + block_seek, buf, count);
	} else {
		/*
		 * We get here, iff offset is block-aligned and count is a
		 * multiple of blocklen - see write_total calculation above
		 */
		data = buf;
	}

	if (mtd_type == MTD_NANDFLASH) {
		/*
		 * NAND: calculate which blocks we are writing. We have
		 * to write one block at a time to skip bad blocks.
		 */
		erasesize = blocklen;
	} else {
		erasesize = erase_len;
	}

	erase.length = erasesize;

	/* This only runs once on NOR flash */
	while (processed < write_total) {
		rc = flash_bad_block (fd, mtd_type, &blockstart);
		if (rc < 0)		/* block test failed */
			return rc;

		if (blockstart + erasesize > top_of_range) {
			fprintf (stderr, "End of range reached, aborting\n");
			return -1;
		}

		if (rc) {		/* block is bad */
			blockstart += blocklen;
			continue;
		}

		erase.start = blockstart;
		ioctl (fd, MEMUNLOCK, &erase);

		if (ioctl (fd, MEMERASE, &erase) != 0) {
			fprintf (stderr, "MTD erase error on %s: %s\n",
				 DEVNAME (dev),
				 strerror (errno));
			return -1;
		}

		if (lseek (fd, blockstart, SEEK_SET) == -1) {
			fprintf (stderr,
				 "Seek error on %s: %s\n",
				 DEVNAME (dev), strerror (errno));
			return -1;
		}

#ifdef DEBUG
		printf ("Write 0x%x bytes at 0x%llx\n", erasesize, blockstart);
#endif
		if (write (fd, data + processed, erasesize) != erasesize) {
			fprintf (stderr, "Write error on %s: %s\n",
				 DEVNAME (dev), strerror (errno));
			return -1;
		}

		ioctl (fd, MEMLOCK, &erase);

		processed  += blocklen;
		block_seek = 0;
		blockstart += blocklen;
	}

	if (write_total > count)
		free (data);

	return processed;
}

/*
 * Set obsolete flag at offset - NOR flash only
 */
static int flash_flag_obsolete (int dev, int fd, off_t offset)
{
	int rc;

	/* This relies on the fact, that obsolete_flag == 0 */
	rc = lseek (fd, offset, SEEK_SET);
	if (rc < 0) {
		fprintf (stderr, "Cannot seek to set the flag on %s \n",
			 DEVNAME (dev));
		return rc;
	}
	rc = write (fd, &obsolete_flag, sizeof (obsolete_flag));
	if (rc < 0)
		perror ("Could not set obsolete flag");

	return rc;
}

static int flash_write (int fd_current, int fd_target, int dev_target)
{
	int rc;

	switch (environment.flag_scheme) {
	case FLAG_NONE:
		break;
	case FLAG_INCREMENTAL:
		(*environment.flags)++;
		break;
	case FLAG_BOOLEAN:
		*environment.flags = active_flag;
		break;
	default:
		fprintf (stderr, "Unimplemented flash scheme %u \n",
			 environment.flag_scheme);
		return -1;
	}

#ifdef DEBUG
	printf ("Writing new environment at 0x%lx on %s\n",
		DEVOFFSET (dev_target), DEVNAME (dev_target));
#endif
	rc = flash_write_buf (dev_target, fd_target, environment.image,
			      CONFIG_ENV_SIZE, DEVOFFSET (dev_target),
			      DEVTYPE(dev_target));
	if (rc < 0)
		return rc;

	if (environment.flag_scheme == FLAG_BOOLEAN) {
		/* Have to set obsolete flag */
		off_t offset = DEVOFFSET (dev_current) +
			offsetof (struct env_image_redundant, flags);
#ifdef DEBUG
		printf ("Setting obsolete flag in environment at 0x%lx on %s\n",
			DEVOFFSET (dev_current), DEVNAME (dev_current));
#endif
		flash_flag_obsolete (dev_current, fd_current, offset);
	}

	return 0;
}

static int flash_read (int fd)
{
	struct mtd_info_user mtdinfo;
	int rc;

	rc = ioctl (fd, MEMGETINFO, &mtdinfo);
	if (rc < 0) {
		perror ("Cannot get MTD information");
		return -1;
	}

	if (mtdinfo.type != MTD_NORFLASH && mtdinfo.type != MTD_NANDFLASH) {
		fprintf (stderr, "Unsupported flash type %u\n", mtdinfo.type);
		return -1;
	}

	DEVTYPE(dev_current) = mtdinfo.type;

	rc = flash_read_buf (dev_current, fd, environment.image, CONFIG_ENV_SIZE,
			     DEVOFFSET (dev_current), mtdinfo.type);

	return (rc != CONFIG_ENV_SIZE) ? -1 : 0;
}

static int flash_io (int mode)
{
	int fd_current, fd_target, rc, dev_target;

	/* dev_current: fd_current, erase_current */
	fd_current = open (DEVNAME (dev_current), mode);
	if (fd_current < 0) {
		fprintf (stderr,
			 "Can't open %s: %s\n",
			 DEVNAME (dev_current), strerror (errno));
		return -1;
	}

	if (mode == O_RDWR) {
		if (HaveRedundEnv) {
			/* switch to next partition for writing */
			dev_target = !dev_current;
			/* dev_target: fd_target, erase_target */
			fd_target = open (DEVNAME (dev_target), mode);
			if (fd_target < 0) {
				fprintf (stderr,
					 "Can't open %s: %s\n",
					 DEVNAME (dev_target),
					 strerror (errno));
				rc = -1;
				goto exit;
			}
		} else {
			dev_target = dev_current;
			fd_target = fd_current;
		}

		rc = flash_write (fd_current, fd_target, dev_target);

		if (HaveRedundEnv) {
			if (close (fd_target)) {
				fprintf (stderr,
					"I/O error on %s: %s\n",
					DEVNAME (dev_target),
					strerror (errno));
				rc = -1;
			}
		}
	} else {
		rc = flash_read (fd_current);
	}

exit:
	if (close (fd_current)) {
		fprintf (stderr,
			 "I/O error on %s: %s\n",
			 DEVNAME (dev_current), strerror (errno));
		return -1;
	}

	return rc;
}

/*
 * s1 is either a simple 'name', or a 'name=value' pair.
 * s2 is a 'name=value' pair.
 * If the names match, return the value of s2, else NULL.
 */

static char *envmatch (char * s1, char * s2)
{

	while (*s1 == *s2++)
		if (*s1++ == '=')
			return s2;
	if (*s1 == '\0' && *(s2 - 1) == '=')
		return s2;
	return NULL;
}

/*
 * Prevent confusion if running from erased flash memory
 */
static int env_init (void)
{
	int crc0, crc0_ok;
	char flag0;
	void *addr0;

	int crc1, crc1_ok;
	char flag1;
	void *addr1;

	struct env_image_single *single;
	struct env_image_redundant *redundant;

	if (parse_config ())		/* should fill envdevices */
		return -1;

	addr0 = calloc (1, CONFIG_ENV_SIZE);
	if (addr0 == NULL) {
		fprintf (stderr,
			"Not enough memory for environment (%ld bytes)\n",
			CONFIG_ENV_SIZE);
		return -1;
	}

	/* read environment from FLASH to local buffer */
	environment.image = addr0;

	if (HaveRedundEnv) {
		redundant = addr0;
		environment.crc		= &redundant->crc;
		environment.flags	= &redundant->flags;
		environment.data	= redundant->data;
	} else {
		single = addr0;
		environment.crc		= &single->crc;
		environment.flags	= NULL;
		environment.data	= single->data;
	}

	dev_current = 0;
	if (flash_io (O_RDONLY))
		return -1;

	crc0 = crc32 (0, (uint8_t *) environment.data, ENV_SIZE);
	crc0_ok = (crc0 == *environment.crc);
	if (!HaveRedundEnv) {
		if (!crc0_ok) {
			fprintf (stderr,
				"Warning: Bad CRC, using default environment\n");
			memcpy(environment.data, default_environment, sizeof default_environment);
		}
	} else {
		flag0 = *environment.flags;

		dev_current = 1;
		addr1 = calloc (1, CONFIG_ENV_SIZE);
		if (addr1 == NULL) {
			fprintf (stderr,
				"Not enough memory for environment (%ld bytes)\n",
				CONFIG_ENV_SIZE);
			return -1;
		}
		redundant = addr1;

		/*
		 * have to set environment.image for flash_read(), careful -
		 * other pointers in environment still point inside addr0
		 */
		environment.image = addr1;
		if (flash_io (O_RDONLY))
			return -1;

		/* Check flag scheme compatibility */
		if (DEVTYPE(dev_current) == MTD_NORFLASH &&
		    DEVTYPE(!dev_current) == MTD_NORFLASH) {
			environment.flag_scheme = FLAG_BOOLEAN;
		} else if (DEVTYPE(dev_current) == MTD_NANDFLASH &&
			   DEVTYPE(!dev_current) == MTD_NANDFLASH) {
			environment.flag_scheme = FLAG_INCREMENTAL;
		} else {
			fprintf (stderr, "Incompatible flash types!\n");
			return -1;
		}

		crc1 = crc32 (0, (uint8_t *) redundant->data, ENV_SIZE);
		crc1_ok = (crc1 == redundant->crc);
		flag1 = redundant->flags;

		if (crc0_ok && !crc1_ok) {
			dev_current = 0;
		} else if (!crc0_ok && crc1_ok) {
			dev_current = 1;
		} else if (!crc0_ok && !crc1_ok) {
			fprintf (stderr,
				"Warning: Bad CRC, using default environment\n");
			memcpy (environment.data, default_environment,
				sizeof default_environment);
			dev_current = 0;
		} else {
			switch (environment.flag_scheme) {
			case FLAG_BOOLEAN:
				if (flag0 == active_flag &&
				    flag1 == obsolete_flag) {
					dev_current = 0;
				} else if (flag0 == obsolete_flag &&
					   flag1 == active_flag) {
					dev_current = 1;
				} else if (flag0 == flag1) {
					dev_current = 0;
				} else if (flag0 == 0xFF) {
					dev_current = 0;
				} else if (flag1 == 0xFF) {
					dev_current = 1;
				} else {
					dev_current = 0;
				}
				break;
			case FLAG_INCREMENTAL:
				if ((flag0 == 255 && flag1 == 0) ||
				    flag1 > flag0)
					dev_current = 1;
				else if ((flag1 == 255 && flag0 == 0) ||
					 flag0 > flag1)
					dev_current = 0;
				else /* flags are equal - almost impossible */
					dev_current = 0;
				break;
			default:
				fprintf (stderr, "Unknown flag scheme %u \n",
					 environment.flag_scheme);
				return -1;
			}
		}

		/*
		 * If we are reading, we don't need the flag and the CRC any
		 * more, if we are writing, we will re-calculate CRC and update
		 * flags before writing out
		 */
		if (dev_current) {
			environment.image	= addr1;
			environment.crc		= &redundant->crc;
			environment.flags	= &redundant->flags;
			environment.data	= redundant->data;
			free (addr0);
		} else {
			environment.image	= addr0;
			/* Other pointers are already set */
			free (addr1);
		}
	}
	return 0;
}


static int parse_config ()
{
	struct stat st;

#if defined(CONFIG_FILE)
	/* Fills in DEVNAME(), ENVSIZE(), DEVESIZE(). Or don't. */
	if (get_config (CONFIG_FILE)) {
		fprintf (stderr,
			"Cannot parse config file: %s\n", strerror (errno));
		return -1;
	}
#else
	strcpy (DEVNAME (0), DEVICE1_NAME);
	DEVOFFSET (0) = DEVICE1_OFFSET;
	ENVSIZE (0) = ENV1_SIZE;
	DEVESIZE (0) = DEVICE1_ESIZE;
	ENVSECTORS (0) = DEVICE1_ENVSECTORS;
#ifdef HAVE_REDUND
	strcpy (DEVNAME (1), DEVICE2_NAME);
	DEVOFFSET (1) = DEVICE2_OFFSET;
	ENVSIZE (1) = ENV2_SIZE;
	DEVESIZE (1) = DEVICE2_ESIZE;
	ENVSECTORS (1) = DEVICE2_ENVSECTORS;
	HaveRedundEnv = 1;
#endif
#endif
	if (stat (DEVNAME (0), &st)) {
		fprintf (stderr,
			"Cannot access MTD device %s: %s\n",
			DEVNAME (0), strerror (errno));
		return -1;
	}

	if (HaveRedundEnv && stat (DEVNAME (1), &st)) {
		fprintf (stderr,
			"Cannot access MTD device %s: %s\n",
			DEVNAME (1), strerror (errno));
		return -1;
	}
	return 0;
}

#if defined(CONFIG_FILE)
static int get_config (char *fname)
{
	FILE *fp;
	int i = 0;
	int rc;
	char dump[128];

	fp = fopen (fname, "r");
	if (fp == NULL)
		return -1;

	while (i < 2 && fgets (dump, sizeof (dump), fp)) {
		/* Skip incomplete conversions and comment strings */
		if (dump[0] == '#')
			continue;

		rc = sscanf (dump, "%s %lx %lx %lx %lx",
			     DEVNAME (i),
			     &DEVOFFSET (i),
			     &ENVSIZE (i),
			     &DEVESIZE (i),
			     &ENVSECTORS (i));

		if (rc < 4)
			continue;

		if (rc < 5)
			/* Default - 1 sector */
			ENVSECTORS (i) = 1;

		i++;
	}
	fclose (fp);

	HaveRedundEnv = i - 1;
	if (!i) {			/* No valid entries found */
		errno = EINVAL;
		return -1;
	} else
		return 0;
}
#endif
