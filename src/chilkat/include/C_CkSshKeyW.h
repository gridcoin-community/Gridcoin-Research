// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkSshKeyWH
#define _C_CkSshKeyWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkSshKeyW CkSshKeyW_Create(void);
CK_VISIBLE_PUBLIC void CkSshKeyW_Dispose(HCkSshKeyW handle);
CK_VISIBLE_PUBLIC void CkSshKeyW_getComment(HCkSshKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSshKeyW_putComment(HCkSshKeyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_comment(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC void CkSshKeyW_getDebugLogFilePath(HCkSshKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSshKeyW_putDebugLogFilePath(HCkSshKeyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_debugLogFilePath(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_getIsDsaKey(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_getIsPrivateKey(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_getIsRsaKey(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC void CkSshKeyW_getLastErrorHtml(HCkSshKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_lastErrorHtml(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC void CkSshKeyW_getLastErrorText(HCkSshKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_lastErrorText(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC void CkSshKeyW_getLastErrorXml(HCkSshKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_lastErrorXml(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC void CkSshKeyW_getPassword(HCkSshKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSshKeyW_putPassword(HCkSshKeyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_password(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_getVerboseLogging(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC void CkSshKeyW_putVerboseLogging(HCkSshKeyW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkSshKeyW_getVersion(HCkSshKeyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_version(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_FromOpenSshPrivateKey(HCkSshKeyW cHandle, const wchar_t *keyStr);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_FromOpenSshPublicKey(HCkSshKeyW cHandle, const wchar_t *keyStr);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_FromPuttyPrivateKey(HCkSshKeyW cHandle, const wchar_t *keyStr);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_FromRfc4716PublicKey(HCkSshKeyW cHandle, const wchar_t *keyStr);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_FromXml(HCkSshKeyW cHandle, const wchar_t *xmlKey);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_GenFingerprint(HCkSshKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_genFingerprint(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_GenerateDsaKey(HCkSshKeyW cHandle, int numBits);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_GenerateRsaKey(HCkSshKeyW cHandle, int numBits, int exponent);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_LoadText(HCkSshKeyW cHandle, const wchar_t *filename, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_loadText(HCkSshKeyW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_SaveLastError(HCkSshKeyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_SaveText(HCkSshKeyW cHandle, const wchar_t *strToSave, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_ToOpenSshPrivateKey(HCkSshKeyW cHandle, BOOL bEncrypt, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_toOpenSshPrivateKey(HCkSshKeyW cHandle, BOOL bEncrypt);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_ToOpenSshPublicKey(HCkSshKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_toOpenSshPublicKey(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_ToPuttyPrivateKey(HCkSshKeyW cHandle, BOOL bEncrypt, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_toPuttyPrivateKey(HCkSshKeyW cHandle, BOOL bEncrypt);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_ToRfc4716PublicKey(HCkSshKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_toRfc4716PublicKey(HCkSshKeyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSshKeyW_ToXml(HCkSshKeyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSshKeyW_toXml(HCkSshKeyW cHandle);
#endif
