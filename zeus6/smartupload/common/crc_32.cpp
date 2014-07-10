#include "stdafx.h"
#include "CRC_32.h"

// use a 100 KB buffer (a larger buffer does not seem to run
// significantly faster, but takes more RAM)
#define BUFFERSIZE 102400

/////////////////////////////////////////////////////////////////////////////
//
//  CRC_32 constructor  (public member function)
//    Sets up the CRC-32 reference table
//
//  Parameters :
//    None
//
//  Returns :
//    Nothing
//
/////////////////////////////////////////////////////////////////////////////

CRC_32::CRC_32()
{
    // This is the official polynomial used by CRC-32 
    // in PKZip, WinZip and Ethernet. 
    ULONG ulPolynomial = 0x04C11DB7;

    // 256 values representing ASCII character codes.
    for (int i = 0; i <= 0xFF; i++)
    {
        Table[i] = Reflect(i, 8) << 24;
        for (int j = 0; j < 8; j++)
            Table[i] = (Table[i] << 1) ^ (Table[i] & (1 << 31) ? ulPolynomial : 0);
        Table[i] = Reflect(Table[i], 32);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
//  CRC_32::Reflect  (private member function)
//    used by the constructor to help set up the CRC-32 reference table
//
//  Parameters :
//    ref [in] : the value to be reflected
//    ch  [in] : the number of bits to move
//
//  Returns :
//    the new value
//
/////////////////////////////////////////////////////////////////////////////

ULONG CRC_32::Reflect(ULONG ref, char ch)
{
    ULONG value = 0;

    // Swap bit 0 for bit 7
    // bit 1 for bit 6, etc.
    for(int i = 1; i < (ch + 1); i++)
    {
        if (ref & 1)
            value |= 1 << (ch - i);
        ref >>= 1;
    }
    return value;
}

/////////////////////////////////////////////////////////////////////////////
//
//  CRC_32::Calculate  (private member function)
//    Calculates the CRC-32 value for the given buffer
//
//  Parameters :
//    buffer [in] : pointer to the data bytes
//    size   [in] : the size of the buffer
//    CRC    [in] : the initial CRC-32 value
//          [out] : the new CRC-32 value
//
//  Returns :
//    Nothing
//
/////////////////////////////////////////////////////////////////////////////

void CRC_32::Calculate(const LPBYTE buffer, UINT size, ULONG &CRC)
{   // calculate the CRC
    LPBYTE pbyte = buffer;

    while (size--)
        CRC = (CRC >> 8) ^ Table[(CRC & 0xFF) ^ *pbyte++];
}
