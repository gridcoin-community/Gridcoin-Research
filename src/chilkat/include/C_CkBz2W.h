// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkBz2WH
#define _C_CkBz2WH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkBz2W CkBz2W_Create(void);
CK_VISIBLE_PUBLIC HCkBz2W CkBz2W_Create2(BOOL bCallbackOwned);
CK_VISIBLE_PUBLIC void CkBz2W_Dispose(HCkBz2W handle);
CK_VISIBLE_PUBLIC void CkBz2W_getDebugLogFilePath(HCkBz2W cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkBz2W_putDebugLogFilePath(HCkBz2W cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkBz2W_debugLogFilePath(HCkBz2W cHandle);
CK_VISIBLE_PUBLIC int CkBz2W_getHeartbeatMs(HCkBz2W cHandle);
CK_VISIBLE_PUBLIC void CkBz2W_putHeartbeatMs(HCkBz2W cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkBz2W_getLastErrorHtml(HCkBz2W cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkBz2W_lastErrorHtml(HCkBz2W cHandle);
CK_VISIBLE_PUBLIC void CkBz2W_getLastErrorText(HCkBz2W cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkBz2W_lastErrorText(HCkBz2W cHandle);
CK_VISIBLE_PUBLIC void CkBz2W_getLastErrorXml(HCkBz2W cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkBz2W_lastErrorXml(HCkBz2W cHandle);
CK_VISIBLE_PUBLIC BOOL CkBz2W_getVerboseLogging(HCkBz2W cHandle);
CK_VISIBLE_PUBLIC void CkBz2W_putVerboseLogging(HCkBz2W cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkBz2W_getVersion(HCkBz2W cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkBz2W_version(HCkBz2W cHandle);
CK_VISIBLE_PUBLIC BOOL CkBz2W_CompressFile(HCkBz2W cHandle, const wchar_t *inFilename, const wchar_t *outBz2Filename);
CK_VISIBLE_PUBLIC BOOL CkBz2W_CompressFileToMem(HCkBz2W cHandle, const wchar_t *inFilename, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkBz2W_CompressMemToFile(HCkBz2W cHandle, HCkByteData inData, const wchar_t *toPath);
CK_VISIBLE_PUBLIC BOOL CkBz2W_CompressMemory(HCkBz2W cHandle, HCkByteData inData, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkBz2W_SaveLastError(HCkBz2W cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkBz2W_UncompressFile(HCkBz2W cHandle, const wchar_t *inFilename, const wchar_t *toPath);
CK_VISIBLE_PUBLIC BOOL CkBz2W_UncompressFileToMem(HCkBz2W cHandle, const wchar_t *inFilename, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkBz2W_UncompressMemToFile(HCkBz2W cHandle, HCkByteData inData, const wchar_t *toPath);
CK_VISIBLE_PUBLIC BOOL CkBz2W_UncompressMemory(HCkBz2W cHandle, HCkByteData inData, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkBz2W_UnlockComponent(HCkBz2W cHandle, const wchar_t *regCode);
#endif
