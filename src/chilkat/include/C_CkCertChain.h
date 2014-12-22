// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCertChain_H
#define _C_CkCertChain_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCertChain CkCertChain_Create(void);
CK_VISIBLE_PUBLIC void CkCertChain_Dispose(HCkCertChain handle);
CK_VISIBLE_PUBLIC void CkCertChain_getDebugLogFilePath(HCkCertChain cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCertChain_putDebugLogFilePath(HCkCertChain cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCertChain_debugLogFilePath(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC void CkCertChain_getLastErrorHtml(HCkCertChain cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertChain_lastErrorHtml(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC void CkCertChain_getLastErrorText(HCkCertChain cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertChain_lastErrorText(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC void CkCertChain_getLastErrorXml(HCkCertChain cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertChain_lastErrorXml(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC int CkCertChain_getNumCerts(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC int CkCertChain_getNumExpiredCerts(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertChain_getUtf8(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC void CkCertChain_putUtf8(HCkCertChain cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCertChain_getVerboseLogging(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC void CkCertChain_putVerboseLogging(HCkCertChain cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCertChain_getVersion(HCkCertChain cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertChain_version(HCkCertChain cHandle);
CK_VISIBLE_PUBLIC HCkCert CkCertChain_GetCert(HCkCertChain cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCertChain_IsRootTrusted(HCkCertChain cHandle, HCkTrustedRoots trustedRoots);
CK_VISIBLE_PUBLIC BOOL CkCertChain_SaveLastError(HCkCertChain cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCertChain_VerifyCertSignatures(HCkCertChain cHandle);
#endif
