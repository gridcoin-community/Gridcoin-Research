// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkPrivateKeyWH
#define _C_CkPrivateKeyWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkPrivateKeyW CkPrivateKeyW_Create(void);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_Dispose(HCkPrivateKeyW handle);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_getDebugLogFilePath(HCkPrivateKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_putDebugLogFilePath(HCkPrivateKeyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_debugLogFilePath(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_getLastErrorHtml(HCkPrivateKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_lastErrorHtml(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_getLastErrorText(HCkPrivateKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_lastErrorText(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_getLastErrorXml(HCkPrivateKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_lastErrorXml(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_getVerboseLogging(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_putVerboseLogging(HCkPrivateKeyW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkPrivateKeyW_getVersion(HCkPrivateKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_version(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_GetPkcs8(HCkPrivateKeyW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_GetPkcs8Encrypted(HCkPrivateKeyW cHandle, const wchar_t *password, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_GetPkcs8EncryptedPem(HCkPrivateKeyW cHandle, const wchar_t *password, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_getPkcs8EncryptedPem(HCkPrivateKeyW cHandle, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_GetPkcs8Pem(HCkPrivateKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_getPkcs8Pem(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_GetRsaDer(HCkPrivateKeyW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_GetRsaPem(HCkPrivateKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_getRsaPem(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_GetXml(HCkPrivateKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkPrivateKeyW_getXml(HCkPrivateKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadEncryptedPem(HCkPrivateKeyW cHandle, const wchar_t *pemStr, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadEncryptedPemFile(HCkPrivateKeyW cHandle, const wchar_t *path, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPem(HCkPrivateKeyW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPemFile(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPkcs8(HCkPrivateKeyW cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPkcs8Encrypted(HCkPrivateKeyW cHandle, HCkByteData data, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPkcs8EncryptedFile(HCkPrivateKeyW cHandle, const wchar_t *path, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPkcs8File(HCkPrivateKeyW cHandle, const wchar_t *path);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPvk(HCkPrivateKeyW cHandle, HCkByteData data, const wchar_t *password);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadPvkFile(HCkPrivateKeyW cHandle, const wchar_t *path, const wchar_t *password);
#endif
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadRsaDer(HCkPrivateKeyW cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadRsaDerFile(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadXml(HCkPrivateKeyW cHandle, const wchar_t *xml);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_LoadXmlFile(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SaveLastError(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SavePkcs8EncryptedFile(HCkPrivateKeyW cHandle, const wchar_t *password, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SavePkcs8EncryptedPemFile(HCkPrivateKeyW cHandle, const wchar_t *password, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SavePkcs8File(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SavePkcs8PemFile(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SaveRsaDerFile(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SaveRsaPemFile(HCkPrivateKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPrivateKeyW_SaveXmlFile(HCkPrivateKeyW cHandle, const wchar_t *path);
#endif
