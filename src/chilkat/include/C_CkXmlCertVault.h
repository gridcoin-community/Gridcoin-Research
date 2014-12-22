// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkXmlCertVault_H
#define _C_CkXmlCertVault_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkXmlCertVault CkXmlCertVault_Create(void);
CK_VISIBLE_PUBLIC void CkXmlCertVault_Dispose(HCkXmlCertVault handle);
CK_VISIBLE_PUBLIC void CkXmlCertVault_getDebugLogFilePath(HCkXmlCertVault cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkXmlCertVault_putDebugLogFilePath(HCkXmlCertVault cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkXmlCertVault_debugLogFilePath(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVault_getLastErrorHtml(HCkXmlCertVault cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkXmlCertVault_lastErrorHtml(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVault_getLastErrorText(HCkXmlCertVault cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkXmlCertVault_lastErrorText(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVault_getLastErrorXml(HCkXmlCertVault cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkXmlCertVault_lastErrorXml(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVault_getMasterPassword(HCkXmlCertVault cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkXmlCertVault_putMasterPassword(HCkXmlCertVault cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkXmlCertVault_masterPassword(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_getUtf8(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVault_putUtf8(HCkXmlCertVault cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_getVerboseLogging(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC void CkXmlCertVault_putVerboseLogging(HCkXmlCertVault cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkXmlCertVault_getVersion(HCkXmlCertVault cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkXmlCertVault_version(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddCert(HCkXmlCertVault cHandle, HCkCert cert);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddCertBinary(HCkXmlCertVault cHandle, HCkByteData certBytes);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddCertChain(HCkXmlCertVault cHandle, HCkCertChain certChain);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddCertEncoded(HCkXmlCertVault cHandle, const char *encodedBytes, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddCertFile(HCkXmlCertVault cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddCertString(HCkXmlCertVault cHandle, const char *certData);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddPemFile(HCkXmlCertVault cHandle, const char *path, const char *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddPfx(HCkXmlCertVault cHandle, HCkPfx pfx);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddPfxBinary(HCkXmlCertVault cHandle, HCkByteData pfxBytes, const char *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddPfxEncoded(HCkXmlCertVault cHandle, const char *encodedBytes, const char *encoding, const char *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_AddPfxFile(HCkXmlCertVault cHandle, const char *path, const char *password);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_GetXml(HCkXmlCertVault cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkXmlCertVault_getXml(HCkXmlCertVault cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_LoadXml(HCkXmlCertVault cHandle, const char *xml);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_LoadXmlFile(HCkXmlCertVault cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_SaveLastError(HCkXmlCertVault cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkXmlCertVault_SaveXml(HCkXmlCertVault cHandle, const char *path);
#endif
