// This is a generated source file for Chilkat version 9.5.0.40
#if defined(WIN32) || defined(WINCE)

#ifndef _C_CkCreateCSWH
#define _C_CkCreateCSWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCreateCSW CkCreateCSW_Create(void);
CK_VISIBLE_PUBLIC void CkCreateCSW_Dispose(HCkCreateCSW handle);
CK_VISIBLE_PUBLIC void CkCreateCSW_getDebugLogFilePath(HCkCreateCSW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCreateCSW_putDebugLogFilePath(HCkCreateCSW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCreateCSW_debugLogFilePath(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC void CkCreateCSW_getLastErrorHtml(HCkCreateCSW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCreateCSW_lastErrorHtml(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC void CkCreateCSW_getLastErrorText(HCkCreateCSW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCreateCSW_lastErrorText(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC void CkCreateCSW_getLastErrorXml(HCkCreateCSW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCreateCSW_lastErrorXml(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCreateCSW_getReadOnly(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC void CkCreateCSW_putReadOnly(HCkCreateCSW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCreateCSW_getVerboseLogging(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC void CkCreateCSW_putVerboseLogging(HCkCreateCSW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCreateCSW_getVersion(HCkCreateCSW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCreateCSW_version(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_CreateFileStore(HCkCreateCSW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_CreateMemoryStore(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_CreateRegistryStore(HCkCreateCSW cHandle, const wchar_t *regRoot, const wchar_t *regPath);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_OpenChilkatStore(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_OpenCurrentUserStore(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_OpenFileStore(HCkCreateCSW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_OpenLocalSystemStore(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_OpenOutlookStore(HCkCreateCSW cHandle);
CK_VISIBLE_PUBLIC HCkCertStoreW CkCreateCSW_OpenRegistryStore(HCkCreateCSW cHandle, const wchar_t *regRoot, const wchar_t *regPath);
CK_VISIBLE_PUBLIC BOOL CkCreateCSW_SaveLastError(HCkCreateCSW cHandle, const wchar_t *path);
#endif

#endif // WIN32 (entire file)
