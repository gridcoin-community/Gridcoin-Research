// This is a generated source file for Chilkat version 9.5.0.40
#if defined(WIN32) || defined(WINCE)

#ifndef _C_CkKeyContainer_H
#define _C_CkKeyContainer_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkKeyContainer CkKeyContainer_Create(void);
CK_VISIBLE_PUBLIC void CkKeyContainer_Dispose(HCkKeyContainer handle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC void CkKeyContainer_getContainerName(HCkKeyContainer cHandle, HCkString retval);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC const char *CkKeyContainer_containerName(HCkKeyContainer cHandle);
#endif
CK_VISIBLE_PUBLIC void CkKeyContainer_getDebugLogFilePath(HCkKeyContainer cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkKeyContainer_putDebugLogFilePath(HCkKeyContainer cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkKeyContainer_debugLogFilePath(HCkKeyContainer cHandle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_getIsMachineKeyset(HCkKeyContainer cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_getIsOpen(HCkKeyContainer cHandle);
#endif
CK_VISIBLE_PUBLIC void CkKeyContainer_getLastErrorHtml(HCkKeyContainer cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkKeyContainer_lastErrorHtml(HCkKeyContainer cHandle);
CK_VISIBLE_PUBLIC void CkKeyContainer_getLastErrorText(HCkKeyContainer cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkKeyContainer_lastErrorText(HCkKeyContainer cHandle);
CK_VISIBLE_PUBLIC void CkKeyContainer_getLastErrorXml(HCkKeyContainer cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkKeyContainer_lastErrorXml(HCkKeyContainer cHandle);
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_getUtf8(HCkKeyContainer cHandle);
CK_VISIBLE_PUBLIC void CkKeyContainer_putUtf8(HCkKeyContainer cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_getVerboseLogging(HCkKeyContainer cHandle);
CK_VISIBLE_PUBLIC void CkKeyContainer_putVerboseLogging(HCkKeyContainer cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkKeyContainer_getVersion(HCkKeyContainer cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkKeyContainer_version(HCkKeyContainer cHandle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC void CkKeyContainer_CloseContainer(HCkKeyContainer cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_CreateContainer(HCkKeyContainer cHandle, const char *name, BOOL machineKeyset);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_DeleteContainer(HCkKeyContainer cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_FetchContainerNames(HCkKeyContainer cHandle, BOOL bMachineKeyset);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_GenerateKeyPair(HCkKeyContainer cHandle, BOOL bKeyExchangePair, int keyLengthInBits);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_GenerateUuid(HCkKeyContainer cHandle, HCkString outGuid);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC const char *CkKeyContainer_generateUuid(HCkKeyContainer cHandle);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_GetNthContainerName(HCkKeyContainer cHandle, BOOL bMachineKeyset, int index, HCkString outName);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC const char *CkKeyContainer_getNthContainerName(HCkKeyContainer cHandle, BOOL bMachineKeyset, int index);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC int CkKeyContainer_GetNumContainers(HCkKeyContainer cHandle, BOOL bMachineKeyset);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC HCkPrivateKey CkKeyContainer_GetPrivateKey(HCkKeyContainer cHandle, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC HCkPublicKey CkKeyContainer_GetPublicKey(HCkKeyContainer cHandle, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_ImportPrivateKey(HCkKeyContainer cHandle, HCkPrivateKey key, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_ImportPublicKey(HCkKeyContainer cHandle, HCkPublicKey key, BOOL bKeyExchangePair);
#endif
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_OpenContainer(HCkKeyContainer cHandle, const char *name, BOOL needPrivateKeyAccess, BOOL machineKeyset);
#endif
CK_VISIBLE_PUBLIC BOOL CkKeyContainer_SaveLastError(HCkKeyContainer cHandle, const char *path);
#endif

#endif // WIN32 (entire file)
