#ifndef _CRC_32_H_EA6C0EE0_BC30_11d5_B625_A58C4DF45B22_INCLUDED
#define _CRC_32_H_EA6C0EE0_BC30_11d5_B625_A58C4DF45B22_INCLUDED

#pragma once

class CRC_32
{
public:
    CRC_32();
	void Calculate (const LPBYTE buffer, UINT size, ULONG &crc);

private:
    ULONG Reflect(ULONG ref, char ch);
    ULONG Table[256];
};

#endif // _CRC_32_H_EA6C0EE0_BC30_11d5_B625_A58C4DF45B22_INCLUDED

/////////////////////////////////////////////////////////////////////////////
//
//  End of CRC_32.h
//
/////////////////////////////////////////////////////////////////////////////
