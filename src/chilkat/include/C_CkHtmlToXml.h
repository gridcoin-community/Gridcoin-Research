// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHtmlToXml_H
#define _C_CkHtmlToXml_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHtmlToXml CkHtmlToXml_Create(void);
CK_VISIBLE_PUBLIC void CkHtmlToXml_Dispose(HCkHtmlToXml handle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_getDebugLogFilePath(HCkHtmlToXml cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToXml_putDebugLogFilePath(HCkHtmlToXml cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_debugLogFilePath(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_getDropCustomTags(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_putDropCustomTags(HCkHtmlToXml cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToXml_getHtml(HCkHtmlToXml cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToXml_putHtml(HCkHtmlToXml cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_html(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_getLastErrorHtml(HCkHtmlToXml cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_lastErrorHtml(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_getLastErrorText(HCkHtmlToXml cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_lastErrorText(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_getLastErrorXml(HCkHtmlToXml cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_lastErrorXml(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC int CkHtmlToXml_getNbsp(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_putNbsp(HCkHtmlToXml cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_getUtf8(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_putUtf8(HCkHtmlToXml cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_getVerboseLogging(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_putVerboseLogging(HCkHtmlToXml cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHtmlToXml_getVersion(HCkHtmlToXml cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_version(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_getXmlCharset(HCkHtmlToXml cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHtmlToXml_putXmlCharset(HCkHtmlToXml cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_xmlCharset(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_ConvertFile(HCkHtmlToXml cHandle, const char *inHtmlPath, const char *destXmlPath);
CK_VISIBLE_PUBLIC void CkHtmlToXml_DropTagType(HCkHtmlToXml cHandle, const char *tagName);
CK_VISIBLE_PUBLIC void CkHtmlToXml_DropTextFormattingTags(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_IsUnlocked(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_ReadFile(HCkHtmlToXml cHandle, const char *path, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_ReadFileToString(HCkHtmlToXml cHandle, const char *filename, const char *srcCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_readFileToString(HCkHtmlToXml cHandle, const char *filename, const char *srcCharset);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_SaveLastError(HCkHtmlToXml cHandle, const char *path);
CK_VISIBLE_PUBLIC void CkHtmlToXml_SetHtmlBytes(HCkHtmlToXml cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_SetHtmlFromFile(HCkHtmlToXml cHandle, const char *filename);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_ToXml(HCkHtmlToXml cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_toXml(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC void CkHtmlToXml_UndropTagType(HCkHtmlToXml cHandle, const char *tagName);
CK_VISIBLE_PUBLIC void CkHtmlToXml_UndropTextFormattingTags(HCkHtmlToXml cHandle);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_UnlockComponent(HCkHtmlToXml cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_WriteFile(HCkHtmlToXml cHandle, const char *path, HCkByteData fileData);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_WriteStringToFile(HCkHtmlToXml cHandle, const char *stringToWrite, const char *filename, const char *outpuCharset);
CK_VISIBLE_PUBLIC BOOL CkHtmlToXml_Xml(HCkHtmlToXml cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHtmlToXml_xml(HCkHtmlToXml cHandle);
#endif
