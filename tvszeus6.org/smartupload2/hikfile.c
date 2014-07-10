#include <stdlib.h>
#include "hikfile.h"

int parse_frame_header(struct hd_frame *frameh,
		       unsigned int *ts, int *nsub)
{
  if (frameh && frameh->flag == 1) {
    *ts = frameh->timestamp;
    *nsub = frameh->d_frames & 0xff;
    return 0;
  }

  //writeDebug("wrong frame header flag");
  return 1;
}

int parse_subframe_header(struct hd_subframe *frameh,
			  int *flag, unsigned int *size)
{
  if (frameh) {
    *flag = frameh->d_flag & 0xffff;
    *size = frameh->framesize;
    return 0;
  }

  //writeDebug("null subframe header");
  return 1;
}

int copy_subframes(FILE *fp_src, FILE *fp_dst, int nsub, int *videokey_found)
{
  struct hd_subframe subframeh;
  int f_flag, bytes;
  unsigned int f_size;
  char *buf = NULL;
  int bufsize = 0;
  int total = 0;

  //writeDebug("copy_subframes");
  if (!fp_src || !fp_dst)
    return -1;

  if (nsub <= 0)
    return -1;

  if (!videokey_found)
    return -1;

  *videokey_found = 0;

  while (nsub > 0) {
    /* copy subframe header to the new file */
    bytes = fread(&subframeh, 1,
		  sizeof(struct hd_subframe), fp_src);
    if (bytes != sizeof(struct hd_subframe)) {
      //writeDebug("fread subframe header");
      total = -1;
      break;
    }
    
    if (parse_subframe_header(&subframeh, &f_flag, &f_size)) {
      total = -1;
      break;
    }

    //writeDebug("subframe header(%x,%u)", f_flag, f_size);
    if (f_flag == 0x1003) {
      *videokey_found = 1;
    }
    
    bytes = fwrite(&subframeh, 1, sizeof(struct hd_subframe), fp_dst);
    if (bytes != sizeof(struct hd_subframe)) {
      //writeDebug("fwrite subframe header");
      total = -1;
      break;
    }
    total += bytes;

    /* copy subframe data to the new file */
    if (f_size > bufsize) {
      buf = realloc(buf, f_size);
      bufsize = f_size;
    }
    if (!buf) {
      //writeDebug("realloc error");
      total = -1;
      break;
    }

    bytes = fread(buf, 1, f_size, fp_src);
    if (bytes != f_size) {
      //writeDebug("fread subframe data");
      total = -1;
      break;
    }

    bytes = fwrite(buf, 1, f_size, fp_dst);
    if (bytes != f_size) {
      //writeDebug("fwrite subframe data");
      total = -1;
      break;
    }

    total += bytes;
    nsub--;
  }

  if (buf) free(buf);

  //writeDebug("copy_subframes done(%d)",total);

  return total;
}

int copy_frames(FILE *fp_src, FILE *fp_dst, FILE *fp_dst_k,
		long long from_offset, long long to_offset,
		long long toffset_start,
		time_t filetime, time_t *new_filetime, time_t *new_len)
{
  int bytes;
  int f_nsub = 0;
  unsigned int ts_start = 0, f_ts;
  struct hd_frame frameh;
  long startpos, nowpos;
  long long new_file_offset = 40;
  long long first_toffset = 0, last_toffset = 0;
  int first_toffset_written = 0;
#ifdef SLOW_COPY
  int bytes_to_flush = 0;
#endif

  if (!fp_src || !fp_dst)
    return 1;

  startpos = ftell(fp_src);

  /* read the 1st time stamp */
  bytes = fread(&frameh, 1,
		sizeof(struct hd_frame), fp_src);
  if (bytes != sizeof(struct hd_frame)) {
    //writeDebug("fread frame header");
    return 1;
  }

  if (parse_frame_header(&frameh, &f_ts, &f_nsub)) {
    return 1;
  }
  ts_start = f_ts;
  //writeDebug("1st ts:%lu", ts_start);

  /* seek to the 1st frame */
  if (from_offset) {
    if (fseek(fp_src, from_offset, SEEK_SET) == -1) {
      //writeDebug("fseek error to %lld", from_offset);
      return 1;
    }
  } else {
    fseek(fp_src, startpos, SEEK_SET);
  }

  int retval = 0;
  while (1) {
    nowpos = ftell(fp_src);
    if (to_offset && nowpos >= to_offset) {
      break;
    }

    /* copy frame header to the new file */
    clearerr(fp_src);
    bytes = fread(&frameh, 1,
		  sizeof(struct hd_frame), fp_src);
    if (bytes != sizeof(struct hd_frame)) {
      //writeDebug("fread frame header2:%d", bytes);
      if (ferror(fp_src))
	retval = 1;
      break;
    }

    if (parse_frame_header(&frameh, &f_ts, &f_nsub)) {
      retval = 1;
      break;
    }

    int ts_diff = f_ts - ts_start;
    long long ts_diff_in_millisec = ts_diff * 1000LL / 64LL; 
    long long absolute_ts_in_millisec = 
      filetime * 1000LL + toffset_start + ts_diff_in_millisec; 
    long long ts_diff_in_millisec_from_new_filetime = 
      absolute_ts_in_millisec - (*new_filetime * 1000LL); 

    while ((ts_diff_in_millisec_from_new_filetime < 0) &&
	   (new_file_offset == 40)) {
      //writeDebug("adjusting new_filetime");
      (*new_filetime)--;
      ts_diff_in_millisec_from_new_filetime = 
	absolute_ts_in_millisec - (*new_filetime * 1000LL);
    }
    //writeDebug("frame header(%d,%u,%lld)", f_nsub, f_ts, ts_diff_in_millisec_from_new_filetime);
    
    if (!first_toffset_written) {
      first_toffset_written = 1;
      first_toffset = ts_diff_in_millisec_from_new_filetime;
      //writeDebug("1st_offset:%lld,%ld,%lld,%lld,%lld",toffset_start,
      //	 ts_diff,ts_diff_in_millisec,
      //	 absolute_ts_in_millisec,ts_diff_in_millisec_from_new_filetime);
    }
    last_toffset = ts_diff_in_millisec_from_new_filetime;

    bytes = fwrite(&frameh, 1, sizeof(struct hd_frame), fp_dst);
    if (bytes != sizeof(struct hd_frame)) {
      //writeDebug("fwrite frame header");
      retval = 1;
      break;
    }

    int videokey_found;
    bytes = copy_subframes(fp_src, fp_dst, f_nsub, &videokey_found);
    if (bytes <= 0) {
      retval = 1;
      break;
    }

#ifdef SLOW_COPY
    bytes_to_flush += bytes;
    if (bytes_to_flush > (100 * 1024)) {
      bytes_to_flush = 0;
      fdatasync(fileno(fp_dst));
    }
#endif

    if (videokey_found) {
      fprintf(fp_dst_k, "%lld,%lld\n",
	      ts_diff_in_millisec_from_new_filetime, new_file_offset);
    }

    new_file_offset += (sizeof(struct hd_frame) + bytes);
  } 

  *new_len = (last_toffset - first_toffset) / 1000;
  if (*new_len < 0) {
    retval = 1;
  }
  //writeDebug("newlen:%ld(%lld,%lld)", *new_len, last_toffset, first_toffset);

#ifdef SLOW_COPY
    if (bytes_to_flush) {
      fdatasync(fileno(fp_dst));
    }
#endif

  return retval;
}
#if 0
  writeDebug("filetime:%ld", filetime);
  /* set start time for the new file */
  new_filetime = filetime;
  if (from_toffset) {
    new_filetime = filetime + (time_t)(from_toffset / 1000);
  }

  writeDebug("new_filetime:%ld", new_filetime);
  localtime_r(&new_filetime, &tm);

  /* do real copy now */
  get_filename(newname, sizeof(newname),
	       fi->ch, 0, new_type, servername,
	       &tm, "264");
  writeDebug("newname:%s", newname);

  newfile = malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile) {
    writeDebug("malloc error");
    retval = -1;
    goto error_exit;
  }
  sprintf(newfile, "%s/%s", dest_dir, newname);

  /* open original data file */
  fp = fopen(fullname, "w");
  if (!fp) {
    writeDebug("fopen1:%s(%s)", strerror(errno), fullname);
    retval = -1;
    goto error_exit;
  }

  /* open new file */
  fpnew = fopen(newfile, "w");
  if (!fpnew) {
    writeDebug("fopen2:%s(%s)", strerror(errno), newfile);
    retval = -1;
    goto error_exit;
  }
  writeDebug("new:%s", newfile);

  newkfile = create_k_filename(newfile);
  if (!newkfile) {
    retval = -1;
    goto error_exit;
  }

  /* open new k file */
  fpnew_k = fopen(newkfile, "w");
  if (!fpnew_k) {
    writeDebug("fopen3:%s(%s)", strerror(errno), newkfile);
    retval = -1;
    goto error_exit;
  }
  writeDebug("newk:%s", newkfile);

  /* copy file header */
  struct hd_file fileh;
  int bytes = fread(&fileh, 1,
		    sizeof(struct hd_file), fp);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fread header(%d, %s)", bytes, fullname);
    delete = 1;
    retval = -1;
    goto error_exit;
  }
  
  int encrypted = 1;
  if (fileh.flag == 0x484b4834)
    encrypted = 0;

  bytes = fwrite(&fileh, 1, sizeof(struct hd_file), fpnew);
  if (bytes != sizeof(struct hd_file)) {
    writeDebug("fwrite header(%s)", newfile);
    delete = 1;
    retval = -1;
    goto error_exit;
  }

  /* copy video/audio frames to the new file */
  if (copy_frames(fp, fpnew, fpnew_k,
		  from_offset, to_offset, toffset_start,
		  filetime, &new_filetime, &new_len)) {
    delete = 1;
    retval = -1;
    goto error_exit;
  }

  /* give the correct file name to the new file */
  localtime_r(&new_filetime, &tm);
  get_filename(newname, sizeof(newname),
	       fi->ch, (unsigned int)new_len, new_type, servername,
	       &tm, "264");
  newfile2 = malloc(strlen(dest_dir) + strlen(newname) + 24); //24:len
  if (!newfile2) {
    writeDebug("malloc error");
    delete = 1;
    retval = -1;
    goto error_exit;
  }
  sprintf(newfile2, "%s/%s", dest_dir, newname);

  writeDebug("rename %s %s", newfile, newfile2);
  if (rename(newfile, newfile2) == -1) {
    writeDebug("rename error (%s) %s %s", strerror(errno), newfile, newfile2);
  }
  /* save the new filename */
  if (new_full_name && (new_full_name_size > 0)) {
    strncpy(new_full_name, newfile2, new_full_name_size);
  }

  /* give the correct file name to the new k file */
  newkfile2 = create_k_filename(newfile2);
  if (!newkfile2) {
    retval = -1;
    goto error_exit;
  }
  if (rename(newkfile, newkfile2) == -1) {
    writeDebug("rename error (%s) %s %s", strerror(errno), newkfile, newkfile2);
  }
  free(newfile2); newfile2 = NULL;
//-----
 error_exit:

  if (fpnew_k) fclose(fpnew_k);
  if (fpnew) fclose(fpnew);
  if (fp) fclose(fp);

  if (delete) {
    writeDebug("deleting %s", newkfile);
    unlink(newkfile);
    writeDebug("deleting %s", newfile);
    unlink(newfile);
  }

  if (newkfile2) free(newkfile2);
  if (newfile2) free(newfile2);
  if (newkfile) free(newkfile);
  if (newfile) free(newfile);
  if (kfilename) free(kfilename);

  if (retval && is_disk_full(dest_dir))
    return 1;

  return retval;
#endif
