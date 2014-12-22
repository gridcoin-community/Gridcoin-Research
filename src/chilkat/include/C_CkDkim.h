// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDkim_H
#define _C_CkDkim_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDkim CkDkim_Create(void);
CK_VISIBLE_PUBLIC void CkDkim_Dispose(HCkDkim handle);
CK_VISIBLE_PUBLIC void CkDkim_getDebugLogFilePath(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDebugLogFilePath(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_debugLogFilePath(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDkimAlg(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDkimAlg(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_dkimAlg(HCkDkim cHandle);
CK_VISIBLE_PUBLIC int CkDkim_getDkimBodyLengthCount(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_putDkimBodyLengthCount(HCkDkim cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDkim_getDkimCanon(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDkimCanon(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_dkimCanon(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDkimDomain(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDkimDomain(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_dkimDomain(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDkimHeaders(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDkimHeaders(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_dkimHeaders(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDkimSelector(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDkimSelector(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_dkimSelector(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDomainKeyAlg(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDomainKeyAlg(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_domainKeyAlg(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDomainKeyCanon(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDomainKeyCanon(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_domainKeyCanon(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDomainKeyDomain(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDomainKeyDomain(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_domainKeyDomain(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDomainKeyHeaders(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDomainKeyHeaders(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_domainKeyHeaders(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getDomainKeySelector(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDkim_putDomainKeySelector(HCkDkim cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDkim_domainKeySelector(HCkDkim cHandle);
CK_VISIBLE_PUBLIC int CkDkim_getHeartbeatMs(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_putHeartbeatMs(HCkDkim cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDkim_getLastErrorHtml(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDkim_lastErrorHtml(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getLastErrorText(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDkim_lastErrorText(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_getLastErrorXml(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDkim_lastErrorXml(HCkDkim cHandle);
CK_VISIBLE_PUBLIC BOOL CkDkim_getUtf8(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_putUtf8(HCkDkim cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkDkim_getVerboseLogging(HCkDkim cHandle);
CK_VISIBLE_PUBLIC void CkDkim_putVerboseLogging(HCkDkim cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDkim_getVersion(HCkDkim cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDkim_version(HCkDkim cHandle);
CK_VISIBLE_PUBLIC BOOL CkDkim_AddDkimSignature(HCkDkim cHandle, HCkByteData mimeIn, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkDkim_AddDomainKeySignature(HCkDkim cHandle, HCkByteData mimeIn, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadDkimPk(HCkDkim cHandle, const char *privateKey, const char *optionalPassword);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadDkimPkBytes(HCkDkim cHandle, HCkByteData privateKeyDer, const char *optionalPassword);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadDkimPkFile(HCkDkim cHandle, const char *privateKeyFilePath, const char *optionalPassword);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadDomainKeyPk(HCkDkim cHandle, const char *privateKey, const char *optionalPassword);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadDomainKeyPkBytes(HCkDkim cHandle, HCkByteData privateKeyDer, const char *optionalPassword);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadDomainKeyPkFile(HCkDkim cHandle, const char *privateKeyFilePath, const char *optionalPassword);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadPublicKey(HCkDkim cHandle, const char *selector, const char *domain, const char *publicKey);
CK_VISIBLE_PUBLIC BOOL CkDkim_LoadPublicKeyFile(HCkDkim cHandle, const char *selector, const char *domain, const char *publicKeyFilepath);
CK_VISIBLE_PUBLIC int CkDkim_NumDkimSignatures(HCkDkim cHandle, HCkByteData mimeData);
CK_VISIBLE_PUBLIC int CkDkim_NumDomainKeySignatures(HCkDkim cHandle, HCkByteData mimeData);
CK_VISIBLE_PUBLIC BOOL CkDkim_PrefetchPublicKey(HCkDkim cHandle, const char *selector, const char *domain);
CK_VISIBLE_PUBLIC BOOL CkDkim_SaveLastError(HCkDkim cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDkim_UnlockComponent(HCkDkim cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkDkim_VerifyDkimSignature(HCkDkim cHandle, int sigIndex, HCkByteData mimeData);
CK_VISIBLE_PUBLIC BOOL CkDkim_VerifyDomainKeySignature(HCkDkim cHandle, int sigIndex, HCkByteData mimeData);
#endif
