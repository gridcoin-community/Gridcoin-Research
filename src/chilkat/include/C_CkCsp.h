// This is a generated source file for Chilkat version 9.5.0.40
#if defined(WIN32) || defined(WINCE)

#ifndef _C_CkCsp_H
#define _C_CkCsp_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCsp CkCsp_Create(void);
CK_VISIBLE_PUBLIC void CkCsp_Dispose(HCkCsp handle);
CK_VISIBLE_PUBLIC void CkCsp_getDebugLogFilePath(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCsp_putDebugLogFilePath(HCkCsp cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCsp_debugLogFilePath(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_getEncryptAlgorithm(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsp_encryptAlgorithm(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getEncryptAlgorithmID(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getEncryptNumBits(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_getHashAlgorithm(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsp_hashAlgorithm(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getHashAlgorithmID(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getHashNumBits(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_getKeyContainerName(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCsp_putKeyContainerName(HCkCsp cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCsp_keyContainerName(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_getLastErrorHtml(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsp_lastErrorHtml(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_getLastErrorText(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsp_lastErrorText(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_getLastErrorXml(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsp_lastErrorXml(HCkCsp cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsp_getMachineKeyset(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_putMachineKeyset(HCkCsp cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC int CkCsp_getNumEncryptAlgorithms(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getNumHashAlgorithms(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getNumKeyContainers(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getNumKeyExchangeAlgorithms(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getNumSignatureAlgorithms(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_getProviderName(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCsp_putProviderName(HCkCsp cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCsp_providerName(HCkCsp cHandle);
CK_VISIBLE_PUBLIC int CkCsp_getProviderType(HCkCsp cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsp_getUtf8(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_putUtf8(HCkCsp cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCsp_getVerboseLogging(HCkCsp cHandle);
CK_VISIBLE_PUBLIC void CkCsp_putVerboseLogging(HCkCsp cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCsp_getVersion(HCkCsp cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsp_version(HCkCsp cHandle);
CK_VISIBLE_PUBLIC HCkStringArray CkCsp_GetKeyContainerNames(HCkCsp cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsp_HasEncryptAlgorithm(HCkCsp cHandle, const char *name, int numBits);
CK_VISIBLE_PUBLIC BOOL CkCsp_HasHashAlgorithm(HCkCsp cHandle, const char *name, int numBits);
CK_VISIBLE_PUBLIC BOOL CkCsp_Initialize(HCkCsp cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsp_NthEncryptionAlgorithm(HCkCsp cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const char *CkCsp_nthEncryptionAlgorithm(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC int CkCsp_NthEncryptionNumBits(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCsp_NthHashAlgorithmName(HCkCsp cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const char *CkCsp_nthHashAlgorithmName(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC int CkCsp_NthHashNumBits(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCsp_NthKeyContainerName(HCkCsp cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const char *CkCsp_nthKeyContainerName(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCsp_NthKeyExchangeAlgorithm(HCkCsp cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const char *CkCsp_nthKeyExchangeAlgorithm(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC int CkCsp_NthKeyExchangeNumBits(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCsp_NthSignatureAlgorithm(HCkCsp cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const char *CkCsp_nthSignatureAlgorithm(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC int CkCsp_NthSignatureNumBits(HCkCsp cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCsp_SaveLastError(HCkCsp cHandle, const char *path);
CK_VISIBLE_PUBLIC int CkCsp_SetEncryptAlgorithm(HCkCsp cHandle, const char *name);
CK_VISIBLE_PUBLIC int CkCsp_SetHashAlgorithm(HCkCsp cHandle, const char *name);
CK_VISIBLE_PUBLIC BOOL CkCsp_SetProviderMicrosoftBase(HCkCsp cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsp_SetProviderMicrosoftEnhanced(HCkCsp cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsp_SetProviderMicrosoftRsaAes(HCkCsp cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsp_SetProviderMicrosoftStrong(HCkCsp cHandle);
#endif

#endif // WIN32 (entire file)
