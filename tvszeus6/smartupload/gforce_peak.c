#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "gforce_peak.h"

/*
 * if (peak > threshhold) save time
 * compare this time to the last one in the array
 * if (new - old) > (pre + post) add start time to array
 * else  update endtime
 * 
 */

#define TIME_IN_LOCAL (0)
#define TIME_IN_UTC   (1)


/* Read NMEMB elements of SIZE bytes into PTR from STREAM.  Returns the
 * number of elements read, and a short count if an eof or non-interrupt
 * error is encountered.  */
static size_t safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret = 0;

	do {
		clearerr(stream);
		ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
	} while (ret < nmemb && ferror(stream) && errno == EINTR);

	return ret;
}

static char *safe_fgets(char *s, int size, FILE *stream)
{
  char *ret;
  
  do {
    clearerr(stream);
    ret = fgets(s, size, stream);
  } while (ret == NULL && ferror(stream) && errno == EINTR);
  
  return ret;
}

/*
 * translate X,Y,Z to FB,LR,UD.
 * returns 1 on wrong order value */
int translate_peak_value(int order, int refX, int refY, int refZ,
			 int peakX, int peakY, int peakZ,
			 float *peakFB, float *peakLR, float *peakUD)
{
  if (((order & 0xf0) == 0x10) || ((order & 0xf0) == 0x20)) {
    if (((order & 0x0f) == 0x01) || ((order & 0x0f) == 0x02)) {
      *peakFB = (peakX - refX) / 37.0;
      *peakLR = (peakY - refY) / 37.0;
      *peakUD = (peakZ - refZ) / 37.0;
    } else {
      *peakFB = (peakY - refY) / 37.0;
      *peakLR = (peakX - refX) / 37.0;
      *peakUD = (peakZ - refZ) / 37.0;
    }
  } else if (((order & 0xf0) == 0x30) || ((order & 0xf0) == 0x40)) {
    if (((order & 0x0f) == 0x01) || ((order & 0x0f) == 0x02)) {
      *peakFB = (peakY - refY) / 37.0;
      *peakLR = (peakZ - refZ) / 37.0;
      *peakUD = (peakX - refX) / 37.0;
    } else {
      *peakFB = (peakZ - refZ) / 37.0;
      *peakLR = (peakY - refY) / 37.0;
      *peakUD = (peakX - refX) / 37.0;
    }
  } else if (((order & 0xf0) == 0x50) || ((order & 0xf0) == 0x60)) {
    if (((order & 0x0f) == 0x01) || ((order & 0x0f) == 0x02)) {
      *peakFB = (peakX - refX) / 37.0;
      *peakLR = (peakZ - refZ) / 37.0;
      *peakUD = (peakY - refY) / 37.0;
    } else {
      *peakFB = (peakZ - refZ) / 37.0;
      *peakLR = (peakX - refX) / 37.0;
      *peakUD = (peakY - refY) / 37.0;
    }
  } else {
    return 1;
  }

  return 0;
}

void add_event_to_head(struct peak_event *pel, time_t start, time_t end)
{
  struct peak_event *tmp;
    tmp = (struct peak_event *)malloc(sizeof(struct peak_event));
    tmp->start = start;
    tmp->end = end;
    list_add(&tmp->list, &pel->list);
    fprintf(stderr, "added item %ld,%ld to head\n", start, end);
}

void add_event_to_tail(struct peak_event *pel, time_t start, time_t end)
{
  struct peak_event *tmp;
    tmp = (struct peak_event *)malloc(sizeof(struct peak_event));
    tmp->start = start;
    tmp->end = end;
    list_add_tail(&tmp->list, &pel->list);
    fprintf(stderr, "added item %ld,%ld to tail\n", start, end);
}

/* returns 1 if added/merged to list */
static int add_peak_event_to_list(struct peak_event *pel, struct tm *bt,
				  int time_is_in_utc,
				  int pre_event, int post_event)
{
  struct list_head *pos, *n;
  struct peak_event *cur;
  time_t t;
  if (time_is_in_utc) {
    t = timegm(bt);
  } else {
    t = mktime(bt);
  }

  int pre_and_post = pre_event + post_event;

  if (list_empty(&pel->list)) {
    add_event_to_head(pel, t, t);
    return 1;
  } else {
    /* check if we can insert in the begining of the list */
    cur = list_entry(pel->list.next, struct peak_event, list);
    /*fprintf(stderr, "comparing 1st item %ld,%ld with %ld\n",
      cur->start, cur->end, t);*/
    /* -------|<--pre--NEW--post-->|--------|<--pre--EVENT--post-->|------
     * insert new record
     */ 
    if (t < (cur->start - pre_and_post)) {
      add_event_to_head(pel, t, t);
      return 1;
    }
    
    list_for_each_safe(pos, n, &pel->list) {
      cur = list_entry(pos, struct peak_event, list);
      /*fprintf(stderr, "comparing item %ld,%ld with %ld\n",
	cur->start, cur->end, t);*/
      
      
      /* -------|<--pre--NEW--post-->|
       *                 --post-->|<--pre--EVENT--post-->|------
       * update the existing start time with the new event       
       */ 
      if ((t >= (cur->start - pre_and_post)) && (t < cur->start)) {
	fprintf(stderr, "updating start %ld to %ld\n", cur->start, t);
	cur->start = t;
	return 1;
	
      /*            |<--pre--NEW--post-->|
       * ------|<--pre--|S--EVENT--E|--post-->|------ (S:start, E:end)
       * event alread handled: no nothing       
       */ 
      } else if ((t >= cur->start) && (t <= cur->end)) {
	return 0;

      /*           |<--pre-- NEW --post-->|
       * ------|<--pre--EVENT--post-->|<--pre--
       * update the existing end time with the new event       
       */ 
      } else if ((t > cur->end) && (t <= (cur->end + pre_and_post))) {
	fprintf(stderr, "updating end %ld to %ld\n", cur->end, t);
	cur->end = t;
	return 1;
      }
    }

    /* if came here, we can append at the end of the list */
    /*                                     |<--pre-- NEW --post-->|
     * ------|<--pre--LASTEVENT--post-->|-----
     * append to the end
     */ 
    add_event_to_tail(pel, t, t);
  }

  return 1;
}

int is_over_peak(struct gforce_trigger_point *tp,
		 float fb, float lr, float ud)
{
  //fprintf(stderr, "is_over_peak:%.2f,%.2f:%.2f,%.2f:%.2f,%.2f\n",
  //	  tp->fb_pos,tp->fb_neg,tp->lr_pos,tp->lr_neg,tp->ud_pos,tp->ud_neg);
  if (fb < 0.0) {
    if (fb <= tp->fb_neg) {
      return 1;
    }
  } else {
    if (fb >= tp->fb_pos) {
      return 1;
    }
  }

  if (lr < 0.0) {
    if (lr <= tp->lr_neg) {
      return 1;
    }
  } else {
    if (lr >= tp->lr_pos) {
      return 1;
    }
  }

  if (ud < 1.0) {
    if (ud <= tp->ud_neg) {
      return 1;
    }
  } else {
    if (ud >= tp->ud_pos) {
      return 1;
    }
  }

  return 0;
}


/* returns # of peak value found */
int scan_GPS_log(char *filename, struct peak_event *pel,
		 int pre_event, int post_event,
		 struct gforce_trigger_point *tp)
{
  int ret, count = 0;
  FILE *fp;
  char line[256];
  int year, month, day;
  double latitude, longitude;
  char ns, ew;
  float speed, direction, lr, fb, ud;
  struct tm bt;

  fp = fopen(filename, "r");
  if (fp) {
    /* read date */
   if (safe_fgets(line, sizeof(line), fp) == NULL) {
     fclose(fp);
     return 0;
   }
   ret = sscanf(line,
		"%04d-%02d-%02d",
		&year, &month, &day);
   if (ret != 3) {
     /* read date from the filename */
     char *date, *end, *file;
     file = strdup(filename);
     date = strchr(file, '_');
     if (date) {
       date++;
       end = strchr(date, '_');
       if (end) {
	 *end = 0;
	 ret = sscanf(date,
		      "%04d%02d%02d",
		      &year, &month, &day);
	 if (ret != 3) {
	   free(file);
	   fclose(fp);
	   return 0;
	 }
       }
     }
     free(file);
   }

   while (safe_fgets(line, sizeof(line), fp) != NULL) {
      if (!strncmp(line, "16,", 3)) {
	ret = sscanf(line + 3,
		     "%d:%d:%d,%lf%c%lf%c%fD%f,%f,%f,%f",
		     &bt.tm_hour, &bt.tm_min, &bt.tm_sec,
		     &latitude, &ns, &longitude, &ew,
		     &speed, &direction, &fb, &lr, &ud);
	if (ret == 12) {
	  bt.tm_year = year - 1900;
	  bt.tm_mon = month - 1;
	  bt.tm_mday = day;
	  bt.tm_isdst = -1;
	  count++;
	  //fprintf(stderr, "%s", line);
	  if (is_over_peak(tp, fb, lr, ud)) {
	    fprintf(stderr, "gforce event:%.2f,%.2f,%.2f\n", fb,lr,ud);
	    add_peak_event_to_list(pel, &bt,
				   TIME_IN_LOCAL, pre_event, post_event);
	  }
	}
      }
    }
    fclose(fp);
  }

  return count;
}

#define UPLOAD_ACK_LEN 10

/* returns # of peak value found */
int scan_peak_data(char *filename, struct peak_event *pel,
		   int time_is_in_utc,
		   int pre_event, int post_event,
		   struct gforce_trigger_point *tp)
{
  int count = 0;
  FILE *fp = NULL;
  unsigned char buf[16];
  int nbytes, items, len;
  int refX, refY, refZ, order;
  float fb, lr, ud;

  fprintf(stderr, "trigger:%.2f,%.2f:%.2f,%.2f:%.2f,%.2f\n",
  	  tp->fb_pos,tp->fb_neg,tp->lr_pos,tp->lr_neg,tp->ud_pos,tp->ud_neg);

  refX = refY = refZ = 0x200;
  order = 0x12;

  fp = fopen(filename, "r");
  if (!fp)
    goto peak_end;

  /* file content: upload ack + 0g + order + n*peakdata + checksum */

  /*
   * read upload_ack command saved in the file to get the data size:
   * 0a 00 04 1f 03 + 4 bytes for data length + chksum
   */
  nbytes = safe_fread(buf, sizeof(char), UPLOAD_ACK_LEN, fp);
  //if (nbytes != UPLOAD_ACK_LEN)
  //goto peak_end;

  len = (buf[5] << 24) | (buf[6] << 16) | (buf[7] << 8) | buf[8];

  /* read 0g(6 bytes) & direction (1 byte) */
  nbytes = safe_fread(buf, sizeof(char), 7, fp);
  //if (nbytes != 7)
  //goto peak_end;

  refX = (buf[0] << 8) | buf[1];
  refY = (buf[2] << 8) | buf[3];
  refZ = (buf[4] << 8) | buf[5];
  order = buf[6];
  fprintf(stderr, "order:%02x,ref-X:%02x,Y:%02x,Z:%02x\n",
	  order, refX, refY, refZ); 
 
  /* read each peak data */
  items = len / 16;
  while (items > 0) {
    nbytes = safe_fread(buf, sizeof(char), 16, fp);
    if (nbytes < 16)
      break;
    if ((buf[0] == 1) && (buf[3] == 2) && (buf[6] == 3)) {
      count++;
      struct tm bt;
      bt.tm_year = (buf[9] >> 4) * 10 + (buf[9] & 0x0f) + 100;
      bt.tm_mon = ((buf[10] >> 4) & 0x01) * 10 + (buf[10] & 0x0f) - 1;
      bt.tm_mday = ((buf[11] >> 4) & 0x03) * 10 + (buf[11] & 0x0f);
      bt.tm_hour = ((buf[12] >> 4) & 0x03) * 10 + (buf[12] & 0x0f);
      bt.tm_min = (buf[13] >> 4) * 10 + (buf[13] & 0x0f);
      bt.tm_sec = (buf[14] >> 4) * 10 + (buf[14] & 0x0f);
      bt.tm_isdst = -1;
      /*
      time_t t = time_is_in_utc ? timegm(&bt) : mktime(&bt);
      fprintf(stderr, "%s,%02x %02x,%02x %02x,%02x %02x\n", 
	      ctime(&t), 
	      buf[1], buf[2], buf[4], buf[5], buf[7], buf[8]); 
      */
      if (!translate_peak_value(order, refX, refY, refZ,
				(buf[1] << 8) | buf[2],
				(buf[4] << 8) | buf[5],
				(buf[7] << 8) | buf[8],
				&fb, &lr, &ud)) {
	fprintf(stderr, "fb:%.2f, lr:%.2f, ud:%.2f\n", fb, lr, ud); 
	if (is_over_peak(tp, fb, lr, ud)) {
	  fprintf(stderr, "gforce event:%.2f,%.2f,%.2f\n", fb,lr,ud);
	  add_peak_event_to_list(pel, &bt,
				 time_is_in_utc, pre_event, post_event);
	}
      }
    }
    items--;
  }

 peak_end:
  if (fp) fclose(fp);

  return count;
}
#if 0
int main()
{
  int ret;
  int pre_event = 30, post_event = 30;
  struct peak_event pel, *tmp;
  struct gforce_trigger_point tp;
  tp.fb_pos = 1.2;
  tp.fb_neg = -1.3;
  tp.lr_pos = 1.0;
  tp.lr_neg = -1.0;
  tp.ud_pos = 3.0;
  tp.ud_neg = -2.0;

  INIT_LIST_HEAD(&pel.list);

  ret = scan_GPS_log("BUS001_20100316_L.001", &pel,
		     pre_event, post_event, &tp);
  fprintf(stderr, "%d items found\n", ret);
  ret = scan_peak_data("BUS001_20090930141745_TAB102log_L.peak",
		       &pel, 0, pre_event, post_event, &tp);
  fprintf(stderr, "%d items found\n", ret);

  struct list_head *pos, *n;
  list_for_each_safe(pos, n, &pel.list) {
    tmp = list_entry(pos, struct peak_event, list);
    fprintf(stderr, "freeing item %ld,%ld\n", tmp->start, tmp->end);
    list_del(pos);
    free(tmp);
  }

  return 0;
}
#endif
