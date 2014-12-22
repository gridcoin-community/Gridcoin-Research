// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkSFtpDirWH
#define _C_CkSFtpDirWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkSFtpDirW CkSFtpDirW_Create(void);
CK_VISIBLE_PUBLIC void CkSFtpDirW_Dispose(HCkSFtpDirW handle);
CK_VISIBLE_PUBLIC void CkSFtpDirW_getDebugLogFilePath(HCkSFtpDirW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSFtpDirW_putDebugLogFilePath(HCkSFtpDirW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkSFtpDirW_debugLogFilePath(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDirW_getLastErrorHtml(HCkSFtpDirW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSFtpDirW_lastErrorHtml(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDirW_getLastErrorText(HCkSFtpDirW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSFtpDirW_lastErrorText(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDirW_getLastErrorXml(HCkSFtpDirW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSFtpDirW_lastErrorXml(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC int CkSFtpDirW_getNumFilesAndDirs(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDirW_getOriginalPath(HCkSFtpDirW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSFtpDirW_originalPath(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSFtpDirW_getVerboseLogging(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDirW_putVerboseLogging(HCkSFtpDirW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkSFtpDirW_getVersion(HCkSFtpDirW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSFtpDirW_version(HCkSFtpDirW cHandle);
CK_VISIBLE_PUBLIC HCkSFtpFileW CkSFtpDirW_GetFileObject(HCkSFtpDirW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkSFtpDirW_GetFilename(HCkSFtpDirW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSFtpDirW_getFilename(HCkSFtpDirW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkSFtpDirW_SaveLastError(HCkSFtpDirW cHandle, const wchar_t *path);
#endif
