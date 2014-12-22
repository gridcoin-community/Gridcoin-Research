// This is a generated source file for Chilkat version 9.5.0.40
#if defined(WIN32) || defined(WINCE)

#ifndef _C_CkCspWH
#define _C_CkCspWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCspW CkCspW_Create(void);
CK_VISIBLE_PUBLIC void CkCspW_Dispose(HCkCspW handle);
CK_VISIBLE_PUBLIC void CkCspW_getDebugLogFilePath(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCspW_putDebugLogFilePath(HCkCspW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_debugLogFilePath(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_getEncryptAlgorithm(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_encryptAlgorithm(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getEncryptAlgorithmID(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getEncryptNumBits(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_getHashAlgorithm(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_hashAlgorithm(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getHashAlgorithmID(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getHashNumBits(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_getKeyContainerName(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCspW_putKeyContainerName(HCkCspW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_keyContainerName(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_getLastErrorHtml(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_lastErrorHtml(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_getLastErrorText(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_lastErrorText(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_getLastErrorXml(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_lastErrorXml(HCkCspW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCspW_getMachineKeyset(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_putMachineKeyset(HCkCspW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC int CkCspW_getNumEncryptAlgorithms(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getNumHashAlgorithms(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getNumKeyContainers(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getNumKeyExchangeAlgorithms(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getNumSignatureAlgorithms(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_getProviderName(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCspW_putProviderName(HCkCspW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_providerName(HCkCspW cHandle);
CK_VISIBLE_PUBLIC int CkCspW_getProviderType(HCkCspW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCspW_getVerboseLogging(HCkCspW cHandle);
CK_VISIBLE_PUBLIC void CkCspW_putVerboseLogging(HCkCspW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCspW_getVersion(HCkCspW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_version(HCkCspW cHandle);
CK_VISIBLE_PUBLIC HCkStringArrayW CkCspW_GetKeyContainerNames(HCkCspW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCspW_HasEncryptAlgorithm(HCkCspW cHandle, const wchar_t *name, int numBits);
CK_VISIBLE_PUBLIC BOOL CkCspW_HasHashAlgorithm(HCkCspW cHandle, const wchar_t *name, int numBits);
CK_VISIBLE_PUBLIC BOOL CkCspW_Initialize(HCkCspW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCspW_NthEncryptionAlgorithm(HCkCspW cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_nthEncryptionAlgorithm(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC int CkCspW_NthEncryptionNumBits(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCspW_NthHashAlgorithmName(HCkCspW cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_nthHashAlgorithmName(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC int CkCspW_NthHashNumBits(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCspW_NthKeyContainerName(HCkCspW cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_nthKeyContainerName(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCspW_NthKeyExchangeAlgorithm(HCkCspW cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_nthKeyExchangeAlgorithm(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC int CkCspW_NthKeyExchangeNumBits(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCspW_NthSignatureAlgorithm(HCkCspW cHandle, int index, HCkString outName);
CK_VISIBLE_PUBLIC const wchar_t *CkCspW_nthSignatureAlgorithm(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC int CkCspW_NthSignatureNumBits(HCkCspW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCspW_SaveLastError(HCkCspW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC int CkCspW_SetEncryptAlgorithm(HCkCspW cHandle, const wchar_t *name);
CK_VISIBLE_PUBLIC int CkCspW_SetHashAlgorithm(HCkCspW cHandle, const wchar_t *name);
CK_VISIBLE_PUBLIC BOOL CkCspW_SetProviderMicrosoftBase(HCkCspW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCspW_SetProviderMicrosoftEnhanced(HCkCspW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCspW_SetProviderMicrosoftRsaAes(HCkCspW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCspW_SetProviderMicrosoftStrong(HCkCspW cHandle);
#endif

#endif // WIN32 (entire file)
