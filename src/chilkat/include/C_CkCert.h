// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCert_H
#define _C_CkCert_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCert CkCert_Create(void);
CK_VISIBLE_PUBLIC void CkCert_Dispose(HCkCert handle);
CK_VISIBLE_PUBLIC void CkCert_getAuthorityKeyId(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_authorityKeyId(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getAvoidWindowsPkAccess(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_putAvoidWindowsPkAccess(HCkCert cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC int CkCert_getCertVersion(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getCspName(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_cspName(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getDebugLogFilePath(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCert_putDebugLogFilePath(HCkCert cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCert_debugLogFilePath(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getExpired(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getForClientAuthentication(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getForCodeSigning(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getForSecureEmail(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getForServerAuthentication(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getForTimeStamping(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getHasKeyContainer(HCkCert cHandle);
CK_VISIBLE_PUBLIC unsigned long CkCert_getIntendedKeyUsage(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getIsRoot(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerC(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerC(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerCN(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerCN(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerDN(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerDN(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerE(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerE(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerL(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerL(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerO(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerO(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerOU(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerOU(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getIssuerS(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_issuerS(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getKeyContainerName(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_keyContainerName(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getLastErrorHtml(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_lastErrorHtml(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getLastErrorText(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_lastErrorText(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getLastErrorXml(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_lastErrorXml(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getMachineKeyset(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getOcspUrl(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_ocspUrl(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getPrivateKeyExportable(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getRevoked(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getRfc822Name(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_rfc822Name(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getSelfSigned(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSerialNumber(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_serialNumber(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSha1Thumbprint(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_sha1Thumbprint(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getSignatureVerified(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getSilent(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectC(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectC(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectCN(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectCN(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectDN(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectDN(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectE(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectE(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectKeyId(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectKeyId(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectL(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectL(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectO(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectO(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectOU(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectOU(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getSubjectS(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_subjectS(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getTrustedRoot(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getUtf8(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_putUtf8(HCkCert cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCert_getValidFrom(HCkCert cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkCert_getValidFromStr(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_validFromStr(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_getValidTo(HCkCert cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkCert_getValidToStr(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_validToStr(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_getVerboseLogging(HCkCert cHandle);
CK_VISIBLE_PUBLIC void CkCert_putVerboseLogging(HCkCert cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCert_getVersion(HCkCert cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCert_version(HCkCert cHandle);
CK_VISIBLE_PUBLIC int CkCert_CheckRevoked(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_ExportCertDer(HCkCert cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCert_ExportCertDerFile(HCkCert cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCert_ExportCertPem(HCkCert cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCert_exportCertPem(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_ExportCertPemFile(HCkCert cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCert_ExportCertXml(HCkCert cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCert_exportCertXml(HCkCert cHandle);
CK_VISIBLE_PUBLIC HCkPrivateKey CkCert_ExportPrivateKey(HCkCert cHandle);
CK_VISIBLE_PUBLIC HCkPublicKey CkCert_ExportPublicKey(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_ExportToPfxData(HCkCert cHandle, const char *password, BOOL includeCertChain, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkCert_ExportToPfxFile(HCkCert cHandle, const char *pfxFilename, const char *pfxPassword, BOOL bIncludeCertChain);
CK_VISIBLE_PUBLIC HCkCert CkCert_FindIssuer(HCkCert cHandle);
CK_VISIBLE_PUBLIC HCkCertChain CkCert_GetCertChain(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_GetEncoded(HCkCert cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCert_getEncoded(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_GetPrivateKeyPem(HCkCert cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCert_getPrivateKeyPem(HCkCert cHandle);
CK_VISIBLE_PUBLIC HCkDateTime CkCert_GetValidFromDt(HCkCert cHandle);
CK_VISIBLE_PUBLIC HCkDateTime CkCert_GetValidToDt(HCkCert cHandle);
CK_VISIBLE_PUBLIC BOOL CkCert_HasPrivateKey(HCkCert cHandle);
#if defined(CK_CRYPTOAPI_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCert_LinkPrivateKey(HCkCert cHandle, const char *keyContainerName, BOOL bMachineKeyset, BOOL bForSigning);
#endif
CK_VISIBLE_PUBLIC BOOL CkCert_LoadByCommonName(HCkCert cHandle, const char *cn);
CK_VISIBLE_PUBLIC BOOL CkCert_LoadByEmailAddress(HCkCert cHandle, const char *emailAddress);
CK_VISIBLE_PUBLIC BOOL CkCert_LoadByIssuerAndSerialNumber(HCkCert cHandle, const char *issuerCN, const char *serialNumber);
CK_VISIBLE_PUBLIC BOOL CkCert_LoadFromBase64(HCkCert cHandle, const char *encodedCert);
CK_VISIBLE_PUBLIC BOOL CkCert_LoadFromBinary(HCkCert cHandle, HCkByteData data);
#if !defined(CHILKAT_MONO)
CK_VISIBLE_PUBLIC BOOL CkCert_LoadFromBinary2(HCkCert cHandle, const unsigned char *pByteData, unsigned long szByteData);
#endif
CK_VISIBLE_PUBLIC BOOL CkCert_LoadFromFile(HCkCert cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCert_LoadPfxData(HCkCert cHandle, HCkByteData pfxData, const char *password);
#if !defined(CHILKAT_MONO)
CK_VISIBLE_PUBLIC BOOL CkCert_LoadPfxData2(HCkCert cHandle, const unsigned char *pByteData, unsigned long szByteData, const char *password);
#endif
CK_VISIBLE_PUBLIC BOOL CkCert_LoadPfxFile(HCkCert cHandle, const char *pfxPath, const char *password);
CK_VISIBLE_PUBLIC BOOL CkCert_PemFileToDerFile(HCkCert cHandle, const char *fromPath, const char *toPath);
CK_VISIBLE_PUBLIC BOOL CkCert_SaveLastError(HCkCert cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCert_SaveToFile(HCkCert cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCert_SetFromEncoded(HCkCert cHandle, const char *encodedCert);
CK_VISIBLE_PUBLIC BOOL CkCert_SetPrivateKey(HCkCert cHandle, HCkPrivateKey privKey);
CK_VISIBLE_PUBLIC BOOL CkCert_SetPrivateKeyPem(HCkCert cHandle, const char *privKeyPem);
CK_VISIBLE_PUBLIC BOOL CkCert_UseCertVault(HCkCert cHandle, HCkXmlCertVault vault);
CK_VISIBLE_PUBLIC BOOL CkCert_VerifySignature(HCkCert cHandle);
#endif
