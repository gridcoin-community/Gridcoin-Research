// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkOmaDrm_H
#define _C_CkOmaDrm_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkOmaDrm CkOmaDrm_Create(void);
CK_VISIBLE_PUBLIC void CkOmaDrm_Dispose(HCkOmaDrm handle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getBase64Key(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrm_putBase64Key(HCkOmaDrm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_base64Key(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getContentType(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrm_putContentType(HCkOmaDrm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_contentType(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getContentUri(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrm_putContentUri(HCkOmaDrm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_contentUri(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getDebugLogFilePath(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrm_putDebugLogFilePath(HCkOmaDrm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_debugLogFilePath(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getDecryptedData(HCkOmaDrm cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC int CkOmaDrm_getDrmContentVersion(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getEncryptedData(HCkOmaDrm cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkOmaDrm_getHeaders(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkOmaDrm_putHeaders(HCkOmaDrm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_headers(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getIV(HCkOmaDrm cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkOmaDrm_putIV(HCkOmaDrm cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC void CkOmaDrm_getLastErrorHtml(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_lastErrorHtml(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getLastErrorText(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_lastErrorText(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_getLastErrorXml(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_lastErrorXml(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_getUtf8(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_putUtf8(HCkOmaDrm cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_getVerboseLogging(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC void CkOmaDrm_putVerboseLogging(HCkOmaDrm cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkOmaDrm_getVersion(HCkOmaDrm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_version(HCkOmaDrm cHandle);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_CreateDcfFile(HCkOmaDrm cHandle, const char *filename);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_GetHeaderField(HCkOmaDrm cHandle, const char *fieldName, HCkString outVal);
CK_VISIBLE_PUBLIC const char *CkOmaDrm_getHeaderField(HCkOmaDrm cHandle, const char *fieldName);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_LoadDcfData(HCkOmaDrm cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_LoadDcfFile(HCkOmaDrm cHandle, const char *filename);
CK_VISIBLE_PUBLIC void CkOmaDrm_LoadUnencryptedData(HCkOmaDrm cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_LoadUnencryptedFile(HCkOmaDrm cHandle, const char *filename);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_SaveDecrypted(HCkOmaDrm cHandle, const char *filename);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_SaveLastError(HCkOmaDrm cHandle, const char *path);
CK_VISIBLE_PUBLIC void CkOmaDrm_SetEncodedIV(HCkOmaDrm cHandle, const char *encodedIv, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkOmaDrm_UnlockComponent(HCkOmaDrm cHandle, const char *b1);
#endif
