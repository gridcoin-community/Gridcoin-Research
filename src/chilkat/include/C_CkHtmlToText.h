// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHtmlToText_H
#define _C_CkHtmlToText_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHtmlToText CkHtmlToText_Create(void);
CK_VISIBLE_PUBLIC void CkHtmlToText_Dispose(HCkHtmlToText handle);
CK_VISIBLE_PUBLIC void CkHtmlToText_getDebugLogFilePath(HCkHtmlToText cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToText_putDebugLogFilePath(HCkHtmlToText cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHtmlToText_debugLogFilePath(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_getDecodeHtmlEntities(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToText_putDecodeHtmlEntities(HCkHtmlToText cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToText_getLastErrorHtml(HCkHtmlToText cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToText_lastErrorHtml(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToText_getLastErrorText(HCkHtmlToText cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToText_lastErrorText(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToText_getLastErrorXml(HCkHtmlToText cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToText_lastErrorXml(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC int CkHtmlToText_getRightMargin(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToText_putRightMargin(HCkHtmlToText cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_getSuppressLinks(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToText_putSuppressLinks(HCkHtmlToText cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_getUtf8(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToText_putUtf8(HCkHtmlToText cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_getVerboseLogging(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToText_putVerboseLogging(HCkHtmlToText cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToText_getVersion(HCkHtmlToText cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToText_version(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_IsUnlocked(HCkHtmlToText cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_ReadFileToString(HCkHtmlToText cHandle, const char *filename, const char *srcCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHtmlToText_readFileToString(HCkHtmlToText cHandle, const char *filename, const char *srcCharset);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_SaveLastError(HCkHtmlToText cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_ToText(HCkHtmlToText cHandle, const char *html, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHtmlToText_toText(HCkHtmlToText cHandle, const char *html);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_UnlockComponent(HCkHtmlToText cHandle, const char *code);
CK_VISIBLE_PUBLIC BOOL CkHtmlToText_WriteStringToFile(HCkHtmlToText cHandle, const char *stringToWrite, const char *filename, const char *outpuCharset);
#endif
