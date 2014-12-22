// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCertChainWH
#define _C_CkCertChainWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCertChainW CkCertChainW_Create(void);
CK_VISIBLE_PUBLIC void CkCertChainW_Dispose(HCkCertChainW handle);
CK_VISIBLE_PUBLIC void CkCertChainW_getDebugLogFilePath(HCkCertChainW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCertChainW_putDebugLogFilePath(HCkCertChainW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCertChainW_debugLogFilePath(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC void CkCertChainW_getLastErrorHtml(HCkCertChainW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertChainW_lastErrorHtml(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC void CkCertChainW_getLastErrorText(HCkCertChainW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertChainW_lastErrorText(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC void CkCertChainW_getLastErrorXml(HCkCertChainW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertChainW_lastErrorXml(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC int CkCertChainW_getNumCerts(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC int CkCertChainW_getNumExpiredCerts(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertChainW_getVerboseLogging(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC void CkCertChainW_putVerboseLogging(HCkCertChainW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCertChainW_getVersion(HCkCertChainW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertChainW_version(HCkCertChainW cHandle);
CK_VISIBLE_PUBLIC HCkCertW CkCertChainW_GetCert(HCkCertChainW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCertChainW_IsRootTrusted(HCkCertChainW cHandle, HCkTrustedRootsW trustedRoots);
CK_VISIBLE_PUBLIC BOOL CkCertChainW_SaveLastError(HCkCertChainW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCertChainW_VerifyCertSignatures(HCkCertChainW cHandle);
#endif
