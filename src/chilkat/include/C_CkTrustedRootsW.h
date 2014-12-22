// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkTrustedRootsWH
#define _C_CkTrustedRootsWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

HCkTrustedRootsW CkTrustedRootsW_Create(void);
HCkTrustedRootsW CkTrustedRootsW_Create2(BOOL bCallbackOwned);
void CkTrustedRootsW_Dispose(HCkTrustedRootsW handle);
void CkTrustedRootsW_getDebugLogFilePath(HCkTrustedRootsW cHandle, HCkString retval);
void CkTrustedRootsW_putDebugLogFilePath(HCkTrustedRootsW cHandle, const wchar_t *newVal);
const wchar_t *CkTrustedRootsW_debugLogFilePath(HCkTrustedRootsW cHandle);
void CkTrustedRootsW_getLastErrorHtml(HCkTrustedRootsW cHandle, HCkString retval);
const wchar_t *CkTrustedRootsW_lastErrorHtml(HCkTrustedRootsW cHandle);
void CkTrustedRootsW_getLastErrorText(HCkTrustedRootsW cHandle, HCkString retval);
const wchar_t *CkTrustedRootsW_lastErrorText(HCkTrustedRootsW cHandle);
void CkTrustedRootsW_getLastErrorXml(HCkTrustedRootsW cHandle, HCkString retval);
const wchar_t *CkTrustedRootsW_lastErrorXml(HCkTrustedRootsW cHandle);
int CkTrustedRootsW_getNumCerts(HCkTrustedRootsW cHandle);
BOOL CkTrustedRootsW_getTrustSystemCaRoots(HCkTrustedRootsW cHandle);
void CkTrustedRootsW_putTrustSystemCaRoots(HCkTrustedRootsW cHandle, BOOL newVal);
BOOL CkTrustedRootsW_getVerboseLogging(HCkTrustedRootsW cHandle);
void CkTrustedRootsW_putVerboseLogging(HCkTrustedRootsW cHandle, BOOL newVal);
void CkTrustedRootsW_getVersion(HCkTrustedRootsW cHandle, HCkString retval);
const wchar_t *CkTrustedRootsW_version(HCkTrustedRootsW cHandle);
BOOL CkTrustedRootsW_Activate(HCkTrustedRootsW cHandle);
BOOL CkTrustedRootsW_AddCert(HCkTrustedRootsW cHandle, HCkCertW cert);
BOOL CkTrustedRootsW_Deactivate(HCkTrustedRootsW cHandle);
HCkCertW CkTrustedRootsW_GetCert(HCkTrustedRootsW cHandle, int index);
BOOL CkTrustedRootsW_LoadCaCertsPem(HCkTrustedRootsW cHandle, const wchar_t *path);
BOOL CkTrustedRootsW_SaveLastError(HCkTrustedRootsW cHandle, const wchar_t *path);
#endif
