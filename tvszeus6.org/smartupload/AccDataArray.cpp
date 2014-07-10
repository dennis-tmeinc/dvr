#include "StdAfx.h"
#include "AccDataArray.h"
#include <algorithm>

using namespace std;
AccDataArray::AccDataArray(void)
{
}

AccDataArray::~AccDataArray(void)
{
}

int AccDataArray::fromFiles( CStringArray& fpath)
{
	RemoveAll();
	for (int k = 0; k < fpath.GetSize(); k++){
		CFile f;
		if(!f.Open(fpath[k], CFile::modeRead)){
			continue;
		}
		int flength = (int)f.GetLength();
		UCHAR* buffer = new UCHAR[flength];
		f.Read(buffer, flength);
		f.Close();
		AccDataItem::xzero = buffer[10];
		AccDataItem::xzero<<= 8;
		AccDataItem::xzero+= buffer[11];
		AccDataItem::yzero = buffer[12];
		AccDataItem::yzero<<= 8;
		AccDataItem::yzero+= buffer[13];
		AccDataItem::zzero = buffer[14];
		AccDataItem::zzero<<= 8;
		AccDataItem::zzero+= buffer[15];
		int itemcount = (flength-17)/16;
		try
		{
			for (int i = 0; i < itemcount; i++){
				AccDataItem adi;
				adi.fromRawData(&buffer[i*16+17]);
				Add(adi);
			}
			delete buffer;
		}
		catch (AccDataException* e)
		{
			delete e;
			delete buffer;
			continue;
		}
	}
	sort();
	return 1;
}

void AccDataArray::sort()
{
	AccDataItem tmp1, tmp2;
	for (int i = GetSize()-1; i > 0; i--){
		for (int j = 0; j < i; j++){
			if (GetAt(j+1)<GetAt(j)){
				tmp1 = GetAt(j);
				tmp2 = GetAt(j+1);
				SetAt(j, tmp2);
				SetAt(j+1, tmp1);
			}
		}
	}
}