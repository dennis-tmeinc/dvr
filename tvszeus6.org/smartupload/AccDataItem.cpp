#include "StdAfx.h"
#include "AccDataItem.h"

int AccDataItem::scale = 37;
int AccDataItem::xzero = 0x200;
int AccDataItem::yzero = 0x200;
int AccDataItem::zzero = 0x200;

AccDataItem::AccDataItem(void)
{
}

AccDataItem::~AccDataItem(void)
{
}

void AccDataItem::fromRawData( UCHAR* rawdata)
{
	int tmp;
	if (rawdata[0]!=1 || rawdata[3]!=2 || rawdata[6]!=3){
		throw new AccDataException(1);
	}
	tmp = rawdata[1];
	tmp<<=8;
	tmp+= rawdata[2];
	accFB = tmp;
	accFB-= xzero;
	accFB/= scale;
	accFB = -accFB;
	tmp = rawdata[4];
	tmp<<=8;
	tmp+= rawdata[5];
	accLR = tmp;
	accLR-= yzero;
	accLR/= scale;
	accLR = -accLR;
	tmp = rawdata[7];
	tmp<<=8;
	tmp+= rawdata[8];
	accUD = tmp;
	accUD-= zzero;
	accUD/= scale;
	int year, month, day, hour, minute, second;
	year = rawdata[9]/16*10;
	year+= rawdata[9]%16;
	year+= 2000;
	month = rawdata[10]/16*10;
	month+= rawdata[10]%16;
	day = rawdata[11]/16*10;
	day+= rawdata[11]%16;
	hour = rawdata[12]/16*10;
	hour+= rawdata[12]%16;
	minute = rawdata[13]/16*10;
	minute+= rawdata[13]%16;
	second = rawdata[14]/16*10;
	second+= rawdata[14]%16;
	if (year < 2000 || year > 2100 || month < 1 || month>12 || day<1 || day>31){
		throw new AccDataException(1);
	}
	timeStamp = CTime(year, month, day, hour, minute, second);
}

bool AccDataItem::operator<( AccDataItem& adi)
{
	if (timeStamp < adi.timeStamp){
		return true;
	}
	return false;
}