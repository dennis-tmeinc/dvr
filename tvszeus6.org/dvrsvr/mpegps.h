#ifndef __MPEGPS_H__
#define __MPEGPS_H__

#include <stdio.h>
#include <stdint.h>
#include "block.h"
#include "dvr.h"

#define COPY_PS /* copy ps header to data */
#ifdef COPY_PS
#define BLOCK_T psblock_t
#else
#define BLOCK_T block_t
#endif

#define min1(a,b) \
  ({ __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a < _b ? _a : _b; })

#define max1(a,b) \
  ({ __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _a : _b; })

typedef int64_t mtime_t ;

#define PS_TK_COUNT (512 - 0xc0)
#define PS_ID_TO_TK( id ) ((id) <= 0xff ? (id) - 0xc0 : ((id)&0xff) + (256 - 0xc0))
#define SUCCESS 0
#define EGENERIC 1

#define GOT_PSM 0x01
#define GOT_E1 0x02 // Filler(type 9) with PTS
#define GOT_E2 0x04 // Sequence parameter set
#define GOT_E3 0x04 // picture parameter set

#define BLOCK_FLAG_TYPE_I     0x0002
#define BLOCK_FLAG_TYPE_PB    0x0010
#define BLOCK_FLAG_TYPE_AUDIO 0x2000

struct psblock_t
{
  mtime_t i_pts;
  mtime_t i_dts;
  int i_buffer;
  uint8_t *p_buffer; /* for pointer manipulation */
  uint8_t *p_buf;
  int i_buf;
  uint32_t    i_flags;
};

typedef struct
{
    bool  b_seen;
    int         i_skip;
    int         i_id;
    mtime_t     i_first_pts;
    mtime_t     i_last_pts;
} ps_track_t;

struct ps_stream {
	FILE *fp;
	int64_t end; // size
};

struct ps_buffer {
	uint8_t *pStart, *pCurrent;
	int size; 
};

class CMpegPs
{
public:
	CMpegPs();
	~CMpegPs();
	void Init();
#ifdef COPY_PS
	int GetFrame(char *buf, int sizeof_buf, int *frametype, int64_t *ts);
#else
	block_t *GetFrame();
#endif
	mtime_t FindFirstTimeStamp();
	void SetStream(FILE *fp);
	void SetBuffer(uint8_t *pBuf, int size);
	int StreamPeek(uint8_t *pBuf, int size );
	mtime_t StreamSize();
	mtime_t GetFirstTimeStamp();
	mtime_t FindLength();
	void SetEncrptTable(unsigned char * pCryptTable, int size = 1024);
	int Mux(int frame_type, int i_es_size, int64_t i_dts,
		int audio_enabled);
	int MuxWritePackHeader( int64_t i_dts );
	int MuxWritePSM( char *p_buffer, int audio_enabled);
	char m_ps_hdr[100];
private:
	bool m_bLostSync, m_bHavePack;
	int m_iMuxRate;
	mtime_t m_iLength, m_iTimeTrack, m_iScr, m_iCurrentPts;
	ps_track_t  m_tk[PS_TK_COUNT];
	struct ps_stream m_stream;
	struct ps_buffer m_buffer;
	int m_iFlag; // for future use
	unsigned char *m_pCryptTable;
	int m_encryptSize; /* max size to encrypt in video/audio data */
	block_t *m_pBlockSave;
	int m_i_header_len; /* header length for I frame */
	/* for mux support */
	bool m_b_mpeg2;
	int m_i_psm_version;
	uint32_t m_crc32_table[256];
	int m_i_dts_delay;
#ifdef COPY_PS
	uint8_t *m_buf; /* should be big enough (500kb) */
	int m_bufSize, m_sizeof_m_buf;
	psblock_t m_block;
	int copyToBuffer(psblock_t *p_pkt);
#endif
	int Demux2(bool b_end, mtime_t *found_ts = NULL);
#ifdef COPY_PS
	int Demux(int *frametype, int64_t *ts);
#else
	int Demux(block_t **pp_block);
#endif
        BLOCK_T *PktRead(uint32_t i_code );
	int PktResynch(uint32_t *i_code);
	BLOCK_T *StreamBlock( int i_size );
	int StreamRead(uint8_t *pBuf, int size);
};

#endif
