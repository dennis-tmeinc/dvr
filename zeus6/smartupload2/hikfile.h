#ifndef _HIKFILE_H_
#define _HIKFILE_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// .264 file struct
struct hd_file {
	unsigned int flag;					//      "4HKH", 0x484b4834
	unsigned int res1[6];
	unsigned int width_height;			// lower word: width, high word: height
	unsigned int res2[2];
};

struct hd_frame {
	unsigned int flag;					// 1
	unsigned int serial;
	unsigned int timestamp;
	unsigned int res1;
	unsigned int d_frames;				// sub frames number, lower 8 bits
	unsigned int width_height;			// lower word: width, higher word: height
	unsigned int res3[6];
};

struct hd_subframe {
	unsigned int d_flag ;				// lower 16 bits as flag, audio: 0x1001  B frame: 0x1005
	unsigned int res1[3];
	unsigned int framesize;			// 0x50 for audio
};

int copy_frames(FILE *fp_src, FILE *fp_dst, FILE *fp_dst_k,
		long long from_offset, long long to_offset,
		long long toffset_start,
		time_t filetime, time_t *new_filetime, time_t *new_len);

#ifdef __cplusplus
}
#endif

#endif
