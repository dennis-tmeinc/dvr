#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* crash, peak file format
 * Byte 1 - 10: command repeated in the data
 *   1: length
 *   2: src ID
 *   3: dst ID
 *   4: command
 *   5: type of packet
 *   6-9: lenght of data
 *   10: checksum
 * Byte 11 - 17: 0G + direction code
 *   11-12: X
 *   13-14: Y
 *   15-16: Z
 *      17: direction code: used by smartftp
 * Byte 18 - : 16 bytes data frames repeated followed by checksum
 *             checumsum calculated from Byte 11
 *   1-3: channel 1 value
 *   4-6: channel 2 value
 *   7-9: channel 3 value
 *   10-15: YY MM DD hh mm ss (in local time)
 *   16: frame number
 */

#define TZDEBUG

int keep_src_file;
char *src_file, *dst_path, *time_zone;

void print_usage(char *cmd)
{
  fprintf(stderr, "Usage: %s [-s src_file] [-d dst_path] [-k]\n"
	  "\t src_file: source file with full path\n"
	  "\t dst_path: destination path\n"
	  "\t k: keep source file after conversion.\n",
	  cmd);
}

static size_t safe_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret = 0;
  
  do {
    clearerr(stream);
    ret += fwrite((char *)ptr + (ret * size), size, nmemb - ret, stream);
  } while (ret < nmemb && ferror(stream) && errno == EINTR);
  
  return ret;
}

static size_t safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret = 0;

  do {
    clearerr(stream);
    ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
  } while (ret < nmemb && ferror(stream) && errno == EINTR);

  return ret;
}

int bcd_to_binary(unsigned char bcd)
{
  return (bcd >> 4) * 10 + (bcd & 0xf);
}

/* x: 0 - 99,999 */
int binaryToBCD(int x)
{
  int m, I, b, cc;
  int bcd;

  cc = (x % 100) / 10;
  b = (x % 1000) / 100;
  I = (x % 10000) / 1000;
  m = x / 10000;

  bcd = (m * 9256 + I * 516 + b * 26 + cc) * 6 + x;

  return bcd;
}

static unsigned char getChecksum(unsigned char *buf, int size)
{
  unsigned char cs = 0;
  int i;

  for (i = 0; i < size; i++) {
    cs += buf[i];
  }

  cs = 0xff - cs;
  cs++;

  return  cs;
}

/* 0: success */
static int
get_filename_from_fullpath(char *fullname, char *name, int name_size)
{
  char *nameWithoutPath = strrchr(fullname, '/');
  if (nameWithoutPath) {
    strncpy(name, ++nameWithoutPath, name_size);
  } else {
    strncpy(name, fullname, name_size);
  }

  return (strlen(name) == 0) ? 1 : 0;
}

void remove_last_slash_from_path(char *path)
{
  char *start, *end;

  start = strchr(path, '/');
  end = strrchr(path, '/');
  if ((path[strlen(path) - 1] == '/') && (start != end)) {
    fprintf(stderr, "removing tail slash from:%s\n", path);
    path[strlen(path) - 1] = '\0';
  }
}

int main(int argc, char *argv[])
{
  int optchar;
  FILE *fp;
  int size = -1, bytes, total_size = 0, frames = 0;
  char *buf = NULL, frame[16], *fname = NULL, *dst = NULL;

  while ((optchar = getopt(argc, argv, "s:d:t:khj?")) != -1) {
    switch (optchar) {
    case 's':
      src_file = strdup(optarg);
      break;
    case 'd':
      dst_path = strdup(optarg);
      remove_last_slash_from_path(dst_path);
      break;
    case 't':
      time_zone = strdup(optarg);
      break;
    case 'k':
      keep_src_file = 1;
      break;
    case 'h':
    case '?':
    default:
      print_usage(argv[0]);
      exit(1);
    }
  }

  if (!src_file || !dst_path || !time_zone) {
    print_usage(argv[0]);
    exit (1);
  }

  setenv("TZ", time_zone, 1);

  fp = fopen(src_file, "r");
  if (!fp) {
    fprintf(stderr, "fopen error:%s\n", strerror(errno));
    exit(1);
  }

  if (!fseek(fp, 0, SEEK_END)) {
    size = ftell(fp);
    if (size != -1) {
      fprintf(stderr, "file size:%d\n", size);
      buf = (char *)malloc(size);
      if (!buf) {
	fprintf(stderr, "malloc error\n");
	exit(1);
      }
    }
  }

  fseek(fp, 0, SEEK_SET);
  /* read command + 0G + direction */
  bytes = safe_fread(buf, 1, 17, fp);
  if (bytes < 17) {
    fprintf(stderr, "read file error\n");
    exit(1);
  }
  total_size += 17;

  while ((bytes = safe_fread(frame, 1, 16, fp)) == 16) {
    time_t t;
    struct tm tm;
    int wrong_format = 0;
    if (!((frame[0] == 1) && (frame[3] == 2) && (frame[6] == 3))) {
      //fprintf(stderr, "wrong format:%02x,%02x,%02x\n",
      //      frame[0],frame[3],frame[6]);
      wrong_format = 1;
      /* strange: good data in the middle of 0xff's */
      //break;
    }

#ifdef TZDEBUG
    if (!wrong_format) {
      fprintf(stderr, "time(%d): %x %x %x %x %x %x ->",
	      frame[15], frame[9], frame[10], frame[11],
	      frame[12], frame[13], frame[14]);
    }
#endif
    if (!wrong_format &&
	!(((frame[9] == 0) && (frame[10] == 0)) || 
	  (((unsigned char)frame[9] == 0xff) &&
	   ((unsigned char)frame[10] == 0xff)))) {
      tm.tm_isdst = -1;
      tm.tm_year = bcd_to_binary(frame[9]) + 100;
      tm.tm_mon = bcd_to_binary(frame[10]) - 1;
      tm.tm_mday = bcd_to_binary(frame[11]);
      tm.tm_hour = bcd_to_binary(frame[12]);
      tm.tm_min = bcd_to_binary(frame[13]);
      tm.tm_sec = bcd_to_binary(frame[14]);
      t = timegm(&tm);
      localtime_r(&t, &tm);
      frame[9] = binaryToBCD(tm.tm_year - 100);
      frame[10] = binaryToBCD(tm.tm_mon + 1);
      frame[11] = binaryToBCD(tm.tm_mday);
      frame[12] = binaryToBCD(tm.tm_hour);
      frame[13] = binaryToBCD(tm.tm_min);
      frame[14] = binaryToBCD(tm.tm_sec);
    }
#ifdef TZDEBUG
    if (!wrong_format) {
      fprintf(stderr, " %x %x %x %x %x %x\n",
	      frame[9], frame[10], frame[11],
	      frame[12], frame[13], frame[14]);
    }
#endif

    memcpy(buf + total_size, frame, 16);
    total_size += 16;
    frames++;
  }
  fclose(fp);
  fp = NULL;

  buf[total_size] = (char)getChecksum((unsigned char *)buf + 17, 
				      total_size - 17);
  total_size++;
  fprintf(stderr, "total size:%d, frames:%d\n", total_size, frames);

  fname = strdup(src_file);
  if (fname) {
    if (!get_filename_from_fullpath(src_file, fname, strlen(fname))) {
      int dst_size = strlen(fname) + strlen(dst_path) + 2;
      dst = (char *)malloc(dst_size);
      if (dst) {
	strcpy(dst, dst_path);
	strcat(dst, "/");
	strcat(dst, fname);
	fp = fopen(dst, "w");
	free(dst);
      }
    }
    free(fname);
  }

  if (fp) {
    bytes = safe_fwrite(buf, 1, total_size, fp);
    fprintf(stderr, "%d bytes written to %s\n", bytes, dst_path);
    fclose(fp);

    if (!keep_src_file && (bytes == total_size)) {
      unlink(src_file);
    }
  }


  if (buf) free(buf);
  if (time_zone) free(time_zone);
  if (dst_path) free(dst_path);
  if (src_file) free(src_file);


  return 0;
}
