// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkPfxWH
#define _C_CkPfxWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkPfxW CkPfxW_Create(void);
CK_VISIBLE_PUBLIC void CkPfxW_Dispose(HCkPfxW handle);
CK_VISIBLE_PUBLIC void CkPfxW_getDebugLogFilePath(HCkPfxW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkPfxW_putDebugLogFilePath(HCkPfxW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkPfxW_debugLogFilePath(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC void CkPfxW_getLastErrorHtml(HCkPfxW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPfxW_lastErrorHtml(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC void CkPfxW_getLastErrorText(HCkPfxW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPfxW_lastErrorText(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC void CkPfxW_getLastErrorXml(HCkPfxW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPfxW_lastErrorXml(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC int CkPfxW_getNumCerts(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC int CkPfxW_getNumPrivateKeys(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC BOOL CkPfxW_getVerboseLogging(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC void CkPfxW_putVerboseLogging(HCkPfxW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkPfxW_getVersion(HCkPfxW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkPfxW_version(HCkPfxW cHandle);
CK_VISIBLE_PUBLIC HCkCertW CkPfxW_GetCert(HCkPfxW cHandle, int index);
CK_VISIBLE_PUBLIC HCkPrivateKeyW CkPfxW_GetPrivateKey(HCkPfxW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkPfxW_LoadPfxBytes(HCkPfxW cHandle, HCkByteData pfxData, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPfxW_LoadPfxEncoded(HCkPfxW cHandle, const wchar_t *encodedData, const wchar_t *encoding, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPfxW_LoadPfxFile(HCkPfxW cHandle, const wchar_t *path, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkPfxW_SaveLastError(HCkPfxW cHandle, const wchar_t *path);
#endif
