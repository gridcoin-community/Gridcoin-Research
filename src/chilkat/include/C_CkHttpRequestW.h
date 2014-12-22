// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHttpRequestWH
#define _C_CkHttpRequestWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHttpRequestW CkHttpRequestW_Create(void);
CK_VISIBLE_PUBLIC void CkHttpRequestW_Dispose(HCkHttpRequestW handle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getCharset(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putCharset(HCkHttpRequestW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_charset(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getContentType(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putContentType(HCkHttpRequestW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_contentType(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getDebugLogFilePath(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putDebugLogFilePath(HCkHttpRequestW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_debugLogFilePath(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getEntireHeader(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putEntireHeader(HCkHttpRequestW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_entireHeader(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getHttpVerb(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putHttpVerb(HCkHttpRequestW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_httpVerb(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getHttpVersion(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putHttpVersion(HCkHttpRequestW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_httpVersion(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getLastErrorHtml(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_lastErrorHtml(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getLastErrorText(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_lastErrorText(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getLastErrorXml(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_lastErrorXml(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC int CkHttpRequestW_getNumHeaderFields(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC int CkHttpRequestW_getNumParams(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getPath(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putPath(HCkHttpRequestW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_path(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_getSendCharset(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putSendCharset(HCkHttpRequestW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_getVerboseLogging(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_putVerboseLogging(HCkHttpRequestW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHttpRequestW_getVersion(HCkHttpRequestW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_version(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_AddBytesForUpload(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *remoteFileName, HCkByteData byteData);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_AddBytesForUpload2(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *remoteFileName, HCkByteData byteData, const wchar_t *contentType);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_AddFileForUpload(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *filePath);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_AddFileForUpload2(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *filePath, const wchar_t *contentType);
CK_VISIBLE_PUBLIC void CkHttpRequestW_AddHeader(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *value);
CK_VISIBLE_PUBLIC void CkHttpRequestW_AddParam(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *value);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_AddStringForUpload(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *filename, const wchar_t *strData, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_AddStringForUpload2(HCkHttpRequestW cHandle, const wchar_t *name, const wchar_t *filename, const wchar_t *strData, const wchar_t *charset, const wchar_t *contentType);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GenerateRequestText(HCkHttpRequestW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_generateRequestText(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GetHeaderField(HCkHttpRequestW cHandle, const wchar_t *name, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_getHeaderField(HCkHttpRequestW cHandle, const wchar_t *name);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GetHeaderName(HCkHttpRequestW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_getHeaderName(HCkHttpRequestW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GetHeaderValue(HCkHttpRequestW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_getHeaderValue(HCkHttpRequestW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GetParam(HCkHttpRequestW cHandle, const wchar_t *name, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_getParam(HCkHttpRequestW cHandle, const wchar_t *name);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GetParamName(HCkHttpRequestW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_getParamName(HCkHttpRequestW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GetParamValue(HCkHttpRequestW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_getParamValue(HCkHttpRequestW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_GetUrlEncodedParams(HCkHttpRequestW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpRequestW_getUrlEncodedParams(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_LoadBodyFromBytes(HCkHttpRequestW cHandle, HCkByteData byteData);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_LoadBodyFromFile(HCkHttpRequestW cHandle, const wchar_t *filePath);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_LoadBodyFromString(HCkHttpRequestW cHandle, const wchar_t *bodyStr, const wchar_t *charset);
CK_VISIBLE_PUBLIC void CkHttpRequestW_RemoveAllParams(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_RemoveHeader(HCkHttpRequestW cHandle, const wchar_t *name);
CK_VISIBLE_PUBLIC void CkHttpRequestW_RemoveParam(HCkHttpRequestW cHandle, const wchar_t *name);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_SaveLastError(HCkHttpRequestW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC void CkHttpRequestW_SetFromUrl(HCkHttpRequestW cHandle, const wchar_t *url);
CK_VISIBLE_PUBLIC BOOL CkHttpRequestW_StreamBodyFromFile(HCkHttpRequestW cHandle, const wchar_t *filePath);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UseGet(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UseHead(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UsePost(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UsePostMultipartForm(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UsePut(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UseUpload(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UseUploadPut(HCkHttpRequestW cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequestW_UseXmlHttp(HCkHttpRequestW cHandle, const wchar_t *xmlBody);
#endif
