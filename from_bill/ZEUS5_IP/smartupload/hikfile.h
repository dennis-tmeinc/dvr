#ifndef _HIKFILE_H_
#define _HIKFILE_H_

#include <stdio.h>
#include "dvrproto.h"

int copy_frames(FILE *fp_src, FILE *fp_dst, FILE *fp_dst_k,
		long long from_offset, long long to_offset,
		long long toffset_start,
		time_t filetime, time_t *new_filetime, time_t *new_len);

#endif
