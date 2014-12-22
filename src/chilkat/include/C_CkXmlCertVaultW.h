// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkXmlCertVaultWH
#define _C_CkXmlCertVaultWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkXmlCertVaultW CkXmlCertVaultW_Create(void);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_Dispose(HCkXmlCertVaultW handle);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_getDebugLogFilePath(HCkXmlCertVaultW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_putDebugLogFilePath(HCkXmlCertVaultW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkXmlCertVaultW_debugLogFilePath(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_getLastErrorHtml(HCkXmlCertVaultW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmlCertVaultW_lastErrorHtml(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_getLastErrorText(HCkXmlCertVaultW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmlCertVaultW_lastErrorText(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_getLastErrorXml(HCkXmlCertVaultW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmlCertVaultW_lastErrorXml(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_getMasterPassword(HCkXmlCertVaultW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_putMasterPassword(HCkXmlCertVaultW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkXmlCertVaultW_masterPassword(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_getVerboseLogging(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_putVerboseLogging(HCkXmlCertVaultW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkXmlCertVaultW_getVersion(HCkXmlCertVaultW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmlCertVaultW_version(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddCert(HCkXmlCertVaultW cHandle, HCkCertW cert);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddCertBinary(HCkXmlCertVaultW cHandle, HCkByteData certBytes);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddCertChain(HCkXmlCertVaultW cHandle, HCkCertChainW certChain);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddCertEncoded(HCkXmlCertVaultW cHandle, const wchar_t *encodedBytes, const wchar_t *encoding);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddCertFile(HCkXmlCertVaultW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddCertString(HCkXmlCertVaultW cHandle, const wchar_t *certData);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddPemFile(HCkXmlCertVaultW cHandle, const wchar_t *path, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddPfx(HCkXmlCertVaultW cHandle, HCkPfxW pfx);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddPfxBinary(HCkXmlCertVaultW cHandle, HCkByteData pfxBytes, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddPfxEncoded(HCkXmlCertVaultW cHandle, const wchar_t *encodedBytes, const wchar_t *encoding, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_AddPfxFile(HCkXmlCertVaultW cHandle, const wchar_t *path, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_GetXml(HCkXmlCertVaultW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkXmlCertVaultW_getXml(HCkXmlCertVaultW cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_LoadXml(HCkXmlCertVaultW cHandle, const wchar_t *xml);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_LoadXmlFile(HCkXmlCertVaultW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_SaveLastError(HCkXmlCertVaultW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVaultW_SaveXml(HCkXmlCertVaultW cHandle, const wchar_t *path);
#endif
