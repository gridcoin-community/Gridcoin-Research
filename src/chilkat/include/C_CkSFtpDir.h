// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkSFtpDir_H
#define _C_CkSFtpDir_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkSFtpDir CkSFtpDir_Create(void);
CK_VISIBLE_PUBLIC void CkSFtpDir_Dispose(HCkSFtpDir handle);
CK_VISIBLE_PUBLIC void CkSFtpDir_getDebugLogFilePath(HCkSFtpDir cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSFtpDir_putDebugLogFilePath(HCkSFtpDir cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkSFtpDir_debugLogFilePath(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDir_getLastErrorHtml(HCkSFtpDir cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSFtpDir_lastErrorHtml(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDir_getLastErrorText(HCkSFtpDir cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSFtpDir_lastErrorText(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDir_getLastErrorXml(HCkSFtpDir cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSFtpDir_lastErrorXml(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC int CkSFtpDir_getNumFilesAndDirs(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDir_getOriginalPath(HCkSFtpDir cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSFtpDir_originalPath(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC BOOL CkSFtpDir_getUtf8(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDir_putUtf8(HCkSFtpDir cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkSFtpDir_getVerboseLogging(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC void CkSFtpDir_putVerboseLogging(HCkSFtpDir cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkSFtpDir_getVersion(HCkSFtpDir cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSFtpDir_version(HCkSFtpDir cHandle);
CK_VISIBLE_PUBLIC HCkSFtpFile CkSFtpDir_GetFileObject(HCkSFtpDir cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkSFtpDir_GetFilename(HCkSFtpDir cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkSFtpDir_getFilename(HCkSFtpDir cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkSFtpDir_SaveLastError(HCkSFtpDir cHandle, const char *path);
#endif
