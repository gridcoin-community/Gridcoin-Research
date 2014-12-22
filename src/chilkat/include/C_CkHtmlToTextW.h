// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHtmlToTextWH
#define _C_CkHtmlToTextWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHtmlToTextW CkHtmlToTextW_Create(void);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_Dispose(HCkHtmlToTextW handle);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_getDebugLogFilePath(HCkHtmlToTextW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_putDebugLogFilePath(HCkHtmlToTextW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToTextW_debugLogFilePath(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_getDecodeHtmlEntities(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_putDecodeHtmlEntities(HCkHtmlToTextW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_getLastErrorHtml(HCkHtmlToTextW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToTextW_lastErrorHtml(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_getLastErrorText(HCkHtmlToTextW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToTextW_lastErrorText(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_getLastErrorXml(HCkHtmlToTextW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToTextW_lastErrorXml(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC int CkHtmlToTextW_getRightMargin(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_putRightMargin(HCkHtmlToTextW cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_getSuppressLinks(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_putSuppressLinks(HCkHtmlToTextW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_getVerboseLogging(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_putVerboseLogging(HCkHtmlToTextW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToTextW_getVersion(HCkHtmlToTextW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToTextW_version(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_IsUnlocked(HCkHtmlToTextW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_ReadFileToString(HCkHtmlToTextW cHandle, const wchar_t *filename, const wchar_t *srcCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToTextW_readFileToString(HCkHtmlToTextW cHandle, const wchar_t *filename, const wchar_t *srcCharset);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_SaveLastError(HCkHtmlToTextW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_ToText(HCkHtmlToTextW cHandle, const wchar_t *html, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToTextW_toText(HCkHtmlToTextW cHandle, const wchar_t *html);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_UnlockComponent(HCkHtmlToTextW cHandle, const wchar_t *code);
CK_VISIBLE_PUBLIC BOOL CkHtmlToTextW_WriteStringToFile(HCkHtmlToTextW cHandle, const wchar_t *stringToWrite, const wchar_t *filename, const wchar_t *outpuCharset);
#endif
