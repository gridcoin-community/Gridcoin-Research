// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkPublicKey_H
#define _C_CkPublicKey_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkPublicKey CkPublicKey_Create(void);
CK_VISIBLE_PUBLIC void CkPublicKey_Dispose(HCkPublicKey handle);
CK_VISIBLE_PUBLIC void CkPublicKey_getDebugLogFilePath(HCkPublicKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkPublicKey_putDebugLogFilePath(HCkPublicKey cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkPublicKey_debugLogFilePath(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC void CkPublicKey_getLastErrorHtml(HCkPublicKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPublicKey_lastErrorHtml(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC void CkPublicKey_getLastErrorText(HCkPublicKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPublicKey_lastErrorText(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC void CkPublicKey_getLastErrorXml(HCkPublicKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPublicKey_lastErrorXml(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_getUtf8(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC void CkPublicKey_putUtf8(HCkPublicKey cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_getVerboseLogging(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC void CkPublicKey_putVerboseLogging(HCkPublicKey cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkPublicKey_getVersion(HCkPublicKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPublicKey_version(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_GetOpenSslDer(HCkPublicKey cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_GetOpenSslPem(HCkPublicKey cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkPublicKey_getOpenSslPem(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_GetRsaDer(HCkPublicKey cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_GetXml(HCkPublicKey cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkPublicKey_getXml(HCkPublicKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadOpenSslDer(HCkPublicKey cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadOpenSslDerFile(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadOpenSslPem(HCkPublicKey cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadOpenSslPemFile(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadPkcs1Pem(HCkPublicKey cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadRsaDer(HCkPublicKey cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadRsaDerFile(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadXml(HCkPublicKey cHandle, const char *xml);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_LoadXmlFile(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_SaveLastError(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_SaveOpenSslDerFile(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_SaveOpenSslPemFile(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_SaveRsaDerFile(HCkPublicKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKey_SaveXmlFile(HCkPublicKey cHandle, const char *path);
#endif
