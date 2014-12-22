// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHtmlToXmlWH
#define _C_CkHtmlToXmlWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHtmlToXmlW CkHtmlToXmlW_Create(void);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_Dispose(HCkHtmlToXmlW handle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_getDebugLogFilePath(HCkHtmlToXmlW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_putDebugLogFilePath(HCkHtmlToXmlW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_debugLogFilePath(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_getDropCustomTags(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_putDropCustomTags(HCkHtmlToXmlW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_getHtml(HCkHtmlToXmlW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_putHtml(HCkHtmlToXmlW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_html(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_getLastErrorHtml(HCkHtmlToXmlW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_lastErrorHtml(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_getLastErrorText(HCkHtmlToXmlW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_lastErrorText(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_getLastErrorXml(HCkHtmlToXmlW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_lastErrorXml(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC int CkHtmlToXmlW_getNbsp(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_putNbsp(HCkHtmlToXmlW cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_getVerboseLogging(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_putVerboseLogging(HCkHtmlToXmlW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_getVersion(HCkHtmlToXmlW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_version(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_getXmlCharset(HCkHtmlToXmlW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_putXmlCharset(HCkHtmlToXmlW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_xmlCharset(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_ConvertFile(HCkHtmlToXmlW cHandle, const wchar_t *inHtmlPath, const wchar_t *destXmlPath);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_DropTagType(HCkHtmlToXmlW cHandle, const wchar_t *tagName);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_DropTextFormattingTags(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_IsUnlocked(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_ReadFile(HCkHtmlToXmlW cHandle, const wchar_t *path, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_ReadFileToString(HCkHtmlToXmlW cHandle, const wchar_t *filename, const wchar_t *srcCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_readFileToString(HCkHtmlToXmlW cHandle, const wchar_t *filename, const wchar_t *srcCharset);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_SaveLastError(HCkHtmlToXmlW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_SetHtmlBytes(HCkHtmlToXmlW cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_SetHtmlFromFile(HCkHtmlToXmlW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_ToXml(HCkHtmlToXmlW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_toXml(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_UndropTagType(HCkHtmlToXmlW cHandle, const wchar_t *tagName);
CK_VISIBLE_PUBLIC void CkHtmlToXmlW_UndropTextFormattingTags(HCkHtmlToXmlW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_UnlockComponent(HCkHtmlToXmlW cHandle, const wchar_t *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_WriteFile(HCkHtmlToXmlW cHandle, const wchar_t *path, HCkByteData fileData);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_WriteStringToFile(HCkHtmlToXmlW cHandle, const wchar_t *stringToWrite, const wchar_t *filename, const wchar_t *outpuCharset);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXmlW_Xml(HCkHtmlToXmlW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHtmlToXmlW_xml(HCkHtmlToXmlW cHandle);
#endif
