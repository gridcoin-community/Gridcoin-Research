// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDirTree_H
#define _C_CkDirTree_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDirTree CkDirTree_Create(void);
CK_VISIBLE_PUBLIC void CkDirTree_Dispose(HCkDirTree handle);
CK_VISIBLE_PUBLIC void CkDirTree_getBaseDir(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDirTree_putBaseDir(HCkDirTree cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDirTree_baseDir(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_getDebugLogFilePath(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDirTree_putDebugLogFilePath(HCkDirTree cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDirTree_debugLogFilePath(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTree_getDoneIterating(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC int CkDirTree_getFileSize32(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC __int64 CkDirTree_getFileSize64(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_getFullPath(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDirTree_fullPath(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_getFullUncPath(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDirTree_fullUncPath(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTree_getIsDirectory(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_getLastErrorHtml(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDirTree_lastErrorHtml(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_getLastErrorText(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDirTree_lastErrorText(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_getLastErrorXml(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDirTree_lastErrorXml(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTree_getRecurse(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_putRecurse(HCkDirTree cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDirTree_getRelativePath(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDirTree_relativePath(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTree_getUtf8(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_putUtf8(HCkDirTree cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkDirTree_getVerboseLogging(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC void CkDirTree_putVerboseLogging(HCkDirTree cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDirTree_getVersion(HCkDirTree cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDirTree_version(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTree_AdvancePosition(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTree_BeginIterate(HCkDirTree cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTree_SaveLastError(HCkDirTree cHandle, const char *path);
#endif
