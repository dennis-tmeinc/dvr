#ifndef _OSDHDR_H_
#define _OSDHDR_H_

struct osd_hdr {
	unsigned char magic[4];
	unsigned short res;
	unsigned short osd_len;
};

struct osd_line_hdr {
	unsigned char magic[2];
	unsigned short res[2];
	unsigned short line_len;
};

struct osd_txt_hdr {
	unsigned char align;
	unsigned char x_offset, y_offset;
	unsigned char res;
};

#endif
