// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkOmaDrmWH
#define _C_CkOmaDrmWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkOmaDrmW CkOmaDrmW_Create(void);
CK_VISIBLE_PUBLIC void CkOmaDrmW_Dispose(HCkOmaDrmW handle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getBase64Key(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrmW_putBase64Key(HCkOmaDrmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_base64Key(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getContentType(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrmW_putContentType(HCkOmaDrmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_contentType(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getContentUri(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrmW_putContentUri(HCkOmaDrmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_contentUri(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getDebugLogFilePath(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrmW_putDebugLogFilePath(HCkOmaDrmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_debugLogFilePath(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getDecryptedData(HCkOmaDrmW cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC int CkOmaDrmW_getDrmContentVersion(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getEncryptedData(HCkOmaDrmW cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getHeaders(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrmW_putHeaders(HCkOmaDrmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_headers(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getIV(HCkOmaDrmW cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkOmaDrmW_putIV(HCkOmaDrmW cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getLastErrorHtml(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_lastErrorHtml(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getLastErrorText(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_lastErrorText(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getLastErrorXml(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_lastErrorXml(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_getVerboseLogging(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrmW_putVerboseLogging(HCkOmaDrmW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkOmaDrmW_getVersion(HCkOmaDrmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_version(HCkOmaDrmW cHandle);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_CreateDcfFile(HCkOmaDrmW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_GetHeaderField(HCkOmaDrmW cHandle, const wchar_t *fieldName, HCkString outVal);
CK_VISIBLE_PUBLIC const wchar_t *CkOmaDrmW_getHeaderField(HCkOmaDrmW cHandle, const wchar_t *fieldName);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_LoadDcfData(HCkOmaDrmW cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_LoadDcfFile(HCkOmaDrmW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC void CkOmaDrmW_LoadUnencryptedData(HCkOmaDrmW cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_LoadUnencryptedFile(HCkOmaDrmW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_SaveDecrypted(HCkOmaDrmW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_SaveLastError(HCkOmaDrmW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC void CkOmaDrmW_SetEncodedIV(HCkOmaDrmW cHandle, const wchar_t *encodedIv, const wchar_t *encoding);
CK_VISIBLE_PUBLIC BOOL CkOmaDrmW_UnlockComponent(HCkOmaDrmW cHandle, const wchar_t *b1);
#endif
