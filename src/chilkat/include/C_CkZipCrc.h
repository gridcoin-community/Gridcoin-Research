// This header is NOT generated.

#ifndef _CkZipCrc_H
#define _CkZipCrc_H
	
#include "chilkatDefs.h"
	
#include "Chilkat_C.h"

HCkZipCrc CkZipCrc_Create(void);
void CkZipCrc_Dispose(HCkZipCrc handle);
BOOL CkZipCrc_getUtf8(HCkZipCrc cHandle);
void CkZipCrc_putUtf8(HCkZipCrc cHandle, BOOL newVal);
void CkZipCrc_BeginStream(HCkZipCrc cHandle);


ckUInt32 CkZipCrc_CalculateCrc(HCkZipCrc cHandle, HCkByteData byteData);
ckUInt32 CkZipCrc_EndStream(HCkZipCrc cHandle);
ckUInt32 CkZipCrc_FileCrc(HCkZipCrc cHandle, const char *filename);
void CkZipCrc_ToHex(HCkZipCrc cHandle, ckUInt32 crc, HCkString strHex);
const char *CkZipCrc_toHex(HCkZipCrc cHandle, ckUInt32 crc);

void CkZipCrc_MoreData(HCkZipCrc cHandle, HCkByteData byteData);
#endif
