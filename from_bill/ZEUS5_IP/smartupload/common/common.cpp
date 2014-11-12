#include "stdafx.h"

DWORD CalculateCRC(char* buf, int size)
{
	DWORD crc = 0;
	int count = size/4;
	int tailnum = 0;
	int tailcount = size-count*4;
	if(size-count*4){
		memcpy(&tailnum, &buf[size-tailcount], tailcount);
	}
	DWORD* pdw = (DWORD*)buf;
	for(int i = 0; i < count; i++){
		crc+= pdw[i];
	}
	if (tailcount) {
		crc+= tailnum;
	}
	return crc;
}

