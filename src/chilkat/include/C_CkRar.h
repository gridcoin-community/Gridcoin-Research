// This header is NOT generated.
#ifndef _CkRar_H
#define _CkRar_H
	
#include "Chilkat_C.h"

HCkRar CkRar_Create(void);
void CkRar_Dispose(HCkRar handle);
void CkRar_getLastErrorHtml(HCkRar cHandle, HCkString retval);
void CkRar_getLastErrorText(HCkRar cHandle, HCkString retval);
void CkRar_getLastErrorXml(HCkRar cHandle, HCkString retval);
long CkRar_getNumEntries(HCkRar cHandle);
BOOL CkRar_getUtf8(HCkRar cHandle);
void CkRar_putUtf8(HCkRar cHandle, BOOL newVal);
BOOL CkRar_Close(HCkRar cHandle);
BOOL CkRar_FastOpen(HCkRar cHandle, const char *filename);
HCkRarEntry CkRar_GetEntryByIndex(HCkRar cHandle, long index);
HCkRarEntry CkRar_GetEntryByName(HCkRar cHandle, const char *filename);
BOOL CkRar_Open(HCkRar cHandle, const char *filename);
BOOL CkRar_SaveLastError(HCkRar cHandle, const char *filename);
BOOL CkRar_Unrar(HCkRar cHandle, const char *dirPath);
const char *CkRar_lastErrorHtml(HCkRar cHandle);
const char *CkRar_lastErrorText(HCkRar cHandle);
const char *CkRar_lastErrorXml(HCkRar cHandle);

#endif
