// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHttpRequest_H
#define _C_CkHttpRequest_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHttpRequest CkHttpRequest_Create(void);
CK_VISIBLE_PUBLIC void CkHttpRequest_Dispose(HCkHttpRequest handle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getCharset(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequest_putCharset(HCkHttpRequest cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_charset(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getContentType(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequest_putContentType(HCkHttpRequest cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_contentType(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getDebugLogFilePath(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequest_putDebugLogFilePath(HCkHttpRequest cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_debugLogFilePath(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getEntireHeader(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequest_putEntireHeader(HCkHttpRequest cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_entireHeader(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getHttpVerb(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequest_putHttpVerb(HCkHttpRequest cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_httpVerb(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getHttpVersion(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequest_putHttpVersion(HCkHttpRequest cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_httpVersion(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getLastErrorHtml(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_lastErrorHtml(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getLastErrorText(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_lastErrorText(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getLastErrorXml(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_lastErrorXml(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC int CkHttpRequest_getNumHeaderFields(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC int CkHttpRequest_getNumParams(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_getPath(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpRequest_putPath(HCkHttpRequest cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_path(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_getSendCharset(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_putSendCharset(HCkHttpRequest cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_getUtf8(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_putUtf8(HCkHttpRequest cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_getVerboseLogging(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_putVerboseLogging(HCkHttpRequest cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHttpRequest_getVersion(HCkHttpRequest cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_version(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_AddBytesForUpload(HCkHttpRequest cHandle, const char *name, const char *remoteFileName, HCkByteData byteData);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_AddBytesForUpload2(HCkHttpRequest cHandle, const char *name, const char *remoteFileName, HCkByteData byteData, const char *contentType);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_AddFileForUpload(HCkHttpRequest cHandle, const char *name, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_AddFileForUpload2(HCkHttpRequest cHandle, const char *name, const char *filePath, const char *contentType);
CK_VISIBLE_PUBLIC void CkHttpRequest_AddHeader(HCkHttpRequest cHandle, const char *name, const char *value);
CK_VISIBLE_PUBLIC void CkHttpRequest_AddParam(HCkHttpRequest cHandle, const char *name, const char *value);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_AddStringForUpload(HCkHttpRequest cHandle, const char *name, const char *filename, const char *strData, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_AddStringForUpload2(HCkHttpRequest cHandle, const char *name, const char *filename, const char *strData, const char *charset, const char *contentType);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GenerateRequestText(HCkHttpRequest cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_generateRequestText(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GetHeaderField(HCkHttpRequest cHandle, const char *name, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_getHeaderField(HCkHttpRequest cHandle, const char *name);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GetHeaderName(HCkHttpRequest cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_getHeaderName(HCkHttpRequest cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GetHeaderValue(HCkHttpRequest cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_getHeaderValue(HCkHttpRequest cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GetParam(HCkHttpRequest cHandle, const char *name, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_getParam(HCkHttpRequest cHandle, const char *name);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GetParamName(HCkHttpRequest cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_getParamName(HCkHttpRequest cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GetParamValue(HCkHttpRequest cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_getParamValue(HCkHttpRequest cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_GetUrlEncodedParams(HCkHttpRequest cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpRequest_getUrlEncodedParams(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_LoadBodyFromBytes(HCkHttpRequest cHandle, HCkByteData byteData);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_LoadBodyFromFile(HCkHttpRequest cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_LoadBodyFromString(HCkHttpRequest cHandle, const char *bodyStr, const char *charset);
CK_VISIBLE_PUBLIC void CkHttpRequest_RemoveAllParams(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_RemoveHeader(HCkHttpRequest cHandle, const char *name);
CK_VISIBLE_PUBLIC void CkHttpRequest_RemoveParam(HCkHttpRequest cHandle, const char *name);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_SaveLastError(HCkHttpRequest cHandle, const char *path);
CK_VISIBLE_PUBLIC void CkHttpRequest_SetFromUrl(HCkHttpRequest cHandle, const char *url);
CK_VISIBLE_PUBLIC BOOL CkHttpRequest_StreamBodyFromFile(HCkHttpRequest cHandle, const char *filePath);
CK_VISIBLE_PUBLIC void CkHttpRequest_UseGet(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_UseHead(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_UsePost(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_UsePostMultipartForm(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_UsePut(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_UseUpload(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_UseUploadPut(HCkHttpRequest cHandle);
CK_VISIBLE_PUBLIC void CkHttpRequest_UseXmlHttp(HCkHttpRequest cHandle, const char *xmlBody);
#endif
