#pragma once

class AccDataException{
public:
	int code;
	AccDataException(int a):code(a){};
};

class AccDataItem
{
public:
	AccDataItem(void);
	~AccDataItem(void);
	double accFB;
	double accLR;
	double accUD;
	CTime timeStamp;
	void fromRawData(UCHAR*);
	static int scale;
	static int xzero, yzero, zzero;
	bool operator < (AccDataItem&);
};
