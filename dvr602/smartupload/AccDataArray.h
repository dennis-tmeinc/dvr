#pragma once
#include "AccDataItem.h"

class AccDataArray:public CArray<AccDataItem>
{
public:
	AccDataArray(void);
	~AccDataArray(void);
	void sort();
	int fromFiles(CStringArray&);
};
