// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDh_H
#define _C_CkDh_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDh CkDh_Create(void);
CK_VISIBLE_PUBLIC void CkDh_Dispose(HCkDh handle);
CK_VISIBLE_PUBLIC void CkDh_getDebugLogFilePath(HCkDh cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDh_putDebugLogFilePath(HCkDh cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDh_debugLogFilePath(HCkDh cHandle);
CK_VISIBLE_PUBLIC int CkDh_getG(HCkDh cHandle);
CK_VISIBLE_PUBLIC void CkDh_getLastErrorHtml(HCkDh cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDh_lastErrorHtml(HCkDh cHandle);
CK_VISIBLE_PUBLIC void CkDh_getLastErrorText(HCkDh cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDh_lastErrorText(HCkDh cHandle);
CK_VISIBLE_PUBLIC void CkDh_getLastErrorXml(HCkDh cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDh_lastErrorXml(HCkDh cHandle);
CK_VISIBLE_PUBLIC void CkDh_getP(HCkDh cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDh_p(HCkDh cHandle);
CK_VISIBLE_PUBLIC BOOL CkDh_getUtf8(HCkDh cHandle);
CK_VISIBLE_PUBLIC void CkDh_putUtf8(HCkDh cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkDh_getVerboseLogging(HCkDh cHandle);
CK_VISIBLE_PUBLIC void CkDh_putVerboseLogging(HCkDh cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDh_getVersion(HCkDh cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDh_version(HCkDh cHandle);
CK_VISIBLE_PUBLIC BOOL CkDh_CreateE(HCkDh cHandle, int numBits, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDh_createE(HCkDh cHandle, int numBits);
CK_VISIBLE_PUBLIC BOOL CkDh_FindK(HCkDh cHandle, const char *E, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDh_findK(HCkDh cHandle, const char *E);
CK_VISIBLE_PUBLIC BOOL CkDh_GenPG(HCkDh cHandle, int numBits, int G);
CK_VISIBLE_PUBLIC BOOL CkDh_SaveLastError(HCkDh cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDh_SetPG(HCkDh cHandle, const char *p, int g);
CK_VISIBLE_PUBLIC BOOL CkDh_UnlockComponent(HCkDh cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC void CkDh_UseKnownPrime(HCkDh cHandle, int index);
#endif
