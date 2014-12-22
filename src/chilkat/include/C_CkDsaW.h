// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDsaWH
#define _C_CkDsaWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDsaW CkDsaW_Create(void);
CK_VISIBLE_PUBLIC void CkDsaW_Dispose(HCkDsaW handle);
CK_VISIBLE_PUBLIC void CkDsaW_getDebugLogFilePath(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDsaW_putDebugLogFilePath(HCkDsaW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_debugLogFilePath(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC int CkDsaW_getGroupSize(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_putGroupSize(HCkDsaW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDsaW_getHash(HCkDsaW cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkDsaW_putHash(HCkDsaW cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC void CkDsaW_getHexG(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_hexG(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getHexP(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_hexP(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getHexQ(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_hexQ(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getHexX(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_hexX(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getHexY(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_hexY(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getLastErrorHtml(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_lastErrorHtml(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getLastErrorText(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_lastErrorText(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getLastErrorXml(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_lastErrorXml(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_getSignature(HCkDsaW cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkDsaW_putSignature(HCkDsaW cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC BOOL CkDsaW_getVerboseLogging(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC void CkDsaW_putVerboseLogging(HCkDsaW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDsaW_getVersion(HCkDsaW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_version(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromDer(HCkDsaW cHandle, HCkByteData derData);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromDerFile(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromEncryptedPem(HCkDsaW cHandle, const wchar_t *password, const wchar_t *pemData);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromPem(HCkDsaW cHandle, const wchar_t *pemData);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromPublicDer(HCkDsaW cHandle, HCkByteData derData);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromPublicDerFile(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromPublicPem(HCkDsaW cHandle, const wchar_t *pemData);
CK_VISIBLE_PUBLIC BOOL CkDsaW_FromXml(HCkDsaW cHandle, const wchar_t *xmlKey);
CK_VISIBLE_PUBLIC BOOL CkDsaW_GenKey(HCkDsaW cHandle, int numBits);
CK_VISIBLE_PUBLIC BOOL CkDsaW_GenKeyFromParamsDer(HCkDsaW cHandle, HCkByteData derBytes);
CK_VISIBLE_PUBLIC BOOL CkDsaW_GenKeyFromParamsDerFile(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_GenKeyFromParamsPem(HCkDsaW cHandle, const wchar_t *pem);
CK_VISIBLE_PUBLIC BOOL CkDsaW_GenKeyFromParamsPemFile(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_GetEncodedHash(HCkDsaW cHandle, const wchar_t *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_getEncodedHash(HCkDsaW cHandle, const wchar_t *encoding);
CK_VISIBLE_PUBLIC BOOL CkDsaW_GetEncodedSignature(HCkDsaW cHandle, const wchar_t *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_getEncodedSignature(HCkDsaW cHandle, const wchar_t *encoding);
CK_VISIBLE_PUBLIC BOOL CkDsaW_LoadText(HCkDsaW cHandle, const wchar_t *path, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_loadText(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SaveLastError(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SaveText(HCkDsaW cHandle, const wchar_t *strToSave, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SetEncodedHash(HCkDsaW cHandle, const wchar_t *encoding, const wchar_t *encodedHash);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SetEncodedSignature(HCkDsaW cHandle, const wchar_t *encoding, const wchar_t *encodedSig);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SetEncodedSignatureRS(HCkDsaW cHandle, const wchar_t *encoding, const wchar_t *encodedR, const wchar_t *encodedS);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SetKeyExplicit(HCkDsaW cHandle, int groupSizeInBytes, const wchar_t *pHex, const wchar_t *qHex, const wchar_t *gHex, const wchar_t *xHex);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SetPubKeyExplicit(HCkDsaW cHandle, int groupSizeInBytes, const wchar_t *pHex, const wchar_t *qHex, const wchar_t *gHex, const wchar_t *yHex);
CK_VISIBLE_PUBLIC BOOL CkDsaW_SignHash(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToDer(HCkDsaW cHandle, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToDerFile(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToEncryptedPem(HCkDsaW cHandle, const wchar_t *password, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_toEncryptedPem(HCkDsaW cHandle, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToPem(HCkDsaW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_toPem(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToPublicDer(HCkDsaW cHandle, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToPublicDerFile(HCkDsaW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToPublicPem(HCkDsaW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_toPublicPem(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsaW_ToXml(HCkDsaW cHandle, BOOL bPublicOnly, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDsaW_toXml(HCkDsaW cHandle, BOOL bPublicOnly);
CK_VISIBLE_PUBLIC BOOL CkDsaW_UnlockComponent(HCkDsaW cHandle, const wchar_t *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkDsaW_Verify(HCkDsaW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsaW_VerifyKey(HCkDsaW cHandle);
#endif
