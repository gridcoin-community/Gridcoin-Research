// This header is NOT generated.

#ifndef _CkRarEntry_H
#define _CkRarEntry_H
	
#include "Chilkat_C.h"

HCkRarEntry CkRarEntry_Create(void);
void CkRarEntry_Dispose(HCkRarEntry handle);
unsigned long CkRarEntry_getCompressedSize(HCkRarEntry cHandle);
unsigned long CkRarEntry_getCrc(HCkRarEntry cHandle);
void CkRarEntry_getFilename(HCkRarEntry cHandle, HCkString retval);
BOOL CkRarEntry_getIsDirectory(HCkRarEntry cHandle);
BOOL CkRarEntry_getIsReadOnly(HCkRarEntry cHandle);
void CkRarEntry_getLastErrorHtml(HCkRarEntry cHandle, HCkString retval);
void CkRarEntry_getLastErrorText(HCkRarEntry cHandle, HCkString retval);
void CkRarEntry_getLastErrorXml(HCkRarEntry cHandle, HCkString retval);
void CkRarEntry_getLastModified(HCkRarEntry cHandle, SYSTEMTIME *retval);
unsigned long CkRarEntry_getUncompressedSize(HCkRarEntry cHandle);
BOOL CkRarEntry_getUtf8(HCkRarEntry cHandle);
void CkRarEntry_putUtf8(HCkRarEntry cHandle, BOOL newVal);
BOOL CkRarEntry_SaveLastError(HCkRarEntry cHandle, const char *filename);
BOOL CkRarEntry_Unrar(HCkRarEntry cHandle, const char *dirPath);
const char *CkRarEntry_filename(HCkRarEntry cHandle);
const char *CkRarEntry_lastErrorHtml(HCkRarEntry cHandle);
const char *CkRarEntry_lastErrorText(HCkRarEntry cHandle);
const char *CkRarEntry_lastErrorXml(HCkRarEntry cHandle);

#endif
