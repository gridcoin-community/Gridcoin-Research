// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkPublicKeyWH
#define _C_CkPublicKeyWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkPublicKeyW CkPublicKeyW_Create(void);
CK_VISIBLE_PUBLIC void CkPublicKeyW_Dispose(HCkPublicKeyW handle);
CK_VISIBLE_PUBLIC void CkPublicKeyW_getDebugLogFilePath(HCkPublicKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkPublicKeyW_putDebugLogFilePath(HCkPublicKeyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkPublicKeyW_debugLogFilePath(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPublicKeyW_getLastErrorHtml(HCkPublicKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPublicKeyW_lastErrorHtml(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPublicKeyW_getLastErrorText(HCkPublicKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPublicKeyW_lastErrorText(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPublicKeyW_getLastErrorXml(HCkPublicKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPublicKeyW_lastErrorXml(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_getVerboseLogging(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC void CkPublicKeyW_putVerboseLogging(HCkPublicKeyW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkPublicKeyW_getVersion(HCkPublicKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPublicKeyW_version(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_GetOpenSslDer(HCkPublicKeyW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_GetOpenSslPem(HCkPublicKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkPublicKeyW_getOpenSslPem(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_GetRsaDer(HCkPublicKeyW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_GetXml(HCkPublicKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkPublicKeyW_getXml(HCkPublicKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadOpenSslDer(HCkPublicKeyW cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadOpenSslDerFile(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadOpenSslPem(HCkPublicKeyW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadOpenSslPemFile(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadPkcs1Pem(HCkPublicKeyW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadRsaDer(HCkPublicKeyW cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadRsaDerFile(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadXml(HCkPublicKeyW cHandle, const wchar_t *xml);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_LoadXmlFile(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_SaveLastError(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_SaveOpenSslDerFile(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_SaveOpenSslPemFile(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_SaveRsaDerFile(HCkPublicKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkPublicKeyW_SaveXmlFile(HCkPublicKeyW cHandle, const wchar_t *path);
#endif
