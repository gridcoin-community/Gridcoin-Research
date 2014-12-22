// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkPrivateKey_H
#define _C_CkPrivateKey_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkPrivateKey CkPrivateKey_Create(void);
CK_VISIBLE_PUBLIC void CkPrivateKey_Dispose(HCkPrivateKey handle);
CK_VISIBLE_PUBLIC void CkPrivateKey_getDebugLogFilePath(HCkPrivateKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkPrivateKey_putDebugLogFilePath(HCkPrivateKey cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_debugLogFilePath(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKey_getLastErrorHtml(HCkPrivateKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_lastErrorHtml(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKey_getLastErrorText(HCkPrivateKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_lastErrorText(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKey_getLastErrorXml(HCkPrivateKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_lastErrorXml(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_getUtf8(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKey_putUtf8(HCkPrivateKey cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_getVerboseLogging(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKey_putVerboseLogging(HCkPrivateKey cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkPrivateKey_getVersion(HCkPrivateKey cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_version(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_GetPkcs8(HCkPrivateKey cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_GetPkcs8Encrypted(HCkPrivateKey cHandle, const char *password, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_GetPkcs8EncryptedPem(HCkPrivateKey cHandle, const char *password, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_getPkcs8EncryptedPem(HCkPrivateKey cHandle, const char *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_GetPkcs8Pem(HCkPrivateKey cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_getPkcs8Pem(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_GetRsaDer(HCkPrivateKey cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_GetRsaPem(HCkPrivateKey cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_getRsaPem(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_GetXml(HCkPrivateKey cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkPrivateKey_getXml(HCkPrivateKey cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadEncryptedPem(HCkPrivateKey cHandle, const char *pemStr, const char *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadEncryptedPemFile(HCkPrivateKey cHandle, const char *path, const char *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPem(HCkPrivateKey cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPemFile(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPkcs8(HCkPrivateKey cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPkcs8Encrypted(HCkPrivateKey cHandle, HCkByteData data, const char *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPkcs8EncryptedFile(HCkPrivateKey cHandle, const char *path, const char *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPkcs8File(HCkPrivateKey cHandle, const char *path);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPvk(HCkPrivateKey cHandle, HCkByteData data, const char *password);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadPvkFile(HCkPrivateKey cHandle, const char *path, const char *password);
#endif
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadRsaDer(HCkPrivateKey cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadRsaDerFile(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadXml(HCkPrivateKey cHandle, const char *xml);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_LoadXmlFile(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SaveLastError(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SavePkcs8EncryptedFile(HCkPrivateKey cHandle, const char *password, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SavePkcs8EncryptedPemFile(HCkPrivateKey cHandle, const char *password, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SavePkcs8File(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SavePkcs8PemFile(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SaveRsaDerFile(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SaveRsaPemFile(HCkPrivateKey cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKey_SaveXmlFile(HCkPrivateKey cHandle, const char *path);
#endif
