// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDsa_H
#define _C_CkDsa_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDsa CkDsa_Create(void);
CK_VISIBLE_PUBLIC void CkDsa_Dispose(HCkDsa handle);
CK_VISIBLE_PUBLIC void CkDsa_getDebugLogFilePath(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDsa_putDebugLogFilePath(HCkDsa cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDsa_debugLogFilePath(HCkDsa cHandle);
CK_VISIBLE_PUBLIC int CkDsa_getGroupSize(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_putGroupSize(HCkDsa cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDsa_getHash(HCkDsa cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkDsa_putHash(HCkDsa cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC void CkDsa_getHexG(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_hexG(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getHexP(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_hexP(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getHexQ(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_hexQ(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getHexX(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_hexX(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getHexY(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_hexY(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getLastErrorHtml(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_lastErrorHtml(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getLastErrorText(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_lastErrorText(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getLastErrorXml(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_lastErrorXml(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_getSignature(HCkDsa cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkDsa_putSignature(HCkDsa cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC BOOL CkDsa_getUtf8(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_putUtf8(HCkDsa cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkDsa_getVerboseLogging(HCkDsa cHandle);
CK_VISIBLE_PUBLIC void CkDsa_putVerboseLogging(HCkDsa cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDsa_getVersion(HCkDsa cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDsa_version(HCkDsa cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromDer(HCkDsa cHandle, HCkByteData derData);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromDerFile(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromEncryptedPem(HCkDsa cHandle, const char *password, const char *pemData);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromPem(HCkDsa cHandle, const char *pemData);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromPublicDer(HCkDsa cHandle, HCkByteData derData);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromPublicDerFile(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromPublicPem(HCkDsa cHandle, const char *pemData);
CK_VISIBLE_PUBLIC BOOL CkDsa_FromXml(HCkDsa cHandle, const char *xmlKey);
CK_VISIBLE_PUBLIC BOOL CkDsa_GenKey(HCkDsa cHandle, int numBits);
CK_VISIBLE_PUBLIC BOOL CkDsa_GenKeyFromParamsDer(HCkDsa cHandle, HCkByteData derBytes);
CK_VISIBLE_PUBLIC BOOL CkDsa_GenKeyFromParamsDerFile(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_GenKeyFromParamsPem(HCkDsa cHandle, const char *pem);
CK_VISIBLE_PUBLIC BOOL CkDsa_GenKeyFromParamsPemFile(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_GetEncodedHash(HCkDsa cHandle, const char *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDsa_getEncodedHash(HCkDsa cHandle, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkDsa_GetEncodedSignature(HCkDsa cHandle, const char *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDsa_getEncodedSignature(HCkDsa cHandle, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkDsa_LoadText(HCkDsa cHandle, const char *path, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDsa_loadText(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_SaveLastError(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_SaveText(HCkDsa cHandle, const char *strToSave, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_SetEncodedHash(HCkDsa cHandle, const char *encoding, const char *encodedHash);
CK_VISIBLE_PUBLIC BOOL CkDsa_SetEncodedSignature(HCkDsa cHandle, const char *encoding, const char *encodedSig);
CK_VISIBLE_PUBLIC BOOL CkDsa_SetEncodedSignatureRS(HCkDsa cHandle, const char *encoding, const char *encodedR, const char *encodedS);
CK_VISIBLE_PUBLIC BOOL CkDsa_SetKeyExplicit(HCkDsa cHandle, int groupSizeInBytes, const char *pHex, const char *qHex, const char *gHex, const char *xHex);
CK_VISIBLE_PUBLIC BOOL CkDsa_SetPubKeyExplicit(HCkDsa cHandle, int groupSizeInBytes, const char *pHex, const char *qHex, const char *gHex, const char *yHex);
CK_VISIBLE_PUBLIC BOOL CkDsa_SignHash(HCkDsa cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToDer(HCkDsa cHandle, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToDerFile(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToEncryptedPem(HCkDsa cHandle, const char *password, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDsa_toEncryptedPem(HCkDsa cHandle, const char *password);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToPem(HCkDsa cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDsa_toPem(HCkDsa cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToPublicDer(HCkDsa cHandle, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToPublicDerFile(HCkDsa cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToPublicPem(HCkDsa cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDsa_toPublicPem(HCkDsa cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsa_ToXml(HCkDsa cHandle, BOOL bPublicOnly, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDsa_toXml(HCkDsa cHandle, BOOL bPublicOnly);
CK_VISIBLE_PUBLIC BOOL CkDsa_UnlockComponent(HCkDsa cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkDsa_Verify(HCkDsa cHandle);
CK_VISIBLE_PUBLIC BOOL CkDsa_VerifyKey(HCkDsa cHandle);
#endif
