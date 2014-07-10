#pragma once

#define CONTROL_PORT 49953
#define STREAM_PORT 49954

#define CONTROL_OPEN_FILE 1
#define CONTROL_CLOSE_FILE 2
#define CONTROL_SEEK		3
#define CONTROL_CHECKSUM	4
#define CONTROL_CHECKSUM_ASYN 5
#define CONTROL_WRITE		6
#define CONTROL_SUCCESS		10000
#define CONTROL_FAIL		10001

#define CRC_OK		1
#define CRC_FAIL	2

#define WM_RECEIVE_DATA WM_APP+1
#define WM_RECEIVE_CRC_MSG WM_APP+2


struct controlresponse{
	DWORD cmd;
};

struct controlresponseseek{
	DWORD cmd;
	INT64 offset;
};

struct controlopenfile{
	DWORD cmd;
	char filename[MAX_PATH];
};

struct controlclose{
	DWORD cmd;
};

struct controlseek{
	DWORD cmd;
	INT64 offset;
	UINT from;
};

struct controlwrite{
	DWORD cmd;
	INT64 offset;
	UINT length;
	DWORD crc;
};

struct CRCAsynMessage{
	DWORD cmd;
	INT64 off_start;
	INT64 off_end;
	DWORD crc;
};

struct CRCMessage{
	DWORD cmd;
	char filename[MAX_PATH];
	INT64 off_start;
	INT64 off_end;
	DWORD crc;
};

struct CRCResponse{
	DWORD cmd;
};

extern DWORD CalculateCRC(char* buf, int size);