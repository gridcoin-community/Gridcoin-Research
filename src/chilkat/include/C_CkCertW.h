// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCertWH
#define _C_CkCertWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCertW CkCertW_Create(void);
CK_VISIBLE_PUBLIC void CkCertW_Dispose(HCkCertW handle);
CK_VISIBLE_PUBLIC void CkCertW_getAuthorityKeyId(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_authorityKeyId(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getAvoidWindowsPkAccess(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_putAvoidWindowsPkAccess(HCkCertW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC int CkCertW_getCertVersion(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getCspName(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_cspName(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getDebugLogFilePath(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCertW_putDebugLogFilePath(HCkCertW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_debugLogFilePath(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getExpired(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getForClientAuthentication(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getForCodeSigning(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getForSecureEmail(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getForServerAuthentication(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getForTimeStamping(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getHasKeyContainer(HCkCertW cHandle);
CK_VISIBLE_PUBLIC unsigned long CkCertW_getIntendedKeyUsage(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getIsRoot(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerC(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerC(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerCN(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerCN(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerDN(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerDN(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerE(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerE(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerL(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerL(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerO(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerO(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerOU(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerOU(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getIssuerS(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_issuerS(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getKeyContainerName(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_keyContainerName(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getLastErrorHtml(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_lastErrorHtml(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getLastErrorText(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_lastErrorText(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getLastErrorXml(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_lastErrorXml(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getMachineKeyset(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getOcspUrl(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_ocspUrl(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getPrivateKeyExportable(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getRevoked(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getRfc822Name(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_rfc822Name(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getSelfSigned(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSerialNumber(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_serialNumber(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSha1Thumbprint(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_sha1Thumbprint(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getSignatureVerified(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getSilent(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectC(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectC(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectCN(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectCN(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectDN(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectDN(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectE(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectE(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectKeyId(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectKeyId(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectL(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectL(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectO(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectO(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectOU(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectOU(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getSubjectS(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_subjectS(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getTrustedRoot(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getValidFrom(HCkCertW cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkCertW_getValidFromStr(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_validFromStr(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_getValidTo(HCkCertW cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkCertW_getValidToStr(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_validToStr(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_getVerboseLogging(HCkCertW cHandle);
CK_VISIBLE_PUBLIC void CkCertW_putVerboseLogging(HCkCertW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCertW_getVersion(HCkCertW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_version(HCkCertW cHandle);
CK_VISIBLE_PUBLIC int CkCertW_CheckRevoked(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_ExportCertDer(HCkCertW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCertW_ExportCertDerFile(HCkCertW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCertW_ExportCertPem(HCkCertW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_exportCertPem(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_ExportCertPemFile(HCkCertW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCertW_ExportCertXml(HCkCertW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_exportCertXml(HCkCertW cHandle);
CK_VISIBLE_PUBLIC HCkPrivateKeyW CkCertW_ExportPrivateKey(HCkCertW cHandle);
CK_VISIBLE_PUBLIC HCkPublicKeyW CkCertW_ExportPublicKey(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_ExportToPfxData(HCkCertW cHandle, const wchar_t *password, BOOL includeCertChain, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkCertW_ExportToPfxFile(HCkCertW cHandle, const wchar_t *pfxFilename, const wchar_t *pfxPassword, BOOL bIncludeCertChain);
CK_VISIBLE_PUBLIC HCkCertW CkCertW_FindIssuer(HCkCertW cHandle);
CK_VISIBLE_PUBLIC HCkCertChainW CkCertW_GetCertChain(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_GetEncoded(HCkCertW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_getEncoded(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_GetPrivateKeyPem(HCkCertW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCertW_getPrivateKeyPem(HCkCertW cHandle);
CK_VISIBLE_PUBLIC HCkDateTimeW CkCertW_GetValidFromDt(HCkCertW cHandle);
CK_VISIBLE_PUBLIC HCkDateTimeW CkCertW_GetValidToDt(HCkCertW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCertW_HasPrivateKey(HCkCertW cHandle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertW_LinkPrivateKey(HCkCertW cHandle, const wchar_t *keyContainerName, BOOL bMachineKeyset, BOOL bForSigning);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadByCommonName(HCkCertW cHandle, const wchar_t *cn);
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadByEmailAddress(HCkCertW cHandle, const wchar_t *emailAddress);
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadByIssuerAndSerialNumber(HCkCertW cHandle, const wchar_t *issuerCN, const wchar_t *serialNumber);
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadFromBase64(HCkCertW cHandle, const wchar_t *encodedCert);
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadFromBinary(HCkCertW cHandle, HCkByteData data);
#if !defined(CHILKAT_MONO)
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadFromBinary2(HCkCertW cHandle, const unsigned char *pByteData, unsigned long szByteData);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadFromFile(HCkCertW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadPfxData(HCkCertW cHandle, HCkByteData pfxData, const wchar_t *password);
#if !defined(CHILKAT_MONO)
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadPfxData2(HCkCertW cHandle, const unsigned char *pByteData, unsigned long szByteData, const wchar_t *password);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertW_LoadPfxFile(HCkCertW cHandle, const wchar_t *pfxPath, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkCertW_PemFileToDerFile(HCkCertW cHandle, const wchar_t *fromPath, const wchar_t *toPath);
CK_VISIBLE_PUBLIC BOOL CkCertW_SaveLastError(HCkCertW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCertW_SaveToFile(HCkCertW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCertW_SetFromEncoded(HCkCertW cHandle, const wchar_t *encodedCert);
CK_VISIBLE_PUBLIC BOOL CkCertW_SetPrivateKey(HCkCertW cHandle, HCkPrivateKeyW privKey);
CK_VISIBLE_PUBLIC BOOL CkCertW_SetPrivateKeyPem(HCkCertW cHandle, const wchar_t *privKeyPem);
CK_VISIBLE_PUBLIC BOOL CkCertW_UseCertVault(HCkCertW cHandle, HCkXmlCertVaultW vault);
CK_VISIBLE_PUBLIC BOOL CkCertW_VerifySignature(HCkCertW cHandle);
#endif
