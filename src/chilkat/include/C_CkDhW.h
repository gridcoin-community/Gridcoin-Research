// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDhWH
#define _C_CkDhWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDhW CkDhW_Create(void);
CK_VISIBLE_PUBLIC void CkDhW_Dispose(HCkDhW handle);
CK_VISIBLE_PUBLIC void CkDhW_getDebugLogFilePath(HCkDhW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDhW_putDebugLogFilePath(HCkDhW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_debugLogFilePath(HCkDhW cHandle);
CK_VISIBLE_PUBLIC int CkDhW_getG(HCkDhW cHandle);
CK_VISIBLE_PUBLIC void CkDhW_getLastErrorHtml(HCkDhW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_lastErrorHtml(HCkDhW cHandle);
CK_VISIBLE_PUBLIC void CkDhW_getLastErrorText(HCkDhW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_lastErrorText(HCkDhW cHandle);
CK_VISIBLE_PUBLIC void CkDhW_getLastErrorXml(HCkDhW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_lastErrorXml(HCkDhW cHandle);
CK_VISIBLE_PUBLIC void CkDhW_getP(HCkDhW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_p(HCkDhW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDhW_getVerboseLogging(HCkDhW cHandle);
CK_VISIBLE_PUBLIC void CkDhW_putVerboseLogging(HCkDhW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDhW_getVersion(HCkDhW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_version(HCkDhW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDhW_CreateE(HCkDhW cHandle, int numBits, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_createE(HCkDhW cHandle, int numBits);
CK_VISIBLE_PUBLIC BOOL CkDhW_FindK(HCkDhW cHandle, const wchar_t *E, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDhW_findK(HCkDhW cHandle, const wchar_t *E);
CK_VISIBLE_PUBLIC BOOL CkDhW_GenPG(HCkDhW cHandle, int numBits, int G);
CK_VISIBLE_PUBLIC BOOL CkDhW_SaveLastError(HCkDhW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDhW_SetPG(HCkDhW cHandle, const wchar_t *p, int g);
CK_VISIBLE_PUBLIC BOOL CkDhW_UnlockComponent(HCkDhW cHandle, const wchar_t *unlockCode);
CK_VISIBLE_PUBLIC void CkDhW_UseKnownPrime(HCkDhW cHandle, int index);
#endif
