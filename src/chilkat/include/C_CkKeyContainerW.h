// This is a generated source file for Chilkat version 9.5.0.40
#if defined(WIN32) || defined(WINCE)

#ifndef _C_CkKeyContainerWH
#define _C_CkKeyContainerWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkKeyContainerW CkKeyContainerW_Create(void);
CK_VISIBLE_PUBLIC void CkKeyContainerW_Dispose(HCkKeyContainerW handle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC void CkKeyContainerW_getContainerName(HCkKeyContainerW cHandle, HCkString retval);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_containerName(HCkKeyContainerW cHandle);
#endif
CK_VISIBLE_PUBLIC void CkKeyContainerW_getDebugLogFilePath(HCkKeyContainerW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkKeyContainerW_putDebugLogFilePath(HCkKeyContainerW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_debugLogFilePath(HCkKeyContainerW cHandle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_getIsMachineKeyset(HCkKeyContainerW cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_getIsOpen(HCkKeyContainerW cHandle);
#endif
CK_VISIBLE_PUBLIC void CkKeyContainerW_getLastErrorHtml(HCkKeyContainerW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_lastErrorHtml(HCkKeyContainerW cHandle);
CK_VISIBLE_PUBLIC void CkKeyContainerW_getLastErrorText(HCkKeyContainerW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_lastErrorText(HCkKeyContainerW cHandle);
CK_VISIBLE_PUBLIC void CkKeyContainerW_getLastErrorXml(HCkKeyContainerW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_lastErrorXml(HCkKeyContainerW cHandle);
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_getVerboseLogging(HCkKeyContainerW cHandle);
CK_VISIBLE_PUBLIC void CkKeyContainerW_putVerboseLogging(HCkKeyContainerW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkKeyContainerW_getVersion(HCkKeyContainerW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_version(HCkKeyContainerW cHandle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC void CkKeyContainerW_CloseContainer(HCkKeyContainerW cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_CreateContainer(HCkKeyContainerW cHandle, const wchar_t *name, BOOL machineKeyset);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_DeleteContainer(HCkKeyContainerW cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_FetchContainerNames(HCkKeyContainerW cHandle, BOOL bMachineKeyset);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_GenerateKeyPair(HCkKeyContainerW cHandle, BOOL bKeyExchangePair, int keyLengthInBits);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_GenerateUuid(HCkKeyContainerW cHandle, HCkString outGuid);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_generateUuid(HCkKeyContainerW cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_GetNthContainerName(HCkKeyContainerW cHandle, BOOL bMachineKeyset, int index, HCkString outName);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkKeyContainerW_getNthContainerName(HCkKeyContainerW cHandle, BOOL bMachineKeyset, int index);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC int CkKeyContainerW_GetNumContainers(HCkKeyContainerW cHandle, BOOL bMachineKeyset);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC HCkPrivateKeyW CkKeyContainerW_GetPrivateKey(HCkKeyContainerW cHandle, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC HCkPublicKeyW CkKeyContainerW_GetPublicKey(HCkKeyContainerW cHandle, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_ImportPrivateKey(HCkKeyContainerW cHandle, HCkPrivateKeyW key, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_ImportPublicKey(HCkKeyContainerW cHandle, HCkPublicKeyW key, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_OpenContainer(HCkKeyContainerW cHandle, const wchar_t *name, BOOL needPrivateKeyAccess, BOOL machineKeyset);
#endif
CK_VISIBLE_PUBLIC BOOL CkKeyContainerW_SaveLastError(HCkKeyContainerW cHandle, const wchar_t *path);
#endif

#endif // WIN32 (entire file)
