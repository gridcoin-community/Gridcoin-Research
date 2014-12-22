// This is a generated source file for Chilkat version 9.5.0.40
#if defined(WIN32) || defined(WINCE)

#ifndef _C_CkCreateCS_H
#define _C_CkCreateCS_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCreateCS CkCreateCS_Create(void);
CK_VISIBLE_PUBLIC void CkCreateCS_Dispose(HCkCreateCS handle);
CK_VISIBLE_PUBLIC void CkCreateCS_getDebugLogFilePath(HCkCreateCS cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCreateCS_putDebugLogFilePath(HCkCreateCS cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCreateCS_debugLogFilePath(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC void CkCreateCS_getLastErrorHtml(HCkCreateCS cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCreateCS_lastErrorHtml(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC void CkCreateCS_getLastErrorText(HCkCreateCS cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCreateCS_lastErrorText(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC void CkCreateCS_getLastErrorXml(HCkCreateCS cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCreateCS_lastErrorXml(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC BOOL CkCreateCS_getReadOnly(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC void CkCreateCS_putReadOnly(HCkCreateCS cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCreateCS_getUtf8(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC void CkCreateCS_putUtf8(HCkCreateCS cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCreateCS_getVerboseLogging(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC void CkCreateCS_putVerboseLogging(HCkCreateCS cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCreateCS_getVersion(HCkCreateCS cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCreateCS_version(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_CreateFileStore(HCkCreateCS cHandle, const char *path);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_CreateMemoryStore(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_CreateRegistryStore(HCkCreateCS cHandle, const char *regRoot, const char *regPath);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_OpenChilkatStore(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_OpenCurrentUserStore(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_OpenFileStore(HCkCreateCS cHandle, const char *path);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_OpenLocalSystemStore(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_OpenOutlookStore(HCkCreateCS cHandle);
CK_VISIBLE_PUBLIC HCkCertStore CkCreateCS_OpenRegistryStore(HCkCreateCS cHandle, const char *regRoot, const char *regPath);
CK_VISIBLE_PUBLIC BOOL CkCreateCS_SaveLastError(HCkCreateCS cHandle, const char *path);
#endif

#endif // WIN32 (entire file)
