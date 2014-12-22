// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDirTreeWH
#define _C_CkDirTreeWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDirTreeW CkDirTreeW_Create(void);
CK_VISIBLE_PUBLIC void CkDirTreeW_Dispose(HCkDirTreeW handle);
CK_VISIBLE_PUBLIC void CkDirTreeW_getBaseDir(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDirTreeW_putBaseDir(HCkDirTreeW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_baseDir(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_getDebugLogFilePath(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDirTreeW_putDebugLogFilePath(HCkDirTreeW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_debugLogFilePath(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTreeW_getDoneIterating(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC int CkDirTreeW_getFileSize32(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC __int64 CkDirTreeW_getFileSize64(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_getFullPath(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_fullPath(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_getFullUncPath(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_fullUncPath(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTreeW_getIsDirectory(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_getLastErrorHtml(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_lastErrorHtml(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_getLastErrorText(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_lastErrorText(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_getLastErrorXml(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_lastErrorXml(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTreeW_getRecurse(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_putRecurse(HCkDirTreeW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDirTreeW_getRelativePath(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_relativePath(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTreeW_getVerboseLogging(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC void CkDirTreeW_putVerboseLogging(HCkDirTreeW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDirTreeW_getVersion(HCkDirTreeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDirTreeW_version(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTreeW_AdvancePosition(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTreeW_BeginIterate(HCkDirTreeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDirTreeW_SaveLastError(HCkDirTreeW cHandle, const wchar_t *path);
#endif
