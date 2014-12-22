// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkTrustedRoots_H
#define _C_CkTrustedRoots_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

HCkTrustedRoots CkTrustedRoots_Create(void);
void CkTrustedRoots_Dispose(HCkTrustedRoots handle);
void CkTrustedRoots_getDebugLogFilePath(HCkTrustedRoots cHandle, HCkString retval);
void CkTrustedRoots_putDebugLogFilePath(HCkTrustedRoots cHandle, const char *newVal);
const char *CkTrustedRoots_debugLogFilePath(HCkTrustedRoots cHandle);
void CkTrustedRoots_getLastErrorHtml(HCkTrustedRoots cHandle, HCkString retval);
const char *CkTrustedRoots_lastErrorHtml(HCkTrustedRoots cHandle);
void CkTrustedRoots_getLastErrorText(HCkTrustedRoots cHandle, HCkString retval);
const char *CkTrustedRoots_lastErrorText(HCkTrustedRoots cHandle);
void CkTrustedRoots_getLastErrorXml(HCkTrustedRoots cHandle, HCkString retval);
const char *CkTrustedRoots_lastErrorXml(HCkTrustedRoots cHandle);
int CkTrustedRoots_getNumCerts(HCkTrustedRoots cHandle);
BOOL CkTrustedRoots_getTrustSystemCaRoots(HCkTrustedRoots cHandle);
void CkTrustedRoots_putTrustSystemCaRoots(HCkTrustedRoots cHandle, BOOL newVal);
BOOL CkTrustedRoots_getUtf8(HCkTrustedRoots cHandle);
void CkTrustedRoots_putUtf8(HCkTrustedRoots cHandle, BOOL newVal);
BOOL CkTrustedRoots_getVerboseLogging(HCkTrustedRoots cHandle);
void CkTrustedRoots_putVerboseLogging(HCkTrustedRoots cHandle, BOOL newVal);
void CkTrustedRoots_getVersion(HCkTrustedRoots cHandle, HCkString retval);
const char *CkTrustedRoots_version(HCkTrustedRoots cHandle);
BOOL CkTrustedRoots_Activate(HCkTrustedRoots cHandle);
BOOL CkTrustedRoots_AddCert(HCkTrustedRoots cHandle, HCkCert cert);
BOOL CkTrustedRoots_Deactivate(HCkTrustedRoots cHandle);
HCkCert CkTrustedRoots_GetCert(HCkTrustedRoots cHandle, int index);
BOOL CkTrustedRoots_LoadCaCertsPem(HCkTrustedRoots cHandle, const char *path);
BOOL CkTrustedRoots_SaveLastError(HCkTrustedRoots cHandle, const char *path);
#endif
