#ifndef _GFORCE_PEAK_H_
#define _GFORCE_PEAK_H_

#include "list.h"

struct peak_event {
  time_t start; /* event start time */
  time_t end;   /* event end time */
  struct list_head list;
};

struct gforce_trigger_point {
  float fb_pos, fb_neg; /* forward: pos */
  float lr_pos, lr_neg; /* right  : pos */
  float ud_pos, ud_neg; /* down   : pos */
};

int scan_GPS_log(char *filename, struct peak_event *pel,
		 int pre_event, int post_event,
		 struct gforce_trigger_point *tp);
int scan_peak_data(char *filename, struct peak_event *pel,
		   int time_is_in_utc,
		   int pre_event, int post_event,
		   struct gforce_trigger_point *tp);
#endif
